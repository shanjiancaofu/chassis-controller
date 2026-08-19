# 系统架构

本文只维护软件边界、目录职责、依赖方向和运行模型。当前版本与下一步见
[`current_status.md`](current_status.md)，构建和实物结果见 [`verification.md`](verification.md)。

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
│  │  ├─ ota_protocol.h
│  │  └─ qspi_flash_identity.h
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
│  │     │  └─ imu/                  # ICM45686 SPI3/DMA 板级适配
│  │     │
│  │     ├─ components/              # 不依赖 HAL 的通用算法组件
│  │     │  ├─ pid/
│  │     │  ├─ ring_buffer/
│  │     │  ├─ icm45686/
│  │     │  ├─ imu_fusion/
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
│  │     │  │  ├─ system_status.c
│  │     │  │  └─ system_status.h
│  │     │  ├─ parameters/
│  │     │  │  ├─ parameter_manager.c
│  │     │  │  └─ parameter_manager.h
│  │     │  ├─ safety/
│  │     │  │  ├─ safety_manager.c
│  │     │  │  ├─ safety_manager.h
│  │     │  │  ├─ fault_manager.c
│  │     │  │  └─ fault_manager.h
│  │     │  └─ imu/                  # 后续姿态业务封装
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
│        │  ├─ boot_installer.c
│        │  ├─ boot_metadata_io.c
│        │  ├─ boot_trace.c
│        │  ├─ ota_metadata_store.c
│        │  ├─ image_validator.c
│        │  └─ app_launcher.c
│        ├─ bsp/
│        │  ├─ flash/
│        │  └─ qspi/
│        ├─ components/
│        │  └─ crc/
│        ├─ config/
│        │  └─ build_info.h
│        └─ tests/
│           └─ unit/
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

只保存 Bootloader、Application 和打包工具必须一致的固定 ABI/硬件契约，包括 Flash
布局、镜像头、OTA 元数据、OTA 帧格式和 W25Q64 JEDEC 身份。不得放驱动、状态机或
可变版本值。

### `app`

负责初始化顺序、模块装配和跨模块流程。`chassis_app.c` 不实现寄存器驱动或通用算法。

### `board`

保存当前开发板的引脚、外设句柄和板级映射。它是 CubeMX 生成层与手写 BSP 之间的适配层。

### `bsp`

封装一种硬件如何操作，包括电机、编码器、FDCAN、UART、LCD、QSPI、ADC 和复位辅助。
BSP 不决定是否允许车辆运动，也不持有业务状态机。

### `components`

保存可独立测试、无 HAL 依赖的组件。当前包括速度 PID、Application CRC32、ICM45686
寄存器/FIFO 协议组件，以及 `imu_fusion` 静止零偏、六轴 Mahony 和独立 roll/pitch
角度+陀螺零偏两状态 Kalman。Mahony 与 Kalman 当前并行输出用于诊断对照，不绑定控制或
安全逻辑；组件不直接持有 SPI、DMA 或 CubeMX 句柄。

### `communication`

处理总线帧、字段校验、序号、握手和链路状态。当前 `ota_transport/` 已包含
UART/CAN FD 收发适配、统一 OTA 会话、QSPI 分块写入、元数据提交和 Application
试运行确认状态机。线协议以
`protocol/canfd_protocol.md` 和 `protocol/ota_canfd_protocol.md` 为准。

### `infrastructure`

提供 Console、诊断文本、遥测和 LCD 状态页。这些能力不得进入实时控制任务。

### `modules`

按 `chassis`、`safety`、`parameters`、`diagnostics` 业务域聚合相关状态和规则。
域内文件职责明确，域之间通过接口协作，不直接操作 CubeMX 句柄。IMU 当前仍属于
BSP/组件能力，姿态结果进入诊断快照；需要上层业务消费时再建立独立业务域。

`modules/diagnostics/system_status` 保存 Application task 组装的统一状态快照。它不读取
硬件或 FreeRTOS 全局对象；`app` 负责更新，Console/LCD/后续 UART 协议只读取快照，避免
各显示和输出路径重复采集同一状态。

### `rtos`

只负责任务创建、优先级、通知和任务健康。业务逻辑仍由 `app` 和 `modules` 提供。

### `config`

保存 Application 的功能开关、控制参数、协议参数和构建版本。硬件引脚属于 `board`，
可运行时修改并持久化的参数由 `parameter_manager` 和 `parameter_storage` 管理。

### `tests/target`

保存必须人工确认的 QSPI、IWDG 和电机板上测试。测试不得成为正常运行路径。

### `firmware/bootloader`

Bootloader 是独立 CubeMX/CubeIDE 工程，拥有独立入口、向量表、链接脚本和版本。
它只负责镜像校验、安装、试运行计数、回滚和 Application 跳转，不运行底盘业务；
Application 在最小健康窗口通过后提交确认元数据。当前 `boot_trace.c` 直接配置
USART1 轮询输出启动诊断，不依赖 CubeMX UART 初始化。

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
- FreeRTOS 入口使用 `RtosApp_<TaskName>TaskMain()`；任务每次调度调用的 Application 周期函数
  使用 `ChassisApp_Run<TaskName>Cycle()`，不使用含义不清的统一 `Run()`。
- `GetSnapshot` 只读并复制状态，`Update`/`Set` 发布或修改状态，`Process`/`Handle` 推进流程，
  `Send` 表示 UART/CAN 输出；函数名应包含对象和动作，避免 `Prepare`、`Run` 等无法说明副作用的
  泛化名称。
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

当前代码已按四个职责明确的任务运行。四个任务覆盖实时控制、通信服务、诊断聚合和显示刷新，
不按硬件名称机械增加任务。

| 任务 | 周期/触发 | 优先级 | 超时/健康 | 作用 |
| --- | --- | --- | --- | --- |
| `control_task` | 100 Hz，TIM6 通知 | 最高业务优先级 | 50 ms 健康超时；单周期目标 10 ms | 编码器、轮速控制、安全停车和 PWM |
| `service_task` | 事件驱动，空闲时 1 ms 轮询 | 中高 | 200 ms 通信超时由协议定义 | UART/CAN、Console、OTA 会话和命令入口 |
| `diagnostics_task` | 100 ms | 普通 | 500 ms 诊断刷新超时 | 更新 `SystemStatusSnapshot`、复位原因、任务统计和遥测快照 |
| `display_task` | 1 ms DMA 推进循环，LCD 1 s 刷新 | 较低 | 2 s 显示刷新超时 | LCD 页面切换和显示快照，不读取硬件拼接状态 |

CubeMX 的 `defaultTask` 在 USER CODE 中调整为 `service_task`；其余三个任务由手写 RTOS 层
静态创建。任务迁移保持 OTA、控制和零 PWM 语义不变。

任务按实时性和阻塞边界划分，不按硬件名称一一建任务。电源和 ADC 属于采集职责：ADC 可由
硬件触发/DMA 完成转换，`diagnostics_task` 周期性消费结果并写入快照；不单独创建
`power_task` 或 `adc_task`。底盘控制只归 `control_task`，通信和 OTA 只归 `service_task`，
LCD 只归 `display_task`，复位原因、任务健康和统一状态发布只归 `diagnostics_task`。这样可
避免六个低频任务互相唤醒、争用 UART/LCD 或把实时控制路径与文本输出混在一起。

RTC 采用同样的边界：启动阶段由初始化流程配置 RTC 和备份寄存器；正常运行时由
`diagnostics_task` 周期读取日期/时间并写入 `SystemStatusSnapshot`。`display_task` 只显示快照，
`service_task` 若将来提供校时命令，只负责协议解析并通过受控接口提交请求，不直接操作 RTC
句柄；`ts_ms` 协议时间戳仍使用单调 uptime，不依赖 RTC。若启用 RTC Alarm/Wakeup，中断只发
通知，实际读取和业务处理仍在对应任务中完成。

多传感器融合使用另一条单调时间轴，不使用 RTC 日历时间。IMU 样本应保留 FIFO/设备时间戳并
映射到本地单调时间；编码器速度在控制采样点记录时间；ADC 使用触发或 DMA 完成时间；SR501
只在稳定状态被接受时记录事件时间。任务开始处理数据的时间不能替代样本采集时间。当前诊断
和 SR501 展示使用毫秒级 uptime 已足够；IMU 与轮式里程计融合前，需要完成统一时间戳和延迟
预算验证，目标是在 100 Hz 控制周期内达到约 1 ms 级对齐，不引入 PTP 等复杂网络校时。

TIM6 ISR 只发送任务通知。通知积压时不补跑历史 PID；控制任务记录 missed tick，
连续超限后进入内部故障。急停 ISR 直接清零硬件 PWM，不等待任务调度。
只有出现明确的阻塞隔离、优先级或周期需求时才新增任务，不按模块数量机械拆任务。

诊断快照至少包含每个任务的栈余量、实际周期、期望周期、健康超时、运行次数、运行状态、
heartbeat age 和 uptime，并记录 RCC 复位原因。快照由诊断职责统一发布；UART 和 LCD 只读
快照，不各自调用 BSP 拼接状态。快照更新不在 ISR 或 `control_task` 中执行；实时控制仍只
消费控制模块接口。

## 控制与安全

控制所有权来源当前包括：

- `CAN_REMOTE`
- `CONSOLE`
- `TARGET_TEST`
- `OTA`

前三项可以提交运动目标；`OTA` 只能持有停车维护锁，不能提交运动目标。任一时刻只有
一个 owner。清除运动命令不等于释放 owner，只有持有者的明确结束流程才能释放自身
owner。切换或释放运动控制源时必须清零目标并停车。CAN 超时、
error-passive、bus-off、急停、内部故障或控制节拍持续积压都会撤销当前控制并清零 PWM。
OTA 是停车后的维护锁，不是运动控制源；持有期间普通控制和危险目标测试均不得启动。

IWDG 只有在 Application task、`control_task`、FDCAN 和关键故障状态均健康时才刷新。
