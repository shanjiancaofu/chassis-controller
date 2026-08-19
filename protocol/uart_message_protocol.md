# UART 消息协议（阶段 2）

本文定义 Application 文本 UART 的正式消息格式。协议版本 1 已实现，但字段集合和分区集合不
冻结。v1 保持 `[RSP]`、`[LOG]`、`[TEL]` 外壳及已有字段语义兼容；增加字段、增加分区或增加
命令不提升协议版本。接收端按标签、`section` 和字段名解析，必须忽略未知字段及未知分区。
只有改变消息外壳或已有字段含义等破坏性变化才提升协议版本。OTA 二进制帧不属于本文，进入
OTA 二进制模式后禁止混入文本。

## 通用规则

- 编码为 ASCII，字段使用 `key=value`，字段之间使用一个空格。
- 每条文本消息只有一行，以 `CRLF`（`\r\n`）结束，不发送裸 `LF`。
- 每行以固定方括号标签开头，方括号属于线协议的一部分。正式文本标签只有 `[RSP]`、`[LOG]`
  和 `[TEL]`；标签后依次为 `v=1`、`ts_ms=...`，再跟随该类消息的专用字段。
- 时间戳统一为 `ts_ms`，取 Application 启动后的毫秒 uptime，使用无符号十进制。
- 协议版本统一为 `v=1`；固件产品版本使用 `fw=`，构建号单独使用 `build=`，两者都不替代协议版本。
- 字段名称使用小写 ASCII；枚举和错误码使用大写下划线命名。
- 接收端必须按整行处理，不依赖字段位置、字段总数或固定分区总数；未知字段和未知分区可忽略，
  未知命令或缺少必需字段返回 `[RSP] result=ERROR`。

## 命令响应

命令完成后只发送一条响应，不再发送没有字段约定的 demo 文本：

```text
[RSP] v=1 ts_ms=123 result=OK command=ping
[RSP] v=1 ts_ms=124 result=ERROR command=pid_target code=INVALID_ARGUMENT
```

成功响应可以附加命令定义的结果字段；失败响应必须包含 `code=`，可选 `detail=`。首批错误码：

`INVALID_ARGUMENT`、`BUSY`、`NOT_OWNER`、`SAFETY_STOP`、`TIMEOUT`、`UNAVAILABLE`、
`OTA_ACTIVE`、`INTERNAL`。

### 命令注册表

| 输入命令 | `command=` | 成功结果字段 | 主要失败码 |
| --- | --- | --- | --- |
| `help` | `help` | `commands` | - |
| `ping` | `ping` | - | - |
| `status` | `status` | `stream=diagnostics`，随后发送四行同序号 `[TEL]` | - |
| `telemetry on\|text\|vofa\|off` | `telemetry` | `mode`，VOFA 另有 `compatibility=LEGACY` | `INVALID_ARGUMENT` |
| `can status` | `can_status` | CAN 诊断字段 | `UNAVAILABLE` |
| `can tx confirm` | `can_tx` | `frame`、`state` | - |
| `pid show` | `pid_show` | 左右轮 `kp/ki/kd` | - |
| `pid left\|right <kp> <ki> <kd>` | `pid_set` | 更新后的左右轮 `kp/ki/kd` | `INVALID_ARGUMENT` |
| `pid target <left> <right>` | `pid_target` | `state=RUNNING` | `INVALID_ARGUMENT`、`NOT_OWNER`、`BUSY`、`SAFETY_STOP` |
| `pid stop` | `pid_stop` | `state=STOPPED` | - |
| `encoder zero` | `encoder_zero` | `state=RESET` | `SAFETY_STOP` |
| `encoder result` | `encoder_result` | `left_total`、`right_total` | - |
| `odometry reset` | `odometry_reset` | `state=RESET` | `SAFETY_STOP` |
| `ota uart confirm` | `ota_uart` | `mode=BINARY` | `BUSY` |
| `qspi test confirm` | `qspi_test` | `state=STARTED` | `BUSY` |
| `iwdg reset confirm` | `iwdg_reset_test` | `state=ARMED` | `BUSY` |
| `motor stop` | `motor_stop` | `state=STOPPED` | - |
| `motor <left\|right> <forward\|reverse> confirm` | 对应 `motor_left_forward` 等 | `state=STARTED` | `BUSY` |

无法完整解析的输入统一返回 `command=unknown code=INVALID_ARGUMENT`。`pid_target` 先取得控制
所有权并确认安全状态确实进入运行后才返回 `OK`；其他控制源占用时返回 `NOT_OWNER`，开环测试
占用控制状态时返回 `BUSY`，急停或 critical fault 返回 `SAFETY_STOP`。

## 异步日志

异步日志使用固定 `[LOG]` 标签，级别放在 `level=` 字段中：

```text
[LOG] v=1 ts_ms=125 level=INFO module=control event=STARTED
[LOG] v=1 ts_ms=126 level=ERROR module=qspi event=READ_FAILED code=IO_ERROR
```

`level` 取 `DEBUG`、`INFO`、`WARN`、`ERROR`；`module` 和 `event` 使用固定注册名。
日志不得进入 `control_task`，不得在 ISR 中打印。

启动阶段可以采用类似 Linux 启动日志的逐行顺序，但每一行仍必须遵守上述固定字段规则，
不能退回无结构的自由文本。例如：

```text
[LOG] v=1 ts_ms=0 level=INFO module=boot event=STARTED fw=0.1.0-b16 reset=POWER_ON
[LOG] v=1 ts_ms=3 level=INFO module=board event=GPIO_READY
[LOG] v=1 ts_ms=8 level=INFO module=qspi event=READY jedec=0xEF4017
[LOG] v=1 ts_ms=12 level=WARN module=imu event=NOT_FOUND code=UNAVAILABLE
[LOG] v=1 ts_ms=15 level=INFO module=application event=READY
```

启动日志只报告初始化阶段发生的事件；周期状态改用 `[TEL]`，命令结果改用 `[RSP]`。
事件名、模块名和错误码需要注册后保持稳定，便于上位机按字段解析和定位启动失败位置。

当前事件注册表：

| `module` | `event` |
| --- | --- |
| `boot` | `STARTED` |
| `board` | `FDCAN_READY`、`FDCAN_INIT_FAILED`、`MOTION_IO_READY`、`MOTION_IO_INIT_FAILED` |
| `motor` | `INIT_FAILED` |
| `sr501` | `WARMING_UP` |
| `imu` | `READY`、`NOT_FOUND`、`INIT_FAILED` |
| `odometry` | `READY`、`INIT_FAILED` |
| `application` | `READY` |
| `ota` | `UART_ARM_TIMEOUT` |
| `iwdg` | `RESET_TEST_ARMED` |
| `qspi` | `RW_TEST` |

## 周期遥测

遥测以固定周期发送，序号从 0 开始递增并在溢出时回绕：

```text
[TEL] v=1 ts_ms=1000 seq=42 section=system fw=0.3.0 build=1 control=RUNNING left_target=0 right_target=0 fault=0
```

没有有效值时使用 `valid=0` 和约定的零值，不使用 `UNKNOWN` 混合类型。当前 `status` 命令返回
`system`、`motor`、`sensors`、`communication` 四行，这四行共享同一 `seq`，便于上位机组装
一次完整快照。四分区只是当前完整诊断视图，不是所有功能都必须扩充的固定容器：新字段只有在
确有长期诊断价值且职责归属明确时才加入；独立领域会使现有分区明显膨胀时，可在 v1 增加新的
`section` 或专用命令结果。不得为同一含义创建不同字段别名。

周期 `telemetry text` 是面向连续观察的精简视图，不要求复制完整 `status`。字段只按实际观察
需求选择；完整诊断仍由 `status` 提供。VOFA 数字流仅在显式 `telemetry vofa` 命令下启用，
属于兼容模式，不与正式 `[TEL]` 字段混用。

### 遥测字段注册表

当前四分区 `status` 字段如下。该表记录已实现线格式，不表示字段集合冻结：

```text
system: fw build uptime_ms supply_valid supply_mv control fault reset reset_flags critical_tasks
        service_task service_period_ms service_expected_ms service_timeout_ms service_age_ms
        service_runs service_stack_free_words control_task control_period_ms control_expected_ms
        control_timeout_ms control_age_ms control_runs control_stack_free_words diagnostics_task
        diagnostics_period_ms diagnostics_expected_ms diagnostics_timeout_ms diagnostics_age_ms
        diagnostics_runs diagnostics_stack_free_words display_task display_period_ms
        display_expected_ms display_timeout_ms display_age_ms display_runs display_stack_free_words
motor: control left_target left_speed left_pwm right_target right_speed right_pwm left_encoder
       right_encoder left_kp left_ki left_kd right_kp right_ki right_kd motor_test overrun missed
       odom_valid odom_ts_ms odom_period_ms odom_age_ms odom_x_mm odom_y_mm
       odom_heading_mrad odom_linear_mm_s odom_angular_mrad_s
sensors: rtc_valid rtc adc_valid adc_mv adc_ts_ms adc_age_ms imu imu_whoami imu_samples
         imu_fifo_frames imu_fifo_errors imu_timestamp_errors imu_fifo_ts imu_ts_ms imu_age_ms
         imu_kalman imu_kalman_roll_mrad imu_kalman_pitch_mrad sr501
         sr501_raw sr501_motion sr501_count sr501_last_ms sr501_warmup_ms button1_pressed
         button1_count button2_pressed button2_count
communication: can can_drops uart_errors qspi_read qspi_id qspi_jedec qspi_capacity_bytes
               qspi_test ota_confirmation ota_source ota_state ota_offset lcd telemetry iwdg_test
```

`odom_ts_ms` 是编码器控制采样点的本地单调毫秒时间；`imu_fifo_ts` 是 ICM45686 设备时间戳，
`imu_ts_ms` 是按 FIFO 队列深度映射后的本地单调时间；`adc_ts_ms` 是 ADC 转换完成时间。所有
`*_age_ms` 均使用无符号时间差计算并支持 32 位 uptime 回绕。里程计输出单位固定为毫米、毫弧度、
毫米每秒和毫弧度每秒。

周期 `telemetry text` 使用 `section=motor`，字段依次为 `supply_mv`、`left_target`、
`left_delta`、`left_rpm_x10`、`left_total`、`left_pwm`、`right_target`、`right_delta`、
`right_rpm_x10`、`right_total`、`right_pwm`、`control`、`fault`、`odom_valid`、`odom_ts_ms`、
`odom_age_ms`、`odom_x_mm`、`odom_y_mm`、`odom_heading_mrad`、`odom_linear_mm_s` 和
`odom_angular_mrad_s`。协议版本 1 内不复用已有字段表达不同含义。

## OTA 隔离

进入 UART OTA 二进制模式后，Application 只接收和发送 OTA 二进制帧，不发送 `[RSP]`、`[LOG]` 或
`[TEL]` 文本。
等待 BEGIN 超时、传输完成和失败状态通过 OTA 二进制响应表达；退出并
确认 DMA 空闲后，才恢复文本模式。

主机升级工具为从旧文本固件迁移到协议版本 1，可额外接受旧固件的精确准备行
`OTA_UART: READY, binary mode`；这只属于发送工具的单向升级兼容，不允许新 Application
继续发送旧格式，也不放宽其他响应字段解析。
