# 变更记录

本文记录 `chassis-controller` 每批实现改动。每批包含变更内容、设计决定和验证结果；详细构建数据与硬件证据仍以 [`verification.md`](verification.md) 为准，当前交接状态以 [`current_status.md`](current_status.md) 为准。

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
| 当前板上 | `0.8.0 build1` | UART OTA confirmed，普通复位回归通过 |
| 上一工作树 | `0.4.0 build1` | LCD 小 Logo 与页面结构调整，尚未上板 |
| 上一修复树 | `0.5.0 build1` | LCD 四页状态显示，调试启动观察到 DMA 调度过慢 |
| 上一修复树 | `0.5.1 build1` | LCD DMA 调度修复，调试启动已进入 `lcd=READY` |
| 上一工作树 | `0.5.2 build1` | LCD 任务诊断周期统一，调试启动并确认 LCD 正常显示 |
| 上一工作树 | `0.6.0 build1` | LCD UI 美化、透明 Logo 和电量估算，已 OTA confirmed |
| 上一工作树 | `0.7.0 build1` | LCD 信息层级、大号电量显示和双栏布局，蓝色横带待修正 |
| 上一工作树 | `0.7.1 build1` | LCD 中性深灰背景和短标题强调线，当前页面配色可接受 |
| 当前工作树 | `0.8.0 build1` | ICM45686 正式启用，目标板 WHO_AM_I 返回 0xFF |

## 后置工作

以下项目没有在本批记录为完成或硬件通过：

- LCD `OVERVIEW`、`MOTOR`、`SENSORS`、`SYSTEM` 四页内容和 PB8 按键循环的目标板验证。
- ICM45686 目标板 WHO_AM_I、FIFO/DMA 连续性、零偏、安装方向和 Kalman/时间轴融合。
- SR501 模块供电/指示灯、OUT 高电平、50 ms 滤波和上升沿事件计数。
- confirmed 镜像断电启动与四路 PWM 电气零输出、Application 安装中断恢复、TRIAL 自动回滚及 rollback 中断恢复。
- CAN FD OTA，以及后续底盘加减速限制、里程计/安全保护和正式 CAN FD 协议。
