# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策和控制命令，STM32 负责双电机控制、状态采集、安全停车和 CAN FD 通信。本仓库与 `cockpit-system` 独立维护。

`firmware/stm32g474/` 的裸机基线已经完成实物验收，当前开始迁移 FreeRTOS。迁移只改变调度方式，暂不引入 Bootloader、完整 OTA、位置环和复杂底盘运动学。

## 目录结构

```text
chassis-controller/
├─ firmware/
│  └─ stm32g474/
│     ├─ Core/              # CubeMX 生成的初始化和中断入口
│     ├─ Drivers/           # CMSIS 与 STM32G4 HAL
│     ├─ Middlewares/       # CubeMX 管理的 FreeRTOS 内核
│     ├─ Application/       # CubeIDE 生成的启动和系统适配文件
│     ├─ app/               # 主流程、自检、状态页和底盘控制
│     ├─ bsp/               # 电机、编码器、LCD、QSPI 和 ADC 板级封装
│     ├─ communication/     # FDCAN 通信
│     ├─ config/            # 项目开关和存储布局
│     ├─ services/          # 后续公共板级服务
│     └─ chassis_controller.ioc
├─ protocol/                # Jetson 与 STM32 的 CAN FD 协议
├─ docs/                    # 硬件接线和阶段进度
├─ picture/                 # LCD 图片原始素材与取模结果
├─ tools/                   # 后续主机侧调试工具
└─ example/                 # 本地商家例程，仅供参考，不提交 Git
```

## 工程入口

- CubeMX 配置：`firmware/stm32g474/chassis_controller.ioc`
- 应用入口：`firmware/stm32g474/app/Src/app_main.c`
- 项目配置：`firmware/stm32g474/config/project_config.h`
- 硬件与接线：`docs/硬件与接线.md`
- CAN FD 协议：`protocol/canfd_protocol.md`
- 阶段进度：`docs/裸机阶段进度.md`

## 当前基线

- 工具链为 STM32CubeMX 6.18、STM32CubeG4 V1.6.3、STM32CubeIDE GCC 和 ST-Link。
- 裸机基线 `v0.1.0-baremetal` 已完成 USART、RTC、QSPI、LCD、CAN FD、ADC、双编码器、双电机、10 ms PID、安全停车和 IWDG 的实物验收。
- FreeRTOS 已由 CubeMX 接入：SysTick 用于内核节拍，TIM7 用于 HAL 时基，现有 `App_Run()` 由 1024 Words 静态任务执行。
- 当前 FreeRTOS 固件已通过 STM32CubeIDE GCC Debug 完整构建，结果为 `0 errors, 0 warnings`；尚未完成烧录后的硬件回归，不能标记为实物 `PASS`。
- 电机 Demo 默认关闭，应用上电后 TIM8 四路 PWM 比较值保持为 0，不会自动转动电机。

## 开发流程

### CubeMX 生成工程

1. 使用 CubeMX 打开 `firmware/stm32g474/chassis_controller.ioc`。
2. `Toolchain / IDE` 选择 `STM32CubeIDE`，勾选 `Generate Under Root`。
3. 生成代码后检查自定义源文件和头文件路径仍在 `.cproject` 中。
4. CubeMX 生成文件只在 `USER CODE BEGIN/END` 区域内手工修改；业务代码放入 `app/`、`bsp/` 或 `communication/`。

### STM32CubeIDE 构建和烧录

首次打开时选择 `File -> Import -> Existing Projects into Workspace`，根目录填写：

```text
E:\code\github\chassis-controller\firmware\stm32g474
```

导入 `chassis_controller` 后：

1. 在 `Project -> Build Configurations -> Set Active` 选择 `Debug`。
2. 日常编译使用 `Project -> Build Project`。
3. 修改公共头文件、CubeMX 配置或怀疑仍在使用旧目标文件时，先执行 `Project -> Clean...`，再执行 `Project -> Build Project`。
4. Console 出现 `Build Finished. 0 errors` 后，Debug 固件位于 `firmware/stm32g474/Debug/chassis_controller.elf`。
5. 打开 `Run -> Debug Configurations -> STM32 C/C++ Application`，Project 选择 `chassis_controller`，Application 选择 `Debug/chassis_controller.elf`。
6. 连接 ST-Link 后点击 `Debug` 完成烧录并进入调试；仅需继续运行时点击 Resume。

CH340 调试串口使用 `115200 8N1`，上电后通过串口查看启动自检、命令响应和遥测。

### VS Code 构建

VS Code 使用本机 `.vscode/tasks.json` 调用 STM32CubeIDE 的命令行程序 `stm32cubeidec.exe`，实际编译器和工程配置仍来自 STM32CubeIDE GCC 与 `.cproject`，没有改用桌面 GCC 或 CMake。

- 按 `Ctrl+Shift+B` 执行默认任务 `STM32: Build Debug`，用于日常增量编译。
- 按 `Ctrl+Shift+P`，选择 `Tasks: Run Task -> STM32: Clean Build Debug`，用于完整清理并重新编译。
- 构建成功时终端显示 `Build Finished. 0 errors, 0 warnings.`。
- 输出仍是 `firmware/stm32g474/Debug/chassis_controller.elf`。

VS Code 构建任务当前只编译，不自动烧录。编译完成后使用 STM32CubeIDE 的 Debug Configuration 将上述 ELF 烧入开发板。

若修改代码后板上行为没有变化，先检查 `Debug/chassis_controller.elf` 和相关 `.o` 的修改时间是否晚于源文件，再执行一次 `STM32: Clean Build Debug` 并重新烧录。仓库默认忽略 `.vscode/`，因此更换电脑后需要按本机 STM32CubeIDE 安装路径重新配置 `tasks.json` 中的 `stm32cubeidec.exe`。

## 开发约定

- 正式硬件配置以 `.ioc` 和 `docs/硬件与接线.md` 为准。
- `Core/`、`Drivers/` 和 `Application/` 保持生成代码职责，不堆放控制业务。
- 不因代码行数机械拆文件，不提前创建无实现的模块和转调 helper。
- 不直接复制完整商家工程，只提取经过芯片、引脚、时钟和外设核对的配置。
- 危险或破坏性测试必须默认关闭，并通过明确命令或编译开关启用。
- 未经过真实硬件测试的功能只能记录为已实现或 READY，不能记录为 PASS。
