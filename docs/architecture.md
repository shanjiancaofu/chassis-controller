# 系统架构

## 目标

`chassis-controller` 是 STM32G474VET6 底盘实时控制固件。它通过 CAN FD 接收
Jetson 的运动命令，在 STM32 内完成双轮控制、安全保护和状态采集。

STM32 负责：

- 100 Hz 双轮速度控制和编码器采集
- 电机 PWM、方向切换和零输出
- 急停、命令超时、故障锁存和看门狗
- ADC、RTC、QSPI、LCD、按键和板级诊断
- CAN FD 控制、状态和诊断
- Application OTA 接收与 Bootloader 安装

Jetson 负责：

- 上层决策和运动目标
- ROS 2 Bridge、融合与复杂运动学
- 诊断展示、参数配置和 OTA 文件发送

Jetson 不直接下发 PWM。底层闭环和最终安全停车保留在 STM32。

## 最终目录结构

```text
chassis-controller/
├─ firmware/
│  ├─ shared/                         # Bootloader 与 Application 共用契约
│  │  ├─ flash_layout.h
│  │  ├─ firmware_image.h
│  │  ├─ ota_metadata.h
│  │  └─ ota_protocol.h
│  │
│  ├─ application/
│  │  └─ stm32g474/                  # 正式底盘 Application，独立 CubeMX 工程
│  │     ├─ Core/                    # CubeMX 生成
│  │     ├─ Drivers/                 # STM32 HAL、CMSIS
│  │     ├─ Middlewares/             # CubeMX 第三方中间件，如 FreeRTOS
│  │     ├─ Application/User/        # CubeMX/CubeIDE 工程入口
│  │     ├─ chassis_controller.ioc
│  │     │
│  │     ├─ board/                   # 当前硬件板资源映射
│  │     │  └─ board_config.h
│  │     │
│  │     ├─ bsp/                     # 板级外设和具体设备驱动
│  │     │  ├─ encoder/
│  │     │  ├─ fdcan/
│  │     │  ├─ lcd/
│  │     │  │  └─ assets/            # LCD 图片和取模素材
│  │     │  ├─ motor/
│  │     │  ├─ power_monitor/
│  │     │  ├─ qspi/
│  │     │  ├─ reset/
│  │     │  ├─ uart/
│  │     │  └─ imu/                  # 硬件确认后再创建
│  │     │
│  │     ├─ components/              # 不依赖 HAL 的通用算法组件
│  │     │  ├─ pid/
│  │     │  ├─ ring_buffer/
│  │     │  └─ crc/
│  │     │
│  │     ├─ communication/           # 通信协议、编解码和传输
│  │     │  ├─ can_transport/
│  │     │  ├─ chassis_protocol/
│  │     │  └─ ota_transport/
│  │     │
│  │     ├─ infrastructure/          # 非实时运行基础设施
│  │     │  ├─ console/
│  │     │  ├─ telemetry/
│  │     │  ├─ status_display/
│  │     │  └─ parameter_storage/    # 参数持久化落地时创建
│  │     │
│  │     ├─ modules/                 # 底盘产品业务模块
│  │     │  ├─ chassis/
│  │     │  │  ├─ command_manager.c
│  │     │  │  ├─ command_manager.h
│  │     │  │  ├─ differential_drive.c
│  │     │  │  ├─ differential_drive.h
│  │     │  │  ├─ wheel_controller.c
│  │     │  │  ├─ wheel_controller.h
│  │     │  │  ├─ odometry.c
│  │     │  │  └─ odometry.h
│  │     │  ├─ diagnostics/
│  │     │  │  ├─ board_health.c
│  │     │  │  └─ board_health.h
│  │     │  ├─ parameters/
│  │     │  │  ├─ parameter_manager.c
│  │     │  │  └─ parameter_manager.h
│  │     │  ├─ safety/
│  │     │  │  ├─ safety_manager.c
│  │     │  │  ├─ safety_manager.h
│  │     │  │  ├─ fault_manager.c
│  │     │  │  └─ fault_manager.h
│  │     │  └─ imu/                  # IMU 阶段再创建
│  │     │
│  │     ├─ rtos/                    # FreeRTOS 对象、任务和 hooks
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
│  │     ├─ app/                     # 初始化、组装和调度
│  │     │  ├─ chassis_app.c
│  │     │  └─ chassis_app.h
│  │     │
│  │     ├─ config/                  # Application 软件配置和版本信息
│  │     │  ├─ app_config.h
│  │     │  ├─ control_config.h
│  │     │  ├─ feature_config.h
│  │     │  ├─ protocol_config.h
│  │     │  ├─ storage_layout.h
│  │     │  ├─ target_test_config.h
│  │     │  └─ build_info.h
│  │     │
│  │     └─ tests/
│  │        ├─ unit/                 # PC 单元测试
│  │        └─ target/               # STM32 板上测试
│  │
│  └─ bootloader/
│     └─ stm32g474/                  # 独立 CubeMX 工程
│        ├─ Core/
│        ├─ Drivers/
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
│  ├─ ota_canfd_protocol.md
│  └─ schema/
├─ docs/
│  ├─ architecture/
│  ├─ requirements/
│  ├─ safety/
│  ├─ bringup/
│  ├─ verification/
│  └─ bootloader/
└─ .github/
   └─ workflows/
```

这是最终目标，不代表所有目录现在已经存在。目录随对应代码落地，禁止只创建空壳。
当前结构与目标结构的差距和迁移顺序记录在 `roadmap.md`。

## 各层职责

### CubeMX/CubeIDE 目录

`Core/`、`Drivers/`、`Middlewares/` 和 `Application/User/` 由 CubeMX/CubeIDE 管理。
自定义业务代码不得长期堆入这些目录；生成文件只在 `USER CODE BEGIN/END` 区域修改。

### `firmware/shared`

只保存 Bootloader、Application 和打包工具必须一致的固定 ABI，包括 Flash 布局、
镜像头、OTA 元数据和 OTA 帧格式。不得放驱动、状态机或可变版本值。

### `app`

负责初始化顺序、模块装配和跨模块流程。`chassis_app.c` 不实现寄存器驱动或通用算法。

### `board`

保存当前开发板的引脚、外设句柄和板级映射。它是 CubeMX 生成层与手写 BSP 之间的适配层。

### `bsp`

封装一种硬件如何操作，包括电机、编码器、FDCAN、UART、LCD、QSPI、ADC 和复位原因。
BSP 不决定是否允许车辆运动，也不持有业务状态机。

### `components`

保存可独立测试、无 HAL 依赖的算法。当前只有速度 PID。

### `communication`

处理总线帧、字段校验、序号、握手和链路状态。线协议以
`protocol/canfd_protocol.md` 为准。

### `infrastructure`

提供 Console、诊断文本、遥测和 LCD 状态页。这些能力不得进入实时控制任务。

### `modules`

按 `chassis`、`safety`、`parameters`、`diagnostics` 业务域聚合相关状态和规则。
域内文件职责明确，域之间通过接口协作，不直接操作 CubeMX 句柄。IMU 在硬件确认后
建立独立业务域。

### `rtos`

只负责任务创建、优先级、通知和任务健康。业务逻辑仍由 `app` 和 `modules` 提供。

### `config`

保存 Application 的功能开关、控制参数、协议参数和构建版本。硬件引脚属于 `board`，
可运行时修改并持久化的参数由 `parameter_manager` 和 `parameter_storage` 管理。

### `tests/target`

保存必须人工确认的 QSPI、IWDG 和电机板上测试。测试不得成为正常运行路径。

### `firmware/bootloader`

Bootloader 是独立 CubeMX/CubeIDE 工程，拥有独立入口、向量表、链接脚本和版本。
它只负责镜像校验、安装、试运行确认、回滚和 Application 跳转，不运行底盘业务。

### `protocol` 与 `docs`

`protocol` 保存线上兼容契约，`docs` 保存架构、路线、硬件、验证和升级设计。
当前文档按唯一职责平铺；同类文档形成实际集合后，再迁入最终分类目录。

`protocol/` 保持仓库顶层，因为 CAN FD 和 OTA 传输协议同时约束 STM32、
Jetson 和主机工具，不属于某个固件工程。LCD 图片素材跟随使用者放在
`firmware/application/stm32g474/bsp/lcd/assets/`，不再放仓库顶层。

## 命名规则

- CubeMX 已使用 `Drivers/` 和 `Middlewares/`，自定义层不再使用同名小写目录。
- 自定义代码统一使用 `board/`、`bsp/`、`components/`、`communication/`、
  `infrastructure/`、`modules/`、`rtos/`、`app/` 和 `config/`。
- 文件名使用 `snake_case`，目录按业务或设备命名。
- 不按文件行数拆层；只有职责、复用或依赖边界成立时才拆分。

## 依赖方向

```text
app / rtos
  -> modules / infrastructure / communication
  -> components / bsp
  -> board
  -> CubeMX HAL
```

约束：

- `components` 不依赖 HAL、业务模块或 Console。
- `bsp` 不依赖 `modules`。
- `modules` 不直接操作 CubeMX 全局句柄。
- `infrastructure` 可以读取模块快照，但不能决定电机安全状态。
- Bootloader 与 Application 只共享固定 OTA 数据格式，不共享业务代码。

## 运行模型

当前运行模型只建立两个职责明确的任务：

| 任务 | 作用 |
| --- | --- |
| `control_task` | 最高业务优先级，100 Hz 轮速控制 |
| Application task | Console、通信、诊断、遥测、LCD 和非实时流程 |

TIM6 ISR 只发送任务通知。通知积压时不补跑历史 PID；控制任务记录 missed tick，
连续超限后进入内部故障。急停 ISR 直接清零硬件 PWM，不等待任务调度。
只有出现明确的阻塞隔离、优先级或周期需求时才新增任务，不按模块数量机械拆任务。

## 控制与安全

运动控制源当前包括：

- `CAN_REMOTE`
- `CONSOLE`
- `TARGET_TEST`

任一时刻只有一个控制源。切换或释放控制源时必须清零目标并停车。CAN 超时、
error-passive、bus-off、急停、内部故障或控制节拍持续积压都会撤销当前控制并清零 PWM。

IWDG 只有在 Application task、`control_task`、FDCAN 和关键故障状态均健康时才刷新。
