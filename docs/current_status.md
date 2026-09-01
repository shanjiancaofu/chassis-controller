# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

## 2026-09-01：1.0.4 PowerReady 真实验证

板上 confirmed Application 已更新为 `1.0.4 build1`。电机主电源为 0mV 时，`pid target 0 0` 返回
`SAFETY_STOP`，状态保持 `STOPPED/fault=0/PWM=0`；PowerReady 已在真实固件生效。IMU 30 秒静态
Gate 为 100.362 Hz、3058/3058 FIFO 连续、FIFO/timestamp errors=0。由于电源仍断开，正式 CAN
RUNNING regression 本轮不执行；待电机电源接通后复跑。

## 2026-09-01：RC motion power readiness

STOPPED + 电机电源 OFF 不锁存 undervoltage；正常运动与 motor self-test 需要 fresh、valid、≥9V，
RUNNING/open-loop 掉电仍 fail-close。当前仅软件验证，板上 confirmed 镜像仍是 `1.0.3 build1`。

## 2026-09-01：控制周期 dt 语义统一

允许范围内的 missed tick 现在同时作用于 encoder measurement、odometry、slew 和 PID dt；严重超周期
仍走 `CHASSIS_FAULT_CONTROL_OVERRUN`。host 行为测试覆盖 1 tick/3 ticks 积分输出，目标板 missed-tick
故障注入和 DWT jitter 实测仍待执行。

## 2026-09-01：Cortex-M crash context 已实现

四类 Cortex-M fault handler 现在保留“先急停”语义，并把异常栈寄存器及 SCB fault status 保存到
`.noinit`；下次正常启动会通过 UART 输出一次 `crash/PREVIOUS_EXCEPTION`。代码和链接通过，真实
fault injection、复位后现场打印和 `addr2line` 对照仍为 `NOT VERIFIED`。

电源采样控制边界已补齐 freshness：未初始化、超过 250 ms 的旧样本或低于 9 V 均进入 latched
`CHASSIS_FAULT_UNDERVOLTAGE`，不再把“无采样”当作“无需检查”。真实 ADC 断线/超时注入仍待硬件 Gate。

RTOS 三个静态任务创建失败会完整回滚已创建句柄；双电机 duty pair 已改为一次关中断、成对更新 TIM8
compare。两项均已通过源级门禁和 Release 构建。

## 2026-09-01：1.0.3 编码器反馈 fail-close

板上 confirmed Application 已更新为 `1.0.3 build1`。左右编码器任一 `read_delta` 失败时，控制周期
立即锁存 critical `CHASSIS_FAULT_ENCODER`、清除命令并执行 EmergencyStop，不再把无效反馈作为
速度零参与 PID。Release flash 为 `111016` 字节、RAM 为 `55552` 字节；BIN SHA-256
`61f6081c93ab06839984fe2878e626071a69685324d87ad53f8c85910f549bd8`，OTA SHA-256
`3c9c0ec32ff761b22678d8d50f1ae377dabe13d44d9459dfc0c730053f6affdd`，payload CRC32
`0xD45B1706`。

UART OTA confirmed 和普通复位后的完整零速度 target regression 均 PASS，电机主电源仍断开。
尚未物理断开左右编码器制造真实 I/O error，因此 encoder fault injection 保持 `NOT VERIFIED`；下一
硬件 Gate 是在架空/断电安全条件下分别注入左右读取失败，确认同一控制周期 PWM=0、fault bit4锁存。

## 2026-09-01：1.0.2 IMU 100 Hz 与 CAN Gate 收口

板上 confirmed Application 已更新为 `1.0.2 build1`。根因是 ICM45686 FIFO mode/depth 位定义
错误，同时承载 FIFO 状态机的 diagnostics task 周期为 100 ms，超过驱动 40 ms 轮询要求；已修正
`FIFO_CONFIG0` 为 MAX depth + STREAM `0x79`，并把 diagnostics 权威 Kconfig 默认周期调整为
10 ms。普通 ST-Link 复位后仍启动 1.0.2，证明 OTA confirmed 持久化完成。

30 秒静态 IMU Gate 实测 `100.506 Hz`，3062 个 sample 与 3062 个 FIFO frame 一致，FIFO error、
timestamp error 均为 0，Kalman 正常。随后完整 target regression 通过 UART、四任务、零 PWM、
0x200/0x240 heartbeat/fault、严格零速度 0x101→0x180、0x181，以及 200 ms command timeout 和
300 ms peer heartbeat timeout。电机主电源保持断开，未发送非零速度；动态 IMU、wheels-up、
非零运动和 bus-off/拔线故障注入仍为 `NOT VERIFIED`。

Release Application flash 为 `111028` 字节、RAM 为 `55552` 字节；BIN 为 `111036` 字节，
SHA-256 `483ba111f56bb360b46ba4ecbfc584547993f9d7e3ce47d13dbea3273fdcf139`；OTA 为
`111100` 字节，SHA-256 `30d8f0922e74093e8974712c175a226dc905327ea580b3918b2e5f99f3fe7c6a`，
payload CRC32 `0xFBAD0A63`。

## 2026-08-31 CAN 联调 preflight

Jetson 端已连接 ST-Link/V2、CH340、USB-C 和 USB 声卡，C1 LiDAR 未连接。USB 枚举已看到
ST-Link `0483:3748`、CH340 `1a86:7523` 和 PCM2902。WCH `ch341.ko` 已针对当前 kernel 编译、加载
并绑定，`/dev/ttyCH341USB0` 可读写。115200 target serial smoke 确认 `fw=1.0.1 build=1`、四任务
RUNNING、`control=STOPPED`、`fault=0`、左右 target/speed/PWM 全零，IMU/QSPI/LCD 正常；ADC 为
0 mV，电机主电源保持断开，UART Gate 为 PASS。

Jetson `can0` 已配置 500 kbit/s nominal、2 Mbit/s data、FD+BRS；修复线束后被动监听已稳定收到 STM32 0x200/0x240。
一次不涉及运动的 0x720 PING 无 ACK，在自动重发模式下真实进入 ERROR-PASSIVE，产生 1 次 bus-off、
1 次自动 restart 和累计 427218 个 bus error；该历史故障不再增长。修复线束并恢复历史 80% data
sample point 后，0x200/0x240 周期帧、0x720/0x721 握手和零速度 0x101 双向均已通过，Jetson 新增
错误为 0、STM32 `tec=0/passive=0`。CAN FD 基础双向链路记为 `HARDWARE PASS`；bus-off 专门故障
注入、电机主电源、非零速度和 wheels-up 仍为 `NOT VERIFIED`。

为减少下一轮手工步骤，新增 `tools/target/jetson_can_preflight.sh`：默认临时停止 brltty，按
`1a86:7523` 动态发现 CH340 并加载/绑定；仅显式 `--fix-brltty` 才持久 mask 服务。脚本以 root 配置
CH341、配置 `can0` 为 500 kbit/s + 2 Mbit/s FD/BRS（默认 data sample point 80%、one-shot on），然后只
被动监听 5 秒；脚本不发送 0x720/0x101，也不启动电机或 Nav2。

2026-08-31 修线后执行 `tools/target/run_target_regression.py --can-interface can0`：UART status、
STM32 heartbeat、0x101 零速度、0x180/0x181/0x200/0x240 和 200 ms command timeout 全部 PASS。
工具结束后停止发送控制帧，目标板按合同回到 `COMMAND_TIMEOUT/fault=0x1` 的安全状态；这不是回归失败，
下一轮开始前需重新提供有效控制命令或复位。

后续日志曾确认 tty 短暂生成后会被 brltty 重新抢占：`usbfs: interface 0 claimed by usb_ch341 while
'brltty' sets config #1`，随后 USB disconnect/re-enumerate，导致 `/dev/ttyCH341USB0` 消失。preflight
现默认只在本次运行中停止服务并清理残留进程，再执行 CH341 bind；需要持久禁用时必须显式传入
`--fix-brltty`。CH340 接口由 `1a86:7523` 动态发现，不再绑定固定 USB 拓扑路径。

本脚本已实际执行：Jetson 84% data sample point 下出现持续 `Bit0 Error` 和 RX error，5 秒仍为 0
有效帧；STM32 `can_status` 为 `tec=135 rec=1 passive=1`。因此 84% A/B 未通过，默认恢复历史曾
成功的 80%（可用 `CAN_DATA_SAMPLE_POINT=0.84` 显式复测）。该 84% 记录属于历史失败实验；当前
80% 下 CAN FD 基础双向链路和零速度 regression 均为 `HARDWARE PASS`。

## 代码基线

### 原生平台化迁移完成度

- 当前 `develop` 路线是 STM32 HAL + FreeRTOS 原生平台化，不是完整 Zephyr RTOS 移植。
- 原生平台化软件架构收敛已完成：目录、Device/Init、DT/Kconfig、主要驱动实例状态、启动层级、
  Application 内部边界和主机故障矩阵已经冻结；剩余为目标板回归。
- 已完成：Device/Init/linker、Kconfiglib、模块化 CMake、`board_config.h` 删除、dummy device
  清除，以及 CAN/UART/motor/encoder/power/IMU/display/flash/watchdog/RTC/time/GPIO consumer
  的 device/API 接入。
- Application 的 CubeMX/CubeIDE 管理内容已集中到 `firmware/application/stm32g474/cubemx/`；
  Application linker script 位于 `boards/chassis_g474/application.ld`，继续使用 `0x08008000 / 480K`。
- 产品 UI 已从顶层 `ui/` 收入 `app/ui/`；`chassis_ui` 仍为独立 target，LCD renderer、presenter
  和主机预览共用实现保持不变。
- 顶层目录已收敛为 11 个：产品模块归入 `app/`，纯算法归入 `lib/`，通信归入
  `subsys/communication/`，FreeRTOS runtime 归入 `kernel/freertos/`；空 `infrastructure/` 已删除。
- Application 内部第二次收敛已完成：`subsys` 只保留 communication，五个产品域直接位于
  `app/`，板载测试迁入 `app/maintenance/self_test/`，`tests/` 只保留 host unit；
  `chassis_app.c` 从 935 行缩为 57 行，五类运行时位于 `app/runtime/`。
- `0.15.0 build12` 已完成 UART OTA、普通复位和启动静态状态回归：四任务 RUNNING，ADC、QSPI、
  IMU、LCD 和 GPIO 正常，控制为 STOPPED、fault 为 0，四路 PWM/target/speed 为零。
- Application 当前源码和板上 confirmed 镜像：`1.0.3 build1`。
- Bootloader：`0.1.0 build22`。
- `0.15.0 build12` 是最后一个 pre-1.0 confirmed 基线；`1.0.0 build1` 是架构、Device/Init、
  配置系统、CAN FD V1 和 STM32 GPIO/callback 修复收口后的首个正式候选。版本提升不改 CAN、
  UART、OTA 或参数存储线协议。
- `1.0.0 build1` 已通过全量 host/schema/config matrix/LCD preview、Debug/Release clean build、
  OTA 打包和真实 UART OTA。普通 ST-Link 复位后仍启动 1.0.0，稳定状态为 STOPPED、fault=0、
  四任务 RUNNING、PWM/target/speed 全零，ADC、IMU、LCD、QSPI、GPIO 和 SR501 READY；Release
  payload 为 `111016` 字节、CRC32 `0x781EE3F3`，OTA 包为 `111080` 字节。
- `1.0.1 build1` 修复 PB8 EXTI HAL mask 与 DTS pin 编号不一致导致的换页失效；已通过全量软件
  回归、UART OTA、普通复位、自动 target serial smoke 和 PB8 实际换页。Release payload 为
  `111040` 字节、CRC32 `0x02FDA59F`，OTA 包为 `111104` 字节。
- `tools/target/run_target_regression.py` 已作为轻量目标板回归入口：自动检查启动日志、版本、UART
  四分区 status、fault/STOPPED、四任务、零 PWM、ADC、IMU、QSPI、LCD 和 OTA 状态，并输出
  PASS/FAIL/JSON。可选 CAN FD smoke 只发送零速度 0x101 和 STOP；当前主机无 `can0`，该项为 SKIP。
- b12/build22 的 factory 文件继续作为冻结恢复产物保留，不改写历史文件。`0.10.0`、`0.11.0`
  的既有验证记录继续保留；`0.11.1`、`0.12.0`、`0.13.0` 和 `0.14.0` 的历史证据不被新版本覆盖。
- 最新 Release `build/arm-release/app-v0.12.0-b1.ota` 的 payload 为 `98088` 字节、CRC32 为
  `0x124D4C30`；OTA 包共 `98152` 字节，已通过 UART OTA 写入并确认。
- 当前 UI Release `build/arm-release/app-v0.13.0-b1.ota` 的 payload 为 `98264` 字节、CRC32 为
  `0x4659F611`；OTA 包共 `98328` 字节，已通过 UART OTA 写入并确认。
- 当前 UI Release `build/arm-release/app-v0.14.0-b1.ota` 的 payload 为 `99628` 字节、CRC32 为
  `0x5BEC2E71`；OTA 包共 `99692` 字节，已通过 UART OTA 写入并确认。
- `0.15.0 build12` payload 为 `111016` 字节、CRC32 `0x46CA92E2`，OTA 包为 `111080` 字节；
  已完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。
- build12 已修复 SPI HAL callback 对象未进入 ELF、ADC 成功值映射、DTS pin 编号到 STM32 HAL
  GPIO mask 的转换，以及急停 ISR/任务确认和机械反跳安全处理。PD2 保持内部上拉、低有效、
  下降沿触发；按下接通 GND，不按时断开。
- 1.0.0 已实测持续按住急停后进入 `EMERGENCY_STOP/fault=0x2` 且左右 target/speed/PWM 全零；
  松开并等待 2 秒后状态和零输出保持不变，机械反跳不会自动解除锁存，记为 `HARDWARE PASS`。
- 当前阶段：此前 OTA V1 冻结范围已解除开发阻塞，后续软件架构、协议、主机测试和构建不再等待
  CAN FD OTA、断电恢复、回滚、电气零输出、SR501 高电平、PID 闭环、里程计或 IMU 动态轴向的
  目标板条件。上述硬件项目继续按机会单独验收，未实测内容保持 `NOT VERIFIED`。SR501 代码、
  接线、60 秒预热和低电平零误计数已有上板证据；模块指示灯和 OUT 高电平仍需实测。当前代码主线已完成
  FreeRTOS 四任务、统一状态快照和正式 UART 消息实现。LCD 四页代码与 DMA 调度修复已完成；
  0.14.0 暗色工业仪表视觉已人工确认，0.15.0 的 `app/ui/lcd` 分层和同源渲染器已随 build12
  上板运行，启动状态为 LCD READY。
  ICM45686 已读取 `WHO_AM_I=0xE9`，FIFO/DMA、10 ms timestamp、静止零偏、Mahony 和 Kalman
  静态输出已上板通过；因模块安装位置和方向尚未固定，安装轴向和动态姿态验证标记为 `NOT VERIFIED`。
  目标加减速限制、
  编码器异常保护和欠压保护代码已实现，带负载阶跃、异常脉冲和欠压注入仍待实测；SR501
  高电平闭环继续标记为 `NOT VERIFIED`，但不阻塞后续软件开发。`0.12.0` 已完成 UART OTA confirmed，统一采样时间戳和差速轮式里程计
  已运行；`0.14.0` 已完成 UART OTA confirmed，新版 LCD UI 视觉、文字排版和四页切换已人工确认正常。
- Chassis CAN FD V1 已收口：正式 `0x101` 不依赖开发握手，`0x100` 仅保留 legacy/development
  compatibility；`0x200` 为双向 heartbeat，300 ms 超时只更新对端在线状态，不刷新 200 ms
  运动命令 timeout。control sequence 允许前向丢帧、拒绝重复/旧帧，并在 200 ms timeout 后重建
  baseline；Heartbeat/Fault 无条件发送，Motion/Odometry 才由 peer ALIVE 门控。cockpit-system
  `can-simulator` 已在隔离 `vcan0` 上完成无开发握手正式链路、
  heartbeat 超时、可选开发握手和状态解码；Navigator 产品接入和目标板 `can0` 尚未联调。

## 当前实现

- Kconfig 语义引擎已切换为 vendored Kconfiglib `14.1.0`；现有 Kconfig/prj.conf/CONFIG 输出和
  Release BIN 与切换前逐哈希一致，自研 lexer/parser/model/evaluator 已删除。

- Application 的独立 CubeMX 工程已加入 SPI3（PC10/PC11/PC12）、两个预留按钮（PD3/PD4）
  和 SR501 输入（PD5）；CubeMX 生成结果包含 `hspi3`、`MX_SPI3_Init()`、SPI3 DMA1 CH5/CH6、
  EXTI1/3/4，以及 PD5 普通输入和内部下拉初始化。
- 已加入 `drivers/button` 通用按钮事件驱动。按钮目前只产生消抖后的 pressed event，不绑定业务。
- LCD 已移除独立全屏封面页，将 taifei 裁剪为带透明掩码的 40x40 RGB565 Logo 并固定绘制在
  四个功能页右上角；原始 `picture_tafei.h` 保留但不再链接到当前页面。`OVERVIEW` 将电压和
  百分比放大到 3 倍字号并扩大电量条，`MOTOR` 使用左右双栏，其余页面按信息层级重新排版。
  主体背景和内容面板改为中性深灰，移除贯穿全屏的青色横线；标题仅保留短青色下划线，
  页脚和双栏分隔使用低对比灰色。
  四页均读取同一 `SystemStatusSnapshot`，已接线的 `PB8/BOOT0` 按键用于循环切页；PD3/PD4
  只保留 CubeMX/BSP 配置且尚未接线。新版布局、按键循环、Logo 和电量显示已人工确认正常。
- 已加入 `drivers/sensor/sr501_stm32.c` 轮询驱动。PD5 由 CubeMX 生成代码配置为内部下拉普通输入；驱动忽略 60 秒预热期，
  使用 50 ms 稳定滤波，只统计 READY 后的稳定低到高事件，并通过 `status` 输出原始电平、
  稳定运动状态、事件计数、最近事件时间和剩余预热时间。状态已进入统一快照、UART 和 LCD，
  但不绑定电机、安全或具体业务。
- ICM45686 已拆分为 HAL 无关的 `lib/icm45686` 寄存器/FIFO 驱动、HAL 无关的
  `lib/imu_fusion` 六轴融合组件，以及 `drivers/sensor/icm45686_stm32.c` STM32 HAL SPI3/DMA 适配层。
  当前支持 WHO_AM_I、软复位、MREG 字节序、量程/ODR、ODR/4 内部低通、FIFO watermark、
  16 字节帧、DMA批量读取、FIFO full/非法帧/传输失败flush恢复、16位timestamp动态采样周期、
  静止窗口零偏标定、Mahony 四元数和 roll/pitch/yaw 诊断输出。当前不启用20-bit、压缩FIFO
  或自检；零偏仅保存在RAM，安装方向和轴映射等待实物确认。`0.8.0` 已启用该路径，启动日志
  根据真实结果输出 `READY / NOT_FOUND / INIT_FAILED`，模块缺失或通信失败不阻塞 Application。
  `0.11.1` 新增独立 roll/pitch 两状态角度+陀螺零偏 Kalman 输出，Mahony 仍作为现有对照输出；
  Kalman 结果已进入 IMU 快照和 UART 诊断，未替换 LCD 当前姿态显示。参考 ICM45686 数据手册
  修正 `SREG_CTRL.SREG_DATA_ENDIAN_SEL` 为 bit 1，并在初始化时回读确认大端配置。

- 内部 Flash 使用 32 KiB Bootloader + 480 KiB 单 Application。
- QSPI 使用双 metadata 和 Slot A/B，confirmed/candidate 由 metadata 分配。
- UART 与 CAN FD 共用 OTA 会话、QSPI 暂存、校验和 `STAGED` 提交流程。
- Bootloader 支持安装、TRIAL、CONFIRMED、confirmed repair 和 rollback。
- 只有双 metadata 全擦除的 factory 场景允许 vector fallback。
- candidate、rollback、confirmed repair 每次擦除内部 Flash 前持久化增加尝试次数，最多 3 次；
  任意一次安装中断后都会先校验 QSPI slot payload 和内部镜像，若上次安装已成功则直接补交
  `TRIAL/CONFIRMED`，不再执行下一次破坏性擦除。
- 进入 `ROLLBACK_PENDING` 时即使 `install_attempts=0` 也先完整验证 confirmed；内部已是健康
  confirmed 时直接提交 `CONFIRMED`，不执行无意义擦除。Flash 布局不支持属于全局 fatal，
  直接进入 Recovery，不尝试同样无法成功的 rollback。
- confirmed 恢复校验区分 `MATCH / INTERNAL_MISMATCH / SOURCE_INVALID / IO_ERROR`。只有 source
  的 header、向量表和完整 payload CRC 均有效，且明确证明内部镜像不匹配时才允许擦写；
  QSPI I/O 最多非破坏性重试 3 次，持续失败或 source 无效均进入 Recovery。
- rollback/repair 成功后从 confirmed slot header 回填 metadata `image_size/image_crc32`。
- UART 最终响应等待对应 DMA token 成功；30 秒超时只作用于等待 BEGIN 阶段。
- factory 工具默认必须同时生成内部组合 BIN 和 QSPI confirmed raw；只生成内部镜像必须显式
  使用 `--internal-only`，且不构成生产回滚基线。
- Application 正常使用约 10 秒 IWDG，OTA 复位前切换约 30 秒；Bootloader 只刷新继承实例。
- 当前代码已运行 `control_task`、`service_task`、`diagnostics_task`、`display_task` 四任务；
  `SystemStatusSnapshot` 记录四个任务的周期、期望周期、超时、运行状态、运行次数、栈余量、
  heartbeat age、uptime、RCC 复位原因，以及板级/传感器/通信/供电/RTC 状态。UART `status`
  和 LCD 四页读取同一快照；正式 `[RSP]`、`[LOG]`、`[TEL]` UART emitter 已实现。VOFA 数字流
  保留为显式兼容模式。`display_task` 以 1 ms 周期推进 LCD 逐行 DMA，保持页面 1 s 刷新。总览页新增
  9.0--12.6 V 电压窗口估算百分比和电量条；该值不是电池 SOC，阈值需按最终电池规格校准。
- 0.15.0 已收敛实际依赖边界：`app/ui/lcd` 持有主题、字模、Logo、四页布局和逐行像素生成，
  `drivers/display/lcd_stm32.c` 只持有控制器命令、SPI DMA、片选和背光；`app/diagnostics/system_status_collector` 负责 driver/RTOS
  状态到诊断 DTO 的组装；RTOS 通过 Core 注入的周期回调调用 Application，不再反向包含 `app`；
  `wheel_controller` 通过电机端口装配，不再直接依赖电机 BSP。
- 2026-08-20 继续完成全仓库边界收敛：FDCAN ISR 只把原始帧写入 drivers/can 固定队列，握手、运动命令和
  OTA 解码均在 service task 执行；上层新增 UART、Flash、watchdog、time、RTC 通用 driver API，
  UART 通过 device/DTS chosen 接入，QSPI/watchdog/RTC/time 的 HAL 实现已移动到 STM32 driver；
  `app/subsys/lib` 不再直接包含 UART/QSPI/RTC/watchdog/time 的
  旧 BSP 头文件；Application CMake 已拆为 vendor、drivers、components、communication、subsys、
  chassis_product、app/ui、kernel/freertos 和 chassis_app 静态目标。
  头文件。
  OTA 解码全部在 `service_task` 执行；communication 公共接口不再暴露 HAL。RTC、单调时间、LED、
  E-STOP、IWDG、SPI/GPIO 回调和复位已通过 drivers/Core 边界访问；TIM6 启动由 Core 回调注入 RTOS。
  IMU SPI/FIFO/DMA 保留在 `drivers/sensor/icm45686_stm32.c`，Mahony/Kalman 状态迁入 `app/sensors/imu_orientation`；Console
  命令执行和 OTA 维护协调分别位于 `app/console/commands` 与 `app/maintenance/chassis_maintenance`，
  LCD 状态 presenter 迁入 `app/ui/lcd`。
- motor/encoder 已完成首批 Device Model 迁移：`drive0`、`left_encoder`、`right_encoder` 由
  `DEVICE_DT_DEFINE()` 注册，WheelController 通过 generic motor API 装配，编码器按左右 device
  分别读取；源码构建通过，电机安全和编码器方向仍需目标板回归。
- `board_config.h` 和全部 `BOARD_*` 依赖已删除；`flash0/watchdog0/rtc0/time0` 已从 dummy device 改为真实 API/vtable/init。button/LED/SR501/E-STOP GPIO consumer 已完成 device/API 接入，DTS 已增加 `gpios` phandle-array。
- UART v1 的消息外壳和已有字段语义保持兼容，字段及分区集合不冻结。四分区只承担当前完整
  诊断，不要求所有新功能都往其中堆字段；周期遥测按实际观察需求保持精简。
- 编码器在 100 Hz 控制采样点记录本地单调时间和实际累计周期，ADC 记录转换完成时间，IMU
  保留 FIFO 设备时间戳并映射到本地采样时间；快照同时报告数据年龄。RTC 仍只用于日历和日志。
- 差速里程计使用 `1320 counts/rev`、`65 mm` 轮胎有效直径和 `220 mm` 轮距，输出左右累计
  距离、`x/y/heading`、线速度和角速度，并接入统一快照、UART、文本遥测和 LCD 电机页；
  `odometry reset` 可在停止状态清零。IMU 安装固定前不进行陀螺 Z 轴航向融合。
- LCD UI `0.15.0` 统一公共 Header/Footer、32 px Logo、状态语义和 RGB565 颜色；普通数据使用白色，青色用于导航/电量重点，
  绿黄红只表达健康、停止/不可用和故障，四页改为明确的仪表层级；保留 PB8 单键切页、透明 Logo 和统一快照数据源。
- PID 参数已加入 QSPI 双副本持久化，修改后立即在 RAM 生效，由 `service_task` 异步保存；当前
  左侧参数已实测保存为 `210/310/1`，返回 `persistence=STORED sequence=1`。
- 电机开环测试支持运行期命令 `motor duty <0..8499>`，默认值仍为 `6500`，测试运行中
  禁止修改，复位后恢复默认值；该参数不写入 PID/QSPI。
- 控制收尾已加入目标加减速限制（每个 10 ms 控制 tick 最多变化 5 counts/tick），停止、
  急停和故障仍立即清零；编码器单周期异常增量和供电低于 9.0 V 会锁存 critical fault 并急停。

## 已验证

- 2026-08-18 Application `0.8.0 build1` 启用 ICM45686 完整路径后完成 CMake 构建：Debug
  `text=103160 data=120 bss=53384`，Release `text=92076 data=120 bss=53376`。Release 已通过
  UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED ->
  CONFIRMED`。普通复位后报告 `fw=0.8.0 build=1`、`ota_confirmation=NOT_REQUIRED`、四任务
  `RUNNING`、`control=STOPPED` 且左右 PWM 为零。重新接线后的 ICM45686 返回 `who_am_i=0x00` 和
  `imu=NOT_FOUND`，证明软件路径已运行，但不构成硬件通过。
- 2026-08-19 `0.11.1 build1` 已通过 UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL
  COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。普通复位后 Bootloader 报告 metadata state
  `0x5`，Application 报告 `ota_confirmation=NOT_REQUIRED`、四任务 `RUNNING`、`fault=0`、
  `control=STOPPED`、左右 PWM 为零。
- 2026-08-19 `0.12.0 build1` 已通过 UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL
  COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。确认后的 `status` 报告 `fw=0.12.0 build=1`、
  `ota_confirmation=CONFIRMED`、四任务 `RUNNING`、`fault=0`、`control=STOPPED`、左右 PWM 为零，
  供电约 `12.188 V`，`odom_valid=1`，LCD 为 `READY`，IMU `WHO_AM_I=0xE9` 且无 FIFO/时间戳
  错误。此次未驱动车轮。
- 2026-08-19 `0.13.0 build1` 已通过 `/dev/ttyUSB0` 完成 UART OTA 的 `STAGED -> INSTALL
  VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。在线复核报告四任务
  `RUNNING`、`fault=0`、`control=STOPPED`、左右 PWM 为零、`lcd=DRAWING`、IMU `READY` 和
  里程计静态零位；本轮未驱动车轮，LCD 新布局仍待人工目视确认。
- 2026-08-19 `0.14.0 build1` 已通过 `/dev/ttyUSB0` 完成 UART OTA 的 `STAGED -> INSTALL
  VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。稳定状态报告四任务均为
  `RUNNING`、`fault=0`、`control=STOPPED`、左右目标/速度/PWM 均为零、`lcd=READY`、IMU
  `READY` 且 FIFO/timestamp 错误为零；新版 LCD 视觉已由用户人工确认正常。
- 2026-08-20 `0.15.0 build1` 完成边界收敛后的 Debug/Release 构建、10 个 Application C 主机
  测试、Bootloader core 主机测试、13 个 OTA Python 测试、真实 C LCD 预览生成和 OTA 打包。
  Debug 为 `text=114020 data=120 bss=55128`，Release 为 `text=101636 data=120 bss=55120`。
  这些是软件证据，不构成目标板通过。
- 2026-08-25 `0.15.0 build12` 完成 UART OTA confirmed 和普通复位回归；GPIO pin/mask、SPI
  callback、ADC valid 与安全输入修复后的稳定状态为四任务 RUNNING、`control=STOPPED`、
  `fault=0`、PWM/target/speed 全零，ADC、IMU、LCD、QSPI 和 GPIO 正常。最终急停松开锁存场景
  未执行，保持 `NOT VERIFIED`。
- 2026-08-25 `1.0.0 build1` 完成 UART OTA confirmed、普通复位和稳定外设回归；持续按住 PD2
  后进入 `EMERGENCY_STOP/fault=0x2`，松开 2 秒后仍保持锁存和左右零 PWM，最终反跳安全场景
  已取得目标板证据。
- ICM45686 实测 `WHO_AM_I=0xE9`。修正端序配置后，调试快照连续 588 帧无解析、timestamp、
  DMA 或传输错误，采样周期为 `10 ms`；200 个静止样本后零偏标定、Mahony 和 Kalman 均有效。
  普通复位后的 UART `status` 再次报告 224 帧、`imu_fifo_errors=0`、
  `imu_timestamp_errors=0`、`imu_kalman=1`。安装位置和方向未固定，动态姿态验证为 `NOT VERIFIED`，
  已重新纳入当前验收。
- 同日通过 `/dev/ttyUSB0` 对 `0.8.0 build1` 做其他硬件在线复核：供电 `12.206--12.215 V`、
  RTC 有效、四任务 `RUNNING`、LCD 驱动 `READY`、CAN 无 bus-off/protocol error、编码器静止
  读数 `left_total=1/right_total=-2`；QSPI 保留区 1024 字节擦写回读自检通过（地址
  `0x007FF000`）。本轮未发送电机、PID 或 OTA 命令，PWM 保持零。
- 2026-08-18 已通过 `/dev/ttyUSB0` 对 `0.9.0 build1` 做架空轮启动复核：左侧 `6500/8499`
  运行 1 秒后编码器累计约 `5837`，右侧约 `4598`；两次结束均回到 `control=STOPPED`、
  `left_pwm=0/right_pwm=0`。方向沿用既有实物验收结论，本轮不重复判定方向。
- 同日已实测 `pid left 210 310 1` 返回 `persistence=QUEUED`，随后收到
  `module=parameters event=SAVED sequence=1`，`pid show` 返回 `persistence=STORED`。
- 2026-08-18 在约 12.22 V、架空轮、单次 1.25 s 测试下复核启动下限：左侧可靠启动约
  `3500/8499`，右侧可靠启动约 `3500/8499`（约 41.2%）；`3000--3200` 左侧和
  `3200` 右侧存在偶发不启动，故不作为可靠下限。测试结束已恢复 `6500` 和零 PWM。
- 同日对 `0.10.0 build1` 做低速闭环实测：`pid target 5 5` 下左右速度约收敛到
  `5 counts/tick`，编码器持续增长，PWM 约稳定在 `1.9k--2.4k`，随后 `pid stop`
  正常回到零 PWM。高目标斜坡测试结束后无 fault 锁存。
- 同日通过 ST-Link 读取目标板 GPIO/SPI 寄存器：PC10/PC11/PC12 为 SPI3 AF6，SPI3 时钟已开，
  PD0 CS 为输出高电平；ICM45686 仍返回 `WHO_AM_I=0x00`，软件配置正常但硬件连线未通过。
- 同日已将 Release `0.6.0 build1` 通过 UART OTA 写入 QSPI 并完成 `STAGED -> INSTALL VERIFIED
  -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。普通复位后仍报告 `fw=0.6.0 build=1`、
  `ota_confirmation=NOT_REQUIRED`、四任务 `RUNNING`、`lcd=READY` 且左右 PWM 为零；真实断电
  重上电尚未执行。
- 直接烧写 `0.5.0 build1` 后复位被 Bootloader 按 confirmed QSPI `0.3.0 build1` 自动恢复，
  属于保护路径；随后 GDB 直接启动 Application 观察到 `lcd=DRAWING`，确认 20 ms 显示任务
  无法及时推进逐行 DMA。本轮已将显示任务周期修复为 1 ms，版本提升为 `0.5.1`；调试运行
  已进入 `lcd=READY`，但发现快照 expected period 仍为 20 ms，已在 `0.5.2` 统一为 1 ms。
- 2026-08-18 Application `0.3.0` build `1` 已在目标板确认四任务均为 `RUNNING`，周期、栈余量、heartbeat age、运行次数
  和复位原因可通过同一 `SystemStatusSnapshot` 读取；`critical_tasks=1`、`control=STOPPED`、
  `fault=0`、`overrun=0`、`missed=0`。
- CMake 3.22 + Ninja + GNU Arm Embedded 14.3.rel1 当前 `0.14.0 build1` 工作树构建通过：
  Application Debug `text=111592 data=120 bss=54736`，Release `text=99500 data=120 bss=54728`。
  Release payload 为 `99628` 字节、CRC32 为 `0x5BEC2E71`；本批 UI 产物已完成 UART OTA confirmed。
- build1 已实测结构化启动 `[LOG]`、命令 `[RSP]`、同序号四分区 `[TEL]`、错误响应、PID 参数读取、
  100 ms 文本遥测、CRLF 换行和 `encoder_result`。nano printf 不支持 `%lld` 导致的编码器及后续
  变参错位已改为独立 64 位十进制格式化，并在目标板确认编码器、PID、overrun/missed 字段正确。
  未执行任何电机命令。
  上板曾发现新增 SR501 行使 `status` 达到 1251 字节，超过原 UART 1200 字节消息限制；b16
  将诊断缓冲和 UART 消息上限统一为 2048 字节并增加编译期约束，完整报告已实测恢复。
  已完成实物闭环的较早 b13 Release 为 `text=185388 data=120 bss=34944`，对应 OTA payload
  185516 字节、CRC32 `0x6FD23D35`；不得把当前新增 SR501 的构建视为同一上板产物。
  Bootloader build22 和 QSPI provisioner 的既有构建结果保持有效。ICM45686 FIFO/MREG 和
  IMU fusion 纯组件测试均使用 MSVC `/W4 /WX` 编译并执行通过。

- 已写入目标板的 b12/build22 factory 产物尺寸仍为 Application `text=183492 data=96 bss=34224`、
  Bootloader `text=13400 data=48 bss=1656`；这是冻结产物证据，不等于当前工作树构建。
- `arm-debug` 已使用 `-Og -g3` 构建，ELF 含源码调试信息和 FreeRTOS 内核符号；VS Code
  Cortex-Debug/OpenOCD 配置已就绪。2026-08-15 已在目标板验证无 `sudo` OpenOCD、GDB
  烧写 Debug ELF、停在 `main`、源码断点、调用栈、FreeRTOS `pxCurrentTCB` 和 Debug IWDG
  冻结；VS Code 图形界面 F5 已于 2026-08-15 人工确认通过。
- 在引入当前 ICM45686/调试改动之前，CMake 生成的三个 BIN 曾与已上板 b12/build22/provisioner
  冻结产物逐哈希一致；当前工作树的 clean build 已变化，不能沿用旧哈希或直接覆盖 `_output/`。
  CMake 作为主命令行构建入口，CubeIDE 暂留作对照和调试。
- OTA Python 共 9 项通过，包含字段顺序/offset 级共享 C/Python ABI 对照；两个目标 clean build
  已由 ARM GCC 验证共享结构体的逐字段 `offsetof` 静态断言。既有 factory、
  UART arm guard、Application metadata 和 Bootloader core 主机测试记录保持有效；本轮新增的
  rollback attempts=0/fatal 分类断言已通过目标 GCC `-Werror` 编译，尚未在宿主机执行。
- 2026-08-17 已使用 build22 从 confirmed b12 通过 UART OTA 安装 b13：发送工具完成
  185580 字节传输并收到 `STAGED`，Bootloader 依次报告 `INSTALL VERIFIED`、
  `TRIAL COMMITTED`、`TRIAL VERIFIED`，b13 健康窗口后报告 `OTA_CONFIRM: CONFIRMED`。
- b13 在没有 ICM45686 的情况下正常启动，串口报告 `ICM45686: NOT_INITIALIZED`、
  `LCD: READY`、`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`；confirmed 普通
  复位后 metadata state 为 `0x5`，随后报告 `OTA_CONFIRM: NOT_REQUIRED`，未出现 critical
  fault 或 IWDG 复位循环。
- factory 普通启动和 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` 已实物通过。
- 已从冻结 Release ELF 生成匹配的 b12/build22 Application BIN、OTA、内部 factory BIN 和
  8 MiB QSPI confirmed raw；OTA Python 9 项重新通过。产物生成不代表已烧录或实物通过。
- 已通过独立 DFU provisioner 将匹配的 b12 OTA package 和 `CONFIRMED/SLOT_A` metadata
  写入 W25Q64 并完成全包读回校验；随后恢复内部 factory BIN，实物确认 build22 读取
  `CONFIRMED` 并启动 b12。Application 稳定报告 QSPI `EF4017`、`OTA_CONFIRM: NOT_REQUIRED`
  和 `MOTOR: DISABLED`。

## 尚未验证

- VS Code 图形界面 F5 已于 2026-08-15 人工确认通过；底层 OpenOCD/GDB 自动烧写、停在
  `main`、源码断点、调用栈、FreeRTOS 符号和 Debug IWDG 冻结也已完成命令行等价目标板验证。

- ICM45686 SPI3、`WHO_AM_I=0xE9`、FIFO/DMA 连续性、10 ms timestamp、静止零偏和
  roll/pitch Kalman 静态输出已上板通过；模块安装位置和方向尚未固定，正负 roll/pitch/yaw
  动态轴向、安装方向、运动恢复和长时间漂移均为 `NOT VERIFIED`。两个预留按钮的机械消抖也尚未
  上板验证。

- `0.7.1` 当前可见页面的中性配色已由用户确认可接受；LCD
  `OVERVIEW -> MOTOR -> SENSORS -> SYSTEM -> OVERVIEW` 四页完整内容、透明 Logo、电量显示
  和 PB8 单键循环已确认正常。PD3/PD4 仍未接线且不参与本轮操作。

- SR501 已按 5 V、共地、OUT 接 PD5 完成接线。b16 实测 `warmup_ms` 递减并在 60 秒后进入
  `READY`，预热期间和 READY 后持续低电平均保持 `motion=0 raw=0 count=0`。模块指示灯未亮，
  OUT 高电平、50 ms 稳定滤波、单次上升沿计数和持续高电平不重复计数均为 `NOT VERIFIED`。

- confirmed `0.12.0 build1` 真实断电重上电和四路 PWM 电气零输出：`NOT VERIFIED`；此前完成普通
  复位和 UART OTA 确认，未执行断电测试。
- `0.14.0 build1` LCD 新布局已上板且驱动报告 `READY`；状态颜色、文字排版和四页切换已人工目视确认正常。
- `1.0.1 build1` 已完成 UART OTA、普通复位、四任务、静态零 PWM、外设启动、急停松开保持锁存
  和 PB8 实际换页复核；真实 CAN FD V1 和电机闭环仍按各自条目验证。
- `0.12.0 build1` 的里程计方向、直线距离、原地旋转角度、时间对齐误差和 LCD 里程计动态显示
  尚未验证；当前只确认固件启动快照中的静态零位输出。
- CAN FD OTA：`NOT VERIFIED`，纳入当前 UART/CAN OTA 同批验收。
- Application 安装过程中断电恢复、TRIAL 不确认自动回滚和 rollback 安装中断电：`NOT VERIFIED`。

- 完整低速 PID 稳定性、带负载停车和长时间运行保护仍未验证；本轮只完成架空轮短时响应。

confirmation 持续失败、QSPI terminal cleanup 和其他 recovery 边角也重新纳入故障注入和恢复验收，
不再以冻结范围排除。

## 下一步

1. 将 `1.0.1 build1` 发布提交 fast-forward 合并到 `main`，创建 annotated `v1.0.1` tag。
2. 在 Jetson 配置真实 CAN FD `can0`（500 kbit/s nominal、2 Mbit/s data、BRS），运行
   `tools/target/run_target_regression.py --can-interface can0` 的零速度安全回归。
3. 单独执行 sequence、200 ms control timeout、300 ms heartbeat timeout 和外部 bus-off 故障注入；
   未准备真实故障条件时保持 `SKIP/NOT VERIFIED`。
4. 再按硬件条件推进 CAN FD OTA、断电/回滚恢复、带负载 PID、里程计校准和 IMU 动态轴向。

当前路线：

```text
1.0.1 main/tag
-> 真实 can0 零速度 target regression
-> sequence/timeout/bus-off
-> CAN FD OTA 与断电/回滚故障注入
-> 带负载 PID、里程计几何校准和 IMU 动态轴向
```

## 对话交接要求

新对话先读本文件和任务对应的权威文档。完成工作后只更新真实发生变化的内容；不要根据旧聊天
推断硬件通过，也不要因为合并来源中缺少某段内容而删除现有文档。
