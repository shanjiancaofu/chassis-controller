# chassis-controller

STM32G474VET6 底盘控制器。STM32 负责双电机闭环、安全停车、板级采集和
CAN FD 通信；Jetson Orin Nano 负责上层控制、诊断和固件发送。本仓库与
`cockpit-system` 独立维护。

## 当前状态

当前进入 Bootloader 与 OTA 阶段；完成状态、下一步和验证结果分别见
`docs/roadmap.md` 与 `docs/verification.md`。

## 目录

```text
firmware/
├─ application/stm32g474/   # 正式 Application CubeMX/CubeIDE 工程
└─ shared/                  # Bootloader、Application、打包器共用固定 ABI
docs/                       # 架构、路线、硬件、验证和 OTA 设计
protocol/                   # CAN FD 线协议
tools/ota/                  # 固件打包工具
picture/                    # LCD 素材
example/                    # 本地参考例程，不提交
```

Application 自有代码位于：

```text
app/  board/  bsp/  components/  communication/
infrastructure/  modules/  rtos/  tests/  config/
```

## 文档

- [系统架构](docs/architecture.md)
- [开发路线](docs/roadmap.md)
- [硬件与接线](docs/hardware.md)
- [构建与实物验证](docs/verification.md)
- [Bootloader 与 OTA](docs/bootloader_and_ota.md)
- [CAN FD 协议](protocol/canfd_protocol.md)
- [Codex/AI 修改规则](AGENTS.md)

同一信息只在一份文档中维护。代码和配置发生变化时，按上述职责更新对应文档。

## 构建

工具链：

- STM32CubeMX 6.18
- STM32CubeG4 V1.6.3
- STM32CubeIDE GCC
- ST-Link 或 STM32CubeProgrammer

CubeIDE 导入目录：

```text
firmware/application/stm32g474
```

日常流程：

1. 在 CubeIDE 选择 `Debug` 或 `Release`。
2. 修改 `.ioc` 或公共头文件后执行 Clean Build。
3. 确认 `0 errors, 0 warnings` 后烧录对应 ELF。
4. CH340 使用 `115200 8N1` 查看启动、自检、命令和遥测。

CubeMX 生成代码时使用 `STM32CubeIDE` 并勾选 `Generate Under Root`。
生成文件只在 `USER CODE BEGIN/END` 区域内手工修改。
