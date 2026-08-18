# 固件产物

当前开发版本和待验证项目见
[`docs/current_status.md`](../docs/current_status.md)，完整证据见
[`docs/verification.md`](../docs/verification.md)。产物文件本身不能证明硬件通过。

烧录和升级产物按类型组织：

```text
application/  Application BIN 和 OTA 包
bootloader/   Bootloader BIN
factory/      Bootloader + Application 组合 BIN，以及 QSPI factory raw
archive/      历史、候选、诊断、读回和宿主测试文件
```

命名规则：

```text
application: app-v<version>-b<N>.<bin|ota>
bootloader:  boot-v<version>-b<N>.bin
factory:     factory-a<app-build>-b<boot-build>.bin
qspi:        factory/qspi-a<app-build>-confirmed.bin
```

当前产物名不带时间戳；版本号和 build 号共同标识发布输入。带时间戳的文件和诊断文件应放入
`archive/`。

历史硬件验证基线（b6/build15）：

```text
application/app-v0.1.0-b6.bin
application/app-v0.1.0-b6.ota
bootloader/boot-v0.1.0-b15.bin
factory/factory-a6-b15.bin
```

这些文件保留较早的 factory/UART OTA 历史闭环，不代表当前源码。当前 b12/build22
factory 和 QSPI confirmed 基线已经上板；2026-08-17 已从该基线通过 UART OTA 安装并确认 b13。

当前冻结的 b12/build22 factory 产物：

```text
application/app-v0.1.0-b12.bin
application/app-v0.1.0-b12.ota
bootloader/boot-v0.1.0-b22.bin
factory/factory-a12-b22.bin
factory/qspi-a12-confirmed.bin
```

这些产物与当前含 ICM45686 和调试配置的工作树构建不同；重新发布前必须分配新 build 号，
不得直接覆盖。2026-08-13 已通过 STM32 ROM DFU 和仓库
`tools/qspi_factory_provisioner/` 维护目标写入并读回匹配的内部 factory 镜像和 QSPI
`CONFIRMED/SLOT_A` 基线；随后已确认 build22 启动 b12。

2026-08-17 从该 confirmed b12 基线通过 UART OTA 安装并确认 b13。当前本地产物为：

```text
application/app-v0.1.0-b13.bin
application/app-v0.1.0-b13.ota
```

b13 payload 为 185516 字节，CRC32 `0x6FD23D35`；该包已完成
`STAGED -> INSTALLING -> TRIAL -> CONFIRMED` 实物闭环。b12 文件继续保留为 factory 历史输入，
不得被 b13 产物覆盖。断电启动、电气零 PWM 和故障注入仍需验证，CAN FD OTA 按当前计划后置。

包含 SR501 和诊断长度修复的当前 b16 产物为：

```text
application/app-v0.1.0-b16.bin
application/app-v0.1.0-b16.ota
```

b16 payload 为 186076 字节，CRC32 `0x995BF0B5`；该包已通过打包校验和 OTA Python 9 项，
并已完成 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`。b14/b15 是诊断消息长度问题的
中间定位版本，不作为当前发布输入。SR501 高电平和事件计数验证按当前计划后置。

历史产物编号与语义版本映射：

| 历史文件编号 | 语义版本 | 源码阶段 |
| --- | --- | --- |
| b6、b12 | `0.1.0` | 初始底盘/OTA factory 基线 |
| b13 | `0.1.1` | 无 ICM45686 也可启动 |
| b14 | `0.2.0` | SR501 接入 |
| b15 | `0.2.1` | 诊断缓冲修复 |
| b16 | `0.2.2` | UART 消息容量修复 |
| b17 | `0.3.0` | FreeRTOS 四任务、统一状态快照和正式 UART 协议 |
| 当前工作树 | `0.4.0` | LCD 小 Logo 与页面结构调整 |

这些映射用于描述源码演进，不改写历史文件中已经固化的镜像头、文件名或实物证据。当前
目标板 confirmed 版本为 `0.3.0 build1`；工作树已进入 `0.4.0 build1`，尚未生成或存放新的
正式产物。后续产物需放入 `application/` 后再更新本节，不得覆盖 b12 factory 文件。

烧录地址：

```text
Application BIN: 0x08008000
Bootloader BIN:  0x08000000
Factory BIN:     0x08000000
QSPI raw image:  外部加载器 / QSPI 基址 0x000000
```

QSPI factory 镜像与 Application BIN 使用同一个 `.ota` 包生成，包含 Slot A 和
`CONFIRMED` Metadata A，Metadata B 保持擦除态。首次安装回滚基线必须通过外部加载器或
等价 factory provisioning 步骤写入。factory 生成器默认同时要求 `--ota` 和
`--qspi-output`；仅内部 Flash 的诊断镜像必须显式使用 `--internal-only`，且不能作为
生产基线。

CubeIDE 当前会更新 ELF，但不保证同步重建 BIN。打包或组合镜像前，必须从匹配的最终 ELF
执行 `arm-none-eabi-objcopy -O binary` 重新生成 BIN。

`archive/` 下的文件不属于发布产物。
