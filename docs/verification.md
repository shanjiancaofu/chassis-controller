# 构建与实物验证

本文件只记录可复现的构建、联调步骤和实物结果。设计目标不在这里标记为 `PASS`。

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
| OTA 完整升级链路 | `NOT VERIFIED` | 尚未完成真实传输、安装、TRIAL 确认、回滚和断电恢复 |

## 构建基线

当前 Application 于 2026-07-28 使用 CubeIDE 2.2.0 GCC 构建：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 222948 | 96 | 34072 | `BUILD PASS`，0 errors，0 warnings |
| Release | 181452 | 96 | 34048 | `BUILD PASS`，0 errors，0 warnings |

Debug/Release 的 `.isr_vector` 均位于 `0x08008000`；反汇编确认 `SystemInit()` 将
VTOR 写入 `0x08008000`。最高 Flash load 地址未超出 `0x0807FFFF`。构建结果只说明
当前源码可编译和链接，不代表迁移后的固件已通过实物启动。

Bootloader 于 2026-07-28 使用 CubeIDE 2.2.0 GCC clean build：

| 配置 | text | data | bss | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 17652 | 44 | 1668 | `BUILD PASS`，0 errors，0 warnings |
| Release | 10268 | 44 | 1668 | `BUILD PASS`，0 errors，0 warnings |

Bootloader 链接脚本只提供 `0x08000000` 起始的 32 KiB Flash。该结果仅证明编译和
链接通过，不证明 QSPI 安装、掉电恢复、Application 跳转或回滚已通过实物验证。

同日从最新 Application Release ELF 重新生成 BIN 并运行
`tools/ota/package_firmware.py`：

```text
payload=181556 bytes
payload_crc32=0x732A8DE6
target=0x08008000
package=_output/chassis-controller-0.1.0-build2.ota
```

打包器已通过向量表、地址、长度、头部 CRC32 和 payload CRC32 自校验。Application
试运行确认代码已构建，并在 `status` 中提供 `OTA_CONFIRM` 状态，但尚未在 QSPI
`TRIAL` 元数据和真实复位链路上验证，因此仍为 `NOT VERIFIED`。

同时生成：

```text
_output/chassis-controller-0.1.0-build2-factory.bin
total=214324 bytes
bootloader=10312 bytes at offset 0x0000
application=181556 bytes at offset 0x8000
```

脚本已确认中间填充全为 `0xFF`，Bootloader `.isr_vector` 位于 `0x08000000`，
Application `.isr_vector` 位于 `0x08008000`。`tools/ota/test_ota_transfer.py` 的 3 项
Python 单元测试通过，覆盖 UART 帧 CRC、CAN 响应布局和 stop-and-wait 分块状态流程；
尚未连接真实串口或 SocketCAN 执行升级。

### Bootloader 启动对照

2026-07-28 使用同一 relocated Application 进行了单变量对照：

| 组合 | Bootloader 时钟 | Bootloader IWDG | 启动结果 |
| --- | --- | --- | --- |
| 候选 I | 16 MHz HSI | 禁用，诊断直跳 | Application LCD 和串口正常 |
| 候选 K | 16 MHz HSI | 启动，约 32 秒 | 黑屏，串口无响应 |
| 候选 L | 16 MHz HSI | 普通路径不启动，完整状态机 | LCD 正常，`status` 完整返回 |
| 候选 N | 170 MHz HSE/PLL | 普通路径不启动，完整状态机 | 黑屏，串口无响应 |
| 正式 factory | 16 MHz HSI | 安装/回滚时按需启动 | DFU 校验成功，普通复位后 LCD 正常 |

因此正式 Bootloader 使用 16 MHz HSI，普通路径不启动 IWDG；安装/回滚启动 IWDG 后
必须通过系统复位结束，不直接继承到 Application。最终正式镜像复位后 Windows 未枚举
串口设备，因此本次正式产物没有重复保存 `status`；候选 L 已使用相同完整状态机取得
`QSPI_ID: PASS`、`LCD: READY` 和 `CONTROL_OVERRUN: count=0 missed=0`。这只证明 factory
启动链路，不代表 OTA 安装、确认或回滚通过。

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

目标命令接口：

```text
pid get [left|right]
pid set left  kp <value>
pid set left  ki <value>
pid set left  kd <value>
pid set right kp <value>
pid set right ki <value>
pid set right kd <value>
pid apply
pid save
pid reset
tune start left <counts_per_tick>
tune start right <counts_per_tick>
tune stop
telemetry on
telemetry off
```

规则：

- `pid set` 只修改 RAM 中的 pending 参数。
- `pid apply` 只在控制周期边界切换参数。
- 只有人工确认稳定后，`pid save` 才允许写入持久化存储。
- 停止、急停、超时、故障和换向时重置积分。
- 先调 `kp`，再增加少量 `ki`，最后仅在确有必要时加入 `kd`。
- 观察目标、测量、误差、输出、饱和和控制 overrun；出现异常立即 `tune stop`。
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

首次操作必须使用组合镜像，不单独烧录位于 `0x08008000` 的 Application：

1. 断开电机主电源，保持急停可用，确认 CAN 不发送运动命令。
2. 在 STM32CubeProgrammer 连接 DFU 或 ST-Link，执行 Full chip erase。
3. 选择 `_output/chassis-controller-0.1.0-build2-factory.bin`，下载地址填写
   `0x08000000`，启用下载后校验。
4. 复位，确认串口出现 Application 启动信息，电机保持零输出。
5. 执行 `status`，记录 `OTA_CONFIRM` 和 `OTA_TRANSFER`；首次无 TRIAL 元数据时不得
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
`HARDWARE PASS`。当前 factory 普通启动已通过，OTA 传输、安装、确认、回滚和断电恢复
仍为 `NOT VERIFIED`。

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
完成构建。PC 单元测试入口尚未使用宿主机 C 编译器执行，因此这里只确认目标工程
`BUILD PASS`，不把单元测试标记为已运行。
