# 变更记录

## 2026-08-24：Application 内部第二次收敛

- 删除 `app/modules` 包装层，chassis/safety/parameters/diagnostics/sensors 五个产品域直接归入
  `app/`；settings、console、telemetry 和 diagnostic report 从通用 subsys 迁回产品域。
- QSPI/IWDG/motor 板载测试迁入 `app/maintenance/self_test` 并统一改名为 SelfTest；`tests/`
  只保留 host unit/fake-HAL，正式 Application 不再依赖 tests。
- `chassis_app.c` 从 935 行缩为 57 行；新增 bootstrap、control、service、diagnostics、display
  五类 runtime，按原状态所有权迁移逻辑，不改变 CAN/UART/OTA wire ABI 或控制算法。
- 删除 `chassis_subsys`、`chassis_target_tests` 和旧 `chassis_modules` target，产品域统一由
  `chassis_product` target 管理；architecture checker 新增反向层依赖检查。
- Debug/Release clean build、13 项 C 主机测试、18 项 OTA/Python、Kconfig/DTS、5 项架构测试
  和 LCD C 预览通过；未烧录、未新增硬件结论。

## 2026-08-23：Application 手写目录收敛

- 顶层目录由 16 个降为 11 个，删除空 `infrastructure/` 和单文件 `linker/`。
- `modules` 迁入 `app/modules`，`components` 重命名为 `lib`，`communication` 迁入
  `subsys/communication`，`rtos_app` 迁入 `kernel/freertos`。
- `app` 保留 8 个装配/协调源文件在根目录，只有达到真实集合规模的 `modules/` 和 `ui/` 使用
  子目录；Application linker script 跟随板级配置放到 `boards/chassis_g474/application.ld`。
- CMake target 边界保持独立，业务源码内容未改；Release、13 项 C 主机测试和 LCD 预览通过，
  最终 BIN 与目录整理前逐哈希一致。

## 2026-08-23：Application CubeMX 工程目录收口

- 新增 `firmware/application/stm32g474/cubemx/`，集中 `.ioc`、CubeIDE 元数据、`Core/`、
  `Drivers/`、`Middlewares/` 和 `Application/User/`。
- Application Flash linker script 独立为 `linker/application.ld`；布局保持
  `FLASH 0x08008000 / 480K`、`RAM 0x20000000 / 128K`。
- CMake、CubeIDE linker 配置和 CubeMX/DTS 校验路径已同步；Release 完整构建通过，BIN 哈希不变。
- 本批只移动目录和修改构建路径，没有修改业务逻辑，也没有烧录。

## 2026-08-23：UI 归入 Application 产品层

- 将顶层 `ui/` 移入 `app/ui/`，LCD renderer、presenter、布局和 Logo 文件内容不变。
- `chassis_ui` 继续作为独立静态 target；CMake、源码 include、架构检查和预览工具路径已同步。
- Release 构建尺寸和 BIN 哈希保持不变；本批未烧录。

## 2026-08-23：原生平台化主机故障矩阵收口

- 新增统一 `application-host-tests` 目标，一次编译执行 13 个 HAL-independent/fake-HAL C 测试。
- 新增 device init failure 隔离测试，验证 optional device 失败不阻塞后续 device，system init
  function 失败仍终止当前 level。
- 使用真实 UART/QSPI driver 源码和 fake HAL 验证 UART RX/TX/队列满/错误恢复及 QSPI DMA
  complete/error/abort/range guard。
- OTA 工具增加 resume、BUSY、重复确认、错误 offset 和 session mismatch 回归；本批未烧录。

## 2026-08-23：原生平台化第二批收口

- UART、QSPI、ICM45686 和 E-STOP 运行状态迁入设备实例；HAL 回调按 handle 路由到 DTS device。
- LED 去除静态 config 指针，motor/encoder STM32 配置与数据类型移入 private header。
- `ChassisApp_Init()` 注册到 APPLICATION init level；device init error 与 required/optional 产品策略分离。
- app 层 FreeRTOS 临界区实现集中到 kernel runtime，架构检查禁止业务层再次直接包含 FreeRTOS。
- Debug/Release、Kconfig/DTS/架构/OTA 测试和 LCD C 预览通过；未烧录、未新增硬件结论。

## 2026-08-23：DT/device data 第一批收口

- 新增公共 Devicetree API，禁止 app/modules/communication/subsys/ui/rtos 直接包含生成头；
  disabled node 不再生成 device instance。
- DTS 移除 `cubemx-handle`、字符串 bus 和手工 phandle，加入 SPI/QSPI/ADC controller-child
  拓扑、`reg` 和可生成的 GPIO phandle-array。
- power、button、SR501、display 的运行状态迁入 `device->data`，STM32 私有配置/数据类型从
  generic driver header 分离。
- Debug/Release、Kconfig/DTS/架构/OTA 主机测试及 LCD C 预览通过；未烧录、未新增硬件结论。

本文记录 `chassis-controller` 每批实现改动。每批包含变更内容、设计决定和验证结果；详细构建数据与硬件证据仍以 [`verification.md`](verification.md) 为准，当前交接状态以 [`current_status.md`](current_status.md) 为准。

## 2026-08-20 - 解除冻结，恢复完整验收路线

### 变更内容

- 解除此前 OTA V1 代码基线和功能主线的冻结决定。
- CAN FD OTA、Application/rollback 安装中断恢复、TRIAL 不确认回滚、断电启动、四路 PWM 零输出、
  低速 PID、负载停车、里程计落地校准、IMU 动态轴向、SR501 高电平事件和保护故障注入全部恢复
  到当前活动验收路线。
- 已有历史 `HARDWARE PASS` 不被改写；尚未实测项目统一使用 `NOT VERIFIED`，不使用 `DEFERRED`
  表示当前排除。

### 验证结果

- 本次只调整验收状态和路线文档，没有烧录或目标板测试。
- 危险目标板测试必须先完成主机故障注入、日志断言、维护锁和四路零 PWM 断言，并在用户明确
  确认后执行。

## 2026-08-20 - 全仓库依赖边界收敛（0.15.0 build1）

### 变更内容

- FDCAN HAL 回调和固定深度原始帧队列下沉到 BSP；中断不再解析握手、运动命令或 OTA，全部由
  `service_task` 中的 communication 逻辑处理。FDCAN/CanTransport 公共接口移除 HAL 类型。
- 新增 time、RTC、LED、E-STOP、watchdog 和中断路由 BSP；TIM6 启动通过 Core 回调注入 RTOS，
  Application/communication/modules/rtos/ui 不再直接包含 CubeMX 外设头或调用 HAL。
- IMU SPI/FIFO/DMA 与姿态融合拆分：BSP 通过注入采样 sink 上报每个 FIFO 样本，
  `modules/sensors/imu_orientation` 持有 Mahony/Kalman 状态。
- 将 Console 命令执行、OTA 维护协调从 `chassis_app.c` 拆入独立 Application 文件；LCD 状态
  presenter 从 infrastructure 迁入 `app/ui/lcd`，电池估算阈值迁入 Application 配置。
- 新增 CanTransport 主机测试，覆盖握手、控制帧、OTA 分发、bus-off 会话撤销和恢复。

### 验证结果

- Debug `text=114020 data=120 bss=55128`，Release `text=101636 data=120 bss=55120`。
- 10 个 Application C 主机测试、Bootloader core 主机测试、13 个 OTA Python 测试、同源 C LCD
  预览和依赖扫描通过。
- Release payload `101764` 字节、CRC32 `0x447F9AC9`，BIN SHA256
  `ec730a156fe12b23a46a0e5b81d872cd2d355a4d718c8f473f8c255c9ac83a2f`；OTA 包 `101828` 字节，
  SHA256 `a7553b9b928de830412cd92b9b462acfdc88e6bb1f8938f5f0439d6c660bea41`。
- 本轮没有烧录；CAN、四任务、IMU、LCD、OTA 和零 PWM 的 0.15.0 目标板结果仍为
  `NOT VERIFIED`。

## 2026-08-19 - LCD 与运行边界收敛（0.15.0 build1）

本节是首轮 0.15.0 软件证据；当前 0.15.0 构建和 OTA 以 2026-08-20 条目为准。

### 变更内容

- 将 LCD 控制器/SPI DMA 留在 `bsp/lcd`，把主题、字模、Logo、DTO、四页布局和逐行渲染移到
  `app/ui/lcd`；Motor/System 分区断带和 Pose 极值文本溢出已修复。
- 预览工具改为主机编译真实 `lcd_ui.c`，Python 只负责 RGB565 转换和 2 倍最近邻 PNG 输出。
- 新增 `app/system_status_collector`，统一映射 BSP/RTOS 状态到诊断 DTO；公共
  `SystemStatusSnapshot` 不再暴露按钮、IMU、SR501 和 ADC 的 BSP 快照类型。
- RTOS 使用 Core 注入的四个 Application 周期回调，移除 `rtos -> app` 反向包含；
  `wheel_controller` 使用可测试的电机端口，不再直接包含电机 BSP。
- NMI、HardFault、MemManage、BusFault 和 UsageFault 在停留前调用有界电机紧急停机入口；
  PB8 显示键的 ISR/消抖下沉到 Button BSP；正式 Application target 不再编译宿主单元测试源码。

### 设计决定

- 已部署的 `0.14.0 build1` 不被同名新源码覆盖；本轮功能和边界变化提升为 `0.15.0 build1`。
- 保留现有 320x240、5x7 字模、RGB565、Logo、逐行 SPI DMA、四任务和线协议，不增加 GUI 框架
  或硬件依赖。
- `chassis_app.c` 只按真实装配/采集职责拆分，不按文件行数机械拆文件。

### 验证结果

- Debug `text=112988 data=120 bss=54544`，Release `text=100712 data=120 bss=54536`。
- 9 个 Application C 主机测试、13 个 OTA Python 测试和 5 张真实 C LCD 预览生成通过。
- Release payload `100840` 字节，CRC32 `0x72FEABE4`；OTA 包 `100904` 字节。当前包尚未烧录，
  不记录 0.15.0 硬件通过。

## 2026-08-19 - LCD 暗色工业仪表 UI 重构（0.14.0 build1）

### 变更内容

- 重排 Overview、Motor、Sensors、System 四页的卡片和信息层级，统一公共 Header/Footer、页码指示和 32 px 透明 Logo。
- 普通数据使用白色，青色用于导航和电量重点，绿/黄/红只表达健康、停止/不可用和故障；弱化固件版本和 `FAULTS 0`。
- 同步 `tools/lcd/render_ui_preview.py` 的坐标、RGB565 颜色、字模和 Logo 缩放，重新生成五张 320x240 预览图。

### 设计决定

- 保留现有 5x7 字模、逐行 SPI DMA、PB8 单键切页和统一 `SystemStatusSnapshot` 数据源，不新增字体库、GUI 框架或硬件依赖。
- `0.14.0` 保留独立的目标板视觉验收；串口驱动状态与用户人工目视结果分别记录。

### 验证结果

- Debug `text=111592 data=120 bss=54736`，Release `text=99500 data=120 bss=54728`，CMake/Ninja 构建通过。
- Release payload `99628` 字节，CRC32 `0x5BEC2E71`；OTA 包 `99692` 字节；`git diff --check` 和 LCD 预览生成通过。
- UART OTA 已完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`；
  四任务运行、故障为零、控制停止、左右 PWM 为零、LCD 驱动 READY。用户已人工确认 LCD 新视觉正常，记录为 `HARDWARE PASS`。

## 2026-08-19 - LCD UI 信息层级与布局优化（0.13.0 build1）

### 变更内容

- 增加四页页眉位置指示，保留 PB8 单键循环和透明 taifei Logo。
- 调整背景、内容区和分隔色的层次，使用暖色突出电量数值，降低整页单一蓝色观感。
- 将电机、传感器和系统页面的长串缩写改为 `CONTROL`、`POSE`、`MOTION`、`HEALTH` 等分组标签，
  不改变字段来源和页面数量。

### 设计决定

- UI 只读取既有 `SystemStatusSnapshot`，不在 LCD 任务中拼接业务状态。
- 本轮提升产品版本为 `0.13.0 build1`；UI 代码通过 UART OTA 更新到目标板，视觉验收仍单独记录。

### 验证结果

- Debug `text=110292 data=120 bss=54144`，Release `text=98136 data=120 bss=54136`，
  两套构建通过；OTA Python 13 项和 `git diff --check` 通过。
- Release payload `98264` 字节，CRC32 `0x4659F611`，OTA 包 `98328` 字节。
- OTA 包已完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`；
  串口启动复核报告四任务运行、故障为零、控制停止和左右 PWM 为零。页眉指示、文字排版、Logo、
  电量条和四页切换仍待人工目视确认。

## 2026-08-19 - 统一采样时间戳与轮式里程计（0.12.0 build1）

### 变更内容

- 编码器、ADC 和 ICM45686 FIFO 数据统一记录本地单调采样时刻与数据年龄；RTC 继续只负责日历
  和日志时间，不参与传感器融合。
- 基于 `1320 counts/rev`、`65 mm` 有效轮径和 `220 mm` 轮距实现差速圆弧里程计，输出累计
  距离、`x/y/heading`、线速度和角速度。
- 里程计进入统一状态快照、UART 状态、按需文本遥测和 LCD 电机页，并增加停止状态下的
  `odometry reset` 命令。
- 增加里程计宿主机测试和显式 CMake 源文件项。

### 设计决定

- UART v1 只保持消息外壳和已有字段语义兼容，字段及分区集合不冻结。四分区不作为所有新功能
  的固定容器；周期遥测保持精简，独立领域过大时可在 v1 增加分区或专用命令结果。
- 里程计使用控制周期采样时刻，IMU 按 FIFO 剩余帧数估算本地样本时刻，不用任务处理时刻冒充
  采样时刻。
- IMU 安装位置和轴向固定前，不将陀螺 Z 轴融合到里程计航向。

### 验证结果

- 里程计宿主机测试通过；Debug `text=110024 data=120 bss=54144`，Release
  `text=97960 data=120 bss=54136`；OTA Python 13 项和 `git diff --check` 通过。
- Release payload 为 `98088` 字节、CRC32 `0x124D4C30`，OTA 包为 `98152` 字节。
- 已通过 UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED ->
  CONFIRMED`，启动后四任务运行、故障为零、控制停止且左右 PWM 为零。时间戳对齐、里程计方向、
  直线距离、旋转角度和 LCD 动态新字段仍为 `NOT VERIFIED`。

## 2026-08-19 - roll/pitch 两状态 Kalman 对照输出（0.11.1 build1）

### 变更内容

- 在现有 Mahony 融合旁新增 roll/pitch 两状态 Kalman：状态为角度和陀螺零偏，观测为加速度重力方向。
- Kalman 输出进入 ICM45686 BSP 快照和 UART `sensors` 诊断；LCD 保持原 Mahony 姿态显示。
- UART v1 `sensors` 字段注册表同步增加 Kalman 有效状态和 roll/pitch 毫弧度字段。
- 增加宿主机测试，覆盖零偏初始化、姿态初始化、预测和无效重力观测处理。
- 按数据手册修正 `SREG_CTRL.SREG_DATA_ENDIAN_SEL` 位号，并在初始化时回读确认大端配置。
- 修正 Kalman 协方差观测更新并归一化 ±π 跨界创新，增加协方差与角度边界测试。

### 设计决定

- Mahony 不删除、不替换；Kalman 只做对照输出，不绑定控制或安全逻辑。
- Kalman 首版为中间 `0.11.0 build1`；动态滤波审阅修复后提升到 `0.11.1 build1`，不复用版本号。
- 模块安装位置和方向尚未固定，不根据临时摆放修改轴映射；安装轴向、动态响应、静止回归和
  长时间漂移统一 `DEFERRED`，主线转入统一单调时间轴和轮式里程计。

### 验证结果

- Debug/Release 构建通过：Debug `text=107576 data=120 bss=53624`，Release
  `text=95748 data=120 bss=53616`；ICM45686、fusion 和 OTA 宿主机测试通过。
- 目标板读取 `WHO_AM_I=0xE9`；FIFO 588 帧零解析/时间戳错误，10 ms 周期稳定，200 个静止
  样本后 Mahony/Kalman 均有效。
- UART OTA 完成 `STAGED -> INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED ->
  CONFIRMED`；普通复位回归保持故障为零、控制停止和左右 PWM 为零。
- 临时安装观察中，近水平 Kalman roll/pitch 为 `[-69, 8] mrad`，车体左侧抬高后为
  `[-52, -212] mrad`，采样连续且 FIFO/timestamp 错误为零。该结果只证明临时摆放下左右倾斜
  主要进入当前 pitch 通道，不构成最终安装轴向或映射验证。

## 2026-08-18 - 四项功能收尾构建与文档同步（0.10.0 build1）

### 变更内容

- 完成目标加减速限制、编码器异常保护、欠压保护和低速 PID 响应代码的收尾整理；保留 ICM45686
  未识别、SR501 高电平、带负载稳定性和故障注入的未验证状态。
- 修正安全故障位宏格式和控制周期异常路径缩进，重新生成最新 Release OTA 产物。

### 设计决定

- 代码实现完成不等同于硬件通过；负载阶跃、异常编码器脉冲、欠压注入和 ICM45686 线序排查
  继续按 `NOT VERIFIED/DEFERRED` 管理。
- 本轮只重新构建和校验 `0.10.0 build1`，不自动烧录目标板，避免把未回归的产物当作板上证据。

### 验证结果

- Debug：`text=106492 data=120 bss=53520`；Release：`text=94852 data=120 bss=53512`。
- OTA payload `94980` 字节，CRC32 `0x9BAB75E6`。
- OTA/ABI/UART Python 测试 13 项，以及参数记录和 UART 64 位格式化 C 宿主机测试通过；
  `git diff --check` 通过。

## 2026-08-18 - 控制安全收尾与低速 PID 响应（0.10.0 build1）

### 变更内容

- 双轮控制加入目标加减速限制，每个 10 ms 控制 tick 最多变化 5 counts/tick；停止、急停、
  超时和故障路径继续立即清零。
- 编码器单周期异常增量和供电低于 9.0 V 时锁存 critical fault 并急停；ADC 采样由诊断任务
  更新缓存，控制任务只读取缓存，不阻塞 ADC。

### 设计决定

- 欠压和编码器异常属于不可自动清除的安全故障，必须复位后重新确认。
- 低速 PID 先验证架空轮短时响应，不把短时响应等同于带负载稳定性或完整停车验收。
- ICM45686 软件配置保持不变；GPIO/SPI 寄存器已核对，硬件 `WHO_AM_I=0x00` 继续后置排查。

### 验证结果

- Debug/Release 构建通过并通过 UART OTA `CONFIRMED`。
- `pid target 5 5` 实测左右速度约收敛到 5 counts/tick，编码器持续增长，PWM 约稳定在
  `1.9k--2.4k`；`pid stop` 后 PWM 回零。
- 高目标短时斜坡测试后无 fault 锁存；目标板供电约 12.2 V，未触发欠压保护。

## 2026-08-18 - 可调开环测试占空比与启动下限复核（0.9.1 build1）

### 变更内容

- 新增运行期命令 `motor duty <0..8499>`，默认测试占空比仍为 `6500`；电机测试运行中
  禁止修改，复位后恢复默认值，不写入 PID 或 QSPI。
- 版本提升为 `0.9.1 build1`，避免把新增命令误作为同一源码版本的重复 build。

### 设计决定

- 测试仍必须经过显式 `confirm` 命令，单次运行约 1 秒并自动回零。
- 启动下限按“重复测试中稳定产生编码器计数”定义，不把偶发转动当作可靠阈值；方向继续
  使用既有实物验收结论。

### 验证结果

- `0.9.1 build1` 已通过 UART OTA 并报告 `ota_confirmation=CONFIRMED`。
- 约 12.22 V、架空轮条件下，左右可靠启动下限约为 `3500/8499`（41.2%）；临界档位
  受静摩擦、供电和负载影响，不能视为绝对物理常数。

## 2026-08-18 - PID 参数持久化与电机启动复核（0.9.0 build1）

### 变更内容

- PID 参数使用 QSPI 双副本记录 magic、格式版本、序号和 CRC32；参数修改后立即在 RAM 生效，
  由 `service_task` 异步保存，避免阻塞 `control_task`。
- 电机开环测试占空比固定为 `6500/8499`，单次运行 1 秒后自动停止并保持零 PWM。

### 设计决定

- 方向使用已有实物验收结论，本轮不重复测试方向；本轮只验证启动、编码器变化和自动回零。
- 参数持久化与电机启动互不绑定，电机启动/停止不会改变 PID 参数。

### 验证结果

- `pid left 210 310 1` 返回 `persistence=QUEUED`，随后收到
  `module=parameters event=SAVED sequence=1`；`pid show` 返回 `persistence=STORED`。
- 左侧启动后编码器约 `5837`，右侧约 `4598`；两次测试后均为 `control=STOPPED`、
  `left_pwm=0/right_pwm=0`。

## 2026-08-18 - ICM45686 正式启用（0.8.0 build1）

### 变更内容

- 打开 `ENABLE_ICM45686`，使 WHO_AM_I、寄存器配置、FIFO/DMA、静止零偏和 Mahony 融合路径
  正式进入 Application 运行镜像。
- 将启动日志从固定 `INITIALIZED` 改为真实的 `READY / NOT_FOUND / INIT_FAILED`，同时输出
  `device=ICM45686 who_am_i=0xNN`。

### 设计决定

- 模块未连接或通信失败时保持 Application 启动和零 PWM，通过每秒非阻塞重试等待恢复。
- 先取得 `WHO_AM_I=0xE9` 和连续 FIFO 数据，再进行安装方向、Kalman 和轮式里程计融合。

### 验证结果

- Debug/Release 构建通过：Debug `text=103160 data=120 bss=53384`，Release
  `text=92076 data=120 bss=53376`。
- Release BIN 为 92204 字节，payload CRC32 `0x1484C456`；UART OTA 完成 `STAGED ->
  INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。
- 目标板启动日志和遥测稳定报告 `imu=NOT_FOUND`、`who_am_i=0xFF`；普通复位后仍为
  `0.8.0 build1 / ota_confirmation=NOT_REQUIRED`，四任务运行、控制停止且左右 PWM 为零。
  该结果是明确的通信排查证据，不是 ICM45686 硬件通过。

## 2026-08-18 - LCD 中性配色修正（0.7.1 build1）

### 变更内容

- 将标题栏、内容面板和页面背景调整为中性深灰，降低大面积蓝色对界面的支配感。
- 移除贯穿全屏的青色横线，标题下方只保留一段短青色强调线。
- 页脚边界和电机左右双栏分隔改为低对比灰色；状态文字和电量条仍使用原有状态色。

### 设计决定

- 保留 `0.7.0` 的字号、坐标、四页信息结构和透明 Logo，只修正背景层级，不重新调整内容。
- 本次属于已部署 UI 的视觉修正，提升补丁版本为 `0.7.1 build1`。

### 验证结果

- Debug/Release 构建通过：Debug `text=89216 data=120 bss=53088`，Release
  `text=79272 data=120 bss=53080`。
- Release BIN 为 79400 字节，payload CRC32 `0xAABB8733`；UART OTA 完成 `STAGED ->
  INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。
- 普通复位后报告 `fw=0.7.1 build=1`、`ota_confirmation=NOT_REQUIRED`、四任务 `RUNNING`、
  `lcd=READY`、控制停止且左右 PWM 为零。中性配色实际观感仍待人工目视确认。

## 2026-08-18 - LCD 信息层级与大号电量显示（0.7.0 build1）

### 变更内容

- 将 LCD 文本布局从固定 7 行改为 10 个可独立配置坐标、字号和颜色的文本区域。
- 总览页使用 3 倍字号显示电压和百分比，将电量条扩大到接近全屏宽，并重新分区控制、CAN、
  QSPI、Fault 和版本信息。
- 电机页改为左右双栏，PWM、编码器和 PID 分组显示；传感器页和系统页按信息层级重新排版。
- 四页统一标题栏、内容面板、青色分隔线和底部状态栏，继续使用 40x40 透明 taifei Logo。

### 设计决定

- 保持现有四页内容和 PB8 单键循环，不新增页面；LCD 仍只读取统一 `SystemStatusSnapshot`。
- 电池百分比继续使用 9.0--12.6 V 电压窗口估算，当前 ADC 为 0 mV 时显示 0% 和空进度条。
- 不改变控制、电机、OTA、SR501 或 ICM45686 行为，所有电机输出保持零。

### 验证结果

- Debug/Release 构建通过：Debug `text=89204 data=120 bss=53088`，Release
  `text=79264 data=120 bss=53080`。
- Release BIN 为 79392 字节，payload CRC32 `0x7B2868B2`；UART OTA 完成 `STAGED ->
  INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。
- 普通复位后报告 `fw=0.7.0 build=1`、`ota_confirmation=NOT_REQUIRED`、四任务 `RUNNING`、
  `lcd=READY`、控制停止且左右 PWM 为零。大号电量显示、透明 Logo、四页无重叠和 PB8 循环
  尚待人工目视确认，保持 `NOT VERIFIED`。

## 2026-08-18 - LCD UI 美化与电量估算（0.6.0 build1）

### 变更内容

- 将 taifei Logo 从 48x47 调整为带透明掩码的 40x40 RGB565 资源，去除页面上的白色矩形背景。
- 四页统一为深色背景、标题栏、青色分隔线、交替信息区和状态色，总览页增加电量进度条与
  百分比显示。
- 百分比按 9.0--12.6 V 电压窗口估算，阈值集中在 `board_config.h`；保留实测电压作为主要
  数据，不把估算百分比当作精确 SOC。

### 设计决定

- 原始 `picture_tafei.h` 和 `tafei.jpg` 保留；页面只链接新的紧凑 Logo 与透明掩码。
- UI 继续由 `display_task` 逐行 DMA 绘制，数据继续来自统一 `SystemStatusSnapshot`，不改变
  电机、控制、OTA 或 UART 协议。

### 验证结果

- Debug/Release 构建通过：Debug `text=88612 data=120 bss=52944`，Release
  `text=78660 data=120 bss=52944`。
- `0.6.0` 已通过 ST-Link 写入并由 GDB 直接启动；串口确认四任务 `RUNNING`、控制停止、左右
  PWM 为零，LCD 在刷新周期中能够进入 `READY`。新 Logo、百分比、电量条和四页视觉效果
  尚待人工确认，保持 `NOT VERIFIED`。
- Release BIN 为 78788 字节，OTA payload CRC32 `0x8DE78C48`；UART OTA 完成 `STAGED ->
  INSTALL VERIFIED -> TRIAL COMMITTED -> TRIAL VERIFIED -> CONFIRMED`。普通复位后仍启动
  `0.6.0 build1` 并报告 `ota_confirmation=NOT_REQUIRED`、`lcd=READY`，真实断电重上电未执行。

## 2026-08-18 - LCD 任务诊断周期统一（0.5.2 build1）

### 变更内容

- 统一 `display_task` 实际 1 ms 周期与 `SystemRuntimeSnapshot.display_expected_period_ms`，
  修复遥测中实际周期为 1 ms、期望周期仍为 20 ms 的不一致。

### 设计决定

- 期望周期必须复用任务调度常量，避免任务调度修改后快照、UART 和 LCD 显示出现不同结论。
- 不改变 LCD 页面、Logo、PB8 按键或控制安全逻辑；版本提升为 `0.5.2`。

### 验证结果

- Debug/Release 构建通过：Debug `text=87892 data=120 bss=52944`，Release
  `text=78052 data=120 bss=52936`；`git diff --check` 通过。
- `0.5.2` 已通过 ST-Link 写入并由 GDB 直接启动；UART 确认四任务 `RUNNING`、显示实际/期望
  周期均为 1 ms、`lcd=READY`、控制停止且 PWM 为零。四页内容、Logo 和 PB8 循环仍保持
  `NOT VERIFIED`，普通上电仍使用 QSPI confirmed `0.3.0 build1`。

## 2026-08-18 - LCD DMA 调度修复（0.5.1 build1）

### 变更内容

- 将 `display_task` 的调度周期从 20 ms 调整为 1 ms，及时推进 LCD 逐行 SPI DMA；LCD 页面
  内容刷新仍保持 1 s。
- 保留四页、Logo、PB8 循环和统一快照设计，不改变控制任务、电机安全或 OTA 逻辑。
- `0.5.0` 调试启动观察到 LCD 长期 `DRAWING` 后，补丁版本提升为 `0.5.1 build1`。

### 设计决定

- LCD BSP 继续使用逐行 DMA，避免分配整屏缓冲；显示任务只增加调度频率，不在控制任务中
  加入 LCD 操作。
- 直接写 Application 后正常复位会触发 Bootloader confirmed repair，这是保护行为；测试不
  修改冻结的 QSPI confirmed 基线。

### 验证结果

- Debug/Release 构建通过：Debug `text=87896 data=120 bss=52944`，Release
  `text=78056 data=120 bss=52936`。
- `0.5.1` 已通过 GDB 直接启动，UART 观察到 `lcd=READY`、四任务 `RUNNING`、控制停止且
  PWM 为零；同时发现快照中显示期望周期仍为 20 ms，已在 `0.5.2` 修复。四页内容、Logo
  和 PB8 循环仍保持 `NOT VERIFIED`。

## 2026-08-18 - LCD 四页状态显示（0.5.0 build1）

### 变更内容

- 新增 `OVERVIEW`、`MOTOR`、`SENSORS`、`SYSTEM` 四个状态页，显示电压、控制、通信、故障、
  电机、编码器、PID、IMU、SR501、RTC、QSPI、任务健康、栈余量、运行时间和复位原因。
- 四页数据统一由 `status_display` 读取一次 `SystemStatusSnapshot` 后转换，LCD BSP 不直接读取
  业务模块；IMU 姿态在转换层换算为整数角度，不依赖目标板浮点格式化。
- PB8/BOOT0 改为单键循环四页，`SYSTEM` 后返回 `OVERVIEW`；切页不再触发诊断自检。
- 48x47 taifei Logo 固定保留在所有页面右上角，并补充 `/` 字形以正确显示页码。

### 设计决定

- PD3/PD4 未接线，继续保留为通用按钮，不参与 LCD 页面操作。
- 电量只显示已校验电压；电池类型、串数和放电曲线确定前不换算百分比。
- 本次可见功能变化进入 `0.5.0`，版本切换后 build 从 1 开始。

### 验证结果

- Debug/Release 构建通过：Debug `text=87896 data=120 bss=52944`，Release
  `text=78056 data=120 bss=52936`；构建无警告，`git diff --check` 通过。
- 四页实际内容、颜色、Logo、刷新和 PB8 循环尚未目标板验证，保持 `NOT VERIFIED`。

## 2026-08-18 - LCD 小 Logo 与单页结构（0.4.0 build1）

### 变更内容

- 保留原始 `picture_tafei.h`，新增从原图缩小生成的 48x47 RGB565 `picture_tafei_logo.h`。
- 移除独立全屏封面页，将小 Logo 固定绘制在状态页右上角，状态页上电直接显示。
- LCD 操作继续使用已接线的 PB8/BOOT0 按键；PD3/PD4 当前未接线，暂不参与页面操作。

### 设计决定

- Logo 是所有功能页可复用的品牌元素，不再占用一个页面；原始全屏资源继续保留，便于后续调整。
- 四页 UI 后续统一由 PB8 单键循环，PD3/PD4 只保留为通用按钮，不建立未接线输入依赖。
- 本次可见功能变化进入 `0.4.0`，版本切换后 build 从 1 开始。

### 验证结果

- Debug/Release 构建通过：Debug `text=85712 data=120 bss=52864`，Release
  `text=76244 data=120 bss=52856`；`git diff --check` 通过。
- 新 Logo、状态页首屏和 PB8 后续循环切页尚未目标板验证，保持 `NOT VERIFIED`。

## 2026-08-18 - 正式 UART 消息与版本语义收口（0.3.0 build1）

### 变更内容

- 正式化三类文本消息：命令响应 `[RSP]`、异步日志 `[LOG]`、周期遥测 `[TEL]`；统一 `v=1`、`ts_ms`、`seq`、字段命名和 CRLF 换行。
- 启动日志采用统一标签格式；遥测按 `system`、`motor`、`sensors`、`communication` 分区，同一轮使用同一序号。
- 修复目标板 nano printf 不支持 `%lld` 导致的 64 位编码器字段错位，增加独立 64 位格式化函数和单元测试。
- 将固件版本从混合字符串拆为 `CHASSIS_FIRMWARE_VERSION="0.3.0"` 与 `CHASSIS_FIRMWARE_BUILD=1`；LCD 和 UART 同时显示版本与 build。
- OTA 工具兼容旧版 `OTA_UART: READY, binary mode` 响应，二进制 OTA 继续与文本协议隔离。

### 设计决定

- 功能、协议或兼容行为变化才递增语义版本；同一版本、源码不变的重复可部署产物才递增 build，版本切换后 build 从 1 开始。
- 结构化输出优先保证字段可解析和稳定，VOFA 数字流只作为显式兼容模式保留。
- 版本信息是产品身份的一部分，必须同时出现在启动日志、遥测和 LCD 上。

### 验证结果

- Debug/Release 构建通过：Debug `text=199892 data=120 bss=52864`，Release `text=190444 data=120 bss=52856`。
- OTA Python 测试 13 项、UART 64 位格式化测试和 CommandManager 回归通过，`git diff --check` 通过。
- `0.3.0 build1` 已完成目标板 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`；目标板报告 `fw=0.3.0 build=1` 和 `ota_confirmation=CONFIRMED`。
- Release BIN 为 190572 字节，payload CRC32 为 `0x5866D20E`；OTA 包为 190636 字节。产物哈希和完整命令见 [`verification.md`](verification.md)。

## 2026-08-17 - FreeRTOS 四任务与统一状态快照（b17 -> 0.3.0）

### 变更内容

- Application 明确划分 `control_task`、`service_task`、`diagnostics_task`、`display_task` 四个任务。
- 建立双缓冲 `SystemStatusSnapshot`，统一记录任务周期、超时、运行状态、运行次数、栈余量、heartbeat、uptime、复位原因以及板级/传感器/通信/供电/RTC 状态。
- UART `status` 和 LCD 状态页读取同一份快照，避免各自拼接状态。

### 设计决定

- 按职责和实时性划分任务，不按硬件数量拆任务；控制任务不执行 Console、LCD、QSPI 擦写、RTC 显示或文本遥测。
- RTC 作为系统时间服务供日志和显示使用；传感器融合仍使用单调采样时间戳。
- 每个任务保留周期、超时、栈余量和运行状态诊断，异常时由安全策略保持停车。

### 验证结果

- 目标板 `0.3.0 build1` 已确认四任务均为 `RUNNING`，周期、栈余量、heartbeat age、运行次数和 RCC 复位原因可读；控制保持 `STOPPED`，未执行电机命令。

## 2026-08-17 - 诊断容量与 UART 长报文修复（b15/b16 -> 0.2.1/0.2.2）

### 变更内容

- 逐步扩大诊断报告缓冲，并将 UART 单条消息上限统一到 2048 字节。
- 增加编译期容量约束，避免加入 SR501 后完整 `status` 报告被截断或发送失败。

### 设计决定

- 诊断消息容量以最长合法报告为准，不通过静默截断隐藏字段。
- 结构化字段顺序保持稳定，协议变化必须同步更新文档和测试。

### 验证结果

- b16 目标板实测完整 `status` 恢复输出；Debug/Release 构建和差异检查通过。

## 2026-08-17 - SR501 接入（b14 -> 0.2.0）

### 变更内容

- 按 `.ioc` 将 HC-SR501 OUT 接入 PD5，配置为普通 GPIO 输入和内部下拉。
- 新增 `bsp/sr501` 轮询驱动：60 秒预热、50 ms 稳定滤波、READY 后仅统计稳定低到高事件，持续高电平不重复计数。
- 将 SR501 状态加入诊断输出；不绑定电机、安全逻辑、LCD 或具体业务。

### 设计决定

- 使用 Application task 轮询，不使用 EXTI；未连接模块时 PD5 下拉，不能阻塞 Application 启动。
- 预热期间不统计事件，所有时间字段使用单调毫秒 tick。

### 验证结果

- 代码、CMake 源文件和 CubeMX GPIO 初始化已构建通过。
- 目标板已观察到预热倒计时、READY 和持续低电平零误计数；模块指示灯、OUT 高电平、滤波和事件计数实测仍为 `DEFERRED`。

## 2026-08-15 - 无 IMU 也可启动与调试工作流（b13 -> 0.1.1）

### 变更内容

- 将 ICM45686 启动依赖后置；无 IMU 时 Application 仍能启动并报告明确的传感器状态。
- 增加 VS Code Cortex-Debug/OpenOCD/GDB 调试配置和 FreeRTOS 内核符号支持。

### 设计决定

- 缺少可选传感器不阻塞底盘基础启动；传感器状态必须显示为 `NOT_FOUND`、`STARTING` 或 `DEGRADED`，不能伪造硬件通过。
- 调试配置固定到本仓库工作区，CubeMX 生成区只保留必要的 USER CODE 修改。

### 验证结果

- b13 在无 ICM45686 的目标板上正常启动；OpenOCD/GDB 烧写、断点、调用栈和 FreeRTOS 符号已验证。

## 2026-08-14 - ICM45686、SPI3 DMA 与预留按键

### 变更内容

- 增加 ICM45686 寄存器/FIFO 组件、六轴融合组件和 STM32 SPI3/DMA BSP 适配。
- 增加 WHO_AM_I、FIFO watermark、批量 DMA、FIFO 异常恢复、时间戳和静止零偏接口。
- 增加两个预留按键的 GPIO/BSP 消抖事件接口。

### 设计决定

- 组件层保持 HAL 无关，BSP 层负责 STM32 外设适配。
- 保留现有 Mahony 作为对照；Kalman 只有在采样连续性、零偏和安装方向证据成立后再接入。
- RTC 只用于日历和日志，多传感器融合使用单调采样时间轴，不用 RTC 直接校时。

### 验证结果

- ICM45686/FIFO、MREG 和 fusion 纯组件测试通过，ARM Debug/Release 构建通过。
- SPI3 接线、WHO_AM_I、FIFO 连续性、零偏收敛和安装方向尚未完成目标板验证，保持 `DEFERRED`。

## 2026-07-25 至 2026-08-13 - 底盘架构与 OTA V1 基线（b6/b12）

### 变更内容

- 建立 Application、Bootloader、BSP、业务模块和 FreeRTOS 的目录边界。
- 完成实时控制任务、控制源所有权、CAN 心跳超时、故障停车和条件喂狗基础。
- 建立 32 KiB Bootloader + 480 KiB Application 的内部 Flash 布局，以及 QSPI 双 metadata、Slot A/B 和 confirmed/candidate 状态。
- 完成 UART/CAN FD OTA 的暂存、校验、安装、TRIAL、CONFIRMED 和回滚代码主链。

### 设计决定

- 启动、超时、通信故障和所有失败路径保持四路电机 PWM 为零。
- OTA 二进制会话与文本串口输出隔离；Bootloader 只在合法镜像和元数据条件下跳转 Application。
- OTA V1 先冻结已完成主链，CAN FD OTA、断电恢复和回滚边角作为后置验收，不阻塞应用主线。

### 验证结果

- b6/b12/build22 作为 factory confirmed 基线保留；Bootloader 普通启动、UART OTA 主链和 QSPI provision 已有实物证据。
- 断电恢复、自动回滚、CAN FD OTA 和四路 PWM 电气零输出仍未完成实物验收。

## 版本序列

旧产物中的 `bN` 是历史开发产物编号，保留原验证记录；下面是源码功能变化到产品版本的映射。

| 历史产物 | 产品版本 | 主要变化 |
| --- | --- | --- |
| b6、b12 | `0.1.0` | 初始底盘与 OTA/factory confirmed 基线 |
| b13 | `0.1.1` | 无 ICM45686 也可启动，IMU 启动依赖后置 |
| b14 | `0.2.0` | SR501 PD5 BSP、预热和诊断接入 |
| b15 | `0.2.1` | 诊断报告缓冲扩大 |
| b16 | `0.2.2` | UART 与诊断容量统一为 2048 字节 |
| b17 | `0.3.0` | FreeRTOS 四任务、统一快照和正式 UART 协议 |
| 历史板上 | `0.8.0 build1` | UART OTA confirmed，普通复位回归通过 |
| 上一工作树 | `0.4.0 build1` | LCD 小 Logo 与页面结构调整，尚未上板 |
| 上一修复树 | `0.5.0 build1` | LCD 四页状态显示，调试启动观察到 DMA 调度过慢 |
| 上一修复树 | `0.5.1 build1` | LCD DMA 调度修复，调试启动已进入 `lcd=READY` |
| 上一工作树 | `0.5.2 build1` | LCD 任务诊断周期统一，调试启动并确认 LCD 正常显示 |
| 上一工作树 | `0.6.0 build1` | LCD UI 美化、透明 Logo 和电量估算，已 OTA confirmed |
| 上一工作树 | `0.7.0 build1` | LCD 信息层级、大号电量显示和双栏布局，蓝色横带待修正 |
| 上一工作树 | `0.7.1 build1` | LCD 中性深灰背景和短标题强调线，当前页面配色可接受 |
| 历史工作树 | `0.8.0 build1` | ICM45686 正式启用，目标板 WHO_AM_I 返回 0xFF |
| 历史工作树 | `0.9.0 build1` | PID 参数持久化和电机启动复核 |
| 历史工作树 | `0.9.1 build1` | 可调开环占空比和启动下限复核 |
| 历史工作树 | `0.10.0 build1` | 控制安全收尾和低速 PID 响应 |
| 当前板上 | `0.11.1 build1` | ICM45686/FIFO/静止 Kalman 上板，UART OTA confirmed |
| 当前板上 | `0.12.0 build1` | 统一采样时间戳与差速轮式里程计，UART OTA confirmed；动态几何验证待完成 |
| 当前板上 | `0.13.0 build1` | LCD UI 信息层级与布局优化，UART OTA confirmed；视觉验收待完成 |
| 当前板上 | `0.14.0 build1` | 暗色工业仪表 UI 已 UART OTA confirmed；视觉人工确认正常 |
| 当前工作树 | `0.15.0 build1` | LCD/状态/RTOS/轮控边界收敛；软件验证通过，尚未烧录 |

## 后置工作

以下项目没有在本批记录为完成或硬件通过：

- `0.13.0 build1` 的页眉指示、标签层级、配色、文本排版和四页切换人工目视确认仍保留为历史待办。
- `0.14.0 build1` 的普通复位回归。
- ICM45686 正负轴向动作、动态姿态、静止回归和长期漂移；模块固定安装前保持 `DEFERRED`。
- `0.12.0 build1` 的编码器/ADC/IMU 本地时间字段、轮式里程计方向和 LCD/UART 动态输出上板复核，
  以及落地直线距离、原地旋转角度和轮径/轮距校准。
- SR501 模块供电/指示灯、OUT 高电平、50 ms 滤波和上升沿事件计数。
- confirmed 镜像断电启动与四路 PWM 电气零输出、Application 安装中断恢复、TRIAL 自动回滚及 rollback 中断恢复。
- CAN FD OTA、底盘加减速限制/编码器异常/欠压保护实测，以及正式 CAN FD 底盘协议。
