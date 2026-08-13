# chassis-controller

STM32G474VET6 底盘控制器。STM32 负责双电机闭环、安全停车、板级采集和
CAN FD 通信；Jetson Orin Nano 负责上层控制、诊断和固件发送。本仓库与
`cockpit-system` 独立维护。

## 当前状态

当前处于 Bootloader 与 OTA V1 实物验证阶段。独立 Bootloader、Application 重定位、
UART/CAN FD 接收、QSPI 暂存、安装、试运行确认和回滚链路已完成代码与构建；最近的
factory 普通启动和 UART OTA 主链已通过实物验证，但对应的是 b6/build15 基线。当前
b12/build22 尚待目标板回归；CAN FD OTA、断电恢复和回滚故障注入仍未验证。最新摘要见
[`docs/current_status.md`](docs/current_status.md)，完整证据见 `docs/verification.md`。
当前不再扩展 OTA recovery；完成 b12/build22 主链和三个关键故障测试后冻结 OTA V1，随后
进入 PID、轮组标定与加减速、里程计、安全保护、诊断和正式 CAN FD 底盘协议。

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
app/  board/  bsp/  components/  communication/
infrastructure/  modules/  rtos/  tests/  config/
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

工具链：

- STM32CubeMX 6.18
- STM32CubeG4 V1.6.3
- STM32CubeIDE GCC
- ST-Link 或 STM32CubeProgrammer

CubeIDE 工作区使用仓库的 `firmware/`，导入两个独立工程：

```text
firmware/application/stm32g474
firmware/bootloader/stm32g474
```

日常流程：

1. 在 CubeIDE 选择 `Debug` 或 `Release`。
2. 修改 `.ioc` 或公共头文件后执行 Clean Build。
3. 确认 `0 errors, 0 warnings` 后烧录对应 ELF。
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
