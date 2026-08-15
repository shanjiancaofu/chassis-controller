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
| KEY 消抖 | `HARDWARE PASS` | 按键切换 LCD 页面 |
| ADC 电压 | `HARDWARE PASS` | 11.96 V 电池，PA2 约 1.086 V |
| 双编码器 | `HARDWARE PASS` | 前进同为正、后退同为负 |
| 双电机开环 | `HARDWARE PASS` | 左右轮正反转和自动停止 |
| 急停 | `HARDWARE PASS` | PD2 触发故障并清零 PWM |
| IWDG 受控复位 | `HARDWARE PASS` | 复位后报告测试通过 |
| 外部 CAN FD | `HARDWARE PASS` | Jetson 与 STM32 三步握手双向通过 |
| 低速 PID 闭环 | `NOT VERIFIED` | 尚无最终稳定性验收记录 |
| FreeRTOS 阶段 4 实物回归 | `NOT VERIFIED` | 代码和构建完成，未重复执行硬件回归 |
| Bootloader factory 启动 | `HARDWARE PASS` | DFU 烧录并校验组合镜像，普通复位后 LCD 进入 Application |
| UART OTA 主升级链路 | `HARDWARE PASS` | build6 完成真实 UART 传输、安装、TRIAL 和 CONFIRMED 确认 |
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
build15 仍是最近一次完成 UART OTA 全闭环验证的 Bootloader。

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
- 未完成正反向、左右轮和安全停车实物验收前，低速 PID 保持 `NOT VERIFIED`。

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

本轮 b12/build22 只按以下冻结清单验收：

匹配的内部 factory 镜像和 QSPI `CONFIRMED/SLOT_A` 基线已通过 DFU provisioner 写入、
完整读回和启动校验；该 factory provisioning 前置项不替代以下七项各自的验收。

| 顺序 | 项目 | 当前状态 | 通过条件 |
| ---: | --- | --- | --- |
| 1 | 普通启动 | `NOT VERIFIED` | 版本输出正确，Application 正常运行 |
| 2 | 上电零 PWM | `NOT VERIFIED` | 四路 PWM 在进入控制前保持为零 |
| 3 | UART OTA | `NOT VERIFIED` | 完整 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` |
| 4 | CAN FD OTA | `NOT VERIFIED` | Jetson SocketCAN 完成同一闭环 |
| 5 | Application 安装中断电 | `NOT VERIFIED` | 重启后继续或安全恢复，不跳入半写镜像 |
| 6 | TRIAL 不确认 | `NOT VERIFIED` | 超过试运行限制后自动恢复 confirmed |
| 7 | rollback 安装中断电 | `NOT VERIFIED` | 重启后继续恢复 confirmed，不无限破坏性重装 |

这七项全部通过后，将 OTA V1 标记为冻结。冻结后除实测发现的 P0/P1 外不再增加 recovery
细节；固件签名、防回滚和 Bootloader CAN Recovery 作为 OTA V2 工作，不阻塞底盘功能。

每次传输都应保存发送工具输出和复位后的 `status`。只有看到完整
`STAGED -> TRIAL -> CONFIRMED` 且新 Application 正常启动，才可记录
`HARDWARE PASS`。当前 factory 普通启动和 UART OTA 的传输、安装、TRIAL、确认已通过；
CAN FD OTA、回滚和断电恢复仍为 `NOT VERIFIED`。

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
