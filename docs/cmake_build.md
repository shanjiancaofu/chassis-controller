# CMake 与 Ninja 构建

CubeMX 继续负责 `.ioc` 和外设生成代码。`Toolchain / IDE` 保持为
`STM32CubeIDE`；仓库内的 CMake 文件独立维护，不由 CubeMX 生成。

## 工具要求

- CMake 3.22 或更高版本。
- Ninja。
- GNU Arm Embedded 14.3.1（STM32CubeIDE 14.3.rel1）。
- Python 3，用于 OTA 和 factory 打包。

将 `arm-none-eabi-gcc`、`cmake` 和 `ninja` 加入 `PATH`。也可以把
`ARM_GNU_TOOLCHAIN_ROOT` 设置为包含 `bin/` 的 GNU Arm 工具链目录。Windows 上当前
CubeIDE 2.2.0 使用插件内的 `tools/` 目录；不得提交机器相关的绝对路径。

## 构建预设

用于正式固件、OTA 和发布的 Release 构建：

```text
cmake --preset arm-release
cmake --build --preset arm-release
```

用于 ST-Link/OpenOCD/GDB 开发调试的 Debug 构建：

```text
cmake --preset arm-debug
cmake --build --preset arm-debug
```

`arm-debug` 使用 `-Og -g3`，`arm-release` 使用 `-Os`。Debug Application 额外定义
`APP_DEBUG_IWDG_FREEZE`，CPU 被调试器暂停时冻结 IWDG；Release 不定义该宏，因此不改变
正式固件的看门狗行为。

Windows 上工具不在 `PATH` 时，可在配置阶段传入位置，不要写入 preset：

```text
cmake --preset arm-release \
  -DARM_GNU_TOOLCHAIN_ROOT=<gnu-arm-tools-directory> \
  -DCMAKE_MAKE_PROGRAM=<ninja-executable>
cmake --build --preset arm-release
```

生成文件分别位于 `build/arm-debug/` 或 `build/arm-release/`：

```text
application.elf / application.bin
bootloader.elf / bootloader.bin
qspi_factory_provisioner.elf / qspi_factory_provisioner.bin
```

`build/` 是 CMake/Ninja 的临时构建目录；`_output/` 是整理后的烧录和升级交付目录，两者
职责不同，不应合并。

## VS Code 调试

F5 先由 `.vscode/tasks.json` 配置并构建 `arm-debug`，再由
`.vscode/launch.json` 启动 OpenOCD、通过 ST-Link SWD 加载并烧写
`build/arm-debug/application.elf`，复位后运行到 `main`。

VS Code 需要安装 Cortex-Debug，并确保 `openocd` 和 `arm-none-eabi-gdb` 在 `PATH`。
OpenOCD udev 规则必须允许当前用户访问 ST-Link；不要使用 `sudo` 启动 VS Code。

2026-08-15 已在 Ubuntu 虚拟机验证无 `sudo` OpenOCD 和命令行 GDB 等价流程可烧写
`build/arm-debug/application.elf`、停在 `main`、命中源码断点并在断点暂停期间冻结 IWDG。
VS Code 图形界面 F5 已于 2026-08-15 人工确认通过。

## 源文件与 CubeMX

`cmake/application.cmake`、`cmake/bootloader.cmake` 和
`cmake/provisioner.cmake` 的显式源文件列表定义目标归属。CubeMX 新增外设源文件或加入
手写模块后，只把文件加入实际拥有它的目标，不使用递归 glob。

CMake 是主要命令行构建入口。CubeIDE 暂时保留用于调试和产物对照，直到 CMake 流程积累
足够的目标板使用记录。CubeMX 仍从各自 `.ioc` 生成外设代码；每次生成后必须检查 diff，
并根据新增或删除的源文件更新对应目标的显式列表。
