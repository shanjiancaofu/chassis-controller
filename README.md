# chassis-controller

STM32G474VET6 底盘控制器。STM32 负责双电机闭环、安全停车、板级采集和
CAN FD 通信；Jetson Orin Nano 负责上层控制、诊断和固件发送。本仓库与
`cockpit-system` 独立维护。

## V1 状态

当前已冻结版本为 `Application 1.0.9 build3`、`Bootloader 0.1.0 build22`。ControlTask 由 TIM6
以 1ms 周期唤醒，在 STM32G474 上形成 1kHz 双轮闭环；命令、PID 参数和遥测继续使用 10ms
等效 encoder-count 单位，以保持既有 CAN/UART 行为和实测调参语义。

```text
Jetson command
    → CAN FD codec / sequence / heartbeat
    → authority + SafetyManager / FaultManager
    → 1kHz ControlTask
    → 10ms sliding encoder window
    → PID + startup boost + feedback watchdog
    → paired TIM8 motor PWM
```

V1 目标板结果：

| 项目 | 结果 | 关键证据 |
|---|---|---|
| 1kHz ControlTask | PASS | 5分钟约30万周期，motion WCET max 33us，0 deadline miss |
| Wheels-up closed loop | PASS | build3 `±50` 双轮收敛、无PWM异常归零，停止PWM=0/0 |
| PowerReady / fail-close | PASS | 无有效供电拒绝运动；fault/timeout清命令并停双轮 |
| Encoder failure injection | PASS | forward/reverse × left/right，fault `0x10`，同snapshot PWM=0/0 |
| HardFault post-mortem | PASS | PWM清零、`.noinit` context、IWDG reset、同ELF `addr2line` |
| UART OTA / provenance | PASS | `1.0.9 build3` CONFIRMED，clean source + ELF/BIN/OTA SHA256 |
| CAN timeout / odom direction | PASS | command/heartbeat timeout；forward x+、reverse x-、yaw双向 |

V1 不声明物理 encoder/CAN 拔线、带负载 PID 标定、精确 odom 几何标定或真实 Nav2 运动；这些项目
明确留在 V2，而不是用软件测试冒充硬件结果。

最新摘要见 [`docs/current_status.md`](docs/current_status.md)，完整证据见
[`docs/verification.md`](docs/verification.md)。

V1 最终入口：

- [V1 验证总览](docs/v1_validation_overview.md)
- [V1 架构与面试要点](docs/v1_architecture_interview.md)
- [V1 求职材料包](docs/portfolio_pack.md)

## 目录

```text
firmware/
├─ application/stm32g474/   # Application：cubemx/ + 项目自维护代码
├─ bootloader/stm32g474/    # 裸机 Bootloader CubeMX/CubeIDE 工程
└─ shared/                  # Bootloader、Application、打包器共用固定 ABI
docs/                       # 架构、路线、硬件、验证和 OTA 设计
protocol/                   # CAN FD 线协议
tools/ota/                  # 打包、factory 镜像和 UART/CAN FD OTA 工具
tools/test/                 # host/config matrix 统一测试入口
tools/target/               # 真实目标板轻量自动回归
```

Application 自有代码位于：

```text
app/  boards/  drivers/  lib/  subsys/communication/
kernel/  tests/  config/  dts/  cubemx/
```

## 文档

- [文档索引与多对话工作方式](docs/README.md)
- [当前开发状态](docs/current_status.md)
- [系统架构](docs/architecture.md)
- [开发路线](docs/roadmap.md)
- [硬件与接线](docs/hardware.md)
- [构建与实物验证](docs/verification.md)
- [Bootloader 与 OTA](docs/bootloader_and_ota.md)
- [CAN FD 协议](protocol/canfd_protocol.md)
- [OTA V1 传输协议](protocol/ota_canfd_protocol.md)
- [Codex/AI 修改规则](AGENTS.md)

同一信息只在一份权威文档中维护。新对话先读取 `AGENTS.md` 和
`docs/current_status.md`，再按 `docs/README.md` 加载任务相关文档。代码和配置发生变化时，
按文档职责更新；合并时另一侧缺失的内容不得直接视为应删除。

## 构建

主要工具链：

- CMake 3.22 + Ninja
- GNU Arm Embedded 14.3.rel1
- STM32CubeMX 6.18 + STM32CubeG4 V1.6.3
- ST-Link/OpenOCD 或 STM32CubeProgrammer

日常命令行构建使用 `cmake --preset arm-debug` 或 `arm-release`，详见
[`docs/cmake_build.md`](docs/cmake_build.md)。CubeIDE 暂时保留作对照和备用调试，其工作区使用
仓库的 `firmware/`，导入两个独立工程：

```text
firmware/application/stm32g474
firmware/bootloader/stm32g474
```

日常流程：

1. 使用 CMake preset 构建 Debug 或 Release。
2. 使用 `python3 tools/test/run_all.py` 运行 host、配置矩阵、协议和预览回归。
3. VS Code F5 通过 Cortex-Debug/OpenOCD 自动构建、烧写并停在 `main`。
4. 修改 `.ioc` 后由 CubeMX 重新生成，再检查 diff、DTS 一致性和 CMake 显式源文件列表。
5. CH340 使用 `115200 8N1` 查看启动、自检、命令和遥测。
6. 上板后使用 `tools/target/run_target_regression.py` 自动生成 PASS/FAIL 和 JSON 报告。
7. 上板软件 encoder fault Gate 使用 `tools/target/run_software_fault_injection.py`；该入口不需要、也不允许物理拔线。

CubeMX 生成代码时使用 `STM32CubeIDE` 并勾选 `Generate Under Root`。
生成文件只在 `USER CODE BEGIN/END` 区域内手工修改。

## 提交规范

提交信息使用 `[type]: description` 格式：

```text
[feature]: add ...
[fix]: handle ...
[refactor]: organize ...
[docs]: update ...
```

- `[feature]`：新增功能。
- `[fix]`：修复缺陷。
- `[refactor]`：调整代码或目录结构，不改变外部行为。
- `[docs]`：只修改文档。
