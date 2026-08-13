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
| Release | 183492 | 96 | 34224 | `BUILD PASS`，2026-08-13，b10，0 errors，0 warnings |

Debug/Release 的 `.isr_vector` 均位于 `0x08008000`；反汇编确认 `SystemInit()` 将
VTOR 写入 `0x08008000`。最高 Flash load 地址未超出 `0x0807FFFF`。构建结果只说明
当前 Application b10 源码可编译和链接，不代表本轮固件已通过实物启动；b6 仍是最近一次
完成 UART OTA 实物闭环的 Application。

Bootloader Release 于 2026-08-13 使用 CubeIDE 2.2.0 GCC clean build；Debug 行保留
2026-07-28 的最近结果：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 17652 | 44 | 1668 | `BUILD PASS`，2026-07-28，0 errors，0 warnings |
| Release | 12884 | 48 | 1656 | `BUILD PASS`，2026-08-13，build20，0 errors，0 warnings |

Bootloader 链接脚本只提供 `0x08000000` 起始的 32 KiB Flash。该结果仅证明编译和
链接通过，不证明 QSPI 安装、掉电恢复、Application 跳转或回滚已通过实物验证。
本次 build20 尚未生成正式发布产物、
烧录或执行目标板回归；build15 仍是最近一次已完成实物启动和 UART OTA 验证的 Bootloader。

2026-08-13 在当前工作树完成宿主机回归：

- `tools/ota/test_ota_transfer.py`：3 项 Python 测试通过，覆盖 UART 帧 CRC、CAN 响应
  布局和 stop-and-wait 分块状态流程。
- `tools/ota/test_factory_image.py`：3 项测试通过，确认 Slot A package、`CONFIRMED` Metadata A、
  CRC、擦除态 Metadata B、默认缺少 QSPI 参数时失败，以及显式 `--internal-only` 调试路径。
- `tools/ota/test_shared_abi.py`：3 项测试通过，直接解析共享 C 头文件并核对两个 Python
  打包工具的 Flash 地址、大小、magic、格式版本、枚举值以及 64 字节镜像头/metadata ABI。
- `test_command_manager.c`：WSL GCC 使用 `-std=c11 -Wall -Wextra -Werror` 编译运行通过，
  覆盖 OTA owner、运动命令清除、非法 source、Console 持续目标和 CAN 200 ms 超时。
- `test_ota_metadata.c`：同参数编译运行通过，覆盖 metadata 状态和严格槽约束。
- `test_ota_uart_arm_guard.c`：同参数编译运行通过，覆盖等待 BEGIN 的 30 秒超时、
  active session 和 tracked response 未完成时禁止禁用 UART transport。
- `bootloader_core_test.c`：同参数编译运行通过，覆盖 CRC32 标准向量、镜像校验、metadata
  双副本选择、factory 擦除态识别、candidate/confirmed 安装三次上限、attempt 1/2/3
  中断后的 `TRIAL/CONFIRMED` salvage、无效镜像重装边界和 confirmed size/CRC 回填。

本轮新增的逐次安装掉电 salvage、QSPI payload CRC、共享 ABI 对照、confirmed metadata
回填和 factory fail-closed 均只完成
代码审查、主机测试或目标构建，尚未完成目标板故障注入/烧录回归。

以上均为主机测试结果，不替代 CubeIDE 目标构建或 STM32 实物验收。

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

## VS Code 构建

VS Code 只作为 STM32CubeIDE 命令行构建入口，编译器仍是 STM32CubeIDE GCC，
工程配置仍来自 `.cproject`，不改用桌面 GCC 或 CMake。

- `Ctrl+Shift+B`：执行本机 `.vscode/tasks.json` 中的 Debug 增量构建任务。
- 工程路径：`firmware/application/stm32g474/`。
- 遇到产物未更新时执行 clean build，再检查 ELF 和相关 `.o` 的修改时间。
- `.vscode/` 是本机配置；换电脑后需更新 `stm32cubeidec.exe` 的实际安装路径。
- VS Code 构建默认只编译，不等同于烧录和板上验证。

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

首次跳转验证完成后，按以下顺序验收：

- UART 与 CAN FD 分别完成同一 `.ota` 包升级
- 错误 magic、地址、长度和 CRC 被拒绝
- QSPI 写入、内部擦除和内部写入阶段断电后可恢复
- 候选版本未确认时恢复已确认镜像
- 无有效 Application 时停留 Bootloader
- 更新全程保持零 PWM
- Bootloader 和 Application 版本、复位原因可诊断

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
