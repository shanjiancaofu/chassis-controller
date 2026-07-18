# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策，STM32 负责电机控制、状态采集和 CAN FD 通信。该仓库与 `cockpit-system` 独立维护。

## 工程入口

- 正式固件：`firmware/stm32g474/`
- CubeMX 配置：`firmware/stm32g474/chassis_controller.ioc`
- 应用入口：`firmware/stm32g474/app/Src/app_main.c`
- CAN FD 协议：`protocol/canfd_protocol.md`
- 裸机进度：`docs/裸机阶段进度.md`
- 本地参考资料：`example/`，不作为产品源码，也不提交到仓库

工具链使用 STM32CubeMX 6.18、STM32CubeG4 V1.6.3、STM32CubeIDE GCC 和 ST-Link。CubeMX 选择 `STM32CubeIDE`，并启用 `Generate Under Root`。

## 当前状态

已完成 170 MHz 系统时钟、SWD、GPIO、USART1、DMA、FDCAN2 内部回环、QSPI Flash、2 英寸 SPI LCD 接口、RTC、IWDG、定时器以及 AT8236 D157B 双电机资源配置。

启动后 USART1 输出启动信息，蓝灯每 500 ms 翻转；FDCAN2 以 500 kbps 仲裁速率、2 Mbps 数据速率发送 CAN FD+BRS 回环帧，接收校验成功亮绿灯，失败亮红灯。RTC 使用板载 32.768 kHz LSE，仅在备份域未初始化时写入默认时间；IWDG 继续使用 LSI，只在主循环和 10 ms 控制节拍均正常且没有内部关键故障时刷新。

裸机双电机控制链路已经接入：TIM2/TIM4 编码器增量、TIM6 10 ms 调度、速度 PID、方向切换保护、200 ms 命令超时、PD2 急停锁存、ADC 电源采样和 USART1 遥测。应用启动时会启动两路编码器和 TIM8 四路 PWM，但四路比较值保持为 0；电机 Demo 默认关闭，不会自动转动。驱动板和底盘线束尚未实物验证，不能跳过接线检查直接带电机上电。

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
| 左编码器 | `PA0/PA1` | TIM2 CH1/CH2，配套编码器 `E1B/E1A` 直接接入 |
| 右编码器 | `PD12/PD13` | TIM4 CH1/CH2，配套编码器 `E2B/E2A` 直接接入 |
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

D157B 的 6P 电机接口给同厂配套霍尔编码器提供 5 V 电源，`E1B/E1A/E2B/E2A` 按上表直接接入 STM32，不需要另加电平转换。这里的 5 V 是编码器供电电压，不等同于 A/B 输出信号高电平；更换其他型号编码器时需要重新确认接口定义和输出电平。

开发板原理图虽然将 `PF2` 标为 `EXT_ADC_CH1`，但 STM32G474VETx 官方引脚数据库表明 `PF2` 没有 ADC 常规输入通道。当前改用扩展口 `PA2/ADC1_IN3` 连接 D157B 的 `ADC` 输出。模块使用 10 kΩ/1 kΩ 分压，换算公式为 `VIN = ADC_RAW / 4095 * 3.3 V * 11`；17 V 输入时 ADC 引脚约为 1.55 V。

## 定时器分工

| 定时器 | 参数 | 用途 |
|---|---|---|
| `TIM2` | 编码器模式 TI12，滤波 8，计数周期 65535 | 左电机霍尔编码器硬件正交计数 |
| `TIM3` | 1 MHz 计数，周期 50 | LCD 背光 20 kHz PWM |
| `TIM4` | 编码器模式 TI12，滤波 8，计数周期 65535 | 右电机霍尔编码器硬件正交计数 |
| `TIM6` | 1 MHz 计数，周期 10000，IRQ 优先级 10 | 10 ms 速度采样和 PID 控制节拍 |
| `TIM8` | 170 MHz 计数，周期 8500 | AT8236 `AIN1/AIN2/BIN1/BIN2` 四路 20 kHz PWM |

编码器计数由定时器硬件完成，不需要每个边沿都进入 CPU 中断。TIM6 中断只增加有上限的待处理计数，编码器读取和 PID 计算在主循环执行；节拍严重积压会锁存内部故障并清零四路 PWM。

## DMA 分工

| DMA | 方向和模式 | 用途 |
|---|---|---|
| `DMA1_Channel1` | USART1 RX，外设到内存，循环，高优先级 | 后续连续接收调试命令，避免逐字节轮询 |
| `DMA1_Channel2` | USART1 TX，内存到外设，普通，中优先级 | 每 100 ms 异步发送控制遥测 |
| `DMA1_Channel3` | SPI2 TX，内存到外设，普通，低优先级 | 批量发送 LCD 像素，减少 CPU 搬运 |

当前启动日志仍使用一次阻塞式 `HAL_UART_Transmit`，周期遥测使用 USART1 TX DMA。LCD 驱动和串口接收模块实现后再启用对应 DMA；ADC 只需每 100 ms 软件触发一次，不使用 DMA。

## 裸机电机控制

- `BspMotor_SetSignedDuty()` 使用 TIM8 原始比较值，范围为 `-8499` 至 `8499`；控制器当前额外限制在 `-849` 至 `849`，约为 10% PWM，供首次架空测试使用。
- 正转输出为 `IN1=PWM, IN2=0`，反转为 `IN1=0, IN2=PWM`，停止为 `IN1=0, IN2=0`。同一电机换向时先输出一个 10 ms 零 PWM 周期，再启用反向通道。
- TIM2/TIM4 保持自由运行，软件以 16 bit 模运算计算相邻 10 ms 的有符号增量，能够处理单次采样范围内的计数器回绕。编码器方向统一由 `project_config.h` 中的左右系数调整。
- PID 速度单位暂为 `encoder counts / 10 ms`。当前左右参数均为 `Kp=40, Ki=10, Kd=0`，输出和积分均限幅，并采用条件积分 anti-windup；这些只是保守初值，必须实车整定。
- 命令超过 200 ms 未更新时立即清零四路 PWM、清空目标并重置 PID；收到新命令后仍需重新启动控制，旧输出不会自动恢复。
- PD2 下降沿中断会直接清零四路 PWM 并锁存急停。只有 PD2 恢复高电平后才允许人工清除，清除后仍保持停车。该动作只是软件切断 PWM，不等同于独立硬件断电急停。

ADC1 每 100 ms 读取 PA2，内部使用 `vin_mv = adc_raw * 3300 * 11 / 4095` 和 64 bit 中间值。公式假设 VDDA 精确为 3.3 V、分压比精确为 11，PA2 上的电压任何时候都不得超过 VDDA；真实硬件必须用万用表校准。当前结果只用于观测，不构成过压或欠压保护。

### 裸机 Demo

在 `firmware/stm32g474/config/project_config.h` 将 `ENABLE_BAREMETAL_MOTOR_DEMO` 改为 `1` 才会运行。顺序为停车、双电机低速正转、停车、低速反转、停车、左正右反、最终停车；全程使用非阻塞时间判断，USART1 每 100 ms 输出电压、目标、编码器增量、PID 输出、状态和故障位。首次启用前必须架空车轮并确认急停能够清零四路 PWM。

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

1. 按 6P 端子定义逐根确认 D157B、STM32 和配套电机线序，并测量编码器 A/B 高电平。
2. 架空车轮，以默认 10% 输出上限验证急停、正反转、左右编码器方向和换向零输出周期。
3. 用万用表校准 PA2 电压换算，确认实际 VDDA 和分压比例。
4. 根据真实编码器 counts/10 ms 逐步整定左右 PI，确认每转计数和减速比后再换算 RPM。
5. 完成硬件 Demo 后接入 CAN FD 控制帧，再根据控制时序和模块数量评估 FreeRTOS 迁移。
