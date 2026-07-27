# 开发路线

## 当前基线

阶段 4 已完成代码实现：

- FreeRTOS 静态 Application task 和高优先级 `control_task`
- TIM6 100 Hz 任务通知，过期控制周期不连续补跑
- 控制源所有权、CAN 200 ms 超时和危险测试互斥
- FDCAN error-passive、bus-off 处理与会话撤销
- 关键任务健康汇总和条件喂狗
- `chassis`、`safety`、`parameters`、`diagnostics` 业务域

Debug 和 Release 曾完成 clean build。是否需要重新回归由实际固件改动决定，
历史结果见 `verification.md`。

## 当前结构到最终结构

`architecture.md` 中的完整目录树是最终设计，不因当前尚未实现而删除或缩写。
当前代码按以下顺序迁移，只有对应逻辑落地时才创建目录和文件：

| 当前实际状态 | 最终位置或变化 | 时机 |
|---|---|---|
| `firmware/shared/ota/ota_image.h` | 拆为 `firmware/shared/` 下四个固定 ABI 头文件 | OTA ABI 冻结时 |
| `board/board_config.h` | 增加 `board_init.c`、`board_pins.h` | 板级句柄和引脚映射确实需要独立时 |
| `components/pid/` | 增加 limiter、filters、ring_buffer、crc | 相应通用组件被实际复用时 |
| `communication/can_transport/` | 增加 `chassis_protocol/`、`ota_transport/` | 正式底盘协议和 OTA 传输实现时 |
| `infrastructure/` | 迁移为 `services/` | OTA 前完成命名和工程路径迁移 |
| `modules/chassis/` 等业务域分组 | 按最终树拆为独立产品模块目录 | 模块接口稳定后逐项迁移 |
| `rtos/rtos_app.c/h` 两任务过渡模型 | 增加 RTOS 对象、六类职责任务和 hooks | 对应职责需要独立调度时 |
| 仅有 `tests/target/` | 增加 `tests/unit/` | 首个无 HAL 组件测试落地时 |
| 尚无 Bootloader 工程 | 建立独立 `firmware/bootloader/stm32g474/` | 当前 OTA 阶段 |
| 文档当前按唯一职责平铺 | 达到多个同类文档后再迁入分类目录 | 文档数量产生真实分类需求时 |

迁移不改变协议和底盘行为，不批量创建空目录，不建立只转发调用的空层。

## 当前阶段：Bootloader 与 OTA

状态：`IN PROGRESS`

已完成：

- 决定内部 32 KiB Bootloader + 480 KiB 单 Application
- 建立 64 字节 OTA 镜像头和基础元数据 ABI
- 建立主机固件打包工具
- 确定 UART 与 CAN FD 都是 OTA V1 正式传输

下一批按顺序执行：

1. 将 QSPI OTA 区改为已确认/候选双固件槽。
2. 将 `infrastructure/` 迁移为最终结构中的 `services/`，同步工程路径。
3. 扩展元数据状态：候选、安装中、试运行、确认、回滚。
4. 建立独立 `firmware/bootloader/stm32g474/` CubeMX/CubeIDE 工程。
5. 实现 QSPI 校验、内部 Flash 安装、断电重试和严格 Application 跳转。
6. 将 Application 链接地址和 VTOR 一起迁移到 `0x08008000`。
7. 实现 Application OTA 会话、停车准入和 QSPI 分块写入。
8. 实现 UART DMA circular + IDLE + 环形缓冲区传输。
9. 实现 CAN FD OTA 传输；两种入口复用同一 OTA 状态机。
10. 分别完成 UART、CAN FD、断电恢复、未确认回滚和损坏镜像拒绝实物验证。

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
