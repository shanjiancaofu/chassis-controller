# Chassis Controller V1 验证总览

日期：2026-09-02。固件：`1.0.8 build1`。详细历史与原始摘要见
[`verification.md`](verification.md) 和 [`verification/chassis/`](verification/chassis/)。

| Feature | Software | Target | Metrics / Evidence | Deferred |
|---|---|---|---|---|
| ARM Debug/Release + host CI | PASS | — | 20 C host tests、四组 config matrix、ARM GNU 14.3.1 | — |
| PowerReady | PASS | PASS | 0mV 时非零目标返回 `SAFETY_STOP`，PWM=0/0 | ADC 物理断线 |
| HardFault / IWDG / symbolication | PASS | PASS | `EXTENDED_FP`，PC 定位 `crash_context.c:49` | — |
| Encoder API failure fail-close | PASS | PASS | 四组 software injection：fault `0x10`、同 snapshot PWM=0/0 | physical unplug |
| ControlTask execution profile | PASS | MEASURED | 10ms period，idle/motion max 29us，deadline miss=0 | ISR→task jitter |
| Wheels-up closed-loop | PASS | PASS | `±50/±100` 双侧反馈、停止 PWM=0 | 带负载 PID 标定 |
| CAN FD timeout safety | PASS | PASS | 0x101、0x180/181/200/240、200ms command、300ms peer timeout | physical unplug/bus-off |
| Odometry direction | PASS | PASS | forward x+、reverse x-、left yaw+、right yaw-、stop velocity=0 | metric calibration |

## 身份与范围

- Firmware source：`c17d89a44c25a354f4c585ce310a880e6b9065d7`。
- Evidence baseline：`f51f436fd6ec4a22a5abb40adcc1e8238268bc29`。
- BIN SHA-256：`a0fb1b2b405bfda3ea3098de5decefb87f774b3cfde63c8258fc4961c01355e7`。
- OTA SHA-256：`1aec5fbedf3218bba502072ed4da00dc14290473c6ac34dcb600f6e74e8c2ea6`。
- Physical encoder/CAN unplug 不属于 V1 Gate；V1 只声明软件注入和正常链路范围内通过。
- `396/84/84/84ms` 是 diagnostic observation interval，不是精确 ControlTask fault-latch latency。
