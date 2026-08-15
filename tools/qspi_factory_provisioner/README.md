# QSPI factory provisioning 工具

当 STM32 ROM DFU 可用、但没有板级 CubeProgrammer External Loader 时，本维护目标用于
初始化 W25Q64 的 factory 回滚基线。它不属于正式 Bootloader 或 Application。

当前目标有意固定为 b12 factory 输入：

```text
内部 Flash 0x08000000: qspi-factory-provisioner.bin
内部 Flash 0x08008000: app-v0.1.0-b12.ota，183660 bytes
内部 Flash 0x0807F000: Metadata A，64 bytes
QSPI 0x00400000: confirmed Metadata A
QSPI 0x00401000: 擦除态 Metadata B
QSPI 0x00402000: Slot A 中的 b12 OTA 包
```

擦除 QSPI 前，provisioner 会校验暂存 OTA header、payload CRC、向量表、metadata CRC、
状态、slot 角色、镜像大小和镜像 CRC。随后擦除并校验两个 metadata sector，写入并完整
比较 Slot A，最后提交 Metadata A。只有全部成功才报告：

```text
PROVISION: PASS
```

在仓库根目录构建维护目标：

```text
cmake --preset arm-release
cmake --build --preset arm-release --target qspi_factory_provisioner
```

产物位于 `build/arm-release/`，工具链配置见
[`docs/cmake_build.md`](../../docs/cmake_build.md)。

`build.ps1` 只用于复现 2026-08-13 当时基于 CubeIDE Release object 的 provisioning
产物，不是当前维护构建入口；源码或 CubeMX 生成代码变化后不得继续使用。provisioning
完成后，应擦除内部 Flash，并恢复匹配的正式 factory 镜像再进入正常运行。
