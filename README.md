# chassis-controller

STM32G474VET6 底盘控制器。STM32 负责双电机闭环、安全停车、板级采集和
CAN FD 通信；Jetson Orin Nano 负责上层控制、诊断和固件发送。本仓库与
`cockpit-system` 独立维护。

## 当前状态

当前处于 OTA V1 持续开发和 Application 原生平台化阶段。独立 Bootloader、Application 重定位、
UART/CAN FD 接收、QSPI 暂存、安装、试运行确认和回滚链路已完成代码与构建。2026-08-13
已写入并读回匹配的 b12/build22 factory 与 QSPI confirmed 基线。2026-08-17 已通过 UART OTA
将 b13 安装、TRIAL 验证并提交为 confirmed；b13 在未连接 ICM45686 时正常启动，普通复位后
保持稳定。此前冻结范围已解除；断电启动、零 PWM、安装/回滚故障注入、CAN FD OTA、PID、
里程计和传感器动态验收均可与软件开发并行推进，未实测内容继续标记为 `NOT VERIFIED`。
最新摘要见
[`docs/current_status.md`](docs/current_status.md)，完整证据见 `docs/verification.md`。
OTA recovery、CAN FD OTA、PID、轮组标定、加减速、里程计、安全保护、诊断和正式 CAN FD
底盘协议都保持在活动路线中，按实现批次和硬件条件分别验证，不以某一项硬件验收阻塞其他开发。

## 目录

```text
firmware/
├─ application/stm32g474/   # 正式 Application CubeMX/CubeIDE 工程
├─ bootloader/stm32g474/    # 裸机 Bootloader CubeMX/CubeIDE 工程
└─ shared/                  # Bootloader、Application、打包器共用固定 ABI
docs/                       # 架构、路线、硬件、验证和 OTA 设计
protocol/                   # CAN FD 线协议
tools/ota/                  # 打包、首次组合镜像和 UART/CAN FD 发送工具
example/                    # 本地参考例程，不提交
```

Application 自有代码位于：

```text
app/  boards/  drivers/  components/  communication/
subsys/  modules/  kernel/  rtos/  tests/  config/
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
2. VS Code F5 通过 Cortex-Debug/OpenOCD 自动构建、烧写并停在 `main`。
3. 修改 `.ioc` 后由 CubeMX 重新生成，再检查 diff 和 CMake 显式源文件列表。
4. CH340 使用 `115200 8N1` 查看启动、自检、命令和遥测。

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
