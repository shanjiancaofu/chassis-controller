# 开发路线

本文保留阶段完成项和后续范围。新对话需要的短状态见
[`current_status.md`](current_status.md)，可复现构建和硬件证据见 [`verification.md`](verification.md)。

## 当前基线

既有 FreeRTOS/底盘基础代码已完成：

- FreeRTOS 静态 Application task 和高优先级 `control_task`
- TIM6 100 Hz 任务通知，过期控制周期不连续补跑
- 控制源所有权、CAN 200 ms 超时和危险测试互斥
- FDCAN error-passive、bus-off 处理与会话撤销
- 关键任务健康汇总和条件喂狗
- `chassis`、`safety`、`parameters`、`diagnostics` 业务域

Application 与 Bootloader 的 Debug 和 Release 已在 OTA 代码落地后重新 clean build。
构建尺寸和未完成的实物回归见 `verification.md`。

## 当前结构到最终结构

`architecture.md` 中的完整目录树是最终设计。只有对应逻辑落地时才创建目录和文件：

| 当前实际状态 | 最终位置或变化 | 时机 |
| --- | --- | --- |
| 五个 `firmware/shared/*.h` | Bootloader、Application 和主机工具共用固定 ABI/硬件契约 | 已完成首版，后续兼容修改必须提升格式版本 |
| Application `components/pid/`、`components/crc/` | Bootloader 保持独立 CRC 实现 | 已按独立工程边界拆分 |
| `communication/can_transport/`、`communication/ota_transport/` | 增加 `chassis_protocol/` | 正式底盘协议编解码落地时 |
| `infrastructure/` | 保留现有命名，后续增加参数存储 | 参数持久化实现时 |
| `modules/chassis/` 等业务域分组 | 保留当前高内聚组织 | 不再反向平铺 |
| `rtos/rtos_app.c/h` 四任务职责模型 | 继续按真实阻塞或周期需求演进，不按硬件数量增加任务 | 已完成首版迁移 |
| `tests/target/` 和 `tests/unit/` | 按风险补目标板测试和无 HAL 主机测试 | CommandManager 首批主机测试已落地 |
| 独立 Bootloader 工程 | 继续与 Application 保持独立 CubeMX、链接脚本和构建配置 | 已完成首版 |

迁移不改变底盘行为，不批量创建空目录，不建立只转发调用的空层。

## 当前阶段：Bootloader 与 OTA

状态：`FROZEN`（2026-08-17，build22 + b13 UART/安装/TRIAL/CONFIRMED 主链；剩余硬件回归后置）

已完成：

- 决定内部 32 KiB Bootloader + 480 KiB 单 Application
- 建立 64 字节镜像头和双副本 OTA 元数据 ABI
- 将板载 QSPI OTA 区划分为物理 Slot A 和 Slot B
- 由元数据分配 confirmed/candidate 角色，不把角色写死到物理地址
- 建立主机固件打包工具
- 确定 UART 与 CAN FD 都是 OTA V1 正式传输
- UART 已具备 circular DMA、IDLE 接收、软件环形缓冲和 TX DMA 队列
- 建立 Bootloader 纯逻辑首批代码：CRC32、镜像头/载荷校验、向量表校验、
  OTA 元数据双副本校验和 sequence 选择
- 建立独立裸机 Bootloader CubeMX/CubeIDE 工程，运行时使用 QSPI、SWD、直接寄存器
  USART1 trace 和时钟；`.ioc` 仍保留 IWDG 外设，但 `MX_IWDG_Init()` 在 USER CODE 中
  直接返回，Bootloader 只刷新继承实例
- Bootloader 链接区限制为 `0x08000000` 起始的 32 KiB
- 接入 W25Q64 JEDEC 检查、元数据双副本读取和交替提交
- 实现候选镜像流式 CRC 校验、内部 Flash 双 Bank 擦写、写后校验和严格跳转
- 实现 `STAGED -> INSTALLING -> TRIAL`、试运行计数和 confirmed 镜像回滚路径
- Debug 与 Release 均完成 clean build，结果见 `verification.md`
- Application 链接区迁移到 `0x08008000`、大小限制为 480 KiB，Debug/Release 的
  VTOR 均经 ELF 反汇编确认写入 `0x08008000`
- Application 增加非实时试运行确认状态机：连续 5 秒关键任务健康后读取双副本元数据，
  仅将合法 `TRIAL` 交替提交为 `CONFIRMED`，并与人工 QSPI 擦写测试互斥
- 当前 Release BIN 已通过 OTA 打包器的向量表、地址、长度和 CRC32 校验
- Application 已实现统一 OTA 会话、停车维护锁、UART 二进制帧解析、CAN FD 64 字节
  分块传输、QSPI DMA 写入、整包/镜像校验和 `STAGED` 元数据提交
- UART 进入二进制模式后仅在等待 BEGIN 的阶段执行 30 秒超时；会话开始或 tracked 响应
  尚未完成时不会禁用 transport 或混发文本
- 失败或中止时等待 QSPI 内部擦写结束后才释放维护锁；清理超过 10 秒则锁存 critical fault、
  保持停车并由 IWDG 复位，不会无限等待或假定 Flash 空闲
- UART OTA 响应使用 token 等待对应 DMA 完成，失败时重试同一帧；最终响应确认成功后才复位
- 已提供 UART 和 SocketCAN CAN FD stop-and-wait 主机发送工具
- 已生成并校验 Bootloader + relocated Application 首次组合烧录产物
- 已使用 DFU 烧录正式组合镜像，验证 Bootloader 普通启动可跳转到 relocated Application
- 已使用 UART 完成 build6 历史镜像以及 2026-08-17 b12 -> b13 的真实传输、QSPI 暂存、
  候选安装、TRIAL 启动和 CONFIRMED 提交；build22 UART OTA 主链达到 `HARDWARE PASS`
- Bootloader 固定使用 16 MHz HSI；不主动启动或重配置 IWDG，只刷新 Application 继承的实例；
  Application 正常周期约 10 秒，OTA 复位前约 30 秒，Recovery 停止刷新
- 运动命令与维护 owner 已拆分；`pid stop` 不能释放 OTA 锁，Console 目标持续到明确停止，
  CAN 目标仍使用 200 ms heartbeat timeout
- OTA 擦除阶段使用独立总时限和内部进度时限，人工 QSPI 测试与试运行确认失败后均等待
  DMA 终止和 Flash WIP 清除
- CAN OTA 响应使用 Tx Event 确认和软件重试；候选安装失败时自动回滚 confirmed 槽
- `FAILED` 禁止向量表 fallback；仅双 metadata 擦除态允许 factory fallback，QSPI/metadata
  故障进入 Recovery；`EMPTY/RECEIVING` 无 confirmed 时也进入 Recovery
- candidate、rollback 和 confirmed repair 的内部安装均使用持久化三次上限
- 三次上限耗尽时先验证内部镜像；若最后一次安装已成功但 metadata 提交前掉电，则补交
  `TRIAL/CONFIRMED`，避免把完整镜像误判为 FAILED；confirmed 成功后同步回填 image size/CRC
- factory 工具可从同一 `.ota` 生成 Slot A + `CONFIRMED` Metadata A 的 8 MiB QSPI raw
  provisioning 镜像；默认缺少 QSPI 参数会失败，仅诊断可显式 `--internal-only`
- 试运行确认最多重试 3 次，持续失败时锁存 critical fault 并由 IWDG 复位；内部 Flash
  改为逐页擦除并在页间刷新继承的 IWDG
- TRIAL/CONFIRMED 启动执行完整 Application CRC 校验，Application/Bootloader 共用
  W25Q64 JEDEC `EF 40 17` 准入契约
- `test_ota_transfer.py`、`test_factory_image.py`、`test_shared_abi.py`、Application OTA metadata
  和 Bootloader core 宿主机测试覆盖安装上限、每次中断后的完整 QSPI/internal salvage、
  rollback attempts=0 健康镜像恢复、状态槽约束、factory fail-closed、QSPI 布局和字段
  offset 级共享 C/Python ABI；共享 C 头同时使用 ARM GCC `offsetof` 静态断言，本轮 Python
  9 项于 2026-08-13 通过
- confirmed 恢复已区分 source invalid、I/O error 和 internal mismatch；只有 source 完整且
  internal mismatch 才允许擦写，I/O 先有限非破坏性重试，confirmed global fatal 直接 Recovery
- Bootloader 已嵌入 `0.1.0` 功能版本和 build22 构建号，启动串口输出两者；build22
  Release 已 clean build，尚未烧板回归
- 已清除 Git 跟踪的 `tools/ota/__pycache__/*.pyc`，并增加通用 Python 缓存忽略规则
- Bootloader `.ioc` 有意保留 IWDG；生成的 `MX_IWDG_Init()` 继续在 USER CODE 中提前返回，
  从而保留 CubeMX 工程结构但不在 Bootloader 主动启动或重配置 IWDG

OTA V1 代码基线已冻结，不再继续扩展 recovery 边角。冻结依据是 build22 + b13 已完成
UART 传输、QSPI 暂存、安装、TRIAL 和 CONFIRMED 实物闭环。以下项目保留为后置回归，
不阻塞当前功能主线，也不得在未实测时标记为通过：

- confirmed 0.3.0 build1 断电启动和四路 PWM 电气零输出
- Application 安装过程中断电恢复
- TRIAL 不确认自动回滚
- rollback 安装过程中断电恢复
- Jetson SocketCAN CAN FD OTA（启用 Jetson OTA 前必须完成）

## 分阶段实施路线

用户侧的 FreeRTOS 调度、IMU/Kalman、SR501、LCD 硬件状态/电量/版本和正式串口消息需求
拆成以下阶段逐项落地。每个阶段先完成代码和构建证据，再进入下一阶段；未完成的硬件项
保持 `READY`、`NOT VERIFIED` 或 `DEFERRED`，不因代码存在而标记硬件通过。

### 阶段 1：FreeRTOS 运行基础

状态：`HARDWARE PASS`（0.3.0 build1 已确认四任务运行字段和统一快照可读）

- 已完成 `control_task`、`service_task`、`diagnostics_task`、`display_task` 四任务迁移；
  `control_task` 保持 100 Hz 控制，`service_task` 承担 UART/CAN/OTA，`diagnostics_task` 承担
  RTC/ADC/IMU/SR501 和快照发布，`display_task` 承担按键/LCD。
- 四个任务均记录周期、超时、运行状态、运行次数、栈余量、heartbeat age 和 uptime；关键任务
  健康状态继续作为 IWDG 刷新条件，显示任务单独诊断但不阻塞控制安全路径。
- UART 和 LCD 当前已读取统一快照中的公共状态，后续不得恢复各自独立拼接。

### 阶段 2：正式 UART 消息

状态：`HARDWARE PASS`（0.3.0 build1 已完成协议、四分区状态和 UART OTA 实物闭环）

正式格式和字段注册表见 [`protocol/uart_message_protocol.md`](../protocol/uart_message_protocol.md)。

- 已将命令响应统一为 `[RSP] v=1 ts_ms=... result=OK|ERROR`，并给出稳定的命令、错误码和字段名。
- 已将异步日志统一为 `[LOG] v=1 ts_ms=... level=... module=... event=...`。
- 已将遥测统一为版本化 `[TEL] v=1 ts_ms=... seq=... section=... field=value...`，方括号标签属于
  固定线协议。
- 所有正式文本消息使用 ASCII、单行、CRLF 换行；版本、时间戳、错误码和字段名称固定，不再输出
  无版本的 demo 文本。
- 启动阶段按 `boot -> board -> 外设 -> application` 顺序输出结构化 `[LOG]` 行，风格类似 Linux
  启动日志；初始化失败使用 `level=WARN|ERROR` 和稳定 `code=`，不使用不可解析的长句。
- `status` 已拆为 `SYSTEM`、`MOTOR`、`SENSORS`、`COMMUNICATION` 分区，允许像启动日志一样
  逐行观察，但每行字段仍保持稳定。
- OTA 二进制会话与文本输出保持隔离，不能混发。
- VOFA 数字流保留为显式兼容模式，不属于正式 `[TEL]` 字段协议。
- 0.3.0 build1 已实测启动 `[LOG]`、命令 `[RSP]`、同序号四分区 `[TEL]`、错误响应、文本遥测和
  CRLF 换行；UART OTA 完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`，普通复位后回到
  `NOT_REQUIRED`。目标板 nano printf 的 64 位整数兼容问题已修复并回归。
- 产品版本与 build 号分开维护：功能、协议或兼容行为变化更新语义版本；build 号只标识具体
  产物，不因普通本地重编译递增。

### Application 版本序列

旧产物文件名中的 `bN` 是当时的开发产物编号，历史文件和原始验证记录保持不变。下面记录
源码功能变化对应的语义版本映射；版本切换后 build 从 1 开始，同一版本的重复产物才递增。

| 历史产物 | 源码阶段 | 语义版本映射 | 主要变化 |
| --- | --- | --- | --- |
| b6、b12 | 初始底盘/OTA 基线 | `0.1.0` | 初始 Application 和 factory confirmed 基线 |
| b13 | 无 ICM45686 也可启动 | `0.1.1` | 延后 IMU 启动依赖，保持底盘可启动 |
| b14 | SR501 接入 | `0.2.0` | PD5 BSP、60 秒预热、诊断字段接入 |
| b15 | 诊断缓冲扩大 | `0.2.1` | 增大报告缓冲，定位长 status 输出问题 |
| b16 | UART 消息容量修复 | `0.2.2` | UART 上限与诊断缓冲统一为 2048 字节 |
| b17 | FreeRTOS 与正式 UART | `0.3.0` | 四任务快照、`[RSP]`/`[LOG]`/`[TEL]` 和 64 位格式化修复 |
| 当前 | 同一 `0.3.0` 首个正式产物 | `0.3.0 build1` | 当前代码和目标板最终版本 |

### 阶段 3：LCD 硬件总览

状态：`PLANNED`（当前直接下一阶段）

- 多页显示硬件子系统状态：电机/编码器、CAN、QSPI、RTC、IMU、SR501、FreeRTOS 和故障。
- 按键循环四页：`OVERVIEW`（电压、控制、CAN、Fault、版本）、`MOTOR`（目标、速度、PWM、
  编码器、PID）、`SENSORS`（IMU、SR501、ADC、RTC）、`SYSTEM`（QSPI、任务健康、运行时间、
  复位原因）。
- 显示固件版本和构建信息。
- 电量先显示经校验的电压；在电池化学体系、串数和电压曲线确定前，不显示伪造的百分比。

### 阶段 4：ICM45686 与估计器

状态：`DEFERRED`

- 先完成 WHO_AM_I、FIFO/DMA、采样连续性、静止零偏和安装方向的实物确认。
- 在 Kalman 或轮式里程计融合前，建立统一单调时间戳：保留 IMU FIFO 时间戳，给编码器和 ADC
  记录采样/触发时刻，给 SR501 记录稳定事件时刻；RTC 只用于日历显示和日志，不参与融合校时。
- 验证传感器时间戳到控制时间轴的映射和延迟，100 Hz 控制下目标对齐误差约 1 ms；未完成前不
  根据任务处理时刻拼接多传感器数据。
- 保留现有 Mahony 作为对照，不直接删除；在数据证据成立后实现角度+陀螺零偏两状态 Kalman。
- yaw 没有磁力计时只能约束短期变化，不能消除长期漂移；底盘航向后续采用轮式里程计和陀螺
  Z 轴融合。

### 阶段 5：SR501 状态展示与硬件收尾

状态：`DEFERRED`

- SR501 已接入统一 `SystemStatusSnapshot` 和正式 UART `sensors` 字段；剩余 LCD 展示随阶段 3
  实现，不绑定电机或安全逻辑。
- 继续保留 60 秒预热、50 ms 滤波和单次上升沿语义。
- 完成模块供电、指示灯、OUT 高电平和事件计数实物排查后，才更新硬件验证结论。

### 阶段 6：底盘控制收尾

状态：`PLANNED`

- 在 100 Hz 双轮控制中实现目标加减速限制，确保停止、急停、故障和超时立即清零。
- 之后进行左右轮标定、里程计累计、堵转/编码器异常/电压保护和 PID 实物调参。

## 后续阶段

### 底盘功能（阶段 6 之后）

阶段 5/6 进入底盘功能收尾后，按以下详细项推进：

1. SR501 接线、GPIO/BSP 和诊断已 `IMPLEMENTED`；b16 已完成 UART OTA/确认、60 秒预热和
   低电平零误计数。模块指示灯和 OUT 均未观察到高电平，剩余实物验证 `DEFERRED`。
2. PID、100 Hz 双轮控制、串口 RAM 调参和遥测入口已 `IMPLEMENTED`；方向、低速闭环、
   停车和稳定性实物验收 `DEFERRED`。
3. 目标加减速限制后置；左右轮实物标定后置。
4. 里程计累计、速度和转角验证。
5. 堵转、编码器异常和电压保护。
6. Fault、Health 和 Reset 诊断闭环。
7. 在已验证行为基础上完善并冻结正式 CAN FD 底盘协议。

### 发布安全

以下内容属于 OTA V2，不进入 OTA V1 冻结范围：

- 固件数字签名
- 防回滚计数
- Jetson 发布流程和版本兼容检查
- 签名验收后再决定是否需要镜像加密
- Bootloader CAN FD Recovery

### IMU 与里程计

ICM45686 目前 `DEFERRED`，不再阻塞 OTA V1 主线。现有 STM32 端 ICM45686 SPI3/DMA、
16 字节 FIFO、timestamp、掉线恢复、RAM 零偏标定和六轴 Mahony 代码保持现状，阶段 4 前不扩展。
阶段 4 先单独验证 `WHO_AM_I`、连续采样、零偏收敛和安装轴向，再实现已定义范围内的
角度/陀螺零偏两状态 Kalman；20-bit、压缩 FIFO 和自检仍不属于当前上板门槛。

STM32 轮式里程计仍属于当前底盘阶段。Jetson 后续消费轮式里程计和 IMU 输出并负责 ROS 2
定位融合，不在 STM32 重复实现导航级融合。

## 暂不实施

- 位置环
- 复杂底盘运动学
- 超出当前六轴Mahony的STM32端复杂姿态/导航融合
- 内部 Flash 双 Application Bank
- Bootloader FreeRTOS
- 无人值守远程发布
