# Chassis Controller 架构、目录结构与后续开发约定

> 建议保存路径：`docs/architecture/chassis_controller_architecture_and_roadmap.md`

## 1. 文档目的

本文档用于固定 `chassis-controller` 仓库后续的目录结构、模块职责、依赖方向和演进路线，供 Codex 后续读取并按约定修改代码。

Codex 修改仓库前应先读取：

1. 本文档
2. 当前仓库 README
3. 裸机阶段进度文档
4. CAN FD 协议文档
5. 当前源码和 CubeMX 工程配置

---

## 2. 项目定位

`chassis-controller` 的目标是：

> 基于 STM32G474 的安全差速底盘控制器，通过 CAN FD 与 Jetson 通信，完成双轮速度闭环、编码器采集、运动学、安全保护、状态反馈、参数管理，并逐步加入 IMU、FreeRTOS、Bootloader 和 OTA。

当前仓库仍处于裸机验证阶段。已有代码主要包括板级外设验证、电机开环、编码器、ADC、LCD、QSPI、FDCAN 自检和初步 PID、安全停车逻辑。

距离完整底盘 Demo 仍缺少：

- 外部 CAN FD 正式链路
- 正式控制协议
- Jetson 下发线速度、角速度
- 差速运动学
- 双轮真实速度闭环
- 里程计或完整原始反馈链路
- 底盘状态机
- 故障管理
- 参数系统
- 可重复验收流程

---

## 3. 仓库职责范围

本仓库只维护：

- STM32G474 Application 固件
- STM32G474 Bootloader
- Bootloader 与 Application 共用契约
- CAN FD 协议
- OTA 协议
- 固件架构、安全、Bring-up 和验证文档
- 固件构建、格式检查和单元测试 CI

以下内容不放在本仓库：

- Jetson ROS 2 Bridge
- Jetson 上位机程序
- 独立 CAN 监控工具
- 独立 PID GUI 调参工具
- 独立固件升级工具
- 其他 PC/Jetson 应用

本仓库只保留 STM32 侧为这些工具提供的协议和功能接口。

---

## 4. 最终目标目录结构

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
│  │     │  ├─ board_config.h
│  │     │  ├─ board_init.c
│  │     │  └─ board_pins.h
│  │     │
│  │     ├─ bsp/                     # 板级外设和具体设备驱动
│  │     │  ├─ motor/
│  │     │  ├─ encoder/
│  │     │  ├─ power_monitor/
│  │     │  ├─ fdcan/
│  │     │  ├─ uart/
│  │     │  ├─ lcd/
│  │     │  ├─ qspi/
│  │     │  └─ imu/
│  │     │
│  │     ├─ components/              # 不依赖 HAL 的通用算法组件
│  │     │  ├─ pid/
│  │     │  ├─ limiter/
│  │     │  ├─ filters/
│  │     │  ├─ ring_buffer/
│  │     │  └─ crc/
│  │     │
│  │     ├─ communication/           # 通信协议、编解码和传输
│  │     │  ├─ can_transport/
│  │     │  ├─ chassis_protocol/
│  │     │  └─ ota_transport/
│  │     │
│  │     ├─ services/                # 通用运行服务
│  │     │  ├─ console/
│  │     │  ├─ telemetry/
│  │     │  ├─ parameter_storage/
│  │     │  └─ status_display/
│  │     │
│  │     ├─ modules/                 # 底盘产品业务模块
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
│  │     ├─ rtos/                    # 引入 FreeRTOS 后创建
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

不要一次创建所有空目录。目录在对应功能开始实现时再建立。

---

## 5. 目录命名约定

CubeMX 已经存在：

```text
Drivers/
Middlewares/
```

因此自定义代码不再使用：

```text
drivers/
middleware/
```

统一使用：

```text
bsp/
communication/
services/
```

避免 Windows 下大小写、复数和语义混淆。

---

## 6. 各层职责

### 6.1 CubeMX 目录

`Core/`、`Drivers/`、`Middlewares/`、`Application/User/` 由 CubeMX/CubeIDE 管理。

规则：

- 不随意移动
- 不长期堆放自定义业务代码
- 不随意修改自动生成区域
- CubeMX 重新生成后检查自定义 include path 和 source folder

`Middlewares/` 仅表示 CubeMX 第三方中间件，例如 FreeRTOS。

### 6.2 `board/`

描述当前硬件板如何连接和使用 MCU 资源，包括：

- 引脚映射
- 定时器和 PWM 通道
- 编码器定时器
- FDCAN、UART 等外设实例
- 电机方向
- 编码器方向
- 硬件版本差异

示例：

```c
#define BOARD_LEFT_MOTOR_TIMER       htim8
#define BOARD_RIGHT_MOTOR_TIMER      htim8
#define BOARD_LEFT_ENCODER_TIMER     htim2
#define BOARD_RIGHT_ENCODER_TIMER    htim4
#define BOARD_CAN_HANDLE             hfdcan2
```

`board/` 不实现 PID、运动学、里程计、协议解析和状态机。

### 6.3 `bsp/`

封装具体硬件操作：

- 电机 PWM 和方向
- 编码器计数器
- ADC 电压采样
- FDCAN 底层收发
- UART DMA
- LCD SPI/DMA
- QSPI Flash
- IMU 寄存器读写

BSP 只回答“这个硬件怎么操作”。

BSP 不决定：

- 是否允许电机运行
- 是否处于 ACTIVE
- 是否 CAN 超时
- 是否急停
- PID 参数
- LCD 业务页面内容

### 6.4 `components/`

放不依赖 HAL、可在 PC 上测试的通用组件：

- PID
- 限幅器
- 滤波器
- Ring Buffer
- CRC

要求：

- 不依赖 STM32 HAL
- 不依赖底盘业务模块
- 输入输出明确
- 可以独立单元测试

### 6.5 `communication/`

负责协议和传输。

`can_transport/`：

- CAN RX/TX 队列
- 发送调度
- 超时
- 分包和重组
- 重传
- 收发统计

`chassis_protocol/`：

- CAN ID
- 控制命令编解码
- 状态、编码器、轮速、IMU、故障帧编解码
- 参数调节命令编解码
- 协议版本和范围检查

`ota_transport/`：

- OTA QUERY/BEGIN/DATA/END/ABORT/RESULT
- 数据块序号
- 偏移
- CRC
- 重传
- 写入非活动 Slot

### 6.6 `services/`

`console/`：串口命令、VOFA+ 命令、状态查询和测试入口。

`telemetry/`：周期遥测、日志、VOFA+ FireWater 数据、控制周期统计。

`parameter_storage/`：参数持久化、CRC、双副本和恢复默认值，不理解 PID、轮径等业务含义。

`status_display/`：LCD 页面组织和状态显示；LCD 底层绘图仍在 `bsp/lcd/`。

### 6.7 `modules/`

`command_manager/`：最新命令、序号、时间戳、超时、ARM、DISARM、ESTOP。

`differential_drive/`：线速度/角速度与左右轮速度互相转换，管理轮径、轮距和方向参数。

`wheel_controller/`：左右轮目标速度、测量速度、PID、限幅和 PWM。

`odometry/`：基础里程计；Jetson 也可根据 STM32 原始编码器数据重新计算并融合。

`imu_manager/`：IMU 初始化、原始数据、时间戳、零偏、基础滤波、掉线检测。

`safety_manager/`：状态机、电机许可、命令超时、急停、欠压和调参安全限制。

`fault_manager/`：故障位、故障等级、锁存、清除条件和故障历史。

`parameter_manager/`：PID、限幅、轮径、轮距、编码器和校准参数。

`diagnostics/`：外设在线状态、编码器异常、CAN 错误、IMU、电压、控制周期和任务健康状态。

建议底盘状态：

```text
BOOT
  ↓
SELF_TEST
  ↓
STANDBY
  ↓ ARM
ARMED
  ↓ 有效运动命令
ACTIVE

任意状态 → ESTOP
严重故障 → FAULT
```

只有 `ACTIVE` 或受限的 `TUNING_ACTIVE` 允许非零 PWM。

### 6.8 `rtos/`

FreeRTOS 引入后再创建。

FreeRTOS 只是调度层，不是业务层。任务文件只负责等待、读取队列、调用模块和发布结果。

建议任务：

```text
control_task          100 Hz，高优先级
communication_task    事件驱动
diagnostics_task      低频
console_task          事件驱动
display_task          低频
storage_task          按需运行
```

不要一个模块对应一个任务。

### 6.9 `app/`

负责系统初始化、模块组装、裸机调度和 RTOS 任务创建，不实现 PID、协议解析和复杂业务。

### 6.10 `config/`

独立保留。

```text
board/
硬件板怎么连接

config/
软件产品怎么运行
```

建议：

```text
config/
├─ app_config.h
├─ control_config.h
├─ feature_config.h
├─ protocol_config.h
└─ build_info.h
```

- `app_config.h`：应用周期和启动策略
- `control_config.h`：控制周期、默认速度、加速度和 PWM 限制
- `feature_config.h`：LCD、IMU、FreeRTOS、遥测等功能开关
- `protocol_config.h`：CAN 超时、心跳、上报和 OTA 超时
- `build_info.h`：当前固件版本、Git Commit 和构建信息

---

## 7. 依赖方向

```text
app / rtos
    ↓
modules
    ↓
communication / services / components
    ↓
bsp
    ↓
board / CubeMX HAL
```

约束：

1. `bsp` 不包含 `modules` 头文件。
2. `components` 不依赖 HAL。
3. `communication` 不直接控制电机。
4. `services` 不决定底盘安全状态。
5. `app` 只组装和调度。
6. `rtos/tasks` 不保存核心业务。
7. FDCAN ISR 不执行 PID。
8. FDCAN ISR 不做复杂协议解析。
9. FDCAN ISR 不打印日志。
10. LCD 和 QSPI 不能阻塞控制循环。

---

## 8. 裸机正式 Demo 范围

裸机阶段必须先完成：

- 外部 CAN FD
- Jetson 下发线速度、角速度
- 正式 CAN FD 协议
- 差速运动学
- 左右轮速度闭环
- 加减速限制
- 编码器采集
- 状态机
- 命令超时停车
- 急停锁存
- 故障管理
- 状态、编码器、轮速反馈
- 基础里程计或完整原始数据反馈
- VOFA+ PID 调参

验收流程：

```text
上电进入 STANDBY
→ Jetson 查询状态
→ ARM
→ 前进
→ 后退
→ 原地左转
→ 原地右转
→ 定半径转弯
→ 返回编码器和轮速
→ 拔掉 CAN 自动停车
→ 急停锁存
→ 清除故障后恢复
```

完成后再引入 FreeRTOS。

---

## 9. ROS 2 Bridge 边界

ROS 2 Bridge 运行在 Jetson，不运行在 STM32。

### STM32 负责

- 编码器采集
- 电机 PWM
- 左右轮轮速计算
- 双轮 PID
- 差速运动学
- 命令超时停车
- 急停和故障保护
- 电压、设备状态
- IMU 原始数据和时间戳
- 编码器回绕处理
- 原始计数和周期增量

### Jetson 负责

- ROS 2 `/cmd_vel`
- CAN FD Bridge
- 编码器里程计积分
- IMU 坐标轴变换、标定和滤波
- 轮速与 IMU 融合
- `robot_localization`
- `/odom`
- `/imu/data_raw`
- `/joint_states`
- `/diagnostics`
- TF：`odom -> base_link`

STM32 建议发送：

```text
左右编码器累计计数
左右编码器周期增量
左右轮测量速度
采样时间戳
IMU ax ay az
IMU gx gy gz
IMU 温度
电压
底盘状态
故障位
控制周期统计
```

正式运行建议 Jetson 下发 `linear + angular`，STM32 完成差速解算和轮速闭环。

协议可同时支持：

```text
CHASSIS_MODE_TWIST
CHASSIS_MODE_WHEEL_SPEED
```

`WHEEL_SPEED` 主要用于调试和标定。

禁止 Jetson 高频计算 PWM 后让 STM32 直接输出。底层闭环和安全必须留在 STM32。

---

## 10. PID 调参方案：VOFA+ + 串口

第一版使用：

> Windows 电脑 + USB 串口 + VOFA+

不开发专用 PID GUI。

```text
VOFA+
  ↓ USB 串口
STM32 USART DMA
  ↓
services/console
modules/parameter_manager
modules/wheel_controller
```

### 10.1 VOFA+ 数据协议

第一版使用 FireWater 文本协议。

STM32 输出：

```text
pid:时间,目标速度,实际速度,误差,P项,I项,D项,PID输出,PWM
```

例如：

```text
pid:15320,120.0,103.5,16.5,3.30,0.42,0.00,3.72,380
```

建议：

```text
波特率：115200 或 460800
遥测频率：50 Hz
每次只调一侧电机
默认关闭遥测
需要时 telemetry on
```

### 10.2 VOFA+ 命令

```text
pid get
pid get left
pid get right

pid set left kp 0.20
pid set left ki 0.01
pid set left kd 0.00

pid set right kp 0.20
pid set right ki 0.01
pid set right kd 0.00

pid apply
pid save
pid reset

tune start left 100
tune start right 100
tune stop

telemetry on
telemetry off
```

含义：

```text
pid set
修改 RAM 中 pending 参数

pid apply
在控制周期边界应用

pid save
确认稳定后持久化

pid reset
恢复默认参数
```

### 10.3 STM32 端模块

```text
components/pid/
modules/parameter_manager/
services/parameter_storage/
services/console/
services/telemetry/
modules/wheel_controller/
```

PID 组件应支持运行时更新和状态重置：

```c
bool PidController_SetConfig(PidController *pid,
                             const PidConfig *config);

void PidController_Reset(PidController *pid);
```

应用新参数时清空积分、上次误差和微分历史，避免旧状态导致 PWM 跳变。

`parameter_manager` 负责查询、范围检查、pending 参数、控制周期边界应用和恢复默认值。

`parameter_storage` 负责 QSPI/Internal Flash、格式版本、CRC、双副本和写入失败保护。

调参过程中默认只修改 RAM。只有收到 `pid save` 才写 Flash/QSPI。

`telemetry` 输出：

- timestamp
- wheel
- target_speed
- measured_speed
- error
- p_term
- i_term
- d_term
- output
- pwm
- encoder_delta

### 10.4 调参安全

调参模式必须：

- 车轮架空或车辆固定
- 限制最大目标速度
- 限制最大 PWM
- 急停始终有效
- 串口命令超时停车
- 严重故障禁止启动
- 退出调参模式立即清零目标
- 参数先应用到 RAM
- 确认稳定后再保存

建议状态：

```text
TUNING_DISABLED
TUNING_READY
TUNING_ACTIVE
TUNING_ERROR
```

---

## 11. FreeRTOS 迁移原则

FreeRTOS 放在完整裸机 Demo 之后。

原则：

> 只改变调度方式，不改变底盘行为。

控制任务建议顺序：

```text
读取编码器
→ 更新命令
→ 安全检查
→ 差速解算
→ 目标限幅
→ PID
→ 更新 PWM
→ 更新状态快照
```

中断只做读数据、清标志、写队列和发通知。

---

## 12. IMU 路线

IMU 不阻塞第一个裸机 Demo。

第一阶段只做：

- 设备识别
- 原始加速度
- 原始角速度
- 温度
- 时间戳
- 零偏标定
- 基础低通
- 掉线检测
- CAN 上报

STM32 不优先实现复杂 EKF。Jetson/ROS 2 负责坐标变换、姿态估计、轮速与 IMU 融合。

---

## 13. Bootloader 与 OTA

Application 和 Bootloader 使用两个独立 CubeMX 工程，因为它们有独立启动入口、向量表、链接脚本、Flash 地址、外设配置、编译产物和版本周期。

Bootloader 应尽可能小。

### 13.1 `firmware/shared/`

```text
firmware/shared/
├─ flash_layout.h
├─ firmware_image.h
├─ ota_metadata.h
└─ ota_protocol.h
```

`flash_layout.h`：Bootloader、Metadata、Application Slot A/B 的地址和大小。不能复制其他 STM32 型号地址。

`firmware_image.h`：镜像类型、版本结构、硬件 ID、长度、SHA-256、签名和镜像头格式。只定义格式，不保存当前版本值。

`ota_metadata.h`：active slot、pending slot、boot state、trial boot count、upgrade sequence 和 CRC。

`ota_protocol.h`：Application 与 Bootloader 共同使用的 OTA 帧定义。当前先定义：

```text
OTA_QUERY
OTA_BEGIN
OTA_DATA
OTA_END
OTA_ABORT
OTA_RESULT
```

### 13.2 版本管理

Application：

```text
firmware/application/stm32g474/config/build_info.h
```

Bootloader：

```text
firmware/bootloader/stm32g474/config/build_info.h
```

两者独立维护版本。Application 可声明最低兼容 Bootloader 版本。

### 13.3 OTA V1

```text
Jetson
  ↓ CAN FD
Application
  ↓
进入 UPDATE 状态，关闭电机
  ↓
接收固件并写入非活动 Slot
  ↓
校验 CRC、SHA-256 和签名
  ↓
写 UPDATE_READY
  ↓
重启
  ↓
Bootloader 校验并试运行
  ↓
Application 自检
  ↓
CONFIRMED 或 ROLLBACK
```

Bootloader V1 不直接通过 CAN FD 接收完整固件。

Bootloader CAN FD Recovery 留到 V2。`ota_protocol.h` 当前先保留，V2 时扩展。

---

## 14. 当前代码迁移建议

```text
当前文件或功能                 目标位置

bsp_motor                     bsp/motor/
bsp_encoder                   bsp/encoder/
bsp_power_sample              bsp/power_monitor/
bsp_qspi_flash                bsp/qspi/
fdcan_driver                  bsp/fdcan/

speed_pid                     components/pid/

board_self_test UART 部分     services/console/
board_self_test 遥测部分      services/telemetry/
board_self_test 硬件检查      modules/diagnostics/
board_self_test 测试命令      tests/target/

LCD 底层绘制                  bsp/lcd/
LCD 页面和业务数据            services/status_display/

app_main                      app/chassis_app.c

chassis_control               拆为 command_manager、differential_drive、
                              wheel_controller、odometry、
                              safety_manager、fault_manager
```

`chassis_control.c` 不应整体原样迁移。

---

## 15. 分阶段实施路线

### 阶段 1：目录重构

只改变目录和职责，不改变行为。

先迁移现有代码到：

```text
board/
bsp/
components/
communication/
services/
modules/
app/
config/
tests/
```

### 阶段 2：完整裸机底盘 Demo

完成 CAN FD、运动学、双轮闭环、状态机、安全、故障、反馈和 VOFA+ PID 调参。

### 阶段 3：FreeRTOS

只迁移调度。

### 阶段 4：IMU

采集并上报原始数据。

### 阶段 5：ROS 2

在独立 Jetson 仓库实现 Bridge、里程计和融合。

### 阶段 6：Bootloader 与 OTA

实现 V1 A/B Slot、确认和回滚。V2 再考虑 Bootloader CAN FD Recovery。

---

## 16. 当前阶段不要做

- 不一次创建全部空目录
- 不立即引入 FreeRTOS
- 不在目录重构时同时重写 PID
- 不在目录重构时同时冻结 OTA 所有细节
- 不复制开源项目 Flash 地址
- 不把 Bootloader 和 Application 合成一个 CubeMX 工程
- 不把业务逻辑写入 BSP
- 不把所有公共代码放入 shared
- 不在 STM32 优先实现复杂 EKF
- 不让 Jetson 直接控制 PWM
- 不让 VOFA+ 每次修改参数就直接写 Flash

---

## 17. Codex 执行要求

Codex 修改仓库时必须：

1. 先读取本文档。
2. 再读取当前仓库代码和 CubeMX 配置。
3. 不一次性重写整个工程。
4. 每次只完成一个清晰目标。
5. 重构前后行为保持一致。
6. 不修改 CubeMX 自动生成区域，除非必要。
7. 移动文件后同步更新 `.cproject`、include path、source folder、构建脚本和 README。
8. 每次输出修改文件、迁移说明、API 变化、编译结果和验证步骤。
9. 不提前建立未使用模块。
10. 新模块必须说明所属层和依赖方向。
11. 如文档与实际 CubeMX 工程冲突，采用最小修正方案。
12. 不在同一个提交中同时进行大规模目录迁移、CAN 协议重写、PID 算法修改、FreeRTOS 引入和 OTA 实现。

---

## 18. 当前最终决定摘要

```text
自定义硬件驱动目录：
bsp/

Application 与 Bootloader：
两个独立 CubeMX 工程

保留独立：
board/
config/

Application 自定义目录：
board/
bsp/
components/
communication/
services/
modules/
rtos/
app/
config/
tests/

firmware/shared/：
flash_layout.h
firmware_image.h
ota_metadata.h
ota_protocol.h

版本：
Application 和 Bootloader 分别使用 config/build_info.h

ROS 2：
Bridge 在 Jetson
STM32 发送编码器、轮速、IMU 原始数据和状态
Jetson 计算里程计、融合和 ROS 2 消息

PID 调参：
Windows + USB 串口 + VOFA+
FireWater 文本协议
STM32 支持 GET/SET/APPLY/SAVE/RESET
参数默认只改 RAM，确认后保存

OTA V1：
Application 接收固件
Bootloader 校验、切换、确认和回滚

OTA V2：
以后再考虑 Bootloader CAN FD Recovery
```

当前第一目标：

> 在保持现有功能可运行的前提下，完成目录和职责重构，然后实现一个可通过 CAN FD 控制、带双轮闭环、安全保护、编码器反馈和 VOFA+ PID 调参能力的完整裸机底盘 Demo。
