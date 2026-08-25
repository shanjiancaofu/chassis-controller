# CAN FD 协议

本文只定义 STM32 与 Jetson 的 CAN FD 线协议。实现和验证状态不以本文为准；当前摘要见
[`../docs/current_status.md`](../docs/current_status.md)，实物证据见
[`../docs/verification.md`](../docs/verification.md)。

协议唯一机器可读定义为 [`chassis_canfd.yaml`](chassis_canfd.yaml)。本文解释语义，字段 ID、
偏移、长度、单位、版本和 CRC 规则必须与 schema 一致。当前 `0x100/0x101`、开发握手以及
`0x180/0x181/0x200/0x240` 均已接入 Application 运行链路；目标板和 Jetson 联调状态只记录在
验证文档中。

## 总线参数

- 帧格式：CAN FD，开启 BRS。
- 仲裁速率：500 kbps。
- 数据速率：2 Mbps。
- Jetson 仲裁段和数据段采样点：80%。
- 标识符：11 位标准 ID。
- 字节序：多字节整数使用小端序。

## 节点

| 节点 | 职责 |
| --- | --- |
| Jetson | 下发运行模式和底盘控制命令 |
| STM32G474 | 执行控制、采集反馈并上报状态和故障 |

## 报文 ID

| ID 范围 | 方向 | 用途 | 当前状态 |
| --- | --- | --- | --- |
| `0x100` | Jetson -> STM32 | Legacy wheel raw 控制 | 仅开发兼容，要求开发握手 |
| `0x101` | Jetson -> STM32 | 正式物理速度控制 | V1 正式接口，不依赖开发握手 |
| `0x180`-`0x1FF` | STM32 -> Jetson | 状态反馈 | Motion/Odometry 已实现 |
| `0x200`-`0x23F` | 双向 | 心跳和运行状态 | 双向 Heartbeat 已实现 |
| `0x240`-`0x27F` | STM32 -> Jetson | 故障和诊断 | Fault Status 已实现 |
| `0x280`-`0x2FF` | 双向 | 参数和管理 | ID 区间保留，消息尚未冻结 |
| `0x700`-`0x77F` | 双向 | 开发联调 | 仅开发阶段使用 |

OTA V1 使用 `0x730/0x731` 的固定 64 字节 CAN FD+BRS 帧，详细格式见
[`ota_canfd_protocol.md`](ota_canfd_protocol.md)。OTA 会话与运动控制会话互斥。

正式报文采用固定 ID 和固定长度，不在同一个 ID 下复用多种数据布局。

## Legacy Wheel Raw 控制

`0x100 CHASSIS_CMD_WHEEL_RAW` 是 8 byte CAN FD+BRS 标准帧：

| Byte | 字段 | 格式 |
| --- | --- | --- |
| 0 | 协议版本 | 固定为 `1` |
| 1 | 使能 | `0` 停车，`1` 闭环运行 |
| 2 | 计数器 | 每帧递增，`255` 后回到 `0` |
| 3 | 保留 | 固定为 `0` |
| 4-5 | 左轮目标 | `int16` 小端，单位 `counts/10 ms` |
| 6-7 | 右轮目标 | `int16` 小端，单位 `counts/10 ms` |

目标范围限制为 `-100` 到 `100 counts/10 ms`。使能为 `0` 时左右目标必须同时为 `0`。
该帧属于 `legacy/development compatibility`，只为既有联调脚本和底层轮控诊断保留；只有开发
握手状态为 `PASSED` 后才接收。新 Jetson 产品代码不得使用该帧。

有效控制帧中断超过 200 ms 后，STM32 清零四路 PWM、重置 PID 并进入 `CHASSIS_CONTROL_COMMAND_TIMEOUT`。恢复必须收到下一帧有效控制命令，旧输出不会自动恢复。

## 物理速度控制

`0x101 CHASSIS_CMD_VELOCITY` 为 12 byte：schema version、ENABLE flags、sequence、保留字节、
`linear_velocity i16 mm/s`、`angular_velocity i16 mrad/s`、保留字和 CRC16。线速度范围为
`-2000..2000 mm/s`，角速度范围为 `-10000..10000 mrad/s`；ENABLE 为零时两个目标必须为零。
Application 根据当前 `1320 counts/rev`、`65 mm` 轮径、`220 mm` 轮距和 `10 ms` 控制周期换算
左右轮目标，任一轮超过 `100 counts/tick` 时拒绝该命令，不刷新控制超时。

正式 Jetson V1 接口使用 `0x101`，不要求也不读取 `0x720/0x721` 开发握手状态；只要帧版本、
CRC、保留位、范围和 sequence 合法，就可以进入现有 CommandManager/SafetyManager。`0x100`
只作为 legacy/development compatibility 保留。
两种控制报文共用一条 rolling sequence；第一帧允许任意起始值，后续以无符号 `uint8` delta
判断：`delta=0` 为重复并拒绝，`1..127` 为前向帧并接受，其中 `delta>1` 表示中间丢帧，
`128..255` 为旧帧/倒序并拒绝。切换 ID 不能绕过该规则。合法 STOP 与普通控制使用相同前向规则，
因此中间丢帧不会阻挡停车。连续 200 ms 没有合法控制命令后清除 sequence baseline，Jetson 进程
重启后下一帧可用新的 sequence 建立基线。

## 双向 Heartbeat 与状态上报

Jetson 和 STM32 都以 100 ms 周期发送 `0x200 CHASSIS_HEARTBEAT`。每个方向独立维护 rolling
sequence、发送端 monotonic uptime、节点状态和故障摘要。接收端第一帧允许任意 sequence；后续只
接受前向 rolling sequence，重复、倒序、非法版本、保留位或 CRC 错误均不得刷新接收时间。
连续 300 ms 没有新的合法对端 heartbeat 时，对端状态变为 `TIMEOUT`；下一帧新的合法 sequence
可恢复为 `ALIVE`。

STM32 Heartbeat 始终按 100 ms 周期发送，Fault 始终允许按变化立即发送并以 100 ms 保活；二者不
依赖对端 heartbeat，避免双方启动时互相等待，也保证对端离线期间仍有故障诊断机会。
Motion/Odometry 以 Jetson heartbeat `ALIVE` 为发送门控。所有发送失败均不推进对应 rolling
sequence，并在后续 service 周期重试。OTA、QSPI 自检或电机自检持有维护资源时暂停
Motion/Odometry，但不停止 Heartbeat/Fault。

- `0x180 CHASSIS_STATUS_MOTION`：20 ms 周期，报告左右轮速度、底盘线/角速度、控制状态以及
  左右输出 permille。flags bit0 表示数据有效，bit1 表示闭环运行。
- `0x181 CHASSIS_REPORT_ODOMETRY`：20 ms 周期并与 Motion 错峰 5 ms，报告 monotonic
  timestamp、`x/y mm`、`heading mrad` 和线/角速度。flags bit0 表示里程计有效。
- `0x200 CHASSIS_HEARTBEAT`：双向 100 ms 周期，报告发送节点状态、uptime 和低 16 位当前故障
  摘要；flags bit0 表示发送节点已完成自身初始化并可参与正式链路。节点状态为 STARTING、READY、
  RUNNING、DEGRADED、FAULT 或 MAINTENANCE。
- `0x240 CHASSIS_FAULT_STATUS`：故障集合变化时立即发送，并以 100 ms 周期保活。active faults
  表示当前未清除故障，latched faults 表示本次启动以来曾出现的故障，fault sequence 在 active
  集合发生变化时递增。severity 为 NONE、WARNING 或 CRITICAL。

Motion/Odometry 使用 CAN FD 链路 CRC；Heartbeat/Fault 使用本文定义的项目级 CRC16。Heartbeat
只用于节点在线观测和状态上报门控，绝不调用 CommandManager，也不刷新、延长或替代 200 ms
运动命令 timeout。即使 heartbeat 持续正常，0x101 中断 200 ms 仍必须停车。

Fault 位定义：

| Bit | 名称 | 当前分类 |
| ---: | --- | --- |
| 0 | `COMMAND_TIMEOUT` | WARNING / 可恢复 |
| 1 | `EMERGENCY_STOP` | CRITICAL / 显式解除后恢复 |
| 2 | `CONTROL_OVERRUN` | CRITICAL |
| 3 | `INTERNAL` | CRITICAL |
| 4 | `ENCODER` | CRITICAL |
| 5 | `UNDERVOLTAGE` | CRITICAL |

未定义位必须发送为零；新增 fault bit 属于协议 minor 兼容扩展，既有位不得改义。

## 通用规则

- `0x100/0x101` 共用递增计数器；0x101 正式控制不依赖开发握手或 heartbeat 前置条件。
- 控制报文超时后必须进入安全状态，不能继续保持最后一次运动命令。
- heartbeat 只能说明对端节点在线，不能代替控制命令或执行器状态反馈。
- 故障报文需要区分当前故障和历史故障。
- 普通状态和旧 `0x100` 使用 CAN FD 链路 CRC；新控制、Heartbeat 和 Fault schema 使用项目级
  CRC16-CCITT-FALSE（poly `0x1021`、init `0xFFFF`、xorout `0`、不反射）。输入为 CAN ID
  两字节小端加不含 CRC 字段的 payload。该 guard 不宣称 AUTOSAR E2E 或功能安全合规。
- 协议不直接传输结构体内存，所有字段必须明确偏移、长度、单位和缩放比例。

## 数据类型与单位

- 固定报文只使用 `bool/u8/i8/u16/i16/u32/i32/u64/i64/enum8/flags/bytes[N]`。
- 多字节整数小端；不在线上传输 C enum、C struct、原生 bool、float、double 或 `\0` 结尾字符串。
- 物理量使用整数缩放：速度 `mm/s`、角速度 `mrad/s`、位置 `mm`、角度 `mrad`、电压 `mV`、
  PWM 输出 `permille`。时间戳为发送节点 monotonic uptime `uint32 ms`，不是 RTC。
- UTF-8 仅允许用于后续管理/诊断变长消息，编码为 `u8 length + bytes`，不用于实时状态帧。
- 固定实时帧不携带逐字段 type；参数协议才使用 `data_type + parameter_id + length + value`。

## 版本规则

- 总协议版本为 `1.0`：major 表示不兼容修改，minor 表示兼容新增。
- 每个正式消息 Byte 0 为 `schema_version`；未知版本必须拒绝，不能猜测字段布局。
- 控制、周期状态和 heartbeat 使用 `uint8` rolling sequence；heartbeat 不得刷新 200 ms 运动命令超时。

## 开发联调握手

`0x720/0x721 CHASSIS_DEV_HANDSHAKE_*` 仅用于开发阶段验证两端控制器、收发器、接线、终端电阻和
双向收发。它维护独立的 development handshake 状态，不属于正式 V1 会话，不门控 0x101、
heartbeat 或正式状态报文。

| 步骤 | CAN ID | 方向 | Payload |
| --- | --- | --- | --- |
| 请求 | `0x720` | Jetson -> STM32 | `50 49 4E 47 01 00 00 00` |
| 响应 | `0x721` | STM32 -> Jetson | `43 48 41 53 53 49 53 01` |
| 确认 | `0x720` | Jetson -> STM32 | `50 41 53 53 01 00 00 00` |

三帧均为 8 byte CAN FD+BRS 标准帧。握手失败不得使合法 0x101 失效；只有 legacy `0x100`
明确要求握手 `PASSED`。该握手本身不会启动电机。

## 尚未冻结

以下内容必须在电机、转向、制动和传感器接口确认后再定义：

- 转角、独立制动命令以及电流和温度反馈。
- 物理速度控制允许误差。
- 新硬件故障来源、降级控制策略和关键故障恢复条件。
