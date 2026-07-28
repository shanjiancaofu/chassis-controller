# OTA V1 传输协议

## 范围

OTA V1 由 Application 接收完整 `.ota` 包并写入 QSPI candidate slot。Bootloader
不直接接收 UART 或 CAN FD 数据，只负责校验、安装、试运行计数和回滚。

UART 与 CAN FD 使用相同的消息语义和 stop-and-wait 流程。任一时刻只能有一个
`source + session_id` 持有 OTA 会话。发送端收到当前消息的 `OK` 响应后才能发送
下一条消息。

`BEGIN` 的首个 `OK` 只表示会话已接受。设备擦除 candidate slot 时状态为
`PREPARING`；发送端必须继续查询 `STATUS`，直到状态为 `RECEIVING` 后才能发送首个
`DATA`。`END` 后同样应等待 `FINALIZING` 结束并确认最终状态为 `STAGED`。

## 通用消息

| 消息 | `argument` | 数据 | 说明 |
| --- | --- | --- | --- |
| `BEGIN` | `.ota` 总字节数 | 整包 CRC32，4 字节小端 | 建立会话并擦除 candidate slot |
| `DATA` | 当前包偏移 | 原始 `.ota` 数据 | 偏移必须等于设备返回的 `next_offset` |
| `END` | `.ota` 总字节数 | 整包 CRC32，4 字节小端 | 完成镜像和整包校验并提交 `STAGED` |
| `ABORT` | `0` | 无 | 放弃当前会话，保持停车 |
| `STATUS` | `0` | 无 | 查询状态，不改变会话 |

CRC32 使用 reflected CRC-32/ISO-HDLC：多项式 `0xEDB88320`、初值
`0xFFFFFFFF`、最终异或 `0xFFFFFFFF`。`.ota` 包仍包含独立的镜像头 CRC32 和
payload CRC32。

响应包含 `result`、当前 transfer state、`session_id` 和 `next_offset`。发生序号、
CRC、超时或 I/O 错误后必须停止当前会话，不得继续旧 offset。

## CAN FD

- 请求 ID：`0x730`，Jetson -> STM32。
- 响应 ID：`0x731`，STM32 -> Jetson。
- 11 位标准 ID，CAN FD+BRS，固定 64 字节。
- 仲裁速率 500 kbit/s，数据速率 2 Mbit/s。

请求帧：

| Byte | 字段 |
| ---: | --- |
| 0 | 协议版本，固定 `1` |
| 1 | 消息类型 |
| 2 | `session_id` |
| 3 | DATA 有效长度，范围 `0..56` |
| 4-7 | `argument`，小端 |
| 8-63 | 数据，不足部分必须填零 |

响应帧同样固定 64 字节：Byte 0 为版本，Byte 1 为 `STATUS`，Byte 2 为
`session_id`，Byte 3 为 `result`，Byte 4 为 transfer state，Byte 5-7 保留为零，
Byte 8-11 为 `next_offset`，其余填零。

CAN 链路 CRC 不能代替 `.ota` 的端到端 CRC32。Bus-Off、Error-Passive、FIFO lost
或协议错误必须撤销 CAN OTA 会话。

## UART

USART1 使用 `115200 8N1`、RX circular DMA + IDLE、软件环形缓冲和 TX DMA 队列。
进入 UART OTA 模式前必须通过 Console 显式执行维护命令，随后 UART 字节流按二进制
帧解析，结束或中止后才返回 Console。

维护命令为 `ota uart confirm`。命令进入二进制模式后 30 秒内未收到合法 `BEGIN`，
设备自动回到文本 Console 并保持停车。进入二进制模式时文本遥测会关闭，避免混入
二进制响应流。UART 已进入维护模式时只接受 UART 的 BEGIN，CAN FD 不得抢占该
维护会话。

请求与响应均使用以下封装：

| Byte | 字段 |
| ---: | --- |
| 0-1 | Magic：`A5 5A` |
| 2 | 协议版本，固定 `1` |
| 3 | 消息类型 |
| 4 | `session_id` |
| 5 | 数据长度，范围 `0..240` |
| 6-7 | 保留，固定为零 |
| 8-11 | `argument`，小端 |
| 12.. | 数据 |
| 末尾 4 字节 | 从 Magic 到数据末尾的帧 CRC32，小端 |

响应使用 `STATUS` 类型，`argument` 为 `next_offset`，数据长度为 2；Data 0 是
`result`，Data 1 是 transfer state。

UART 帧 CRC 只保护当前串口帧，整包仍按 `BEGIN/END` 携带的 CRC32 验证。

## 安全准入

建立 OTA 会话前必须满足：

- 控制状态为 `STOPPED`；
- 左右 PWM 都为零；
- 目标速度为零并撤销普通控制源；
- QSPI 人工测试、IWDG 复位测试和电机目标测试均未运行；
- 当前没有另一 OTA source 持有会话。

OTA 开始后锁定维护所有权。CAN 控制帧、Console 速度目标和危险目标测试均不得取得
控制权。成功、失败、超时或中止后仍保持停车；只有明确结束会话后才释放维护所有权。

## 兼容规则

- 未知版本、消息类型、非零保留字段或越界长度必须拒绝。
- DATA 必须严格连续，不接受重复、跳跃或乱序 offset。
- V1 不支持压缩、加密、签名和 Bootloader Recovery。
- 后续改变字段含义或状态语义时必须提升协议版本。
