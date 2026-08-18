# 构建与实物验证

本文件只记录可复现的构建、联调步骤和实物结果。设计目标不在这里标记为 `PASS`。
当前开发摘要见 [`current_status.md`](current_status.md)；本文件保留带日期、版本和适用范围的
历史证据，整理或合并时不得因为最新摘要未重复这些内容而删除。

## 状态定义

| 状态 | 含义 |
| --- | --- |
| `BUILD PASS` | 指定构建配置成功 |
| `READY` | 初始化或测试入口存在，尚未完成实物验收 |
| `HARDWARE PASS` | 已按明确步骤完成实物验收 |
| `NOT VERIFIED` | 尚未验证或固件变化后需要回归 |
| `DEFERRED` | 已知尚未完成，按当前计划后置且不阻塞主线 |

## 已有基线

以下结果来自 2026-07 的开发板联调，后续修改相关驱动、安全逻辑或链接布局后需要回归。

| 项目 | 结果 | 依据 |
| --- | --- | --- |
| USART1 TX/RX DMA | `HARDWARE PASS` | 串口命令、响应和遥测双向工作 |
| RTC 读取 | `HARDWARE PASS` | LSE 计时连续 |
| RTC 掉电保持 | `HARDWARE PASS` | 仅保留备用电池约 5 分钟后时间连续 |
| QSPI JEDEC | `HARDWARE PASS` | `EF4017`，识别 8 MiB |
| QSPI 擦写 | `HARDWARE PASS` | 保留扇区 1 KiB DMA 写入和回读 |
| LCD SPI DMA | `HARDWARE PASS` | 封面和动态状态页显示正常 |
| LCD 四页状态显示 | `NOT VERIFIED` | 0.7.1 build1 已 OTA confirmed；中性配色、大号电量显示和新版布局尚待人工验收 |
| KEY 消抖 | `HARDWARE PASS` | 历史按键消抖已验证；0.7.1 PB8 四页循环需重新验收 |
| ADC 电压 | `HARDWARE PASS` | 11.96 V 电池，PA2 约 1.086 V |
| 双编码器 | `HARDWARE PASS` | 前进同为正、后退同为负 |
| 双电机开环 | `HARDWARE PASS` | 左右轮正反转和自动停止 |
| 急停 | `HARDWARE PASS` | PD2 触发故障并清零 PWM |
| IWDG 受控复位 | `HARDWARE PASS` | 复位后报告测试通过 |
| 外部 CAN FD | `HARDWARE PASS` | Jetson 与 STM32 三步握手双向通过 |
| 低速 PID 闭环 | `DEFERRED` | PID/100 Hz 控制和调参入口已实现；方向、停车和稳定性实物验收后置 |
| FreeRTOS 四任务运行字段 | `HARDWARE PASS` | 2026-08-18，0.3.0 build1 四任务均为 `RUNNING`，周期、栈余量、heartbeat 和运行次数可读 |
| 正式 UART 文本协议 | `HARDWARE PASS` | 2026-08-18，0.3.0 build1 的 `[LOG]`、`[RSP]`、四分区 `[TEL]`、错误响应、遥测和 CRLF 已实测 |
| HC-SR501 输入 | `DEFERRED` | b16 已验证 60 秒预热和低电平零误计数；模块指示灯未亮，高电平和事件计数后置 |
| Bootloader factory 启动 | `HARDWARE PASS` | DFU 烧录并校验组合镜像，普通复位后 LCD 进入 Application |
| UART OTA 主升级链路 | `HARDWARE PASS` | 2026-08-18，build22 已完成到 0.3.0、0.6.0、0.7.0 和 0.7.1 build1 的真实 UART 传输、安装、TRIAL 和 CONFIRMED 确认 |
| OTA 回滚、断电恢复与 CAN FD 传输 | `NOT VERIFIED` | 尚未完成故障注入、回滚和真实 CAN FD OTA 验收 |

## 构建基线

Application Release 于 2026-08-13 使用 CubeIDE 2.2.0 GCC clean build；Debug 行保留
2026-07-28 的最近结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 222948 | 96 | 34072 | `BUILD PASS`，2026-07-28，0 errors，0 warnings |
| Release | 183492 | 96 | 34224 | `BUILD PASS`，2026-08-13，b12，0 errors，0 warnings |

Debug/Release 的 `.isr_vector` 均位于 `0x08008000`；反汇编确认 `SystemInit()` 将
VTOR 写入 `0x08008000`。最高 Flash load 地址未超出 `0x0807FFFF`。构建结果只说明
当前 Application b12 源码可编译和链接，不代表本轮固件已通过实物启动；b6 仍是最近一次
完成 UART OTA 实物闭环的 Application。

Bootloader Release 于 2026-08-13 使用 CubeIDE 2.2.0 GCC clean build；Debug 行保留
2026-07-28 的最近结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 17652 | 44 | 1668 | `BUILD PASS`，2026-07-28，0 errors，0 warnings |
| Release | 13400 | 48 | 1656 | `BUILD PASS`，2026-08-13，build22，0 errors，0 warnings |

Bootloader 链接脚本只提供 `0x08000000` 起始的 32 KiB Flash。该结果仅证明编译和
链接通过，不证明 QSPI 安装、掉电恢复、Application 跳转或回滚已通过实物验证。
本次 build22 已于 2026-08-13 从当时的 Release ELF 生成正式配套产物。本段记录的是烧录前
状态；后续同日已完成匹配 factory/QSPI 基线写入并确认 build22 启动 b12，详见下一节。
build22 已于 2026-08-17 完成 b12 到 b13 的 UART OTA 全闭环验证。

### b12/build22 上板准备（2026-08-13）

从当前 Release ELF 重新执行 `arm-none-eabi-objcopy -O binary`，随后生成 OTA、内部 factory
组合镜像和 QSPI confirmed raw。工具输出确认 Application payload CRC32 为 `0x3A5DFAD0`，
QSPI metadata 为 `CONFIRMED/SLOT_A`。产物如下：

| 产物 | 字节数 | SHA-256 |
| --- | ---: | --- |
| `app-v0.1.0-b12.bin` | 183596 | `1A881962E44E298A8BE064834E5FF3CA81366B5CFB925004FF942FF03FD716FF` |
| `app-v0.1.0-b12.ota` | 183660 | `F7FF629B1E02C45CF582ABF96904E0E3500C00EBB7F6BB6F7358D8320505F6FF` |
| `boot-v0.1.0-b22.bin` | 13448 | `B56E787611A0FBA816E11801EE4BAF62DEB9D61A965B369D58A4A276B2A25358` |
| `factory-a12-b22.bin` | 216364 | `A7FB68D07022D7C12D88F2EA9D744B7E9EE249947B5E9E35F60A4ADAA9FBE243` |
| `qspi-a12-confirmed.bin` | 8388608 | `6A86EC1E297D525023CE8D024971E359B13939302C6B3D8A5EA439CD39A26F81` |

同轮重新运行 OTA Python 9 项测试，全部通过。COM9 可打开并返回 `PONG` 和完整 `status`；
在线固件报告 QSPI `EF4017`、`MOTOR: DISABLED`、`OTA_CONFIRM: NOT_REQUIRED`，但串口命令
不输出 Application/Bootloader 版本，不能据此认定板上已是 b12/build22。

STM32CubeProgrammer 2.23.0 未检测到 ST-Link 或 DFU 设备，安装目录也没有匹配本板
STM32G474 + W25Q64 自定义引脚的 External Loader。因此本轮未执行内部 Flash/QSPI 烧录，
当时冻结清单七项继续保持 `NOT VERIFIED`。

后续同日通过 STM32 ROM DFU 和独立
`tools/qspi_factory_provisioner/` 完成匹配基线写入。Provisioner 固定校验内部暂存的 b12
OTA package 和 `CONFIRMED/SLOT_A` Metadata A，擦除并读回确认 Metadata A/B，擦写 Slot A，
逐字节读回比较完整 183660 字节 package，最后提交并读回 Metadata A。板上日志为：

```text
PROVISION: INPUTS VALID
PROVISION: METADATA ERASED
PROVISION: SLOT A ERASED
PROVISION: SLOT A WRITTEN
PROVISION: SLOT A VERIFIED
PROVISION: METADATA A VERIFIED
PROVISION: METADATA B ERASED
PROVISION: PASS
```

随后通过 DFU 全擦内部临时工具和暂存数据，将 `factory-a12-b22.bin` 下载到
`0x08000000` 并由 CubeProgrammer 校验成功。启动日志确认：

```text
BOOT: VERSION=0.1.0 BUILD=22
BOOT: QSPI READY
BOOT: METADATA READY
BOOT: STATE=0x00000005
chassis-controller started
```

Application 健康窗口后 `status` 报告 QSPI `EF4017`、`OTA_CONFIRM: NOT_REQUIRED`、
`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`。因此匹配的 QSPI factory confirmed
基线写入和 b12/build22 confirmed 启动已完成实物验证。该启动由 DFU `go 0x08000000`
触发；仍需单独执行普通按键/断电复位回归。`MOTOR: DISABLED` 是软件状态证据，四路 PWM
上电电气零输出仍需示波器或逻辑分析仪测量。

2026-08-13 在当前工作树完成宿主机回归：

- `tools/ota/test_ota_transfer.py`：3 项 Python 测试通过，覆盖 UART 帧 CRC、CAN 响应
  布局和 stop-and-wait 分块状态流程。
- `tools/ota/test_factory_image.py`：3 项测试通过，确认 Slot A package、`CONFIRMED` Metadata A、
  CRC、擦除态 Metadata B、默认缺少 QSPI 参数时失败，以及显式 `--internal-only` 调试路径。
- `tools/ota/test_shared_abi.py`：3 项测试通过，直接解析共享 C 头文件并核对两个 Python
  打包工具的 Flash 地址、大小、magic、格式版本、枚举值，以及镜像头/metadata 的字段类型、
  声明顺序、数组长度、逐字段 offset、C `_Static_assert(offsetof(...))` 和 64 字节总大小。
- `test_command_manager.c`：WSL GCC 使用 `-std=c11 -Wall -Wextra -Werror` 编译运行通过，
  覆盖 OTA owner、运动命令清除、非法 source、Console 持续目标和 CAN 200 ms 超时。
- `test_ota_metadata.c`：同参数编译运行通过，覆盖 metadata 状态和严格槽约束。
- `test_ota_uart_arm_guard.c`：同参数编译运行通过，覆盖等待 BEGIN 的 30 秒超时、
  active session 和 tracked response 未完成时禁止禁用 UART transport。
- `bootloader_core_test.c`：此前使用同参数编译运行通过，覆盖 CRC32 标准向量、镜像校验、
  metadata 双副本选择、factory 擦除态识别、candidate/confirmed 安装三次上限、attempt 1/2/3
  中断后的 `TRIAL/CONFIRMED` salvage、无效镜像重装边界和 confirmed size/CRC 回填。
- 本轮继续新增恢复 source 状态和“仅 internal mismatch 允许重装”的纯策略断言。
  ARM GCC 使用 `-std=c11 -Wall -Wextra -Werror` 编译通过；Windows 环境无桌面 GCC，WSL
  启动返回 `E_ACCESSDENIED`，因此新增断言尚未在宿主机执行。目标 build22 已编译链接通过。

本轮新增的可诊断 confirmed 恢复、I/O 重试、confirmed fatal 处理和编译器 ABI 断言均只完成
代码审查、主机测试或目标构建，尚未完成目标板故障注入/烧录回归。

主机测试和目标构建均不替代 STM32 实物验收。

### Bootloader 启动对照

2026-07-28 使用同一 relocated Application 进行了单变量对照：

| 组合 | Bootloader 时钟 | Bootloader IWDG | 启动结果 |
| --- | --- | --- | --- |
| 候选 I | 16 MHz HSI | 禁用，诊断直跳 | Application LCD 和串口正常 |
| 候选 K | 16 MHz HSI | 启动，约 32 秒 | 黑屏，串口无响应 |
| 候选 L | 16 MHz HSI | 普通路径不启动，完整状态机 | LCD 正常，`status` 完整返回 |
| 候选 N | 170 MHz HSE/PLL | 普通路径不启动，完整状态机 | 黑屏，串口无响应 |
| 正式 factory | 16 MHz HSI | 不启动，仅刷新继承实例 | DFU 校验成功，普通复位后 LCD 正常 |

因此正式 Bootloader 使用 16 MHz HSI，任何路径都不主动启动或重配置 IWDG，只刷新
Application 在软件复位前已启动的实例。Recovery 不刷新，让继承的 IWDG 触发复位；
冷启动且 IWDG 未运行时，直接写 reload key 不会启动它。Application 启动后重新配置
正常周期并负责持续刷新。

### UART OTA 实物闭环（2026-07-30）

使用 `_output/application/app-v0.1.0-b6.ota`
在 COM8、115200 8N1 下完成真实传输：

```text
OTA staged over UART: next_offset=182856
BOOT: STATE=0x00000002
BOOT: INSTALL CANDIDATE
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: STATE=0x00000004
BOOT: TRIAL VERIFIED
BOOT: JUMP APPLICATION
OTA_CONFIRM: CONFIRMED
```

最终 Bootloader build15 经 DFU 烧录和校验后，普通复位得到：

```text
BOOT: STATE=0x00000005
QSPI_ID: PASS jedec=EF4017 capacity=8MiB
MOTOR: DISABLED
CONTROL_OVERRUN: count=0 missed=0
```

本次同时确认 Bootloader 必须提供 `SysTick_Handler()` 并将 VTOR 显式设置为
`0x08000000`；缺少 SysTick ISR 时首个 1 ms 中断会进入默认处理器，表现为启动日志被截断。
最终产物为：

```text
_output/bootloader/boot-v0.1.0-b15.bin 11684 bytes
_output/application/app-v0.1.0-b6.bin 182792 bytes
_output/application/app-v0.1.0-b6.ota 182856 bytes
_output/factory/factory-a6-b15.bin 215560 bytes
```

UART OTA 主链已达到 `HARDWARE PASS`。CAN FD OTA、安装中断电、TRIAL 失败回滚和无有效
Application 的 Recovery 行为仍为 `NOT VERIFIED`。

### b13 无 IMU启动与 UART OTA 闭环（2026-08-17）

使用 CMake 3.22、Ninja 和 GNU Arm Embedded 14.3.rel1 构建 b13 Release。Application 结果为
`text=185388 data=120 bss=34944`，生成 payload 185516 字节、CRC32 `0x6FD23D35` 的
`app-v0.1.0-b13.ota`；OTA Python 9 项通过。

目标板起点为 build22 + confirmed b12。`send_uart.py` 通过 `/dev/ttyUSB0`、115200 8N1
完成 185580 字节 package 传输并收到：

```text
OTA staged over UART: session=193 next_offset=185580; device reset expected
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: STATE=0x00000004
BOOT: TRIAL VERIFIED
```

b13 在未连接 ICM45686 时完成启动和 5 秒健康确认：

```text
ICM45686: NOT_INITIALIZED whoami=0x00 samples=0
LCD: READY
MOTOR: DISABLED
CONTROL_OVERRUN: count=0 missed=0
OTA_CONFIRM: CONFIRMED
```

随后通过 ST-Link 执行普通复位，Bootloader 报告 `STATE=0x00000005`，Application 再次正常
启动；等待健康窗口后 `status` 报告 `OTA_CONFIRM: NOT_REQUIRED`，屏幕保持点亮，未观察到
critical fault 或 IWDG 复位循环。因此 b13 无 ICM45686 启动和 build22 UART OTA 主链均为
`HARDWARE PASS`。该结果不包含四路 PWM 电气测量、断电启动、CAN FD OTA 或三个故障注入测试。

## CMake/Ninja 等价构建（2026-08-13）

使用 GNU Arm Embedded 14.3.rel1（GCC 14.3.1）、CMake 和 Ninja clean build 三个目标。
`--specs=nano.specs` 同时用于 C 编译和链接，以保持 newlib-nano 头文件 ABI 与 CubeIDE
一致。结果如下：

| 目标 | text | data | bss | BIN SHA-256 | 结果 |
| --- | ---: | ---: | ---: | --- | --- |
| Application | 183492 | 96 | 34224 | `1A881962E44E298A8BE064834E5FF3CA81366B5CFB925004FF942FF03FD716FF` | `BUILD PASS`，与 b12 基线一致 |
| Bootloader | 13400 | 48 | 1656 | `B56E787611A0FBA816E11801EE4BAF62DEB9D61A965B369D58A4A276B2A25358` | `BUILD PASS`，与 build22 基线一致 |
| QSPI provisioner | 9192 | 12 | 1644 | `01EF25D46787D45A91B66C7BAA481990DADA5E1C5C6C789B4375A42B8CF70E30` | `BUILD PASS`，与已上板工具一致 |

Application `.isr_vector` 位于 `0x08008000`，Bootloader 和 provisioner `.isr_vector` 位于
`0x08000000`。同轮 OTA Python 9 项测试全部通过。三个 BIN 均与已经生成或上板验证的基线
逐哈希一致，因此不需要为 CMake 构建重复烧录同一字节产物。

## 分阶段路线阶段 1a：FreeRTOS 运行基础（四任务迁移前，2026-08-17）

在保留两任务基线和既有电机/安全路径的前提下，新增双缓冲 `SystemStatusSnapshot`，统一记录
板级、传感器、通信、供电、RTC、OTA 基础状态，以及任务 uptime、实际/期望周期、超时、运行
次数、运行状态、heartbeat age、栈余量和 RCC 复位原因；`status` 与 LCD 当前页读取同一快照。
四任务目标模型已写入 `architecture.md`，该轮记录的是迁移前快照基础。该结果只证明代码构建和链接，
不代表新增运行字段已完成目标板实物验收。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Application Debug | 196660 | 120 | 43112 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |
| Application Release | 187948 | 120 | 43104 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |

执行命令：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

本阶段未执行目标板烧录或 FreeRTOS 运行字段实物验证，保持 `NOT VERIFIED`。在该轮记录时四任务
拆分、正式 UART `OK/ERROR`、`LOG`/`TEL` 和 LCD 四页仍未实施；不得用该轮构建结果替代后续阶段
的验证。

## 分阶段路线阶段 1b：四任务调度迁移（2026-08-17）

在保留控制任务 100 Hz 通知、OTA 维护锁和电机安全路径的前提下，完成四任务实际迁移：
`service_task` 负责 UART/CAN/Console/OTA/遥测，`diagnostics_task` 负责 RTC、ADC、电源、
IMU、SR501 和双缓冲快照，`display_task` 负责按键和 LCD。四个任务均纳入周期、超时、运行
状态、运行次数、栈余量和 heartbeat 诊断；关键任务健康仍是 IWDG 刷新条件。该结果只证明代码
构建和链接，不代表目标板运行字段或显示行为已完成实物验收。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Application Debug | 198048 | 120 | 48736 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |
| Application Release | 189216 | 120 | 48728 | `BUILD PASS`，CMake/Ninja，GNU Arm Embedded 14.3.rel1 |

执行命令：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

本阶段未执行目标板烧录。正式 UART `OK/ERROR`、`LOG`/`TEL` emitter 和 LCD 四页仍未实施，
ICM45686 多传感器时间轴、Kalman 以及 SR501 高电平事件验证继续按路线后置。

## CubeIDE 构建与烧录

1. 以仓库 `firmware/` 为工作区，分别导入 `application/stm32g474/` 和
   `bootloader/stm32g474/`。
2. 选择 `Debug` 或 `Release`，执行 `Project > Clean` 后再执行 `Build Project`。
3. 确认生成的 `Debug/chassis_controller.elf` 或
   `Release/chassis_controller.elf` 时间晚于本次源码修改。CubeIDE 当前不会自动更新
   BIN，OTA 打包前必须从最新 ELF 重新运行 `arm-none-eabi-objcopy -O binary`。
4. 日常调试使用 ST-Link；使用 USB Type-C DFU 时，通过 STM32CubeProgrammer
   选择正确的 DFU 设备和 ELF/HEX/BIN，再烧录并复位。
5. 烧录后的串口启动信息和固件版本必须与本次产物一致。

CubeMX 重新生成后必须重新检查自定义 source folder、include path 和 FreeRTOS 配置，
不能用旧 ELF 判断当前代码。

## VS Code Cortex-Debug

仓库共享 `.vscode/tasks.json` 和 `.vscode/launch.json`。F5 依次配置、构建 `arm-debug`，
启动 OpenOCD，加载并烧写 `build/arm-debug/application.elf`，复位后停在 `main`。所需命令
`cmake`、`openocd` 和 `arm-none-eabi-gdb` 必须在 `PATH`，VS Code 需安装 Cortex-Debug。

2026-08-14 软件侧检查：Debug ELF 含 DWARF，GDB 可定位 `main.c:77`，并可解析
`pxCurrentTCB`；Debug Application 含 `APP_DEBUG_IWDG_FREEZE`，Release compile commands
确认不含该宏。

2026-08-15 目标板检查：ST-Link `0483:3748` 和 CH340 已在 Ubuntu 虚拟机枚举；OpenOCD
不使用 `sudo` 启动，通过 ST-Link SWD 识别 STM32G47/G48、目标电压约 3.24 V，并在
3333 端口提供 GDB server。`arm-none-eabi-gdb build/arm-debug/application.elf` 已完成
`load` 烧写 Debug ELF，命中 `main()` 源码断点 `Core/Src/main.c:77`。

同一会话继续验证源码断点和运行状态：`ChassisApp_Init()` 在 `app/chassis_app.c:80` 命中，
Call Stack 回到 `main.c:121`；`ControlTaskMain(argument=0x0)` 在 `rtos/rtos_app.c:72`
命中，`pxCurrentTCB = 0x20007a0c <control_task_buffer>`。断点暂停时读取
`DBGMCU->APB1FZR1 = 0x1800`，包含 `DBG_IWDG_STOP` 位 `0x1000`；保持暂停 12 秒后仍停在
同一 FreeRTOS 任务栈，未发生 IWDG 复位。因此底层 OpenOCD/GDB 自动烧写、源码断点、
调用栈、FreeRTOS 符号和 Debug IWDG 冻结为 `HARDWARE PASS`。

2026-08-15 用户在 VS Code 图形界面按 F5 人工确认通过：Cortex-Debug 自动执行
`CMake: build arm-debug`、启动 OpenOCD、烧写 `build/arm-debug/application.elf`，并进入源码调试。
因此 VS Code F5 前端流程同样记为 `HARDWARE PASS`。

CMake/Ninja 命令和工具链要求见 [`cmake_build.md`](cmake_build.md)。

## 串口

CH340 参数：

```text
115200 baud, 8 data bits, no parity, 1 stop bit
```

常用命令以固件输出的 `COMMANDS` 为准。危险测试必须保持车轮架空并显式输入确认命令。

## PID 调参

使用 Windows、USB 串口和 VOFA+，不另建专用 GUI。车轮架空，每次只调一侧，
遥测默认关闭，控制仍由 STM32 的 100 Hz `control_task` 执行。

建议 FireWater 文本：

```text
pid:time,target,measured,error,p,i,d,output,pwm
```

当前实际命令接口：

```text
pid show
pid left <kp> <ki> <kd>
pid right <kp> <ki> <kd>
pid target <left_counts_per_tick> <right_counts_per_tick>
pid stop
telemetry text
telemetry vofa
telemetry off
```

规则：

- `pid left/right` 写入 RAM pending 参数，并在下一个控制周期边界自动应用。
- 当前没有 `pid save/apply/reset` 或 `tune start/stop` 命令；参数不会持久化。
- `pid target` 持续保持到 `pid stop`，不使用 CAN 的 200 ms heartbeat timeout。
- 停止、急停、超时、故障和换向时重置积分。
- 先调 `kp`，再增加少量 `ki`，最后仅在确有必要时加入 `kd`。
- `telemetry vofa` 以 10 ms 周期输出左右目标、增量、RPM x10、PWM、电压、状态和故障；
  出现异常立即执行 `pid stop`。
- 正反向、左右轮和安全停车实物验收按当前计划后置，低速 PID 标记为 `DEFERRED`。

## CAN FD 联调

Jetson 配置：

```bash
sudo modprobe can
sudo modprobe can_raw
sudo modprobe mttcan

sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.8 \
  dbitrate 2000000 dsample-point 0.8 fd on one-shot off \
  berr-reporting on restart-ms 1000
sudo ip link set can0 up
ip -details -statistics link show can0
```

正常基线应为 `ERROR-ACTIVE`，当前 `tx/rx` 错误计数应为零或稳定不增长。

终端 1：

```bash
candump can0
```

终端 2发送握手请求：

```bash
cansend can0 720##150494E4701000000
```

收到 STM32 `0x721` 后发送确认：

```bash
cansend can0 720##15041535301000000
```

通过条件：

- Jetson 收到 `0x721`
- STM32 `status` 显示 `FDCAN_EXTERNAL: PASS`
- `can status` 中没有持续增长的 TEC、REC、bus-off 或 protocol error

排障原则：

- FDCAN TX/RX 与收发器同名连接，不像 UART 那样交叉。
- 断电检查 CANH-CANL 约 60 Ω，并检查 CANH、CANL、GND 端到端连通。
- 只有看到 STM32 返回帧才证明双向链路；`candump` 中本地 `0x720` 回显不算。
- Error-Passive 或 Bus-Off 后先停止发送并修复接线、终端和时序。

## 电机与安全回归

涉及电机、控制任务、安全管理或配置变化时：

1. 架空车轮，保持电机电源断开。
2. 上电确认四路 PWM 为零且电机测试默认关闭。
3. 接通电机电源，分别验证左右轮正反转和编码器符号。
4. 运行闭环时确认目标、测量和输出方向一致。
5. 触发急停，确认 PWM 立即归零且故障锁存。
6. 停止 CAN 控制帧，确认 200 ms 后停车且旧命令不会恢复。
7. 检查控制 overrun、关键任务健康和 IWDG 行为。

## OTA 首次烧录与验收

首次操作必须同时写内部组合镜像和 QSPI confirmed 基线：

1. 断开电机主电源，保持急停可用，确认 CAN 不发送运动命令。
2. 在 STM32CubeProgrammer 连接 DFU 或 ST-Link，执行 Full chip erase。
3. 选择匹配版本的 `factory-a*-b*.bin`，下载地址填写 `0x08000000`，启用下载后校验。
4. 使用 W25Q64 External Loader 或等价工装，将匹配的 `qspi-a*-confirmed.bin` 从 QSPI
   地址 `0x000000` 写入并校验。
5. 复位，确认串口出现 Application 启动信息，电机保持零输出。
6. 执行 `status`，记录 `OTA_CONFIRM` 和 `OTA_TRANSFER`；首次无 TRIAL 元数据时不得
   将 READY 当成 OTA 硬件通过。

当前 OTA V1 按以下清单验收：

匹配的内部 factory 镜像和 QSPI `CONFIRMED/SLOT_A` 基线已通过 DFU provisioner 写入、
完整读回和启动校验；该 factory provisioning 前置项不替代下列项目各自的验收。

| 顺序 | 项目 | 当前状态 | 通过条件 |
| ---: | --- | --- | --- |
| 1 | 普通启动 | `HARDWARE PASS` | 2026-08-17，confirmed b13 经 ST-Link 普通复位后正常运行；断电启动仍需单独验收 |
| 2 | 上电零 PWM | `DEFERRED` | 四路 PWM 在进入控制前保持为零 |
| 3 | UART OTA | `HARDWARE PASS` | 2026-08-17，b12 -> b13 完整 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` |
| 4 | CAN FD OTA | `DEFERRED` | 当前后置；启用 Jetson OTA 前再用 SocketCAN 完成同一闭环 |
| 5 | Application 安装中断电 | `DEFERRED` | 重启后继续或安全恢复，不跳入半写镜像 |
| 6 | TRIAL 不确认 | `DEFERRED` | 超过试运行限制后自动恢复 confirmed |
| 7 | rollback 安装中断电 | `DEFERRED` | 重启后继续恢复 confirmed，不无限破坏性重装 |

2026-08-17 决定冻结 OTA V1 代码基线并推进功能主线。CAN FD OTA、断电启动、四路 PWM
电气零输出和三个故障注入项目统一后置为 `DEFERRED`；它们不构成已通过证据，在启用对应
发布能力或收尾验收前仍需执行。除实测发现的 P0/P1 外不再增加 recovery 细节；固件签名、
防回滚和 Bootloader CAN Recovery 作为 OTA V2 工作。

每次传输都应保存发送工具输出和复位后的 `status`。只有看到完整
`STAGED -> TRIAL -> CONFIRMED` 且新 Application 正常启动，才可记录
`HARDWARE PASS`。当前 factory 普通启动和 UART OTA 的传输、安装、TRIAL、确认已通过；
CAN FD OTA 为 `DEFERRED`，回滚和断电恢复仍为 `NOT VERIFIED`。

## Bootloader 单元测试

当前已有 PC 单元测试入口：

```text
firmware/bootloader/stm32g474/tests/unit/bootloader_core_test.c
```

覆盖内容：

- CRC32 标准向量 `123456789 -> 0xCBF43926`
- OTA 镜像头、payload CRC 和向量表校验
- OTA 元数据双副本 CRC 校验和最新 sequence 选择

Bootloader 的 Debug 与 Release 已使用 STM32CubeIDE 自带 `arm-none-eabi-gcc`
完成构建。该 PC 测试已于 2026-07-30 使用 WSL GCC 和
`-std=c11 -Wall -Wextra -Werror` 编译运行通过；结果仅覆盖纯逻辑，不覆盖 QSPI、内部
Flash、IWDG、复位和跳转的目标板行为。

## 2026-08-14 ICM45686 与预留按钮构建

- 独立 CubeMX 生成器已使用带目标目录的 CLI 脚本完成生成：
  `E:\STM32CubeMX\STM32CubeMX.exe -q .cubemx-generate.script`。
- CMake 3.22 + Ninja + GNU Arm Embedded 14.3.rel1 clean build：Application、Bootloader、QSPI provisioner 均通过。
- ICM45686、VS Code Debug 配置和 IWDG 调试冻结接入后，当前 Release clean build 通过：
  Application `text=198060 data=120 bss=35232`、Bootloader `text=13480 data=48 bss=1656`、
  QSPI provisioner `text=9272 data=12 bss=1644`。
- `test_icm45686.c` 使用 Visual Studio 2022 MSVC `/std:c11 /W4 /WX` 编译并实际运行通过，
  覆盖软复位恢复、MREG 字节序、ODR/4低通、FIFO配置/计数/状态/flush、16字节大端帧解析、
  timestamp正常差值与16位回绕，以及SI单位换算。
- `test_imu_fusion.c` 使用相同 MSVC 参数编译并实际运行通过，覆盖 200 样本静止零偏收敛、
  姿态初始化、四元数归一化和 1 秒陀螺旋转积分。
- SPI3 DMA1 CH5/CH6 已写入 `.ioc`；CubeMX 6.18 CLI 已生成 `hdma_spi3_rx/tx`、DMA NVIC
  配置和 CH5/CH6 IRQ handler，原临时 BSP DMA glue 已移除。
- 本轮仅有源码、宿主测试和编译/链接证据；ICM45686、FIFO/DMA、零偏、姿态和 PD3/PD4
  按钮尚未进行实物验收。

## 2026-08-17 SR501 代码接入构建

SR501 使用 `.ioc` 中的 PD5 普通输入、内部下拉配置。已使用 CubeMX 6.18.1 和
STM32Cube FW_G4 V1.6.3 生成 `SR501_OUT` 引脚定义及 GPIO 初始化；`bsp/sr501` 负责 60 秒
预热、50 ms 稳定滤波、READY 后稳定低到高单次计数，以及 `status` 诊断输出。当前未绑定
电机、安全、LCD 或业务。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

结果：

| 配置 | text | data | bss | 状态 |
| --- | ---: | ---: | ---: | --- |
| Debug | 194364 | 120 | 35104 | `BUILD PASS` |
| Release | 185948 | 120 | 35096 | `BUILD PASS` |

本轮只证明源码编译和链接通过。SR501 的 5 V 供电、OUT 高低电平、60 秒预热、50 ms 滤波、
持续高电平不重复计数和断开模块时 PD5 下拉行为均为 `NOT VERIFIED`。

同日将包含 SR501 的 Application 提升为 b14，重新执行上述 Debug/Release 构建并生成：

| 产物 | 字节数 | CRC32 / SHA-256 |
| --- | ---: | --- |
| `app-v0.1.0-b14.bin` | 186076 | CRC32 `0x1E5B771D`；SHA-256 `85755554e0e2f0234c7071fcbd7867309048933247d8e317260930f268028ec4` |
| `app-v0.1.0-b14.ota` | 186140 | SHA-256 `20548218f4db0ba3b5721c8d8f9c6c7b97b17d2d6fcb18c85a2c9214f397de66` |

打包器确认目标地址为 `0x08008000`，OTA Python 9 项执行通过。随后 CH340 和 ST-Link 已透传
到 Ubuntu，b13 在线状态为 `MOTOR: DISABLED`，因此通过 UART 发送 b14。Bootloader 报告
`INSTALL VERIFIED`、`TRIAL COMMITTED`、`TRIAL VERIFIED`，Application 正常响应 `PONG`，
但 `status` 无输出。源码检查确认完整报告加 SR501 行后超过 UART 单条消息 1200 字节上限。

b15 将诊断格式缓冲从 1664 增至 2048，但 UART 上限未同步，复测仍无 `status`。b16 将
`BSP_UART_MAX_WRITE_SIZE` 同步为 2048，并用 `_Static_assert` 约束诊断缓冲不得超过该上限。
b16 重新构建结果和正式待测产物为：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 194364 | 120 | 42272 | `BUILD PASS` |
| Release ELF | 185948 | 120 | 42264 | `BUILD PASS` |
| `app-v0.1.0-b16.bin` | 186076 | - | - | CRC32 `0x995BF0B5`；SHA-256 `ca963794d3dc912b32c671061ea68c07acbc54b68975e80985b884b0ea1ca563` |
| `app-v0.1.0-b16.ota` | 186140 | - | - | SHA-256 `4999ec00755b372482ea2ea370db20389f43ee0271f1fd479d3e85c268e9c2ba` |

b16 经 UART 收到 `STAGED` 后，Bootloader 再次报告 `INSTALL VERIFIED`、`TRIAL COMMITTED`、
`TRIAL VERIFIED`；随后 `status` 返回 1251 字节完整报告并显示 `OTA_CONFIRM: CONFIRMED`、
`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`。这同时实证旧 1200 字节限制不足。

SR501 首次状态为：

```text
SR501: WARMING_UP motion=0 raw=0 count=0 last_ms=0 warmup_ms=24831
SR501: WARMING_UP motion=0 raw=0 count=0 last_ms=0 warmup_ms=3328
SR501: READY motion=0 raw=0 count=0 last_ms=0 warmup_ms=0
```

随后连续监听 60 秒，始终为 `READY motion=0 raw=0 count=0`。因此 60 秒预热、预热期间不计数、
READY 后低电平零误计数已有实物证据；模块指示灯未亮，传感器 OUT 未观察到高电平。
2026-08-17 决定将模块供电/设置排查、50 ms 稳定滤波、稳定低到高单次计数和持续高电平
不重复计数统一标记为 `DEFERRED`，不阻塞 PID 主线。

## 2026-08-18 正式 UART 消息构建与实物验证

Application 文本输出已迁移到统一 emitter：命令响应使用 `[RSP]`，异步事件使用 `[LOG]`，
周期状态使用 `[TEL]`。`status` 现在以同一 `seq` 输出 `system`、`motor`、`sensors`、
`communication` 四个分区；VOFA 数字流保留为显式兼容模式。UART OTA 进入二进制模式后，
Application 跳过所有文本诊断和遥测输出；`send_uart.py` 已同步等待 `[RSP] command=ota_uart`。
`test_uart_message.py` 的 4 项主机测试覆盖正式/旧版 OTA ready 响应、错误响应和重复/混合字段拒绝；
旧版准备行只用于主机工具从旧固件单向升级，不放宽新 Application 的输出格式。
`test_command_manager.c` 重新通过宿主机 `-Werror` 回归，覆盖明确的 `ACCEPTED`、`NOT_OWNER` 和
`INVALID_ARGUMENT` 提交结果。目标板首次验证发现 nano printf 不正确支持 `%lld`，导致编码器
显示为 `ld` 并让后续变参错位；现已用 `UartProtocol_FormatSigned64()` 替代 Application 内全部
`%lld`。新增 C 测试覆盖 `0`、`INT64_MAX`、`INT64_MIN` 和容量不足。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --clean-first --parallel
cmake --preset arm-release
cmake --build --preset arm-release --clean-first --parallel
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/ota -p 'test_*.py' -v
gcc -std=c11 -Wall -Wextra -Werror -DUART_PROTOCOL_HOST_TEST ...
gcc -std=c11 -Wall -Wextra -Werror -DCOMMAND_MANAGER_HOST_TEST ...
git diff --check
```

结果：

| 配置 | Application text | data | bss | 状态 |
| --- | ---: | ---: | ---: | --- |
| Debug | 199868 | 120 | 52864 | `BUILD PASS` |
| Release | 190404 | 120 | 52856 | `BUILD PASS` |

同一 preset 的 Bootloader 和 QSPI provisioner 也完成构建：Debug Bootloader `text=15596 data=48
bss=1656`、Debug provisioner `text=10556 data=12 bss=1644`；Release Bootloader
`text=13480 data=48 bss=1656`、Release provisioner `text=9272 data=12 bss=1644`。
Python OTA 测试共 13 项通过；UART 64 位格式化和 CommandManager 两个 C 宿主测试均以
`-Wall -Wextra -Werror` 编译并执行通过。最终 b17 产物为：

| 产物 | 字节数 | CRC32 / SHA-256 |
| --- | ---: | --- |
| b17 Release BIN | 190532 | CRC32 `0xA68BC9CE`；SHA-256 `c007a8b794ef3dfdf868f8a6fbfec09a181fc79e9b836491f3343245bdc04a04` |
| b17 OTA package | 190596 | SHA-256 `b4747c5df5268565a8eb2aae30f837ca1c53f926f1270795e90a060353217341` |

通过 `/dev/ttyUSB0`、115200 8N1 将 b17 OTA 包发送到 confirmed b16，发送工具收到：

```text
OTA staged over UART: session=120 next_offset=190596; device reset expected
```

b17 进入试运行并完成确认后，`ping`、`status` 和 `encoder result` 实测确认：

```text
[RSP] v=1 ts_ms=59862 result=OK command=ping
[TEL] v=1 ts_ms=60064 seq=1 section=system fw=0.1.0-b17 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=60064 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=60064 seq=1 section=communication ... ota_confirmation=CONFIRMED ...
[RSP] v=1 ts_ms=60266 result=OK command=encoder_result left_total=0 right_total=0
```

同轮已验证未知命令返回 `INVALID_ARGUMENT`、`pid show` 字段正确、`telemetry text` 以 100 ms
周期发送、采集的 30 行全部以 CRLF 结束，随后用 `telemetry off` 恢复。未执行电机命令。

最后通过 ST-Link/OpenOCD 执行普通软件复位，Bootloader 和 Application 报告：

```text
BOOT: VERSION=0.1.0 BUILD=22
BOOT: STATE=0x00000005
[LOG] v=1 ts_ms=47 level=INFO module=boot event=STARTED fw=0.1.0-b17
[LOG] v=1 ts_ms=310 level=INFO module=application event=READY tasks=4
[TEL] v=1 ts_ms=11219 seq=1 section=communication ... ota_confirmation=NOT_REQUIRED ...
```

稳定状态下四任务均为 `RUNNING`，`critical_tasks=1`、`control=STOPPED`、`fault=0`、
`overrun=0`、`missed=0`，QSPI 为 `EF4017`，LCD 为 `READY`。该证据不覆盖按键/断电启动、
四路 PWM 电气测量、SR501 高电平、事件计数、CAN FD OTA、断电恢复或回滚；这些项目继续保持
`DEFERRED` 或 `NOT VERIFIED`。

## 2026-08-18 Application 版本语义拆分过渡产物（0.2.0 build18）

本轮将 Application 产品版本从混合字符串 `0.1.0-b17` 拆为独立字段，曾暂用
`0.2.0 build18` 验证格式。随后按历史版本序列重新编号为当前的 `0.3.0 build1`；本节产物
保留为真实过渡证据，不作为当前版本基线。版本用于表示功能、协议或兼容行为变化；build 只
标识同一版本下的具体产物，普通本地重编译不递增。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --clean-first --parallel
cmake --preset arm-release
cmake --build --preset arm-release --clean-first --parallel
python3 tools/ota/package_firmware.py build/arm-release/application.bin \
  /tmp/chassis-controller-app-v0.2.0-b18.ota --version 0.2.0 --build 18
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tools/ota -p 'test_*.py'
git diff --check
```

结果：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 199896 | 120 | 52864 | `BUILD PASS` |
| Release ELF | 190444 | 120 | 52856 | `BUILD PASS` |
| build18 Release BIN | 190572 | - | - | CRC32 `0x6FCF40CC`；SHA-256 `1e14c0dd4076d228d737b5de58bc58ee9473fe29a08a68a7b816015b08b8c907` |
| build18 OTA package | 190636 | - | - | SHA-256 `cfcf7ff038d661a0278551667827f81cfb077daf77a27d43be53eb8bc4f519bb` |

OTA 工具完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`。目标板状态实测为：

```text
[TEL] v=1 ts_ms=17001 seq=1 section=system fw=0.2.0 build=18 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=17001 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=17001 seq=1 section=communication ... ota_confirmation=CONFIRMED ... lcd=READY
[RSP] v=1 ts_ms=17203 result=OK command=encoder_result left_total=0 right_total=0
```

四任务均为 `RUNNING`，控制保持 `STOPPED`、故障为零，未执行任何电机命令。SR501 高电平、
按键/断电启动、四路 PWM 电气测量、CAN FD OTA、断电恢复和回滚仍按计划保持 `DEFERRED`
或 `NOT VERIFIED`。

## 2026-08-18 Application 版本序列最终基线（0.3.0 build1）

按路线文档中的历史映射，b17 的 FreeRTOS、统一快照和正式 UART 协议变化进入 `0.3.0`，
版本切换后 build 从 1 开始。重新生成并发送的最终包为：

| 配置/产物 | text/字节数 | data | bss | CRC32 / SHA-256 |
| --- | ---: | ---: | ---: | --- |
| Debug ELF | 199892 | 120 | 52864 | `BUILD PASS` |
| Release ELF | 190444 | 120 | 52856 | `BUILD PASS` |
| `app-v0.3.0-b1.bin` | 190572 | - | - | CRC32 `0x5866D20E`；SHA-256 `117c4a9cf45276f2c3722c9a67cf0de739cfe960a31e609de2970537383d5e1c` |
| `app-v0.3.0-b1.ota` | 190636 | - | - | SHA-256 `d5560e6710706f1560c2f28d00ab8b117ae71f8405acd1ca7cf515f3709f29df` |

目标板通过 UART OTA 完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`，实测：

```text
[TEL] v=1 ts_ms=14349 seq=1 section=system fw=0.3.0 build=1 ... critical_tasks=1 ...
[TEL] v=1 ts_ms=14349 seq=1 section=motor ... left_encoder=0 right_encoder=0 ... overrun=0 missed=0
[TEL] v=1 ts_ms=14349 seq=1 section=communication ... ota_confirmation=CONFIRMED ... lcd=READY
[RSP] v=1 ts_ms=14552 result=OK command=encoder_result left_total=0 right_total=0
```

四任务均为 `RUNNING`，控制保持 `STOPPED`，未执行电机命令。后续同一 `0.3.0` 源码的重复
可部署产物才使用 build2、build3；功能或协议变化时进入下一个语义版本并将 build 重置为 1。

## 2026-08-18 LCD 小 Logo 与单页结构构建（0.4.0 build1）

本轮保留原始 `assets/tafei/picture_tafei.h`，从原始 JPG 复制生成 48x47 RGB565
`picture_tafei_logo.h`。LCD 不再使用独立全屏封面页，小 Logo 改为状态页右上角固定元素；当前
继续使用已接线的 PB8/BOOT0 按键，PD3/PD4 尚未接线且不参与 LCD 操作。四个功能页和 PB8
循环切页仍是后续工作。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 85712 | 120 | 52864 | `BUILD PASS` |
| Release | 76244 | 120 | 52856 | `BUILD PASS` |

原全屏 RGB565 资源不再链接到 Application，改为链接 4512 字节的小 Logo 像素数据，因此
Flash 占用明显下降。本轮未执行烧录或 LCD 实物观察，Logo 尺寸、颜色、位置和 PB8 页面操作均
保持 `NOT VERIFIED`。

## 2026-08-18 LCD 四页状态显示构建（0.5.0 build1）

LCD 已实现 `OVERVIEW`、`MOTOR`、`SENSORS`、`SYSTEM` 四页，固定显示 48x47 taifei Logo。
四页均由 `status_display` 一次读取 `SystemStatusSnapshot` 后转换数据；PB8/BOOT0 每次有效按下
循环到下一页，`SYSTEM` 后返回 `OVERVIEW`，不再触发诊断自检。PD3/PD4 未接线且不参与本轮操作。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87896 | 120 | 52944 | `BUILD PASS` |
| Release | 78056 | 120 | 52936 | `BUILD PASS` |

构建未产生警告。四页实际文字、颜色、Logo、1 秒刷新以及
`OVERVIEW -> MOTOR -> SENSORS -> SYSTEM -> OVERVIEW` 的 PB8 循环尚未烧录观察，均保持
`NOT VERIFIED`。本轮未执行电机命令，也未改变 SR501、ICM45686 或 OTA 的既有硬件结论。

## 2026-08-18 LCD DMA 调度修复与上板观察（0.5.1 build1）

### 观察与修复

使用 ST-Link 直接写入 Application 区 `0x08008000` 后执行普通复位，Bootloader 检测到内部
镜像与 QSPI confirmed `0.3.0 build1` 不匹配，按 confirmed repair 路径恢复旧镜像；Bootloader
和 QSPI 未被修改。随后使用 GDB 从 Application 向量直接启动 `0.5.0 build1`，UART 确认：

```text
[TEL] ... fw=0.5.0 build=1 ... control=STOPPED ... left_pwm=0 right_pwm=0 ...
[TEL] ... display_task=RUNNING ... lcd=DRAWING ...
```

调试读取表明 LCD DMA 已完成但显示任务每 20 ms 只推进一行，约 4.8 秒才能完成一帧；1 秒
刷新会在完成后立即再次请求绘制，导致状态长期停留在 `DRAWING`。因此将
`DISPLAY_TASK_EXPECTED_PERIOD_MS` 改为 1 ms，并将补丁版本提升为 `0.5.1`。

### 0.5.1 构建

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87896 | 120 | 52944 | `BUILD PASS` |
| Release | 78056 | 120 | 52936 | `BUILD PASS` |

`0.5.1` 调试启动已观察到 `lcd=READY`、四任务 `RUNNING`、`control=STOPPED` 和四路 PWM 为零；
四页实际内容、Logo 颜色和 PB8 循环仍未人工观察，保持 `NOT VERIFIED`。同一输出暴露
`display_period_ms=1` 但 `display_expected_ms=20` 的快照字段不一致，已在 `0.5.2` 修复。

## 2026-08-18 LCD 任务诊断周期统一（0.5.2 build1）

将 `SystemRuntimeSnapshot.display_expected_period_ms` 改为复用
`DISPLAY_TASK_EXPECTED_PERIOD_MS`，使实际 1 ms 调度周期、期望周期和 UART/LCD 诊断一致。
本修正不改变 LCD 页面、Logo、按键或电机控制逻辑。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 87892 | 120 | 52944 | `BUILD PASS` |
| Release | 78052 | 120 | 52936 | `BUILD PASS` |

`0.5.2` 已通过 ST-Link 写入 Application 区并由 GDB 从 Application 向量直接启动。串口状态为：

```text
[TEL] ... fw=0.5.2 build=1 ... control=STOPPED ... critical_tasks=1
[TEL] ... service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
[TEL] ... display_period_ms=1 display_expected_ms=1 ... left_pwm=0 right_pwm=0
[TEL] ... lcd=READY ...
```

停止 OpenOCD 后 `ping` 仍返回正式 `[RSP]`，Application 保持运行。该验证证明 LCD DMA 状态机
能够完成一帧并进入 `READY`，不代表四页文字、颜色、Logo 或 PB8 循环已完成人工目视验收；
这些项目继续保持 `NOT VERIFIED`。普通复位仍会由 Bootloader 恢复 QSPI confirmed
`0.3.0 build1`，因此本节也不构成 `0.5.2` 正常上电启动或 OTA 确认证据。

## 2026-08-18 LCD UI 美化与电量估算构建（0.6.0 build1）

本轮将 taifei Logo 裁剪为 40x40 RGB565，并增加透明掩码去除白色背景；LCD 四页增加深色
标题栏、分隔面板、状态色和总览页电量条。百分比使用 9.0--12.6 V 电压窗口估算，实际电池
化学体系、串数和放电曲线确认后仍需校准。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
git diff --check
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 88612 | 120 | 52944 | `BUILD PASS` |
| Release | 78660 | 120 | 52944 | `BUILD PASS` |

`0.6.0` 已通过 ST-Link 写入 Application 区并由 GDB 从 Application 向量直接启动。`status`
确认 `fw=0.6.0 build=1`、四任务 `RUNNING`、`control=STOPPED`、左右 PWM 为零，LCD 在 1 s
刷新周期中可观察到 `DRAWING` 并回到 `READY`；停止 OpenOCD 后 `ping` 仍正常响应。当前电机
电源未接入，ADC 报告 `supply_mv=0`，因此百分比实际电压效果仍需带电池观察。

透明 Logo、百分比和电量条、四页颜色布局以及 PB8 循环尚待人工目视，均保持
`NOT VERIFIED`。本轮未执行电机命令，也未改变 SR501 或 ICM45686 的既有硬件结论；
初次直接写内部 Application 后普通复位仍会恢复当时 QSPI confirmed `0.3.0 build1`。

为使本版断电后可由 Bootloader 正常启动，随后从 `build/arm-release/application.bin` 生成
`0.6.0 build1` OTA 包并通过 `/dev/ttyUSB0`、115200 8N1 发送：

```text
payload=78788 bytes
payload_crc32=0x8DE78C48
bin_sha256=93725db9139c330f4b25b29254ecf475b4825329ae39694804b4a1af11d675a1
ota_sha256=fc1e70013396d729f853253011b35ba0923aca4b3eacc63d98e775aaea7203a8
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后 `status` 报告 `fw=0.6.0 build=1`、`ota_confirmation=CONFIRMED`、四任务
`RUNNING`、`control=STOPPED` 和左右 PWM 为零。随后通过 ST-Link 执行普通复位，目标板继续
报告 `fw=0.6.0 build=1`、`ota_confirmation=NOT_REQUIRED`、`lcd=READY`，证明内部镜像与
QSPI confirmed 基线已经一致。真实断电重上电未执行，不能据此记录断电启动 `PASS`。

## 2026-08-18 LCD 信息层级与大号电量显示（0.7.0 build1）

本轮将 LCD 文本布局扩展为 10 个可独立设置坐标、字号和颜色的区域。总览页使用 3 倍字号
显示电压和百分比，并将电量条扩大到 288x16；电机页改为左右双栏，传感器页和系统页重新
按信息层级排版。四页继续使用统一标题栏、内容面板、分隔线、底部状态栏和 40x40 透明 Logo。

执行：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug --parallel
cmake --preset arm-release
cmake --build --preset arm-release --parallel
```

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 89204 | 120 | 53088 | `BUILD PASS` |
| Release | 79264 | 120 | 53080 | `BUILD PASS` |

Release `application.bin` 和 OTA 包信息：

```text
payload=79392 bytes
payload_crc32=0x7B2868B2
bin_sha256=c3bcc39853b10a530853e445c1c2e9aabf2dae073c849a3d1d7e811ca029beac
ota_sha256=e012ab69122be5dc46f0a0c5cdce72e727f556f9573ebf04d65c7d58d7794202
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后 `status` 报告 `fw=0.7.0 build=1` 和 `ota_confirmation=CONFIRMED`。随后通过
ST-Link 执行普通复位，复位后再次读取 `status`：

```text
fw=0.7.0 build=1 reset=SOFTWARE critical_tasks=1
service_task=RUNNING control_task=RUNNING diagnostics_task=RUNNING display_task=RUNNING
control=STOPPED left_pwm=0 right_pwm=0
lcd=READY ota_confirmation=NOT_REQUIRED
```

当前 ADC 报告 0 mV，所以页面应显示 0% 和空进度条。该串口结果只证明软件状态、普通复位
启动和零 PWM；大号电量显示、透明 Logo、双栏/分组布局无重叠以及 PB8 四页循环仍需人工
目视确认，保持 `NOT VERIFIED`。真实断电重上电和四路 PWM 电气测量未执行，继续 `DEFERRED`。

## 2026-08-18 LCD 中性配色修正（0.7.1 build1）

本轮保留 `0.7.0` 的页面信息、坐标、字号和透明 Logo，将标题栏、内容面板和背景调整为中性
深灰，移除贯穿全屏的青色横线；标题下方仅保留短青色强调线，页脚和电机双栏分隔改为灰色。

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 89216 | 120 | 53088 | `BUILD PASS` |
| Release | 79272 | 120 | 53080 | `BUILD PASS` |

Release 产物：

```text
payload=79400 bytes
payload_crc32=0xAABB8733
bin_sha256=6c828adf6845add29a1be8b23476c8363b9a7beec478056604e889ee0fbe9738
ota_sha256=febdea7b478d970a3d26259b5d0d01f6a5946196fc8498e83cba845c17b902e9
BOOT: INSTALL VERIFIED
BOOT: TRIAL COMMITTED
BOOT: TRIAL VERIFIED
```

健康窗口后报告 `fw=0.7.1 build=1`、`ota_confirmation=CONFIRMED`。ST-Link 普通复位后再次
读取状态：四任务均为 `RUNNING`，`control=STOPPED`，左右 PWM 为零，`lcd=READY`，
`ota_confirmation=NOT_REQUIRED`。串口证据不等于配色人工验收，中性背景和短标题强调线的
实际观感仍保持 `NOT VERIFIED`。
