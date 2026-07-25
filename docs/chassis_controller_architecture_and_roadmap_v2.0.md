# chassis-controller 架构与演进路线 v2.0

> 文档状态：当前架构基线
> 更新时间：2026-07-25
> 适用仓库：`chassis-controller`
> 适用 MCU：STM32G474VET6

## 0. 文档用途

本文档基于当前仓库源码、CubeMX/CubeIDE 工程配置、裸机实物验收结果和 CAN FD 联调结果整理，用于固定：

- 当前真实实现状态
- 最终目录结构
- 各层职责和依赖方向
- FreeRTOS 运行模型
- 控制、通信、安全和时间边界
- Bootloader 与 OTA 的演进方向
- 后续重构顺序和验收条件

后续修改仓库前，应先读取：

1. 本文档
2. `README.md`
3. `docs/裸机阶段进度.md`
4. `docs/硬件与接线.md`
5. `protocol/canfd_protocol.md`
6. 当前源码、`.ioc` 和 `.cproject`

本文档已合并旧版架构路线图的有效内容，是架构、迁移规则和后续演进的唯一依据。文档与实现冲突时，以当前实际代码、`.ioc`、`.cproject` 和已完成的硬件验证为准，并同步修正文档。

---

## 1. 项目定位

`chassis-controller` 是 STM32G474 侧的安全差速底盘控制器。

STM32 负责：

- 双电机 PWM 和方向控制
- 双编码器采集
- 轮速计算和 PID 闭环
- 差速运动学
- CAN FD 命令接收和状态反馈
- 命令超时停车
- 急停、故障和看门狗安全链路
- 电压、RTC、QSPI、LCD、按键等板级资源
- 后续 IMU 原始数据采集
- 后续 Application OTA 接收和升级状态管理

Jetson Orin Nano 负责：

- ROS 2 Bridge
- `/cmd_vel` 接入
- 感知、规划和上层决策
- 里程计融合
- IMU 坐标变换和高层滤波
- 参数配置、诊断展示和 OTA 文件发送

底层闭环和安全控制必须保留在 STM32。Jetson 不直接计算并下发电机 PWM。

---

## 2. 仓库职责范围

本仓库维护：

- STM32G474 Application 固件
- STM32G474 Bootloader
- Application 与 Bootloader 共用的固件契约
- CAN FD 协议文档
- OTA 协议文档
- 固件架构、安全、Bring-up 和验证文档
- 固件构建、格式检查和单元测试配置

本仓库不维护：

- Jetson ROS 2 Bridge 实现
- Jetson 上位机程序
- 独立 PC CAN 监控软件
- 独立 PID GUI
- 独立固件升级 GUI
- 其他与 STM32 固件无关的主机应用

当前不新增 `host/` 或独立 PC 工具工程。STM32 侧只提供这些工具所需的协议和调试入口。

---

## 3. 信息源优先级

同一信息在多个文件中重复时，按以下优先级判断：

1. 硬件引脚、时钟和外设配置：`chassis_controller.ioc` 与 `docs/硬件与接线.md`
2. 当前已冻结 CAN 帧：`protocol/canfd_protocol.md`
3. 已实现、READY、PASS 状态：`docs/裸机阶段进度.md`
4. 软件分层、目录职责和迁移顺序：本文档
5. README：项目入口和简要使用说明

未经过真实硬件验证的功能只能标记为“已实现”或 `READY`，不能标记为 `PASS`。

---

## 4. 当前真实基线

### 4.1 已验收裸机基线

裸机基线版本为 `v0.1.0-baremetal`，已完成以下实物验收：

- USART DMA 收发
- RTC 读取和掉电保持
- QSPI JEDEC ID、8 MiB 容量、保留扇区擦写和 1 KiB DMA 校验
- ST7789 LCD 基础显示、SPI DMA、封面和动态状态页
- FDCAN2 外部 CAN FD+BRS 双向通信
- ADC 电源电压采样
- 双编码器方向和计数
- 双电机正反转
- 10 ms 双轮低速 PID
- 上电默认零输出
- PD2 急停锁存
- CAN 控制命令约 200 ms 超时停车
- IWDG 受控复位测试
- KEY 消抖和页面切换

当前实物结论只覆盖架空低速闭环和板级外设，不代表车辆落地后的运动学、路径控制和高动态性能。

详细测试结果以 `docs/裸机阶段进度.md` 为准，不在本文档重复维护具体测量数据和每次 PID 整定记录。

### 4.2 当前 FreeRTOS 状态

FreeRTOS 不是“只生成但未使用”，当前已经进入实际运行路径：

```text
main()
  ↓
osKernelInitialize()
  ↓
MX_FREERTOS_Init()
  ↓
osKernelStart()
  ↓
defaultTask
  ↓
RtosApp_Run()
  ↓ 每约 1 ms
ChassisApp_Run()
```

当前特征：

- CubeMX 生成 FreeRTOS 和 CMSIS-RTOS2 适配层
- `defaultTask` 使用静态分配
- 当前任务栈为 1024 Words
- `defaultTask` 进入 `RtosApp_Run()`，任务内部使用原生 FreeRTOS 延时接口周期调用 `ChassisApp_Run()`
- SysTick 用于 FreeRTOS 内核节拍
- TIM7 用于 HAL 1 ms 时基
- TIM6 继续提供 10 ms 控制节拍
- TIM6 ISR 只增加有上限的待处理控制周期计数
- PID 和电机控制仍在应用任务上下文执行
- 尚未拆分独立控制、通信、诊断和控制台任务

目录迁移后的 FreeRTOS 工程已于 2026-07-25 从
`firmware/application/stm32g474/` 完成 STM32CubeIDE GCC Debug 和 Release
全量 clean build，两种配置均为 `0 errors, 0 warnings`。完成 UART、Console、
Telemetry 和 Command Manager 边界拆分后，Debug 镜像为 `text=207368`、
`data=96`、`bss=29192`；Release 镜像为 `text=172452`、`data=96`、
`bss=29176`。BSS 增量主要来自
固定容量的 UART DMA 发送队列，不使用动态分配。

当前 FreeRTOS 固件尚未完成烧录后的全量硬件回归。

### 4.3 当前自有代码位置

当前迁移后的主要自有代码为：

```text
firmware/application/stm32g474/
├─ app/
│  ├─ chassis_app.c
│  └─ chassis_app.h
├─ board/
│  └─ board_config.h
├─ bsp/
│  ├─ motor/
│  ├─ encoder/
│  ├─ power_monitor/
│  ├─ qspi/
│  └─ lcd/
├─ components/
│  └─ pid/
├─ communication/
│  └─ fdcan/
├─ modules/
│  ├─ chassis_control/
│  └─ diagnostics/
├─ infrastructure/
│  └─ status_display/
├─ rtos/
│  ├─ rtos_app.c
│  └─ rtos_app.h
└─ config/
   ├─ app_config.h
   ├─ build_info.h
   ├─ control_config.h
   ├─ feature_config.h
   ├─ protocol_config.h
   └─ storage_layout.h
```

### 4.4 目录迁移完成状态

阶段 0 已于 2026-07-25 完成：

1. `.gitignore` 已覆盖新工程根目录的 Debug、Release、`.metadata`、launch 和本地索引文件。
2. `.cproject` 的 Debug 和 Release 配置已注册当前所有实际 include path 和 source entry，包括 `board/` 与 `rtos/`。
3. `board/board_config.h` 已集中提供 `BOARD_*` 外设句柄和引脚映射，BSP 不再直接绑定 CubeMX 全局句柄。
4. `rtos/rtos_app.c/.h` 已承接静态任务循环，`defaultTask` 只进入 `RtosApp_Run()`。
5. 应用生命周期 API 已统一为 `ChassisApp_*`，`app_main` 和旧 `App_*` 命名已清零。
6. 原 `project_config.h` 已拆分为应用、构建、控制、功能和协议配置，`storage_layout.h` 独立保留。
7. `services/status_display/` 已迁移到 `infrastructure/status_display/`。
8. Debug 和 Release 均从新工程根目录完成全量 clean build，所有 BSP、modules、infrastructure 和 `rtos_app.o` 均进入链接输入。
9. 使用临时 Git 索引验证迁移结果，不修改工作区真实暂存区。
10. 本地 `.ai-bridge/` 和 `docs/rollout-*.jsonl` 会话数据已排除在产品提交之外。

尚未完成的是阶段 1 的烧录和 FreeRTOS 全量硬件回归。完成该回归前，不继续大规模业务拆分。

---

## 5. 总体架构

目标架构不是单一严格垂直调用栈，而是以 `app/rtos` 为组装层、以 `modules` 为业务核心、以 `bsp` 为硬件边界的分层结构：

```text
                         Jetson / ROS 2
                               │
                            CAN FD
                               │
                     communication layer
                               │
                  app / rtos orchestration
                     ┌─────────┴─────────┐
                     │                   │
                  modules          infrastructure
                     │                   │
                     └──────┬────────────┘
                            │
                        components
                            │
                           bsp
                            │
                    board / CubeMX HAL
```

核心原则：

1. CubeMX 生成代码保持独立。
2. BSP 只封装硬件操作，不决定业务状态。
3. 通信层只负责传输和协议，不直接控制电机。
4. 业务状态机、轮速控制和安全策略放在 `modules/`。
5. 通用算法放在 `components/`，不依赖 HAL。
6. 调试、显示、存储和时间同步放在 `infrastructure/`。
7. FreeRTOS 任务只负责调度，不保存核心业务规则。
8. 每次只迁移一个清晰职责，保持可编译和可回归。

---

## 6. 最终目标目录结构

允许提前建立最终目录骨架。空叶目录使用 `.gitkeep`，不要创建没有实现内容的空 `.c/.h` 文件。加入真实文件后删除对应 `.gitkeep`。

命名约定：

- CubeMX 已占用 `Drivers/` 和 `Middlewares/`，自有代码不再创建仅大小写不同的 `drivers/` 或 `middleware/`。
- 自有目录统一使用小写 `snake_case`，避免 Windows 下的大小写、单复数和语义混淆。
- 公共运行设施统一使用 `infrastructure/`，不再恢复旧 `services/` 命名。
- 目录名称描述职责，不使用无法判断所有权的 `misc/`、`common/` 或泛化 `utils/`。

```text
chassis-controller/
├─ firmware/
│  ├─ shared/
│  │  ├─ flash_layout.h
│  │  ├─ firmware_image.h
│  │  ├─ ota_metadata.h
│  │  └─ ota_protocol.h
│  │
│  ├─ application/
│  │  └─ stm32g474/
│  │     ├─ Core/                         # CubeMX 生成
│  │     ├─ Drivers/                      # HAL、CMSIS
│  │     ├─ Middlewares/                  # FreeRTOS 等第三方中间件
│  │     ├─ Application/User/             # CubeIDE 启动和系统适配
│  │     ├─ chassis_controller.ioc
│  │     ├─ STM32G474VETX_FLASH.ld
│  │     │
│  │     ├─ board/
│  │     │
│  │     ├─ bsp/
│  │     │  ├─ motor/
│  │     │  ├─ encoder/
│  │     │  ├─ power_monitor/
│  │     │  ├─ fdcan/
│  │     │  ├─ uart/
│  │     │  ├─ rtc/
│  │     │  ├─ timebase/
│  │     │  ├─ watchdog/
│  │     │  ├─ lcd/
│  │     │  ├─ qspi/
│  │     │  └─ imu/
│  │     │
│  │     ├─ components/
│  │     │  ├─ pid/
│  │     │  ├─ limiter/
│  │     │  ├─ filters/
│  │     │  ├─ ring_buffer/
│  │     │  └─ crc/
│  │     │
│  │     ├─ communication/
│  │     │  ├─ can_transport/
│  │     │  └─ ota_transport/
│  │     │
│  │     ├─ infrastructure/
│  │     │  ├─ console/
│  │     │  ├─ telemetry/
│  │     │  ├─ parameter_storage/
│  │     │  ├─ status_display/
│  │     │  └─ time_service/
│  │     │
│  │     ├─ modules/
│  │     │  ├─ command_manager/
│  │     │  ├─ differential_drive/
│  │     │  ├─ wheel_controller/
│  │     │  ├─ odometry/
│  │     │  ├─ imu_manager/
│  │     │  ├─ safety_manager/
│  │     │  ├─ fault_manager/
│  │     │  ├─ parameter_manager/
│  │     │  └─ diagnostics/
│  │     │
│  │     ├─ rtos/
│  │     │  ├─ rtos_app.c
│  │     │  ├─ rtos_app.h
│  │     │  ├─ rtos_objects.c
│  │     │  ├─ rtos_objects.h
│  │     │  ├─ tasks/
│  │     │  │  ├─ control_task.c
│  │     │  │  ├─ communication_task.c
│  │     │  │  ├─ diagnostics_task.c
│  │     │  │  ├─ console_task.c
│  │     │  │  ├─ display_task.c
│  │     │  │  └─ storage_task.c
│  │     │  └─ hooks/
│  │     │     └─ freertos_hooks.c
│  │     │
│  │     ├─ app/
│  │     │  ├─ chassis_app.c
│  │     │  └─ chassis_app.h
│  │     │
│  │     ├─ config/
│  │     │  ├─ app_config.h
│  │     │  ├─ control_config.h
│  │     │  ├─ feature_config.h
│  │     │  ├─ protocol_config.h
│  │     │  ├─ storage_layout.h
│  │     │  └─ build_info.h
│  │     │
│  │     └─ tests/
│  │        ├─ unit/
│  │        └─ target/
│  │
│  └─ bootloader/
│     └─ stm32g474/
│        ├─ Core/
│        ├─ Drivers/
│        ├─ Application/User/
│        ├─ bootloader.ioc
│        ├─ boot/
│        │  ├─ boot_main.c
│        │  ├─ boot_state_machine.c
│        │  ├─ image_validator.c
│        │  ├─ bank_manager.c
│        │  └─ app_launcher.c
│        ├─ bsp/
│        │  ├─ flash/
│        │  ├─ fdcan/
│        │  ├─ uart/
│        │  └─ watchdog/
│        ├─ components/
│        │  ├─ crc/
│        │  ├─ sha256/
│        │  └─ signature/
│        └─ config/
│           ├─ boot_config.h
│           └─ build_info.h
│
├─ protocol/
│  ├─ canfd_protocol.md
│  └─ ota_canfd_protocol.md
├─ docs/
└─ .github/workflows/
```

目录骨架可以提前创建，但实现仍按阶段逐步加入。

---

## 7. 各层职责

### 7.1 CubeMX/CubeIDE 目录

以下目录由 CubeMX/CubeIDE 管理：

```text
Core/
Drivers/
Middlewares/
Application/User/
```

规则：

- 不随意移动生成文件。
- 不在生成目录长期堆放业务代码。
- 手工修改优先限制在 `USER CODE BEGIN/END` 区域。
- 重新生成后必须核对 include path、source folder、中断优先级和 HAL 时基。
- `Application/User/` 是 CubeIDE 的启动和 Newlib 适配目录，不等同于自定义 `app/`。

### 7.2 `board/`

`board/` 描述当前硬件板如何连接 MCU 资源：

- GPIO 映射
- 外设句柄映射
- PWM 定时器和通道
- 编码器定时器
- FDCAN、UART、SPI、QSPI、RTC 实例
- 电机和编码器方向
- 硬件版本差异

`board/` 不实现：

- PID
- 协议解析
- 状态机
- 运动学
- 故障策略

### 7.3 `bsp/`

BSP 回答“这个硬件怎么操作”。

典型职责：

- `motor/`：PWM、方向、立即禁止输出
- `encoder/`：计数器读取、回绕处理、方向换算
- `power_monitor/`：ADC 采样和电压换算
- `fdcan/`：HAL FDCAN 初始化后的底层收发封装
- `uart/`：UART DMA 收发和底层缓冲
- `rtc/`：RTC 读取、备份寄存器和复位标记
- `timebase/`：单调时间和高精度计时接口
- `watchdog/`：IWDG 初始化、刷新和复位原因
- `lcd/`：ST7789 绘图和 SPI DMA
- `qspi/`：JEDEC、擦除、写入、读取和 DMA
- `imu/`：寄存器读写和底层总线

BSP 不决定：

- 是否允许电机运行
- 是否处于 ACTIVE
- 是否命令超时
- 是否应进入 FAULT
- PID 参数
- LCD 页面业务内容

### 7.4 `components/`

放不依赖 HAL、可在 PC 上单元测试的通用组件：

- PID
- 限幅器
- 斜坡和加速度限制
- 滤波器
- Ring Buffer
- CRC

要求：

- 不包含 STM32 HAL 头文件
- 不依赖底盘业务模块
- 输入输出明确
- 不保存全局硬件句柄
- 可使用宿主机编译器测试

### 7.5 `communication/`

负责传输和协议，不负责业务执行。

`can_transport/`：

- CAN 原始帧接收、发送调度和硬件诊断适配
- 底盘 CAN ID、握手帧和控制帧编解码
- 协议版本、长度、保留位和参数范围检查
- 控制序号、握手重建和待处理命令管理
- 后续代码量增长时在该目录内按源文件拆分，不建立只做函数转发的平行目录

`ota_transport/`：

- OTA QUERY/BEGIN/DATA/END/ABORT/RESULT
- 块序号、偏移和重传
- 与升级写入状态机的数据接口

通信层不得：

- 直接调用电机输出
- 直接切换底盘安全状态
- 在 ISR 中执行复杂协议解析
- 在 ISR 中打印日志

### 7.6 `infrastructure/`

`infrastructure/` 表示嵌入式系统公共设施，替代容易误解的 `services/` 命名。

`console/`：

- UART 命令解析
- 开发测试入口
- 状态查询
- VOFA+ 命令入口
- 已实现固定长度行缓冲和固定深度命令队列；超长行整行丢弃，队列溢出有计数
- 只输出结构化命令，不直接操作 CAN、电机、PID、QSPI 或 IWDG

`telemetry/`：

- 周期遥测
- VOFA+ FireWater 输出
- 控制周期统计
- 日志和诊断快照输出
- 已接管 TEXT、VOFA 和 OFF 模式及发送周期，只消费应用层提供的只读快照

`parameter_storage/`：

- 参数持久化
- 格式版本
- CRC
- 双副本
- 恢复默认值
- 写入失败保护

它不理解 PID、轮径、轮距等业务含义。

`status_display/`：

- LCD 页面组织
- 状态、故障、版本和外设结果显示

底层绘图仍在 `bsp/lcd/`。

`time_service/`：

- Jetson 时间同步
- 单调时间与 UTC/RTC 时间映射
- 跨设备时间戳转换

基础设施不得决定电机是否允许运行。

### 7.7 `modules/`

`modules/` 保存底盘产品业务。

`command_manager/`：

- 保存最新有效命令
- 序号和会话状态
- 命令时间戳
- ARM、DISARM、ESTOP
- 命令超时状态

`differential_drive/`：

- 线速度和角速度转换为左右轮目标速度
- 左右轮速度反算底盘运动
- 轮径、轮距和方向参数

`wheel_controller/`：

- 左右轮目标速度
- 测量速度
- PID
- 输出限幅
- 积分清零
- 输出到电机 BSP

`safety_manager/`：

- 底盘状态机
- 电机许可
- 急停
- 命令超时
- 欠压
- 堵转
- 超速
- 调参模式限制

`fault_manager/`：

- 当前故障位
- 历史故障位
- 故障等级
- 锁存和清除条件
- 复位原因

`parameter_manager/`：

- PID、轮径、轮距、编码器和限幅参数
- 参数范围检查
- pending 参数
- 控制周期边界应用
- 默认参数恢复

`diagnostics/`：

- 外设在线状态
- CAN 错误统计
- 编码器异常
- 电压状态
- IMU 状态
- 控制周期超时
- 任务健康状态

`odometry/`：

- STM32 侧基础里程计或原始运动增量
- 为 Jetson 提供可重新计算的数据

`imu_manager/`：

- IMU 初始化和在线状态
- 原始加速度、角速度和温度
- 零偏
- 基础滤波
- 时间戳

### 7.8 `rtos/`

FreeRTOS 是调度层，不是业务层。

任务文件只负责：

- 等待通知、队列或事件
- 调用模块
- 发布快照
- 记录任务健康

禁止在任务文件中复制 PID、协议、状态机和故障判断。

### 7.9 `app/`

`app/` 负责：

- 初始化顺序
- 模块组装
- 当前过渡调度
- RTOS 对象创建入口
- 致命错误统一处理

`app/` 不实现复杂算法和长期业务状态。

### 7.10 `config/`

`config/` 独立保留。

```text
board/   = 硬件板如何连接
config/  = 软件产品如何运行
```

建议文件：

- `app_config.h`：启动策略和应用周期
- `control_config.h`：控制周期、限幅和默认控制参数
- `feature_config.h`：LCD、IMU、遥测等功能开关
- `protocol_config.h`：CAN 超时、心跳和上报周期
- `storage_layout.h`：QSPI 参数、日志、测试和 OTA 区域
- `build_info.h`：Application 当前版本和构建信息

Application 与 Bootloader 分别维护自己的 `config/build_info.h`。不在 `firmware/shared/` 放当前固件版本值。

### 7.11 `tests/`

`tests/unit/`：

- 宿主机单元测试
- 不连接真实硬件
- 优先覆盖纯算法和协议编解码

`tests/target/`：

- STM32 板上测试
- 危险操作必须显式确认
- 不在正常启动中自动转动电机、擦除 QSPI 或触发复位

---

## 8. 依赖方向

推荐依赖关系：

```text
app / rtos
  ├─ modules
  ├─ communication
  └─ infrastructure

modules
  ├─ components
  └─ bsp

communication
  ├─ components
  └─ bsp/fdcan

infrastructure
  ├─ components
  └─ bsp/uart、bsp/lcd、bsp/qspi、bsp/rtc

bsp
  └─ board / CubeMX HAL

components
  └─ C 标准库或无外部依赖
```

强制约束：

1. `components` 不依赖 HAL。
2. `bsp` 不包含 `modules` 头文件。
3. `communication` 不直接控制电机。
4. `communication` 不依赖具体业务模块实现。
5. `infrastructure` 不决定底盘状态和电机许可。
6. `modules` 不直接访问 CubeMX 全局句柄，优先通过 BSP。
7. `rtos/tasks` 不保存核心业务状态。
8. `app` 只组装和调度。
9. FDCAN ISR 不执行 PID、状态机、日志和 LCD 操作。
10. LCD、QSPI 和串口输出不能阻塞 10 ms 控制周期。
11. 控制和安全逻辑不依赖 RTC 日期时间。
12. 模块间传递明确的数据结构或只读快照，不通过任意全局变量耦合。

---

## 9. 当前代码拆分映射

### 9.1 `chassis_control.c`

当前 `modules/chassis_control/chassis_control.c` 同时包含：

- 命令保存和超时
- 控制状态机
- 急停和内部故障
- 编码器累计
- 开环测试
- PID
- 电机输出

目标拆分：

```text
命令缓存、序号、时间戳       → modules/command_manager/
左右轮目标和 PID             → modules/wheel_controller/
差速解算                     → modules/differential_drive/
状态机和电机许可             → modules/safety_manager/
故障位、锁存和清除           → modules/fault_manager/
编码器累计和基础运动增量     → modules/odometry/
```

拆分必须渐进进行，不能一次重写整个文件。

2026-07-25 已完成 `modules/command_manager/`：

- 唯一保存当前有效左右轮目标、接收时间戳、命令来源和可选序号
- 统一校验目标范围；CAN 来源必须携带序号，Console 和 Demo 来源不得伪造序号
- 提供来源受限的时间戳刷新，避免其他入口延长 Demo 命令寿命
- 统一执行 200 ms 新鲜度判断，但超时停车和故障状态仍由 `chassis_control` 决定
- CAN 重复帧、跳帧、回绕和握手会话仍由 `communication/can_transport` 校验
- `chassis_control` 不再保存 `has_command` 和 `last_command_ms`

### 9.2 `board_self_test.c`

2026-07-25 已完成第一轮职责拆分。原文件曾同时包含：

- UART DMA
- 串口命令解析
- VOFA+ 遥测
- PID 在线调参
- 电机和编码器测试
- RTC 输出
- QSPI 测试状态机
- IWDG 测试
- FDCAN 诊断

目标拆分：

```text
UART DMA 底层               → bsp/uart/
串口命令解析                → infrastructure/console/
VOFA+ 遥测                  → infrastructure/telemetry/
硬件状态检查                → modules/diagnostics/
PID 参数业务                → modules/parameter_manager/
持久化                      → infrastructure/parameter_storage/
危险板上测试入口            → tests/target/
```

当前已完成：

- USART1 HAL 和 RX/TX DMA 所有权收敛到 `bsp/uart/`
- RX 使用 128 字节循环 DMA 和 256 字节软件环形缓冲，错误后由任务上下文自动重启
- TX 使用固定深度静态队列，DMA 完成后自动续发，不阻塞 10 ms 控制周期
- 串口命令成帧、语法解析和命令队列迁入 `infrastructure/console/`
- TEXT/VOFA 遥测模式、周期和格式化迁入 `infrastructure/telemetry/`
- PID、编码器和 CAN 命令调度由 `app/chassis_app.c` 组装，不再放在自检模块
- `board_self_test` 仅保留启动硬件检查、QSPI/IWDG 测试、板上电机测试请求和自检报告

### 9.3 CAN 通信

原 `communication/fdcan/fdcan_driver.c` 混合了：

- HAL FDCAN 接入
- 中断收发
- 开发三步握手
- 控制协议解析
- 版本、范围和序号检查
- 命令缓存

目标拆分：

```text
HAL 收发和 IRQ              → bsp/fdcan/
握手、帧解析、发送和统计    → communication/can_transport/
命令缓存和超时              → modules/command_manager/
```

当前阶段不额外保留 `fdcan_driver` 或 `chassis_protocol` 转发层。CAN 通信实现统一放在 `communication/can_transport/`，BSP 只提供与业务协议无关的过滤器、原始帧收发和控制器状态读取。

### 9.4 其他当前文件

```text
components/pid/speed_pid.*                 保留在 components/pid/
bsp/motor/bsp_motor.*                      保留在 bsp/motor/
bsp/encoder/bsp_encoder.*                  保留在 bsp/encoder/
bsp/power_monitor/bsp_power_sample.*       保留在 bsp/power_monitor/
bsp/qspi/bsp_qspi_flash.*                  保留在 bsp/qspi/
bsp/lcd/bsp_lcd.*                          保留在 bsp/lcd/
services/status_display/status_display.*   迁移到 infrastructure/status_display/
app/chassis_app.*                          保留在 app/，统一旧 app_main 命名
```

---

## 10. FreeRTOS 目标运行模型

### 10.1 当前过渡模型

当前保持：

```text
TIM6 ISR
  ↓ 增加待处理控制周期计数
1 ms defaultTask
  ↓
RtosApp_Run()
  ↓
ChassisApp_Run()
  ↓
处理 10 ms 控制周期和其他轮询逻辑
```

在完成目录迁移、重新构建和硬件回归前，不改变该调度行为。

### 10.2 目标任务

```text
control_task          100 Hz，最高业务优先级
communication_task    事件驱动
console_task          事件驱动或低频
 diagnostics_task     低频
 display_task         低频
 storage_task         按需运行
```

不要求一个模块对应一个任务。

### 10.3 控制任务

目标执行链：

```text
TIM6 ISR 发任务通知
  ↓
读取编码器
  ↓
更新命令快照
  ↓
检查安全状态
  ↓
差速解算
  ↓
目标和加速度限幅
  ↓
PID
  ↓
更新 PWM
  ↓
发布控制状态快照
  ↓
记录任务健康
```

控制任务要求：

- 固定 10 ms 周期
- 不执行串口格式化输出
- 不执行 LCD 刷屏
- 不执行 QSPI 擦写
- 不等待普通互斥锁的长时间阻塞
- 过期通知不得无限补跑
- 发生严重超时立即进入安全停车

### 10.4 中断规则

中断只做：

- 读取或写入最少量寄存器/数据
- 清除中断标志
- 写入无阻塞缓冲
- 发送任务通知或队列
- 执行必须立即完成的硬件安全清零

当前 FDCAN、TIM6、UART、DMA、QSPI 和 EXTI 等相关中断优先级为 5，处于当前 FreeRTOS `MAX_SYSCALL_INTERRUPT_PRIORITY` 边界。未来使用 `FromISR` API 前必须重新审查优先级，能够调用 RTOS API 的中断，其数值优先级不得高于允许边界。

急停 ISR 的 PWM 清零不能依赖普通任务及时运行。

---

## 11. 时间体系

系统区分三类时间。

### 11.1 单调时间

目标目录：

```text
bsp/timebase/
```

用途：

- PID 周期
- CAN 命令超时
- 堵转持续时间
- 任务健康
- 控制周期统计
- 遥测间隔

接口示例：

```c
time_us_t Timebase_GetMonotonicUs(void);
time_ms_t Timebase_GetMonotonicMs(void);
```

单调时间不得因 RTC 校时而跳变。

### 11.2 HAL 时基

当前 TIM7 提供 HAL 1 ms Tick，用于 HAL 超时。

它属于 CubeMX/HAL 运行基础，不直接作为最终业务时间服务接口。

### 11.3 RTC 日期时间

目标目录：

```text
bsp/rtc/
```

当前硬件使用：

- 32.768 kHz LSE
- RTC
- 纽扣电池

用途：

- 故障日期
- 维护记录
- 人类可读日志时间
- 复位测试标记

RTC 不用于 PID、命令超时和堵转计时。

### 11.4 Jetson 同步时间

目标目录：

```text
infrastructure/time_service/
```

用途：

- ROS 2 数据对齐
- CAN 数据时间戳映射
- IMU 和编码器融合
- STM32 单调时间与 Jetson 时间的偏移估计

同步时间调整不能破坏单调控制时间。

---

## 12. 安全和故障架构

### 12.1 当前已存在的安全原语

当前代码已经具备：

- 上电 PWM 为 0
- 电机 Demo 默认关闭
- 急停 ISR 立即清零 PWM
- 急停锁存
- CAN 有效命令超时停车
- 无效 CAN 帧不刷新命令时间
- 内部错误统一进入安全停车
- 初始化失败进入锁存故障
- 控制待处理周期过多时故障
- 电机方向切换前先输出零值
- IWDG 约 2 秒受控复位

急停当前是软件清零 PWM，不等同于独立硬件断电急停。

### 12.2 目标状态机

建议状态：

```text
BOOT
  ↓
SELF_TEST
  ↓
STANDBY
  ↓ ARM
ARMED
  ↓ 有效非零命令
ACTIVE

受控调参：TUNING_READY → TUNING_ACTIVE
升级：UPDATE
任意状态 → ESTOP
严重故障 → FAULT
```

只有 `ACTIVE` 或受限的 `TUNING_ACTIVE` 允许非零 PWM。

### 12.3 安全管理

`safety_manager` 负责判断：

- 电机许可
- 命令是否新鲜
- 急停状态
- 欠压
- 超速
- 堵转
- 控制周期超时
- 调参限制
- 升级状态禁止运动

堵转基础条件：

```text
目标速度超过阈值
且 PWM 超过阈值
且编码器速度接近 0
且持续超过规定时间
```

必须使用持续时间和恢复滞回，不能用单个采样点判定。

### 12.4 故障管理

`fault_manager` 区分：

- 当前故障
- 历史故障
- 警告
- 可自动恢复故障
- 必须人工清除的锁存故障
- 必须复位的严重故障

复位原因至少包括：

- Power On Reset
- Software Reset
- IWDG Reset
- Brownout Reset

复位原因通过 CAN、UART 和 LCD 输出。

### 12.5 Watchdog

当前喂狗主要根据控制周期最近是否正常和内部故障状态判断。

最终不允许普通周期任务无条件喂狗。目标流程：

```text
control_task health
communication_task health
console/diagnostics health
关键硬件状态
        │
        ▼
watchdog health aggregator
        │
        ▼
全部满足条件才刷新 IWDG
```

低优先级显示或遥测短暂异常不一定立即禁止喂狗，但控制、安全和关键通信健康必须纳入判定。

---

## 13. 控制和数据链路

### 13.1 当前控制单位

当前 `0x100` 控制帧使用左右轮 `counts/10 ms`，用于低速闭环联调。

该单位可以继续作为 `WHEEL_SPEED` 调试模式，但正式运行应优先支持：

```text
CHASSIS_MODE_TWIST
CHASSIS_MODE_WHEEL_SPEED
```

- `TWIST`：Jetson 下发线速度和角速度，STM32 完成差速解算。
- `WHEEL_SPEED`：Jetson 直接下发左右轮目标，仅用于调试、标定和受限模式。

### 13.2 差速运动学

`differential_drive` 负责：

```text
linear_velocity + angular_velocity
  ↓
left_wheel_target + right_wheel_target
```

参数包括：

- 左右轮有效半径
- 轮距
- 左右轮方向
- 编码器 counts/rev
- 目标速度范围

运动学不直接写 PWM。

### 13.3 轮速控制

`wheel_controller` 每 10 ms：

1. 读取编码器周期增量。
2. 计算左右轮测量速度。
3. 应用目标斜坡和限幅。
4. 执行 PID。
5. 限制输出。
6. 根据方向写入 `bsp/motor`。
7. 停车或故障时清空积分和历史状态。

PID 组件保持通用，不理解左右轮和安全状态。

### 13.4 反馈数据

STM32 建议上报：

- 左右编码器累计计数
- 左右编码器周期增量
- 左右轮测量速度
- 左右轮目标速度
- 左右轮 PID 输出
- STM32 单调时间戳
- IMU 原始加速度和角速度
- IMU 温度
- 电源电压
- 底盘状态
- 当前和历史故障位
- CAN 错误统计
- 控制周期统计
- 固件版本和复位原因

Jetson 可根据原始计数重新计算里程计并完成融合。

---

## 14. CAN FD 与协议设计

### 14.1 当前总线参数

当前已验证配置：

- CAN FD
- BRS 开启
- 11 位标准 ID
- 仲裁速率 500 kbit/s
- 数据速率 2 Mbit/s
- STM32 nominal 采样点 80.00%
- STM32 data 采样点 82.35%（170 MHz FDCAN 时钟、Prescaler=5、TSEG1=13、TSEG2=3）
- Jetson nominal/data 采样点 80.00%（mttcan）
- 工程目标统一按 80% 配置；不将 STM32 数据段实际量化值四舍五入记录为 80%
- 多字节整数使用小端序

### 14.2 当前已实现帧

当前正式控制试验帧：

```text
0x100 Jetson → STM32
8 byte
版本、使能、序号、保留位、左右轮 counts/10 ms
```

当前开发联调三步握手：

```text
0x720 Jetson → STM32 请求/确认
0x721 STM32 → Jetson 响应
```

三步握手只属于开发 Bring-up，不应长期承担正式生产控制会话和安全授权职责。

### 14.3 当前序号缺陷

当前实现要求新序号严格等于上一序号加一。

这会导致：

```text
收到序号 10
丢失序号 11
收到序号 12 → 拒绝
后续 13、14... → 持续拒绝
```

直到重新执行开发握手并重置序号状态，控制通道才可能恢复。

正式协议禁止使用“丢失一帧后永久拒绝后续所有帧”的策略。

目标序号语义：

- 第一帧建立序号基准。
- 相同序号识别为重复帧并拒绝。
- 小范围前向跳变允许接收，同时累计丢帧数。
- 明显旧帧或乱序帧拒绝。
- `255 → 0` 正常回绕。
- 命令超时、会话重建或明确 RESET 后允许重新建立基准。
- 重新建立序号基准不等于自动恢复非零电机输出。

具体算法和会话规则必须在 `protocol/canfd_protocol.md` 冻结后实现。

### 14.4 正式协议要求

每个正式帧必须明确：

- CAN ID
- 方向
- 固定长度
- 协议版本
- 字节偏移
- 字节序
- 单位和缩放
- 合法范围
- 保留位
- 序号语义
- 超时行为
- 错误处理

协议不得直接传输 C 结构体内存。

协议层至少支持以下错误类型：

- `INVALID_LENGTH`
- `UNSUPPORTED_VERSION`
- `INVALID_PARAMETER`
- `INVALID_STATE`
- `DUPLICATE_SEQUENCE`
- `STALE_SEQUENCE`

### 14.5 目标消息类别

后续协议逐步增加：

- Chassis command
- Wheel-speed debug command
- Chassis state
- Wheel feedback
- Encoder feedback
- Heartbeat
- Fault and diagnostics
- Parameter query/update
- IMU raw data
- Time synchronization
- OTA transport

在 `protocol/canfd_protocol.md` 更新前，不在代码中自行分配正式 ID。

### 14.6 超时原则

- 只有完整通过版本、长度、保留位、范围、状态和序号检查的控制帧才能刷新命令时间。
- 心跳不能代替控制命令新鲜度。
- 控制命令超时必须停车并重置 PID。
- 通信恢复后，必须经过有效状态和命令检查才能重新运行。
- 旧输出不能自动恢复。

---

## 15. Console、遥测和 PID 调参

第一版继续使用：

```text
Windows + USB 串口 + VOFA+
```

不开发专用 PID GUI。

### 15.1 当前串口和 VOFA+ 接口

当前固件已经支持：

```text
telemetry text
telemetry vofa
telemetry off

pid show
pid left <kp> <ki> <kd>
pid right <kp> <ki> <kd>
pid target <left> <right>
pid stop
```

当前 `telemetry vofa` 输出逗号分隔文本，每行字段顺序为：

```text
left_target,left_delta,left_rpm_x10,left_output,
right_target,right_delta,right_rpm_x10,right_output,
vin_mv,state,fault_flags
```

默认关闭遥测。调参时应架空车轮或固定车辆，优先一次只调整一侧电机，并根据串口带宽限制遥测频率。

### 15.2 目标链路和职责

目标链路：

```text
VOFA+
  ↓ UART DMA
bsp/uart
  ↓
infrastructure/console
  ↓
modules/parameter_manager
  ↓
modules/wheel_controller
```

遥测链路：

```text
控制状态只读快照
  ↓
infrastructure/telemetry
  ↓
VOFA+ FireWater
```

### 15.3 目标参数接口

建立 `parameter_manager` 和正式参数存储后，串口接口应逐步统一为：

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

tune start left <target>
tune start right <target>
tune stop
```

参数行为：

- `set` 修改 RAM 中 pending 参数。
- `apply` 在控制周期边界应用。
- 应用新 PID 参数时清空积分和历史误差。
- `save` 才允许持久化。
- `reset` 恢复默认值。
- 参数范围由 `parameter_manager` 判断。
- 存储格式和双副本由 `parameter_storage` 负责。

PID 组件目标接口至少支持配置更新和状态清零：

```c
bool PidController_SetConfig(PidController *pid,
                             const PidConfig *config);
void PidController_Reset(PidController *pid);
```

应用新参数时必须清空积分、上次误差和微分历史，避免旧控制状态造成 PWM 跳变。

调参安全要求：

- 车轮架空或车辆固定
- 限制目标速度
- 限制最大 PWM
- 急停始终有效
- 串口目标必须有超时
- 严重故障禁止启动
- 退出调参立即清零目标
- 默认只改 RAM
- 确认稳定后再保存

控制任务不得直接格式化和发送大段遥测字符串，只发布快照。

---

## 16. QSPI 和参数存储

当前 QSPI 已验证：

- JEDEC ID
- 8 MiB 容量
- 保留扇区擦写
- 1 KiB DMA 写入和回读

当前测试扇区 `0x007FF000` 只用于受控测试，不得用于参数、日志和 OTA 正式分区。

目标 QSPI 布局由 `config/storage_layout.h` 定义，至少区分：

- 参数主副本
- 参数备份副本
- 日志区
- 测试保留区
- OTA 临时区或外部镜像区

在 Bootloader Flash 分区和 OTA 方案冻结前，不随意改变存储布局。

`parameter_storage` 必须支持：

- Magic
- 格式版本
- 数据长度
- CRC
- 写入序号
- 双副本选择
- 掉电恢复
- 恢复默认值

控制任务不得直接执行 QSPI 擦除和写入。

---

## 17. ROS 2 Bridge 边界

ROS 2 Bridge 运行在 Jetson，不运行在 STM32。

STM32 负责：

- 编码器读取和回绕处理
- 左右轮速度
- 双轮 PID
- 差速运动学
- PWM
- 命令超时
- 急停和故障保护
- 原始 IMU 数据和采样时间
- 电压和设备状态

Jetson 负责：

- `/cmd_vel`
- CAN FD Bridge
- 里程计积分和融合
- IMU 坐标轴变换和高层标定
- `robot_localization`
- `/odom`
- `/imu/data_raw`
- `/joint_states`
- `/diagnostics`
- TF：`odom -> base_link`

STM32 可提供基础里程计，但 Jetson 必须能够根据原始编码器数据重新计算。

---

## 18. IMU 路线

IMU 第一阶段只实现：

- 设备识别
- 原始加速度
- 原始角速度
- 温度
- 单调时间戳
- 零偏标定
- 基础低通
- 掉线检测
- CAN 上报

STM32 当前不优先实现复杂 EKF。姿态估计、坐标变换和轮速融合放在 Jetson/ROS 2。

---

## 19. Bootloader 与 OTA

### 19.1 工程边界

Application 和 Bootloader 使用两个独立 CubeMX 工程，因为它们具有不同的：

- 启动入口
- 向量表
- 链接脚本
- Flash 地址
- 外设配置
- 编译产物
- 版本周期

Bootloader 保持尽可能小。

### 19.2 当前内存状态

当前 Application 链接脚本仍使用：

```text
FLASH ORIGIN = 0x08000000
FLASH LENGTH = 512K
```

即 Application 当前占用完整内部 Flash 地址空间，没有为 Bootloader、Metadata 和 A/B Slot 预留区域。

在实现 Bootloader 前，必须先完成：

1. STM32G474 实际 Flash 容量核对。
2. Bootloader 大小预算。
3. Metadata 区布局。
4. Application Slot A/B 大小。
5. 向量表重定位。
6. Application 链接地址修改。
7. 失败回滚和试运行次数定义。

不得复制其他 STM32 型号或开源项目的 Flash 地址。

### 19.3 `firmware/shared/`

仅保存 Bootloader 与 Application 的稳定契约：

- `flash_layout.h`
- `firmware_image.h`
- `ota_metadata.h`
- `ota_protocol.h`

不把普通 BSP、业务模块和当前版本值放入 `shared/`。

### 19.4 版本管理

Application：

```text
firmware/application/stm32g474/config/build_info.h
```

Bootloader：

```text
firmware/bootloader/stm32g474/config/build_info.h
```

两者独立维护版本。Application 可以声明最低兼容 Bootloader 版本。

### 19.5 OTA V1

目标流程：

```text
Jetson
  ↓ CAN FD
Application 进入 UPDATE，禁止电机
  ↓
接收固件并写入非活动 Slot
  ↓
校验长度、CRC、SHA-256 和签名
  ↓
写入 UPDATE_READY
  ↓
重启
  ↓
Bootloader 校验并试运行新镜像
  ↓
Application 完成启动自检
  ↓
CONFIRMED 或 ROLLBACK
```

V1 中 Bootloader 不直接通过 CAN FD 接收完整固件。

### 19.6 OTA V2

V2 再增加 Bootloader CAN FD Recovery。当前 `ota_protocol.h` 可以预留帧定义，但不提前实现完整 Recovery 状态机。

---

## 20. CubeMX、CubeIDE 和构建约定

当前阶段继续使用：

- STM32CubeMX 6.18
- STM32CubeG4 V1.6.3
- STM32CubeIDE GCC
- ST-Link

当前不迁移到 CMake。

### 20.1 工程根目录

正式 Application 工程根目录为：

```text
firmware/application/stm32g474/
```

README、VS Code 本地任务和 CubeIDE 导入路径最终都必须同步到该位置。

### 20.2 `.cproject`

目录迁移后必须同时更新 Debug 和 Release：

- include paths
- source entries
- linker script 路径
- 排除目录
- 自定义源码目录

当前按模块平铺后，不能继续只配置旧的 `app/Inc`、`bsp/Inc` 和 `communication/Inc`。

新目录至少应包含实际存在头文件的模块路径，或采用经过验证的统一 include 策略。不要为了少配 include path 把所有头文件重新堆到公共 `Inc/`。

### 20.3 `.gitignore`

迁移后必须忽略新路径中的：

```text
firmware/application/stm32g474/Debug/
firmware/application/stm32g474/Release/
firmware/application/stm32g474/.metadata/
```

本地语言索引、launch、workspace 缓存和 IDE 浏览器缓存也不得提交。

只跟踪重现工程所必需的 CubeIDE 项目配置。

### 20.4 构建验证

完成目录迁移后按顺序执行：

1. 删除或忽略旧 Debug/Release 产物。
2. CubeIDE Clean Debug。
3. CubeIDE Build Debug。
4. CubeIDE Clean Release。
5. CubeIDE Build Release。
6. 检查所有自有源码都实际进入构建。
7. 检查 ELF、MAP 和对象文件路径来自新目录。
8. 烧录并执行 FreeRTOS 硬件回归。

仅看到旧 ELF 文件存在，不代表当前源码已被编译。

---

## 21. 测试和验收

### 21.1 单元测试优先级

优先为以下模块增加 PC 单元测试：

1. `components/pid`
2. `components/limiter`
3. `components/filters`
4. `components/crc`
5. `communication/can_transport`
6. 序号回绕、重复、跳帧和旧帧判断
7. `differential_drive`
8. `fault_manager` 状态转换
9. 参数范围和存储格式

### 21.2 板上测试

`tests/target/` 保存：

- 电机受控测试
- 编码器测试
- QSPI 保留区测试
- IWDG 受控复位测试
- RTC 备份测试
- CAN Bring-up 测试

危险测试默认 `DISABLED`，必须通过明确命令和确认词启动。

### 21.3 FreeRTOS 硬件回归

目录迁移后，必须重新验证：

- 上电四路 PWM 为 0
- 启动自检输出正常
- USART DMA 收发
- RTC 和掉电保持
- QSPI ID、DMA 和受控擦写
- LCD DMA、页面刷新和 KEY
- CAN FD 外部握手
- `0x100` 控制和 200 ms 超时
- 双编码器
- 双电机低速闭环
- 急停锁存
- IWDG 复位
- 长时间运行无控制节拍积压

当前迁移版 Debug 固件回归状态：**PASS（2026-07-25）**。已完成并确认：非零 `0x100` 闭环及运动后编码器反馈、PD2 急停锁存/释放/复位恢复、LCD 页面和 KEY 切页、RTC 掉电保持、Release 固件实物回归、长时间运行与 Jetson CAN 错误计数稳定性，以及重复帧、丢帧、跳帧、序号回绕和握手重建测试。

### 21.4 完整底盘功能验收

完成正式协议、运动学和安全模块后，执行以下端到端验收：

```text
上电进入 STANDBY
→ Jetson 查询状态
→ ARM
→ 前进
→ 后退
→ 原地左转
→ 原地右转
→ 定半径转弯
→ 返回编码器、轮速、状态和故障
→ 停发控制帧后自动停车
→ 急停触发并保持锁存
→ 清除故障并重新 ARM 后恢复
```

验收至少覆盖：

- `CHASSIS_MODE_TWIST` 正式控制
- `CHASSIS_MODE_WHEEL_SPEED` 受限调试
- 目标速度、加速度和 PWM 限制
- 双轮速度闭环
- CAN 命令超时
- 急停和严重故障锁存
- 编码器、轮速、电源、状态和故障反馈
- Jetson 根据原始编码器数据重新计算里程计

### 21.5 状态定义

继续使用：

- 已实现：代码接入并通过编译
- READY：等待外部设备或人工验收
- PASS：通过自动校验或真实硬件验收
- DISABLED：危险测试默认关闭

---

## 22. 分阶段实施路线

### 阶段 0：完成目录迁移

目标：只整理工程，不改变运行行为。

状态：**PASS（2026-07-25）**

必须完成：

- 更新 `.gitignore`
- 清理新路径 `.metadata/Debug/Release`
- 修正 `.cproject` Debug/Release 路径
- 注册所有实际源码目录
- 落地 `board/`、`rtos/` 和拆分后的 `config/`
- 统一 `ChassisApp_*` 应用生命周期 API
- `services` 改为 `infrastructure`
- Git 正确识别文件迁移
- Debug 和 Release 全量 clean build

验收：

- `0 errors, 0 warnings`
- 构建日志引用新目录
- 无 IDE 缓存进入 Git
- 不改变固件功能

### 阶段 1：FreeRTOS 基线硬件回归

目标：证明目录迁移后的单任务 FreeRTOS 固件保持裸机行为。

状态：**PASS（2026-07-25）**

验收：完成第 21.3 节全部项目。

在该阶段不拆分业务模块和任务。

### 阶段 2：拆分硬件、协议和调试边界

状态：**IN PROGRESS（2026-07-25）**

按小步顺序：

1. `fdcan_driver` 拆出 `bsp/fdcan`。（已完成）
2. 将握手、控制帧解析、序号状态和发送调度收敛到 `communication/can_transport`，删除转发壳。（已完成）
3. `board_self_test` 拆出 `bsp/uart` 和 `infrastructure/console`。（已完成）
4. 拆出 `infrastructure/telemetry`。（已完成）
5. `status_display` 迁移到 `infrastructure/`。（已完成）

每一步都必须保持当前控制帧和硬件行为不变。

### 阶段 3：拆分底盘业务模块

状态：**IN PROGRESS（2026-07-25）**

按顺序拆分：

1. `command_manager`（已完成）
2. `fault_manager`
3. `safety_manager`
4. `wheel_controller`
5. `differential_drive`
6. `odometry`
7. `parameter_manager`

优先拆出可测试的纯逻辑，再调整调用链。

### 阶段 4：建立独立控制任务

- TIM6 ISR 改为原生任务通知。
- 建立 100 Hz 高优先级 `control_task`。
- 将通信解析和低优先级诊断移出控制执行链。
- 建立任务健康和 Watchdog 汇总。

验收：

- 控制周期稳定
- 无长期通知积压
- 显示、串口和 QSPI 不影响控制
- 急停和故障停车保持立即有效

### 阶段 5：正式底盘协议和运动学

- 冻结 `TWIST` 控制帧
- 修正序号语义
- 增加状态、轮速、编码器和故障反馈
- 完成差速运动学
- 增加目标斜坡和限幅
- 与 Jetson ROS 2 Bridge 联调

### 阶段 6：参数和遥测

- `parameter_manager`
- `parameter_storage`
- VOFA+ 参数 pending/apply/save/reset
- 控制快照和遥测
- 参数 CRC 和双副本

### 阶段 7：IMU 和里程计

- IMU 原始数据
- 单调时间戳
- 基础滤波和掉线检测
- 编码器与 IMU 原始数据上报
- Jetson 侧里程计和融合

### 阶段 8：Bootloader 与 OTA

- 冻结 Flash Layout
- 独立 Bootloader 工程
- A/B Slot
- 镜像头、校验和签名
- Trial Boot、Confirm、Rollback
- OTA V1
- V2 再增加 Bootloader CAN FD Recovery

---

## 23. 当前阶段禁止事项

- 不在目录迁移时同时重写 PID。
- 不在目录迁移时同时重写 CAN 协议。
- 不一次拆完 `chassis_control.c`。
- 不一次创建大量空实现源码。
- 不为每个模块单独创建 FreeRTOS 任务。
- 不在 ISR 中执行复杂解析、日志、LCD 和 PID。
- 不让业务模块直接访问 CubeMX 全局句柄。
- 不让通信层直接控制电机。
- 不让显示和 QSPI 阻塞控制周期。
- 不使用 RTC 日期时间执行控制超时。
- 不复制其他芯片的 Bootloader Flash 地址。
- 不把 Bootloader 和 Application 合成一个 CubeMX 工程。
- 不把普通公共代码全部塞入 `firmware/shared/`。
- 不让 Jetson 直接下发 PWM。
- 不让 VOFA+ 每次修改参数就自动写 Flash。
- 不根据旧 Debug/Release 产物判断新目录构建成功。
- 不提交 `.metadata`、IDE 浏览器缓存和本地辅助上下文文件。

---

## 24. Codex 执行要求

Codex 修改仓库时必须：

1. 先读取本文档和当前代码。
2. 检查 Git 工作区，避免覆盖未提交修改。
3. 每次只完成一个清晰目标。
4. 重构前后保持硬件行为一致。
5. 移动文件后同步更新 `.cproject`、include path、source folder、README 和 `.gitignore`。
6. 不修改 CubeMX 自动生成区域，除非接口接入确实需要。
7. 修改生成区域时必须位于 `USER CODE` 区或说明原因。
8. 新模块必须说明所属层、职责和依赖。
9. 不创建假的空 `.c/.h`；空目录只使用 `.gitkeep`。
10. 构建后说明使用的配置、错误数和警告数。
11. 未做硬件验证时明确标记为未验证。
12. 协议变更必须同步更新 `protocol/` 文档。
13. 安全行为变化必须单独说明并安排真实硬件回归。
14. 不在一个提交中同时进行大规模目录迁移、协议重写、PID 修改、RTOS 任务拆分和 OTA 实现。

---

## 25. 当前最终决定摘要

```text
正式 Application 路径：
firmware/application/stm32g474/

自定义硬件驱动：
bsp/

硬件映射与软件配置：
board/ 与 config/ 独立

通用算法：
components/

通信传输和协议：
communication/

底盘核心业务：
modules/

调试、遥测、存储、显示和时间同步：
infrastructure/

调度：
rtos/

系统组装：
app/

当前 FreeRTOS：
单静态应用任务 + TIM6 10 ms 节拍

下一阶段 FreeRTOS：
独立高优先级 control_task + 通信/诊断边界

Application 与 Bootloader：
两个独立 CubeMX 工程

firmware/shared/：
只保存 Flash、镜像、Metadata 和 OTA 稳定契约

版本：
Application 与 Bootloader 分别使用 config/build_info.h

ROS 2：
Bridge、融合和高层里程计在 Jetson

PID 调参：
Windows + USB 串口 + VOFA+
参数默认只修改 RAM，确认后保存

OTA V1：
Application 接收镜像，Bootloader 校验、切换、确认和回滚

OTA V2：
以后增加 Bootloader CAN FD Recovery
```

当前第一优先级：

> 烧录 `firmware/application/stm32g474/` 目录迁移后的固件并完成 FreeRTOS
> 基线硬件回归。在此之前，不继续进行大规模业务模块拆分。
