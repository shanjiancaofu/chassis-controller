# 开发路线

## 当前基线

阶段 4 已完成代码实现：

- FreeRTOS 静态 Application task 和高优先级 `control_task`
- TIM6 100 Hz 任务通知，过期控制周期不连续补跑
- 控制源所有权、CAN 200 ms 超时和危险测试互斥
- FDCAN error-passive、bus-off 处理与会话撤销
- 关键任务健康汇总和条件喂狗
- `chassis`、`safety`、`parameters`、`diagnostics` 业务域

Debug 和 Release 曾完成 clean build。历史结果见 `verification.md`；OTA 相关代码
落地后必须重新构建和回归。

## 当前结构到最终结构

`architecture.md` 中的完整目录树是最终设计。只有对应逻辑落地时才创建目录和文件：

| 当前实际状态 | 最终位置或变化 | 时机 |
| --- | --- | --- |
| 四个 `firmware/shared/*.h` | Bootloader、Application 和主机工具共用固定 ABI | 已完成首版，后续兼容修改必须提升格式版本 |
| `components/pid/` | 按真实复用需求增加 crc 等组件 | Bootloader 校验代码落地时 |
| `communication/can_transport/` | 增加 `chassis_protocol/`、`ota_transport/` | 正式协议或 OTA 会话实现时 |
| `infrastructure/` | 保留现有命名，后续增加参数存储 | 参数持久化实现时 |
| `modules/chassis/` 等业务域分组 | 保留当前高内聚组织 | 不再反向平铺 |
| `rtos/rtos_app.c/h` 两任务模型 | 仅按真实阻塞或周期需求新增任务 | OTA 接收需要独立非实时任务时 |
| 仅有 `tests/target/` | 增加 `tests/unit/` | 首个无 HAL 组件测试落地时 |
| 尚无 Bootloader 工程 | 建立独立 `firmware/bootloader/stm32g474/` | 下一批 |

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
- 建立 Bootloader PC 单元测试入口；当前本机未找到 C 编译器，尚未形成
  `BUILD PASS`

下一批按顺序执行：

1. 建立独立 `firmware/bootloader/stm32g474/` CubeMX/CubeIDE 工程，
   接入最小启动代码、链接脚本、IWDG 和 QSPI 初始化。
2. 将已落地的 Bootloader 纯逻辑接到 QSPI 元数据读取和镜像读取。
3. 实现内部 Flash 安装、写后校验、断电重试和严格 Application 跳转。
4. 将 Application 链接地址和 VTOR 一起迁移到 `0x08008000`。
5. 实现 Application OTA 会话、停车准入、传输源互斥和 QSPI 分块写入。
6. 在现有 UART BSP 上接入 OTA 帧解析，不复制第二套 DMA 或环形缓冲。
7. 实现 CAN FD OTA 分块传输；UART 与 CAN FD 复用同一 OTA 状态机。
8. 完成 UART、CAN FD、断电恢复、未确认回滚和损坏镜像拒绝实物验证。

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
