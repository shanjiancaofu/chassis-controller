# Chassis Controller V1 验证总览

日期：2026-09-03。固件：`1.0.9 build3`。详细历史与原始摘要见
[`verification.md`](verification.md) 和 [`verification/chassis/`](verification/chassis/)。

| Feature | Software | Target | Metrics / Evidence | Deferred |
|---|---|---|---|---|
| ARM Debug/Release + host CI | PASS | — | 21 C host tests、四组 config matrix、ARM GNU 14.3.1 | — |
| PowerReady | PASS | PASS | 0mV 时非零目标返回 `SAFETY_STOP`，PWM=0/0 | ADC 物理断线 |
| HardFault / IWDG / symbolication | PASS | PASS | `EXTENDED_FP`，PC 定位 `crash_context.c:49` | — |
| Encoder API failure fail-close | PASS | PASS | 四组 software injection：fault `0x10`、同 snapshot PWM=0/0 | physical unplug |
| ControlTask execution profile | PASS | PASS | 1ms period；5分钟30万周期、±50 motion max 33us，deadline miss=0 | ISR→task jitter |
| Wheels-up closed-loop | PASS | PASS | build3 `±50` 双侧收敛且无PWM异常归零；停止PWM=0 | `±100`为1.0.8历史证据；带负载PID标定 |
| CAN FD timeout safety | PASS | PASS | 0x101、0x180/181/200/240、200ms command、300ms peer timeout | physical unplug/bus-off |
| Odometry direction | PASS | PASS | forward x+、reverse x-、left yaw+、right yaw-、stop velocity=0 | metric calibration |

## 身份与范围

- Firmware source：`bbfc833b1e3bc6d5c372e7c85fcdaf844674c5ad`。
- Evidence commit：`f8c4d4167ad1bb34be49c28bbc826452c879e951`。
- BIN SHA-256：`e0ec13a28626bc3979a76045b105958c895f857bc02af96dec2ff1f98352f37b`。
- OTA SHA-256：`c133a75e4f7ca68c73076253cc118ddad4ad886522d99bbf06291a24200ab18d`。
- Physical encoder/CAN unplug 不属于 V1 Gate；V1 只声明软件注入和正常链路范围内通过。
- `396/84/84/84ms` 是 diagnostic observation interval，不是精确 ControlTask fault-latch latency。
