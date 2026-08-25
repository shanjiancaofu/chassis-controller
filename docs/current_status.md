# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

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
- 尚未完成：本轮架构代码的 UART OTA、启动静态状态、UART/QSPI/IMU/GPIO/LCD 和电机零输出目标板回归。

- Application 工作树：`0.15.0 build2`；板上 confirmed 镜像：`0.15.0 build1`。
- Bootloader：`0.1.0 build22`。
- 板上 confirmed 镜像已通过 UART OTA 更新为 Application `0.14.0 build1`；Bootloader 仍为
  build22，b12/build22 的 factory 文件继续作为冻结恢复产物保留，不改写历史文件。`0.10.0`
  和 `0.11.0` 的既有验证记录继续保留；`0.11.1` 和 `0.12.0` 已完成 UART OTA 和普通复位回归。
- 最新 Release `build/arm-release/app-v0.12.0-b1.ota` 的 payload 为 `98088` 字节、CRC32 为
  `0x124D4C30`；OTA 包共 `98152` 字节，已通过 UART OTA 写入并确认。
- 当前 UI Release `build/arm-release/app-v0.13.0-b1.ota` 的 payload 为 `98264` 字节、CRC32 为
  `0x4659F611`；OTA 包共 `98328` 字节，已通过 UART OTA 写入并确认。
- 当前 UI Release `build/arm-release/app-v0.14.0-b1.ota` 的 payload 为 `99628` 字节、CRC32 为
  `0x5BEC2E71`；OTA 包共 `99692` 字节，已通过 UART OTA 写入并确认。
- 当前工作树 Release `build/arm-release/app-v0.15.0-b1.ota` 的 payload 为 `101764` 字节、
  CRC32 为 `0x447F9AC9`；OTA 包共 `101828` 字节。该包只完成构建、打包和主机验证，尚未烧录。
- `0.15.0 build1` 已通过 UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED ->
  TRIAL VERIFIED -> CONFIRMED`。稳定状态保持四任务 RUNNING 和左右 PWM/target/speed 全零，但
  暴露 SPI HAL callback adapter 未进入 ELF，导致 LCD 长期 DRAWING、IMU 零样本，以及 ADC
  `int/-errno` 成功值被反向映射为 invalid。当前修复版 build2 payload `110416` 字节、CRC32
  `0x8EFB9966`，OTA 包 `110480` 字节；已完成构建和主机验证，尚未烧录。
- 当前板上 PD2 急停原始电平为低，`EMERGENCY_STOP/fault=0x2` 是真实 active-low 安全输入；
  build2 UART OTA 因维护锁返回 `code=BUSY`，没有开始传输。物理释放急停后才能继续 OTA。
- 当前阶段：此前 OTA V1 冻结范围已解除开发阻塞，后续软件架构、协议、主机测试和构建不再等待
  CAN FD OTA、断电恢复、回滚、电气零输出、SR501 高电平、PID 闭环、里程计或 IMU 动态轴向的
  目标板条件。上述硬件项目继续按机会单独验收，未实测内容保持 `NOT VERIFIED`。SR501 代码、
  接线、60 秒预热和低电平零误计数已有上板证据；模块指示灯和 OUT 高电平仍需实测。当前代码主线已完成
  FreeRTOS 四任务、统一状态快照和正式 UART 消息实现，
  LCD 四页代码与 DMA 调度修复已完成；0.14.0 已按暗色工业仪表规范重排层级、状态颜色、卡片和页脚，目标板视觉已人工确认正常。0.15.0 将页面渲染从 LCD BSP 拆到 `app/ui/lcd`，预览工具直接编译同一 C 渲染器，当前重构尚未上板。
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
- `0.15.0 build1` 尚未烧录；LCD 四页、PB8 切页、四任务状态、异常零 PWM 和 UART OTA 安装均需
  在本版上重新复核，当前为 `NOT VERIFIED`。
- `0.12.0 build1` 的里程计方向、直线距离、原地旋转角度、时间对齐误差和 LCD 里程计动态显示
  尚未验证；当前只确认固件启动快照中的静态零位输出。
- CAN FD OTA：`NOT VERIFIED`，纳入当前 UART/CAN OTA 同批验收。
- Application 安装过程中断电恢复、TRIAL 不确认自动回滚和 rollback 安装中断电：`NOT VERIFIED`。

- 完整低速 PID 稳定性、带负载停车和长时间运行保护仍未验证；本轮只完成架空轮短时响应。

confirmation 持续失败、QSPI terminal cleanup 和其他 recovery 边角也重新纳入故障注入和恢复验收，
不再以冻结范围排除。

## 下一步

1. 在用户明确确认后执行 UART OTA。
2. 复核四任务、fault、零 PWM、UART、QSPI、LCD、IMU、GPIO consumer 和重启 confirmed。
3. 根据真实上板结果关闭剩余 `NOT VERIFIED`，不从主机测试推断硬件通过。

当前路线：

```text
架构检查 -> UART/QSPI/watchdog/RTC/time device boundary
-> OTA/CAN/断电/回滚验收 -> FreeRTOS 快照 -> 四任务调度
-> 正式 UART 消息 -> LCD 四页 -> ICM45686/Kalman -> SR501 UI/动态实物验证 -> 完整 OTA/保护故障注入
-> 里程计/安全保护实测 -> 目标加减速实测 -> 正式 CAN FD 协议
```

## 对话交接要求

新对话先读本文件和任务对应的权威文档。完成工作后只更新真实发生变化的内容；不要根据旧聊天
推断硬件通过，也不要因为合并来源中缺少某段内容而删除现有文档。
