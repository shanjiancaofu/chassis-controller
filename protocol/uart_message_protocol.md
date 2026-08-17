# UART 消息协议（阶段 2 草案）

本文定义 Application 文本 UART 的正式消息格式。实现前先冻结字段顺序、错误码和换行规则；
OTA 二进制帧不属于本文，进入 OTA 二进制模式后禁止混入文本。

## 通用规则

- 编码为 ASCII，字段使用 `key=value`，字段之间使用一个空格。
- 每条文本消息只有一行，以 `CRLF`（`\r\n`）结束，不发送裸 `LF`。
- 时间戳统一为 `ts_ms`，取 Application 启动后的毫秒 uptime，使用无符号十进制。
- 协议版本统一为 `v=1`；固件产品版本单独使用 `fw=`，不替代协议版本。
- 字段名称使用小写 ASCII；枚举和错误码使用大写下划线命名。
- 接收端必须按整行处理，未知字段可忽略，未知消息类型或缺少必需字段返回 `ERROR`。

## 命令响应

命令完成后只发送一条响应，不再发送没有字段约定的 demo 文本：

```text
OK ts_ms=123 v=1 command=ping
ERROR ts_ms=124 v=1 command=pid_target code=INVALID_ARGUMENT
```

成功响应可以附加命令定义的结果字段；失败响应必须包含 `code=`，可选 `detail=`。首批错误码：

`INVALID_ARGUMENT`、`BUSY`、`NOT_OWNER`、`SAFETY_STOP`、`TIMEOUT`、`UNAVAILABLE`、
`OTA_ACTIVE`、`INTERNAL`。

## 异步日志

日志不能伪装成命令响应，格式为：

```text
LOG ts_ms=125 level=INFO module=control event=STARTED v=1
LOG ts_ms=126 level=ERROR module=qspi event=READ_FAILED code=IO_ERROR v=1
```

`level` 取 `DEBUG`、`INFO`、`WARN`、`ERROR`；`module` 和 `event` 使用固定注册名。
日志不得进入 `control_task`，不得在 ISR 中打印。

启动阶段可以采用类似 Linux 启动日志的逐行顺序，但每一行仍必须遵守上述固定字段规则，
不能退回无结构的自由文本。例如：

```text
LOG ts_ms=0 level=INFO module=boot event=STARTED v=1
LOG ts_ms=3 level=INFO module=board event=GPIO_READY v=1
LOG ts_ms=8 level=INFO module=qspi event=READY jedec=0xEF4017 v=1
LOG ts_ms=12 level=WARN module=imu event=NOT_FOUND code=UNAVAILABLE v=1
LOG ts_ms=15 level=INFO module=application event=READY v=1
```

启动日志只报告初始化阶段发生的事件；周期状态改用 `TEL`，命令结果改用 `OK`/`ERROR`。
事件名、模块名和错误码需要注册后保持稳定，便于上位机按字段解析和定位启动失败位置。

## 周期遥测

遥测以固定周期发送，序号从 0 开始递增并在溢出时回绕：

```text
TEL v=1 seq=42 ts_ms=1000 fw=0.1.0-b16 control=RUNNING left_target=0 right_target=0 fault=0
```

同一遥测流的字段顺序固定；没有有效值时使用 `valid=0` 和约定的零值，不使用 `UNKNOWN` 混合
类型。遥测周期、启停命令和字段集合由后续实现文档冻结。

## OTA 隔离

进入 UART OTA 二进制模式后，Application 只接收和发送 OTA 二进制帧，不发送 `OK`、`ERROR`、
`LOG` 或 `TEL` 文本。等待 BEGIN 超时、传输完成和失败状态通过 OTA 二进制响应表达；退出并
确认 DMA 空闲后，才恢复文本模式。
