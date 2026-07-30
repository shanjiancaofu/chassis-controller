# 开发路线

## 当前基线

阶段 4 已完成代码实现：

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
| `rtos/rtos_app.c/h` 两任务模型 | 仅按真实阻塞或周期需求新增任务 | OTA 接收需要独立非实时任务时 |
| `tests/target/` 和 `tests/unit/` | 按风险补目标板测试和无 HAL 主机测试 | CommandManager 首批主机测试已落地 |
| 独立 Bootloader 工程 | 继续与 Application 保持独立 CubeMX、链接脚本和构建配置 | 已完成首版 |

迁移不改变底盘行为，不批量创建空目录，不建立只转发调用的空层。

## 当前阶段：Bootloader 与 OTA

状态：`IN PROGRESS`

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
- 建立独立裸机 Bootloader CubeMX/CubeIDE 工程，只启用 QSPI、SWD 和时钟；IWDG 仅刷新继承实例
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
- UART 进入二进制模式后 30 秒内未收到 BEGIN 会自动退出并释放维护锁
- 失败或中止时等待 QSPI 内部擦写结束后才释放维护锁；最终响应提交并排空后才复位
- 已提供 UART 和 SocketCAN CAN FD stop-and-wait 主机发送工具
- 已生成并校验 Bootloader + relocated Application 首次组合烧录产物
- 已使用 DFU 烧录正式组合镜像，验证 Bootloader 普通启动可跳转到 relocated Application
- Bootloader 固定使用 16 MHz HSI；不主动启动或重配置 IWDG，只刷新 Application 继承的实例；
  Application 正常周期约 10 秒，OTA 复位前约 30 秒，Recovery 停止刷新
- 运动命令与维护 owner 已拆分；`pid stop` 不能释放 OTA 锁，Console 目标持续到明确停止，
  CAN 目标仍使用 200 ms heartbeat timeout
- OTA 擦除阶段使用独立总时限和内部进度时限，人工 QSPI 测试与试运行确认失败后均等待
  DMA 终止和 Flash WIP 清除
- CAN OTA 响应使用 Tx Event 确认和软件重试；候选安装失败时自动回滚 confirmed 槽
- TRIAL/CONFIRMED 启动执行完整 Application CRC 校验，Application/Bootloader 共用
  W25Q64 JEDEC `EF 40 17` 准入契约

下一批按顺序执行：

1. 使用 Jetson SocketCAN 完成同一镜像的 CAN FD 升级。
2. 验证错误头、错误 CRC、会话超时和 CAN 错误均保持停车且不会安装。
3. 验证 QSPI 暂存、内部安装期间断电恢复，以及未确认试运行回滚。

## 后续阶段

### 发布安全

- 固件数字签名
- 防回滚计数
- Jetson 发布流程和版本兼容检查
- 签名验收后再决定是否需要镜像加密
- Bootloader CAN FD Recovery

### 正式底盘协议

- 冻结物理速度单位、反馈帧、故障帧和心跳
- 参数持久化和正式遥测
- Jetson 与 STM32 版本兼容规则

### IMU 与里程计

IMU 暂缓。后续只在硬件确认后实现原始数据、时间戳和掉线检测；
融合和 ROS 2 里程计放在 Jetson。

## 暂不实施

- 位置环
- 复杂底盘运动学
- STM32 端复杂姿态融合
- 内部 Flash 双 Application Bank
- Bootloader FreeRTOS
- 无人值守远程发布
