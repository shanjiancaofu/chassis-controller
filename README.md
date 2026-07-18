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

已完成 170 MHz 系统时钟、SWD、GPIO、USART1、DMA、FDCAN2 内部回环、QSPI Flash、2 英寸 SPI LCD 接口、RTC、IWDG、定时器以及 AT8236 D157B 双电机资源配置。

启动后 USART1 输出启动信息，蓝灯每 500 ms 翻转；FDCAN2 以 500 kbps 仲裁速率、2 Mbps 数据速率发送 CAN FD+BRS 回环帧，接收校验成功亮绿灯，失败亮红灯。RTC 使用板载 32.768 kHz LSE，仅在备份域未初始化时写入默认时间；IWDG 继续使用 LSI，并在主循环中刷新。

应用启动时会启动两路编码器和 TIM8 四路 PWM，但四路比较值保持为 0，AT8236 处于滑行/休眠状态。驱动板和底盘线束尚未实物验证，不能跳过接线检查直接带电机上电。

## 外设与引脚

| 功能 | 外设/引脚 | 当前用途 |
|---|---|---|
| 外部时钟 | `PF0/PF1` | 8 MHz HSE，经 PLL 得到 170 MHz SYSCLK |
| RTC 时钟 | `PC14/PC15` | 板载 32.768 kHz LSE；外部 BAT 与 3.3 V 经 BAT54C 给 VBAT 供电 |
| 调试 | `PA13/PA14` | SWDIO/SWCLK，烧录和断点调试 |
| RGB LED | `PC0/PC1/PC2` | 蓝灯心跳，绿/红灯显示 FDCAN 回环结果 |
| 板载按键 | `PB8/BOOT0` | EXTI8，上升沿中断 |
| 调试串口 | `PA9/PA10` | USART1 TX/RX，115200 baud |
| CAN FD | `PB6/PB5` | FDCAN2 TX/RX，连接 MCP2562FD 收发器 |
| QSPI Flash | `PE10` 至 `PE15` | QUADSPI1，W25Q Flash 候选 8 MiB 地址空间 |
| LCD 控制 | `PA5/PA7/PB14` | 复位、片选、数据/命令 |
| LCD SPI | `PB13/PB15` | SPI2 SCK/MOSI，Mode 3，约 10.625 Mbit/s |
| LCD 背光 | `PA6` | TIM3 CH1，20 kHz PWM |
| 左编码器 | `PA0/PA1` | TIM2 CH1/CH2，A/B 相正交计数；输入必须为 3.3 V |
| 右编码器 | `PD12/PD13` | TIM4 CH1/CH2，A/B 相正交计数；输入必须为 3.3 V |
| 电机 A 输入 | `PC6/PC7` | TIM8 CH1/CH2，连接 AT8236 `AIN1/AIN2` |
| 电机 B 输入 | `PC8/PC9` | TIM8 CH3/CH4，连接 AT8236 `BIN1/BIN2` |
| 电源采样 | `PA2` | ADC1 IN3，连接模块 `ADC`，软件触发、12 bit、无 DMA |
| 急停候选 | `PD2` | 上拉、低有效、EXTI2 下降沿，优先级 3 |

AT8236 每台电机使用两个逻辑输入，没有独立的方向和使能脚。当前采用快速衰减控制：正转为 `IN1=PWM, IN2=0`，反转为 `IN1=0, IN2=PWM`，停止时两个输入均为 0，进入滑行/休眠。PWM 为 20 kHz，低于芯片手册规定的 50 kHz 上限。

### AT8236 D157B 接线

| 模块信号 | STM32/外部连接 | 说明 |
|---|---|---|
| `AIN1/AIN2` | `PC6/PC7` | 左电机控制，TIM8 CH1/CH2 |
| `BIN1/BIN2` | `PC8/PC9` | 右电机控制，TIM8 CH3/CH4 |
| `AO1/AO2` | 左电机两根动力线 | 方向不一致时交换动力线或在软件中反向 |
| `BO1/BO2` | 右电机两根动力线 | 方向不一致时交换动力线或在软件中反向 |
| `E1B/E1A` | `PA0/PA1` | 左编码器 A/B 相 |
| `E2B/E2A` | `PD12/PD13` | 右编码器 A/B 相 |
| `ADC` | `PA2` | 模块输入电压的 1/11 分压 |
| `GND` | STM32 `GND` | 控制信号必须共地 |

D157B 的 6P 电机接口把编码器电源固定接到 5 V，而 `PA0/PA1/PD12` 不是 5 V 容忍输入。当前接法必须用模块的 3.3 V 输出给霍尔编码器供电并使用定制线束；若使用 6P 接口自带的 5 V 编码器供电，则 A/B 相必须先经过 3.3 V 电平转换。不能把 5 V 编码器输出直接接到这些引脚。

开发板原理图虽然将 `PF2` 标为 `EXT_ADC_CH1`，但 STM32G474VETx 官方引脚数据库表明 `PF2` 没有 ADC 常规输入通道。当前改用扩展口 `PA2/ADC1_IN3` 连接 D157B 的 `ADC` 输出。模块使用 10 kΩ/1 kΩ 分压，换算公式为 `VIN = ADC_RAW / 4095 * 3.3 V * 11`；17 V 输入时 ADC 引脚约为 1.55 V。

## 定时器分工

| 定时器 | 参数 | 用途 |
|---|---|---|
| `TIM2` | 编码器模式 TI12，滤波 8，计数周期 65535 | 左电机霍尔编码器硬件正交计数 |
| `TIM3` | 1 MHz 计数，周期 50 | LCD 背光 20 kHz PWM |
| `TIM4` | 编码器模式 TI12，滤波 8，计数周期 65535 | 右电机霍尔编码器硬件正交计数 |
| `TIM6` | 1 MHz 计数，周期 10000，IRQ 优先级 10 | 预留 10 ms 控制节拍，后续速度计算和控制环 |
| `TIM8` | 170 MHz 计数，周期 8500 | AT8236 `AIN1/AIN2/BIN1/BIN2` 四路 20 kHz PWM |

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

1. `BspMotor_Init()` 先把 TIM8 四路比较值清零，再启动两路编码器和四路 PWM。
2. `BspMotor_Set(left, right)` 接收 `-1000` 至 `1000` 的左右有符号千分比，超出范围会被限制。
3. `BspMotor_Coast()` 将四路比较值清零，AT8236 进入滑行/休眠。
4. `BspMotor_ReadEncoderDelta()` 读取相邻两次调用之间的左右编码器增量，不清零硬件计数器。
5. 急停、CAN 命令超时和故障路径后续必须统一调用 `BspMotor_Coast()`，并锁存故障直到满足人工复位条件。

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

1. 制作 3.3 V 编码器线束，逐根确认 D157B、STM32 和电机接口定义。
2. 实现急停锁存、CAN 命令超时、输出归零和复位条件。
3. 硬件到位后依次验证编码器电平与方向、ADC 电压换算、架空车轮低占空比运行。
4. 基础驱动稳定后再启动 TIM6 的 10 ms 控制节拍并计算转速。
5. 最后增加速度闭环，不提前移植商家例程中的 PI 参数。
