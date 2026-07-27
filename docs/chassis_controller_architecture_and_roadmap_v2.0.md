# chassis-controller 架构与演进路线 v2.0

> 文档状态：当前架构基线
> 更新时间：2026-07-27
> 适用仓库：`chassis-controller`
> 适用 MCU：STM32G474VET6

## 0. 文档用途

本文档基于当前仓库源码、CubeMX/CubeIDE 工程配置、裸机实物验收结果和 CAN FD 联调结果整理，用于固定：

- 当前真实实现状态
- 最终目录结构
- 各层职责和依赖方向
- FreeRTOS 运行模型
- 控制、通信、安全和时间边界
- 后续重构顺序和验收条件

后续修改仓库前，应先读取：

1. 本文档
2. `README.md`
3. `docs/硬件与接线.md`
4. `protocol/canfd_protocol.md`
5. 当前源码、`.ioc` 和 `.cproject`

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

本仓库当前维护：

- STM32G474 Application 固件
- CAN FD 协议文档
- 固件架构、安全、Bring-up 和验证文档
- 固件构建、格式检查和单元测试配置

Bootloader 和 OTA 只保留远期边界说明，尚未进入实现范围。

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
3. 已实现、READY、PASS 状态和验收结论：本文档第 4、21、22 节
4. 软件分层、目录职责和迁移顺序：本文档
5. README：项目入口和简要使用说明

未经过真实硬件验证的功能只能标记为“已实现”或 `READY`，不能标记为 `PASS`。

---

## 4. 当前真实基线

### 4.1 已验收裸机基线

裸机基线版本为 `v0.1.0-baremetal`，验收日期为 2026-07-23，
STM32CubeIDE GCC Debug 完整构建结果为 `0 errors, 0 warnings`。已完成以下实物验收：

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

保留的裸机基线测量结果如下：

| 项目 | 实测结果 |
|---|---|
| 编码器 | 左轮 10 圈 `13208 counts`，右轮 10 圈 `13195 counts`，控制换算取 `1320 counts/rev`；车体向前为正、向后为负 |
| ADC 分压 | 电池 `11.96 V`，PA2 `1.086 V`，理论 `1.087 V`，分压测量误差约 `0.12%` |
| 低速 PID | 架空完成双轮 `±10/±20/±30 counts/10ms` 和单轮 `±20 counts/10ms`；稳态误差不超过约 `0.5 counts/10ms` |
| PID 参数 | 左轮 `kp/ki/kd=200/300/0`，右轮 `250/400/0`；右轮目标 `±30` 时峰值输出约 `7620`，未触及 `8000` 上限 |
| RTC 保持 | 仅保留纽扣电池、主电源断开约 5 分钟，时间由 `02:14:22` 连续增长至 `02:19:27` |
| QSPI | JEDEC ID `EF4017`，容量 8 MiB；`0x007FF000` 完成擦除、空白检查、1 KiB DMA 写入和回读校验 |
| 电机开环 | 左右电机正反转、编码器方向和 1 秒自动停车均通过；人工测试使用单电机 50% PWM |

AT8236 控制方式、编码器和 ADC 标定、CAN FD 时序、RTC 接线及 QSPI 分区的硬件细节
统一维护在 `docs/硬件与接线.md`。不再按开发阶段单独维护重复的进度文档。

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

TIM6 ISR
  ↓ FreeRTOS 任务通知
control_task
  ↓ 每次唤醒最多一次
ChassisApp_RunControlTick()
```

当前特征：

- CubeMX 生成 FreeRTOS 和 CMSIS-RTOS2 适配层
- `defaultTask` 使用静态分配
- 当前任务栈为 1024 Words
- `defaultTask` 进入 `RtosApp_Run()`，任务内部使用原生 FreeRTOS 延时接口周期调用 `ChassisApp_Run()`
- `control_task` 使用静态分配，栈为 512 Words，优先级为 `configMAX_PRIORITIES - 2`
- SysTick 用于 FreeRTOS 内核节拍
- TIM7 用于 HAL 1 ms 时基
- TIM6 继续提供 10 ms 控制节拍
- TIM6 ISR 只发送原生 FreeRTOS 任务通知，急停 ISR 仍直接清零硬件 PWM
- PID、电机控制和控制安全检查在独立 `control_task` 中执行
- Console、UART 文本、遥测、LCD、QSPI、RTC、诊断、目标测试和阻塞采样仍在普通应用任务中执行
- 通知积压时只执行一次有效控制计算，不连续补跑历史 PID；积压量进入诊断计数
- 单次丢失超过 4 个周期或连续 3 次发生积压时，锁存控制 overrun 故障并安全停车

阶段 4 全部代码于 2026-07-27 完成，并从
`firmware/application/stm32g474/` 完成 STM32CubeIDE GCC Debug 和 Release
全量 clean build，两种配置均为 `0 errors, 0 warnings`。Debug 镜像为
`text=211964`、`data=96`、`bss=31384`；Release 镜像为
`text=175436`、`data=96`、`bss=31368`。新增 BSS 主要来自静态控制任务栈和任务对象。

阶段 3 之前的迁移固件已完成实物回归。按当前项目决定，阶段 4 不重复执行零输出、
PID、电机、急停和 IWDG 实物回归；未执行的故障注入、时序测量和硬件验证不标记为
`PASS`。

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
│  ├─ fdcan/
│  ├─ uart/
│  ├─ qspi/
│  ├─ reset/
│  └─ lcd/
├─ components/
│  └─ pid/
├─ communication/
│  └─ can_transport/
├─ modules/
│  ├─ chassis/
│  ├─ diagnostics/
│  ├─ parameters/
│  └─ safety/
├─ infrastructure/
│  ├─ console/
│  ├─ status_display/
│  └─ telemetry/
├─ rtos/
│  ├─ rtos_app.c
│  └─ rtos_app.h
├─ tests/
│  └─ target/
└─ config/
   ├─ app_config.h
   ├─ build_info.h
   ├─ control_config.h
   ├─ feature_config.h
   ├─ protocol_config.h
   ├─ storage_layout.h
   └─ target_test_config.h
```

### 4.4 目录迁移完成状态

阶段 0 已于 2026-07-25 完成：

1. `.gitignore` 已覆盖新工程根目录的 Debug、Release、`.metadata`、launch 和本地索引文件。
2. `.cproject` 的 Debug 和 Release 配置已注册当前所有实际 include path 和 source entry，包括 `board/` 与 `rtos/`。
3. `board/board_config.h` 已集中提供电机、编码器、ADC、LCD 和 QSPI 的主要
   `BOARD_*` 映射；FDCAN、UART、RTC 备份寄存器和 TIM6 接入仍直接引用 CubeMX
   全局句柄，属于待逐步收口的过渡状态，不能描述为完全解耦。
4. `rtos/rtos_app.c/.h` 已承接静态任务循环，`defaultTask` 只进入 `RtosApp_Run()`。
5. 应用生命周期 API 已统一为 `ChassisApp_*`，`app_main` 和旧 `App_*` 命名已清零。
6. 原 `project_config.h` 已拆分为应用、构建、控制、功能和协议配置，`storage_layout.h` 独立保留。
7. `services/status_display/` 已迁移到 `infrastructure/status_display/`。
8. Debug 和 Release 均从新工程根目录完成全量 clean build，所有 BSP、modules、infrastructure 和 `rtos_app.o` 均进入链接输入。
9. 使用临时 Git 索引验证迁移结果，不修改工作区真实暂存区。
10. 本地 `.ai-bridge/`、IDE 工作区和构建产物已排除在产品提交之外；会话记录不属于固件源码。

阶段 1 实物回归已经完成。阶段 3 拆分后的新镜像仍需在全部代码迁移完成后统一回归。

### 4.5 当前代码审核结论

本节记录 2026-07-26 对当前 `main` 分支的代码和工程配置审核。审核只确认
静态代码事实，不替代编译和硬件验证；本次未修改任何固件源码、`.ioc`、`.cproject`
或其他工程配置。

#### 已确认的优点

- 电机上电保持零输出，`WheelController_Stop()` 和急停路径都会清零 PWM 并重置 PID。
- PD2 急停 ISR 直接调用 `BspMotor_EmergencyStop()`，不依赖任务调度。
- CAN 控制帧已检查版本、保留位、目标范围和序号，只有合法帧进入命令管理。
- QSPI 目标测试使用保留扇区、非阻塞状态机和 DMA，不在中断中执行擦写。
- `chassis`、`safety`、`parameters`、`diagnostics` 已形成实际业务域，不需要继续创建空模块。
- Debug 和 Release 的 `.cproject` 均注册当前 `app`、`board`、`bsp`、`components`、
  `communication`、`infrastructure`、`modules`、`rtos` 和 `tests` 源目录。

#### 阶段 4 已完成代码实现

- 已建立静态高优先级 `control_task`，由 TIM6 ISR 使用任务通知直接唤醒。
- 每次唤醒最多进行一次编码器采样和 PID 计算，不再使用
  `control_ticks_pending` 或循环补跑历史周期。
- 通知积压时按通知数量折算本次编码器测量，记录 overrun 和 missed tick；
  单次或连续积压超过阈值时锁存故障并安全停车。
- 普通应用任务继续承担 Console、显示、存储、遥测和目标测试等非实时流程。
- `command_manager` 使用 `NONE`、`CAN_REMOTE`、`CONSOLE`、`TARGET_TEST`
  所有权；非所有者不能覆盖当前运动目标，超时和会话故障会释放所有权。
- FDCAN 已启用 warning、error-passive、bus-off、FIFO full/lost 和协议错误通知；
  错误事件撤销远程会话，bus-off 在普通任务中有限重启，恢复后必须重新握手。
- QSPI、IWDG 和电机目标测试统一先停车、清零 PWM、撤销普通控制源并取得
  `TARGET_TEST` 所有权；三类测试互斥，结束或失败后保持停车。
- 应用任务和控制任务分别维护心跳；关键任务失活、关键故障、FDCAN 恢复失败或
  IWDG 目标测试会抑制喂狗；栈溢出和分配失败 Hook 直接进入最小安全路径。
- 阶段 4 Debug 和 Release 全量 clean build 均为 `0 errors, 0 warnings`。
- 按当前决定不执行零输出、PID、电机、急停和 IWDG 重复回归；这些项目不能因
  本次代码实现或构建成功重新标记为硬件 `PASS`。

#### P0：已完成实现，保留联调验证

| 问题 | 当前实现 | 剩余风险 | 验收状态 |
|---|---|---|---|
| FDCAN 错误状态闭环 | `fdcan_bsp` 启用完整错误通知；`can_transport` 在 ISR 中只记录事件，在应用任务中撤销会话、有限恢复并要求重新握手；诊断累计 warning、passive、bus-off、协议错误、FIFO 和恢复次数 | 尚未注入真实 error-passive/bus-off 验证收发器与 HAL 恢复行为 | 代码与 Debug/Release 构建完成；实物故障注入未执行，不标记 `PASS` |
| 危险目标测试准入和互斥 | `chassis_app` 统一执行停车、零 PWM、所有权切换和测试互斥；QSPI 完成/失败和电机测试结束后释放 `TARGET_TEST`，IWDG 测试保持停车直到复位 | 尚未交叉执行三类命令验证所有拒绝路径 | 代码与 Debug/Release 构建完成；实物交叉命令验证未执行 |

#### P1：已完成实现

| 问题 | 当前实现 | 剩余风险 | 验收状态 |
|---|---|---|---|
| 控制源所有权 | `command_manager` 独立保存所有者；同源可更新，异源提交被拒绝；安全停止、CAN 超时和会话失效释放所有权 | 尚未执行 CAN、Console、目标测试交叉命令回归 | 代码与构建完成 |
| 控制任务时序 | 独立静态 `control_task`、单次有效计算、积压计数和阈值停车已完成 | 未测最坏执行时间、抖动和栈高水位 | 本次按决定不做板上压力测试，不能标记时序 `PASS` |
| Watchdog 健康汇总 | 应用和控制任务心跳、关键故障、通信恢复失败、IWDG 测试共同控制喂狗；致命 FreeRTOS Hook 安全停车后停止喂狗 | 未通过故障注入验证最终复位路径 | 代码与构建完成 |
| `AutoRetransmission = DISABLE` | 保持 `.ioc` 不变；握手响应只进行 3 次、间隔 10 ms 的有限入队重试，失败撤销会话；bus-off 最多进行 3 次、间隔 100 ms 的受控重启 | 未注入 ACK 丢失验证具体控制器行为 | 策略已明确并实现；硬件故障注入未执行 |

#### P2：后续按实际收益收口

| 问题 | 代码依据 | 风险 | 修复方向 | 阶段与验收 |
|---|---|---|---|---|
| 硬件句柄仍有过渡耦合 | `bsp/fdcan/fdcan_bsp.c` 直接使用 `hfdcan2`，`bsp/uart/uart_bsp.c` 直接使用 `huart1`，`bsp/reset/bsp_reset.c` 直接使用 `hrtc`，`app/chassis_app.c` 直接使用 `htim6` 和 `hiwdg` | 更换实例或做主机测试时影响范围较大，文档也容易高估解耦程度 | 仅在真实复用或测试需求出现时收口到 `board/` 或明确 BSP 接口，不为形式增加转发层 | 后续维护项。依赖搜索结果与文档一致，且不新增只转发一次的空壳 |
| 顶层应用编排职责仍偏多 | `app/chassis_app.c` 同时负责初始化、命令分发、控制入口、目标测试和 Watchdog | 后续修改时仍可能误把非实时工作带入控制任务 | 保持现有实时/非实时入口边界；只有出现明确复用或阻塞问题时才继续拆分，不按行数机械拆文件 | 后续维护项。控制任务调用图只包含实时控制和安全快照，普通应用任务保留其他流程 |

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

## 6. 目录演进约束

当前目录以第 4.3 节的实际仓库结构为准，不提前建立 Bootloader、OTA、ROS 2 Bridge、
主机工具、额外 RTOS 任务或尚无逻辑的算法目录。新目录只在同时满足以下条件时创建：

- 已有真实实现文件和明确职责。
- 能形成独立业务域、硬件边界或依赖边界。
- `.cproject` 的 Debug 和 Release 配置能够同步注册并完成构建。
- 不只是为了转发一次调用、缩短单个文件或预留远期名称。

命名约定：

- CubeMX 已占用 `Drivers/`、`Middlewares/` 和 `Application/`，自有代码不创建仅大小写不同的同名目录。
- 自有目录统一使用小写 `snake_case`。
- 公共运行设施使用 `infrastructure/`，不恢复旧 `services/` 命名。
- 目录名称描述职责，不使用 `misc/`、`common/` 或泛化 `utils/`。
- 不创建空 `.c/.h`、空模块或无实际消费者的转发层。

阶段 4 只在 `rtos/` 中增加了独立控制任务所需的最少文件。是否再拆通信、Console、
显示或存储任务，由后续控制周期测量和阻塞行为决定，不预先创建完整任务目录树。

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

当前职责：

- `motor/`：PWM、方向、立即禁止输出
- `encoder/`：计数器读取、回绕处理、方向换算
- `power_monitor/`：ADC 采样和电压换算
- `fdcan/`：HAL FDCAN 初始化后的底层收发封装
- `uart/`：UART DMA 收发和底层缓冲
- `lcd/`：ST7789 绘图和 SPI DMA
- `qspi/`：JEDEC、擦除、写入、读取和 DMA
- `reset/`：复位原因、RTC 备份寄存器和 IWDG 测试标记

RTC、TIM6 和 IWDG 的部分操作目前仍在 `app/` 或现有 BSP 中直接使用 CubeMX 句柄。
只有形成实际可复用边界时才增加新 BSP，不预建 `rtc/`、`timebase/` 或 `watchdog/`。

BSP 不决定：

- 是否允许电机运行
- 是否处于 ACTIVE
- 是否命令超时
- 是否应进入 FAULT
- PID 参数
- LCD 页面业务内容

### 7.4 `components/`

当前只包含不依赖 HAL 的 PID。限幅器、滤波器、Ring Buffer、CRC 等只有出现实际
复用需求时再增加。

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

`status_display/`：

- LCD 页面组织
- 状态、故障、版本和外设结果显示

底层绘图仍在 `bsp/lcd/`。

基础设施不得决定电机是否允许运行。

### 7.7 `modules/`

`modules/` 保存底盘产品业务。

`chassis/`：

- `command_manager` 保存最新有效命令、来源、序号、时间戳和超时状态
- `differential_drive` 完成左右轮目标混合和原始轮增量反算
- `odometry` 根据调用者提供的编码器增量累计基础运动数据，不直接读取硬件
- `wheel_controller` 保存左右轮目标、测量、PID 和输出，只负责正式闭环轮控

`safety/`：

- `safety_manager` 保存底盘状态、电机许可、急停和命令超时状态
- `fault_manager` 保存故障位、等级以及锁存和清除条件

`parameters/`：

- PID、轮径、轮距、编码器和限幅参数
- 参数范围检查
- pending 参数
- 控制周期边界应用
- 默认参数恢复

`diagnostics/`：

- `board_health` 只采集并发布板级健康快照
- 不格式化串口报告，不执行擦写、复位或电机动作
- 不直接访问 CubeMX 全局句柄

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

当前文件：

- `app_config.h`：启动策略和应用周期
- `control_config.h`：控制周期、限幅和默认控制参数
- `feature_config.h`：LCD、IMU、遥测等功能开关
- `protocol_config.h`：CAN 超时、心跳和上报周期
- `storage_layout.h`：当前 QSPI 分区和测试保留区
- `target_test_config.h`：显式确认后才能执行的板上测试参数
- `build_info.h`：Application 当前版本和构建信息

### 7.11 `tests/`

当前只有 `tests/target/`：

- STM32 板上测试
- 危险操作必须显式确认
- 不在正常启动中自动转动电机、擦除 QSPI 或触发复位

宿主机单元测试目录尚未建立；出现首个可运行测试时再创建，不提前放空骨架。

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

### 9.1 底盘业务拆分

原 `modules/chassis_control/chassis_control.c` 的职责按以下边界完成拆分：

```text
命令缓存、序号、时间戳       → modules/chassis/command_manager.*
左右轮目标和 PID             → modules/chassis/wheel_controller.*
差速解算                     → modules/chassis/differential_drive.*
状态机和电机许可             → modules/safety/safety_manager.*
故障位、锁存和清除           → modules/safety/fault_manager.*
编码器累计和基础运动增量     → modules/chassis/odometry.*
```

拆分于 2026-07-25 分两批完成，原 `modules/chassis_control/` 已删除。

第一批完成：

- `fault_manager` 唯一保存当前故障位，区分可恢复故障和严重故障
- `safety_manager` 唯一保存请求状态和急停锁存，急停与严重故障覆盖普通请求状态
- `wheel_controller` 唯一保存左右 PID、目标、测量和输出，只执行正式闭环控制

第二批完成：

- `differential_drive` 提供左右轮目标混合和原始轮增量反算，当前使用无损原始 count 分量
- `odometry` 接收控制执行链提供的编码器周期增量，保存左右累计和基础前进/转向原始增量
- `parameter_manager` 校验 PID 范围，保存 active/pending 参数，并在 10 ms 控制周期边界应用
- `app/chassis_app.c` 只负责命令、安全、故障、参数、里程计和轮控的运行时编排
- 遥测和 LCD 从对应模块快照读取，不再依赖聚合状态结构

`modules/chassis/command_manager.*` 已完成：

- 唯一保存当前有效左右轮目标、接收时间戳、命令来源和可选序号
- 统一校验目标范围；CAN 来源必须携带序号，Console 和 Demo 来源不得伪造序号
- 提供来源受限的时间戳刷新，避免其他入口延长 Demo 命令寿命
- 统一执行 200 ms 新鲜度判断，超时停车和故障状态由 `safety_manager` 与 `fault_manager` 决定
- CAN 重复帧、跳帧、回绕和握手会话仍由 `communication/can_transport` 校验

当前 `parameter_manager` 完成的是运行时 PID 参数切片。轮径、轮距、编码器、
限幅参数和 QSPI CRC 双副本持久化仍属于阶段 6，不在本次目录拆分中提前实现。

### 9.2 板级健康和目标测试

原 `modules/diagnostics/board_self_test.c` 曾同时包含：

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
硬件状态检查                → modules/diagnostics/board_health.*
PID 参数业务                → modules/parameters/parameter_manager.*
持久化                      → infrastructure/parameter_storage/
危险板上测试入口            → tests/target/
自检报告格式化              → infrastructure/console/diagnostic_report.*
```

当前已完成：

- USART1 HAL 和 RX/TX DMA 所有权收敛到 `bsp/uart/`
- RX 使用 128 字节循环 DMA 和 256 字节软件环形缓冲，错误后由任务上下文自动重启
- TX 使用固定深度静态队列，DMA 完成后自动续发，不阻塞 10 ms 控制周期
- 串口命令成帧、语法解析和命令队列迁入 `infrastructure/console/`
- TEXT/VOFA 遥测模式、周期和格式化迁入 `infrastructure/telemetry/`
- PID、编码器和 CAN 命令调度由 `app/chassis_app.c` 组装，不再放在自检模块
- QSPI JEDEC、DMA abort、复位原因和 RTC Backup Register 访问收敛到 BSP
- `board_health` 只发布 QSPI、复位测试和按键健康快照
- QSPI 擦写、IWDG 复位和电机开环测试分别位于 `tests/target/`
- 自检、QSPI 和电机测试文本由 `infrastructure/console/diagnostic_report.*` 输出
- 电机开环测试已从正式 `wheel_controller` 删除
- 原 `board_self_test.c/.h` 已删除

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
命令缓存和超时              → modules/chassis/command_manager.*
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
  ↓ 发送任务通知
control_task
  ↓ 每次唤醒最多一次
ChassisApp_RunControlTick()

1 ms defaultTask
  ↓
RtosApp_Run()
  ↓
ChassisApp_Run() 处理非实时轮询逻辑
```

阶段 4 调度已经实现。控制任务与普通应用任务使用两个执行上下文，
目前不继续拆分通信、Console、显示或存储任务。

### 10.2 目标任务

```text
control_task          100 Hz，最高业务优先级，只处理实时控制
application_task      普通优先级，承接当前非实时轮询和分发
```

阶段 4 已落地这两个执行上下文。不要求一个模块对应一个任务；只有测量证明某项工作
存在独立阻塞、周期或优先级需求时，才继续拆分通信、Console、显示或存储任务。

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
- TIM6 ISR 使用原生 FreeRTOS 任务通知唤醒
- 每次任务唤醒最多执行一次有效编码器采样和 PID 更新
- 通知值大于 1 时丢弃过期周期，记录 control overrun，不连续补跑历史 PID
- 连续 overrun 或积压超过阈值时，目标归零并进入锁存故障或安全停车
- 只读取命令、安全和参数快照，不解析 Console 或 CAN 文本
- 不执行串口格式化输出
- 不执行 VOFA 遥测
- 不执行 LCD 刷屏
- 不执行 QSPI 擦写
- 不执行 RTC 显示、大型诊断报告、目标测试流程或阻塞式 ADC 采样
- 不等待普通互斥锁的长时间阻塞
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

### 12.6 控制源所有权

阶段 4 使用简单所有权模型，不引入复杂运行模式系统：

```text
NONE
CAN_REMOTE
CONSOLE
TARGET_TEST
```

规则：

- 任一时刻只能有一个控制源拥有运动控制权。
- 新控制源申请所有权前，旧目标必须归零，PID 必须重置，PWM 必须先回到零。
- 非所有者只能读取状态，不能覆盖当前运动目标。
- 危险目标测试启动前撤销普通控制源，并在结束、失败或超时后释放为 `NONE`。
- CAN 命令超时、error-passive、bus-off、协议故障或会话失效时，立即释放
  `CAN_REMOTE` 并停车。
- CAN 恢复后不得恢复旧所有权或旧目标，必须重新握手并收到新的合法控制帧。
- 当前编译期开关 Demo 视为开发测试来源；阶段 4 不为它增加独立复杂模式。
- 后续 ROS 2 Bridge 或自动控制模块只能通过冻结后的控制协议申请所有权，不能绕过仲裁。

### 12.7 危险目标测试准入

QSPI 擦写、IWDG 复位和电机目标测试使用同一组前置条件和互斥规则：

- 系统处于 `STOPPED`。
- 四路电机 PWM 和已记录 applied duty 均为零。
- 当前不存在有效运动目标。
- 当前控制源为 `NONE`，或已先撤销并完成零输出切换。
- 任一时刻只能运行一个危险目标测试。
- 测试启动、结束、失败和超时后都保持停车，不自动恢复旧目标。

QSPI 测试还必须限制在保留扇区；IWDG 测试必须先停车再写入测试标记并停止正常喂狗；
电机目标测试继续保留显式确认、单电机、限时和自动清零。

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

### 14.7 FDCAN 错误与恢复

当前实现已激活以下通知并形成错误恢复闭环：

- error warning、error passive、bus-off 和错误状态变化通知。
- RX FIFO full、RX FIFO message lost 和协议错误通知。
- error-passive 或 bus-off 时立即撤销远程控制会话、释放 `CAN_REMOTE`、目标归零并停车。
- bus-off 后受控停止并重新初始化或重新启动 FDCAN；恢复成功后状态回到等待握手。
- 恢复后必须重新完成握手并收到新的合法控制帧，旧命令和非零输出不得自动恢复。
- 累计记录 TEC、REC、LEC、DLEC、warning、passive、bus-off 次数、FIFO 丢失次数和恢复次数。

当前生成代码中 `AutoRetransmission = DISABLE`。这意味着发送失败后 FDCAN 不会自动重传；
优点是不会在错误总线上形成不可控硬件重试，代价是握手和状态帧的可靠性由上层状态机
负责。阶段 4 保持 `.ioc` 不变：握手响应最多进行 3 次、间隔 10 ms 的有限入队重试；
bus-off 最多进行 3 次、间隔 100 ms 的受控重启。失败后撤销会话并等待重新握手，不使用
忙循环或无上限重发。

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
modules/parameters/parameter_manager
  ↓
modules/chassis/wheel_controller
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

`qspi test confirm` 启动前必须通过第 12.7 节统一准入：系统已停车、PWM 为零、
无运动目标、控制源已释放且没有其他危险测试运行。擦写期间不得启动闭环、开环电机测试
或 IWDG 复位测试；测试结束、失败或超时后继续保持停车。

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

当前不创建 Bootloader、OTA、`firmware/shared/` 或相关空目录。Application 链接脚本仍使用：

```text
FLASH ORIGIN = 0x08000000
FLASH LENGTH = 512K
```

因此当前没有为 Bootloader、Metadata 或 A/B Slot 预留内部 Flash。该方向排在阶段 4
及底盘正式控制链路之后；进入实施前必须单独冻结 Flash 容量、布局、向量表、镜像格式、
校验、确认和回滚策略。不得复制其他芯片的地址，也不在当前阶段预留代码或协议文件。

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

阶段 4 每个可运行步骤都必须完成 Debug 和 Release clean build，检查 `0 errors`、
新增警告、ELF/MAP、任务静态内存、栈高水位和所有新增对象确实进入链接。构建通过只能
记为“已实现”，不能代替 STM32 实物回归或 Jetson CAN FD 联调。

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

板上测试必须遵守以下基线规则：

- 启动自检不得转动电机、擦除或写入 QSPI，也不得主动触发 IWDG 复位。
- 启动阶段只允许读取 QSPI JEDEC ID 和容量；擦写测试只能使用保留扇区 `0x007FF000`。
- 电机开环测试只能通过带 `confirm` 的明确命令启动，每次仅驱动一个电机，50% PWM，1 秒后自动清零。
- `motor stop`、PD2 急停、内部故障和 IWDG 测试准备阶段均必须清零四路 PWM。
- QSPI、IWDG 和电机目标测试必须通过第 12.7 节统一准入并互斥运行。
- 测试命令到达时若仍存在控制所有者、运动目标或非零 PWM，必须拒绝或先完成安全停车，
  不能直接进入测试状态。
- IWDG 测试只能通过 `iwdg reset confirm` 启动，并同时使用 RTC Backup Register 标记和 RCC 复位原因判断结果。
- QSPI 参数区、日志区和升级包区不得占用人工测试扇区。
- 没有硬件读回能力的项目不能只根据 HAL 返回值标记为 `PASS`；LCD、USART 物理链路、RTC 掉电保持、编码器和电机需要人工或外部仪器验收。

启动自检的状态语义保持为：安全且已初始化的链路输出 `READY`，完成自动读回校验的项目
可以输出 `PASS`，会引起运动、擦写或复位的测试保持 `DISABLED`，直到收到明确确认命令。

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
- 人工阻塞普通应用任务时，控制任务仍保持 100 Hz；通知积压只记录一次 overrun，
  不补跑历史 PID
- FDCAN error-passive、bus-off、恢复和重新握手，确认旧控制目标不恢复
- CAN、Console 和目标测试的所有权竞争、切换归零和释放
- QSPI、IWDG、电机目标测试的准入、互斥、失败和超时停车
- 控制任务、应用任务和通信健康汇总能够正确决定是否喂狗

阶段 3 拆分前的 FreeRTOS 迁移基线回归状态：**PASS（2026-07-25）**。已完成并确认：非零 `0x100` 闭环及运动后编码器反馈、PD2 急停锁存/释放/复位恢复、LCD 页面和 KEY 切页、RTC 掉电保持、Release 固件实物回归、长时间运行与 Jetson CAN 错误计数稳定性，以及重复帧、丢帧、跳帧、序号回绕和握手重建测试。阶段 4 当前只完成代码与构建验证，不继承或伪造新的硬件 `PASS`。

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

状态：**COMPLETE（2026-07-25）**

按小步顺序：

1. `fdcan_driver` 拆出 `bsp/fdcan`。（已完成）
2. 将握手、控制帧解析、序号状态和发送调度收敛到 `communication/can_transport`，删除转发壳。（已完成）
3. 删除 `board_self_test`，拆出 `board_health`、目标测试和诊断报告。（已完成）
4. 拆出 `infrastructure/telemetry`。（已完成）
5. `status_display` 迁移到 `infrastructure/`。（已完成）

每一步都必须保持当前控制帧和硬件行为不变。

### 阶段 3：拆分底盘业务模块

状态：**COMPLETE（2026-07-25）**

按顺序拆分：

1. `command_manager`（已完成）
2. `fault_manager`（已完成）
3. `safety_manager`（已完成）
4. `wheel_controller`（已完成）
5. `differential_drive`（已完成）
6. `odometry`（已完成）
7. `parameter_manager`（已完成运行时参数边界）

原 `modules/chassis_control/` 已删除，业务模块已收敛为 `chassis`、`safety`、
`parameters` 和 `diagnostics` 四个域。Debug 和 Release 均完成 CubeIDE
全量 clean build，域内对象与 `tests/target` 对象均进入 `objects.list`，
无旧聚合对象和 `board_self_test.o`。

### 阶段 4：建立独立控制任务

状态：**COMPLETE（代码与构建，2026-07-27）**

阶段 4 的第 1 至 9 项已完成代码实现。第 10 项中的 Debug/Release clean build
已完成；按当前决定，不重复执行零输出、PID、电机、急停和 IWDG 实物回归：

1. [已实现] 建立静态分配、最高业务优先级的 100 Hz `control_task`。
2. [已实现] TIM6 ISR 改为原生 FreeRTOS 任务通知；急停 ISR 继续直接清零硬件 PWM。
3. [已实现] 控制任务每次唤醒只读取一次编码器、检查一次快照并执行一次 PID，不补跑历史周期。
4. [已实现] 增加 control overrun 计数、连续积压阈值和安全停车。
5. [已实现] 保留普通 `application_task` 承接 Console、UART 文本、VOFA、LCD、QSPI、RTC、
   阻塞 ADC、诊断报告和目标测试流程。
6. [已实现] 增加 `NONE`、`CAN_REMOTE`、`CONSOLE`、`TARGET_TEST` 控制源所有权和切换归零。
7. [已实现] 增加 FDCAN warning、passive、bus-off、FIFO full/lost、协议错误处理、受控恢复和重新握手。
8. [已实现] 统一 QSPI、IWDG、电机目标测试的停车准入与互斥。
9. [已实现] 建立关键任务健康汇总，由汇总结果决定是否刷新 IWDG。
10. [构建已完成] Debug/Release clean build；实物故障注入和时序测量未执行。

验收：

- 100 Hz 控制周期稳定，测得最坏执行时间、抖动和栈高水位。
- 通知积压时不连续补跑；overrun 可观测，连续超限安全停车。
- 显示、串口、VOFA、QSPI、RTC、ADC 和诊断活动不进入或阻塞控制任务。
- CAN error-passive 和 bus-off 立即停车，恢复后必须重新握手和接收新命令。
- 控制源互斥，切换前零目标、零 PWM，旧目标不恢复。
- 危险目标测试只能在 STOPPED、零 PWM、无所有者时启动，并且彼此互斥。
- 急停和故障停车保持立即有效，不依赖任务调度。
- Watchdog 只在关键任务和通信状态健康时刷新。
- Debug 和 Release 均全量 clean build。未执行的实物故障注入、时序测量和
  回归项目保持“未验证”，不得标记为 `PASS`。

### 阶段 5：正式底盘协议和运动学

阶段 4 代码与构建完成后，冻结正式底盘协议、序号语义、运动学和反馈；本阶段暂不展开实现细节。

### 阶段 6：参数和遥测

阶段 5 稳定后再规划参数持久化和正式遥测；本阶段暂不创建存储模块。

### 阶段 7：IMU 和里程计

- IMU 原始数据
- 单调时间戳
- 基础滤波和掉线检测
- 编码器与 IMU 原始数据上报
- Jetson 侧里程计和融合

### 阶段 8：Bootloader 与 OTA

底盘实时控制和正式通信链路稳定后另立设计评审，不提前创建目录、协议或代码。

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
9. 不创建假的空 `.c/.h`、空模块或仅用于占位的目录。
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
静态 application_task + 独立高优先级 control_task + TIM6 任务通知

Bootloader 与 OTA：
当前不创建目录和代码，待底盘实时控制与正式通信链路稳定后单独规划

ROS 2：
Bridge、融合和高层里程计在 Jetson

PID 调参：
Windows + USB 串口 + VOFA+
参数默认只修改 RAM，确认后保存

```

当前第一优先级：

> 阶段 4 代码实现和 Debug/Release 构建已完成。下一步进入阶段 5，
> 先冻结 Jetson 与 STM32 的正式底盘控制协议、控制序号语义和差速运动学边界，
> 不提前展开 Bootloader、OTA 或 ROS 2 Bridge 实现。
