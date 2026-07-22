# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策和控制命令，STM32 负责双电机控制、状态采集、安全停车和 CAN FD 通信。本仓库与 `cockpit-system` 独立维护。

当前重点是 `firmware/stm32g474/` 裸机固件。FreeRTOS、Bootloader、完整 OTA、位置环和复杂底盘运动学将在板级外设与双电机安全链路完成实物验收后再引入。

## 目录结构

```text
chassis-controller/
├─ firmware/
│  └─ stm32g474/
│     ├─ Core/              # CubeMX 生成的初始化和中断入口
│     ├─ Drivers/           # CMSIS 与 STM32G4 HAL
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
- 裸机进度：`docs/裸机阶段进度.md`

## 当前基线

- 工具链为 STM32CubeMX 6.18、STM32CubeG4 V1.6.3、STM32CubeIDE GCC 和 ST-Link。
- 已接入 USART DMA、RTC、QSPI、LCD SPI DMA、FDCAN 内部回环、ADC、电机 PWM、双编码器、10 ms PID、安全停车和条件喂狗。
- LCD 封面、动态状态页、QSPI 8 MiB 识别、板级启动自检和 ADC 电压采样已经完成实物运行；准确验收状态以 `docs/裸机阶段进度.md` 为准。
- 电机 Demo 默认关闭，应用上电后 TIM8 四路 PWM 比较值保持为 0，不会自动转动电机。
- 外部 CAN、RTC 掉电保持、编码器方向和双电机控制链路仍需继续实物验证。

## 开发流程

1. 使用 CubeMX 打开 `.ioc`，工具链选择 `STM32CubeIDE`，启用 `Generate Under Root`。
2. 生成后在 STM32CubeIDE 导入 `firmware/stm32g474/`，使用 GCC 构建 Debug 或 Release。
3. 使用 ST-Link 烧录和调试，CH340 串口使用 115200 baud 查看自检与遥测。
4. 修改 CubeMX 生成文件时只写入 `USER CODE BEGIN/END` 区域，业务代码放入 `app/`、`bsp/` 或 `communication/`。

## 开发约定

- 正式硬件配置以 `.ioc` 和 `docs/硬件与接线.md` 为准。
- `Core/`、`Drivers/` 和 `Application/` 保持生成代码职责，不堆放控制业务。
- 不因代码行数机械拆文件，不提前创建无实现的模块和转调 helper。
- 不直接复制完整商家工程，只提取经过芯片、引脚、时钟和外设核对的配置。
- 危险或破坏性测试必须默认关闭，并通过明确命令或编译开关启用。
- 未经过真实硬件测试的功能只能记录为已实现或 READY，不能记录为 PASS。
