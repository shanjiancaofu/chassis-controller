# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策，STM32 负责电机控制、状态采集和 CAN FD 通信。该仓库与 `cockpit-system` 独立维护。

## 工程入口

- 正式固件：`firmware/stm32g474/`
- CubeMX 配置：`firmware/stm32g474/chassis_controller.ioc`
- 应用入口：`firmware/stm32g474/app/Src/app_main.c`
- CAN FD 协议：`protocol/canfd_protocol.md`
- 本地参考资料：`example/`，不作为产品源码，也不提交到仓库

工具链使用 STM32CubeMX 6.18、STM32CubeG4 V1.6.3、STM32CubeIDE GCC 和 ST-Link。CubeMX 选择 `STM32CubeIDE`，并启用 `Generate Under Root`。

## 当前状态

已完成 170 MHz 系统时钟、SWD、GPIO、USART1、DMA、FDCAN2 内部回环、QSPI Flash、2 英寸 SPI LCD 接口、RTC、IWDG、定时器和双电机候选资源配置。

启动后 USART1 输出启动信息，蓝灯每 500 ms 翻转；FDCAN2 以 500 kbps 仲裁速率、2 Mbps 数据速率发送 CAN FD+BRS 回环帧，接收校验成功亮绿灯，失败亮红灯。RTC 仅在备份域未初始化时写入默认时间，IWDG 在主循环中刷新。

电机和编码器目前只完成 CubeMX 资源准备，PWM 未启动、电机使能默认低。引脚尚未经过真实驱动板和底盘线束验证，不能直接接电机上电。

## 外设与引脚

| 功能 | 外设/引脚 | 当前用途 |
|---|---|---|
| 外部时钟 | `PF0/PF1` | 8 MHz HSE，经 PLL 得到 170 MHz SYSCLK |
| 调试 | `PA13/PA14` | SWDIO/SWCLK，烧录和断点调试 |
| RGB LED | `PC0/PC1/PC2` | 蓝灯心跳，绿/红灯显示 FDCAN 回环结果 |
| 板载按键 | `PB8/BOOT0` | EXTI8，上升沿中断 |
| 调试串口 | `PA9/PA10` | USART1 TX/RX，115200 baud |
| CAN FD | `PB6/PB5` | FDCAN2 TX/RX，连接 MCP2562FD 收发器 |
| QSPI Flash | `PE10` 至 `PE15` | QUADSPI1，W25Q Flash 候选 8 MiB 地址空间 |
| LCD 控制 | `PA5/PA7/PB14` | 复位、片选、数据/命令 |
| LCD SPI | `PB13/PB15` | SPI2 SCK/MOSI，Mode 3，约 10.625 Mbit/s |
| LCD 背光 | `PA6` | TIM3 CH1，20 kHz PWM |
| 左编码器 | `PA0/PA1` | TIM2 CH1/CH2，A/B 相正交计数 |
| 右编码器 | `PD12/PD13` | TIM4 CH1/CH2，A/B 相正交计数 |
| 电机 PWM | `PC6/PC7` | TIM8 CH1/CH2，两路 20 kHz PWM |
| 电机方向 | `PC8/PC9` | 左/右方向 GPIO，默认低 |
| 电机使能 | `PD0/PD1` | 左/右使能 GPIO，默认低，最后拉高 |
| 急停候选 | `PD2` | 上拉、低有效、EXTI2 下降沿，优先级 3 |

电机部分采用常见的 `PWM + DIR + EN` 双通道候选接口。若实际驱动板需要两路互补 PWM、SHDN、故障反馈或电流采样，必须根据驱动器手册重新配置，不能直接沿用本表。

霍尔编码器 A/B 输出必须是 STM32 可接受的 3.3 V 逻辑；如果电机编码器输出为 5 V 或开集电极，应先确认是否需要电平转换和外部上拉。

## 定时器分工

| 定时器 | 参数 | 用途 |
|---|---|---|
| `TIM2` | 编码器模式 TI12，滤波 8，计数周期 65535 | 左电机霍尔编码器硬件正交计数 |
| `TIM3` | 1 MHz 计数，周期 50 | LCD 背光 20 kHz PWM |
| `TIM4` | 编码器模式 TI12，滤波 8，计数周期 65535 | 右电机霍尔编码器硬件正交计数 |
| `TIM6` | 1 MHz 计数，周期 10000，IRQ 优先级 10 | 预留 10 ms 控制节拍，后续速度计算和控制环 |
| `TIM8` | 170 MHz 计数，周期 8500 | 左右电机两路 20 kHz PWM，比较值范围 0 至 8499 |

编码器计数由定时器硬件完成，不需要每个边沿都进入 CPU 中断。`TIM6` 当前只完成配置，尚未启动；等电机控制逻辑实现后再调用 `HAL_TIM_Base_Start_IT(&htim6)`。

## DMA 分工

| DMA | 方向和模式 | 用途 |
|---|---|---|
| `DMA1_Channel1` | USART1 RX，外设到内存，循环，高优先级 | 后续连续接收调试命令，避免逐字节轮询 |
| `DMA1_Channel2` | USART1 TX，内存到外设，普通，中优先级 | 后续异步发送日志或调试数据 |
| `DMA1_Channel3` | SPI2 TX，内存到外设，普通，低优先级 | 批量发送 LCD 像素，减少 CPU 搬运 |

当前启动日志仍使用阻塞式 `HAL_UART_Transmit`；LCD 驱动和串口接收模块实现后才实际启动对应 DMA。QSPI、编码器、PWM、RTC 和看门狗目前不使用 DMA。

## 电机资源使用顺序

硬件接口确认后，电机代码按以下顺序使用：

1. 保持 `MOTOR_LEFT_EN`、`MOTOR_RIGHT_EN` 为低，PWM 比较值为 0。
2. 使用 `HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_ALL)` 和 `HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL)` 启动编码器。
3. 使用 `HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1/2)` 启动 PWM，比较值仍保持 0。
4. 设置方向 GPIO，再通过 `__HAL_TIM_SET_COMPARE` 写入 0 至 8499 的占空比。
5. 所有状态正常后最后拉高电机使能。
6. 停止时先拉低使能，再清零比较值；急停、CAN 命令超时和故障路径必须执行同一安全顺序。

`PD2` 现在只完成急停输入和中断配置，尚未实现故障锁存、输出切断和人工复位条件，不能把它当成已经完成的安全功能。

## CAN FD 收发器

外接模块使用 MCP2562FD-E/SN。模块 VIO 必须选择 3.3 V，CANH/CANL 使用双绞线；120 欧终端电阻只在总线两个物理端点启用。首轮实总线联调保持 500 kbps/2 Mbps，不直接从 8 Mbps 开始。

## 开发约定

- 正式硬件配置以 CubeMX `.ioc` 为准。
- CubeMX 生成代码只在 `USER CODE BEGIN/END` 区域内修改。
- `Core/` 只放 CubeMX 生成代码，程序流程放在 `app/`。
- 不因代码行数机械拆文件，不提前创建空模块和转调 helper。
- 不直接复制完整商家工程，只吸收经过芯片、引脚、时钟和外设核对的配置。
- 当前先使用裸机主循环；控制任务和时序证明有需要后再引入 FreeRTOS。

## 下一批任务

1. 确认实际电机驱动板型号、输入拓扑、有效电平、电机数量和霍尔编码器电压，再冻结候选引脚。
2. 实现最小电机 BSP：安全启停、方向、占空比和编码器计数，不先写 PID。
3. 实现急停锁存、CAN 命令超时、输出归零和复位条件。
4. 硬件到位后依次验证 SWD、USART1、LED、FDCAN、编码器方向和空载低占空比电机运行。
5. 基础驱动稳定后再增加 10 ms 速度计算、速度闭环和 ADC 电流/电压采样。
