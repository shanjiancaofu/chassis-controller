# 硬件与接线

硬件和引脚变更先修改 `chassis_controller.ioc`，生成并检查无冲突后再同步本文。
本文不记录软件阶段和验收结论；当前状态见 [`current_status.md`](current_status.md)，
实物证据见 [`verification.md`](verification.md)。

## 硬件

| 器件 | 型号/用途 |
| --- | --- |
| 主控 | XPlus STM32G474VET6 |
| 上位机 | Jetson Orin Nano |
| 电机驱动 | AT8236 D157B 双 H 桥 |
| 电机 | 两台配套霍尔编码器减速电机 |
| CAN FD 收发器 | MCP2562FD-E/SN 模块 |
| LCD | 2 英寸 ST7789，320 x 240 |
| 外部 Flash | W25Q64，8 MiB，JEDEC `EF4017` |
| 调试 | ST-Link/V2、CH340 |
| RTC | 32.768 kHz LSE，外接 3 V 备用电池 |
| 人体红外 | HC-SR501，数字高电平表示检测到移动 |

## 电源与安全

- STM32 调试阶段由 USB-C 供电，D157B 由 5.5 V 至 17 V 电机电源供电。
- STM32、D157B、CAN 收发器和非隔离 CAN 节点必须共地。
- `PD2` 急停只清零 PWM 和锁存故障，不等同于硬件断电急停。
- 更换编码器或驱动板前必须重新确认逻辑电平，不得超过 STM32 引脚允许范围。
- 首次电机测试架空车轮，上电默认保持四路 PWM 为零。

## STM32 引脚

| 功能 | 引脚/外设 |
| --- | --- |
| HSE / LSE | `PF0/PF1` / `PC14/PC15` |
| SWD | `PA13/PA14` |
| RGB LED | `PC0/PC1/PC2` |
| KEY/BOOT | `PB8`, EXTI8 |
| USART1 TX/RX | `PA9/PA10`, 115200 8N1 |
| FDCAN2 TX/RX | `PB6/PB5` |
| QSPI CLK/NCS | `PE10/PE11` |
| QSPI IO0..IO3 | `PE12/PE13/PE14/PE15` |
| LCD RST/BLK/CS | `PA5/PA6/PA7` |
| LCD SCK/DC/MOSI | `PB13/PB14/PB15` |
| ICM45686 SPI3 | `PC10/PC11/PC12`，SCK/MISO/MOSI |
| ICM45686 CS/INT1 | `PD0/PD1`，高电平空闲/EXTI1 上升沿 |
| 通用按钮 1/2 | `PD3/PD4`，内部上拉、低有效、EXTI3/EXTI4 |
| HC-SR501 OUT | `PD5`，普通输入、内部下拉、高有效 |
| 左编码器 | `PA0/PA1`, TIM2 CH1/CH2 |
| 右编码器 | `PD12/PD13`, TIM4 CH1/CH2 |
| 左电机 | `PC6/PC7`, TIM8 CH1/CH2 |
| 右电机 | `PC8/PC9`, TIM8 CH3/CH4 |
| 电机电压采样 | `PA2`, ADC1 IN3 |
| 急停 | `PD2`, 上拉、低有效、EXTI2 |

## 调试连接

ST-Link：

```text
SWDIO -> PA13
SWCLK -> PA14
GND   -> GND
```

CH340：

```text
TXD -> PA10 / USART1_RX
RXD -> PA9  / USART1_TX
GND -> GND
```

Application 使用 USART1 DMA 收发；Bootloader 使用同一 `PA9/PA10`，由
`boot_trace.c` 直接配置为 115200 8N1 并轮询输出启动诊断。Bootloader 当前不接收串口
命令，UART OTA 数据仍由 Application 接收。

## 电机驱动

```text
PC6 / TIM8_CH1 -> AIN1
PC7 / TIM8_CH2 -> AIN2
PC8 / TIM8_CH3 -> BIN1
PC9 / TIM8_CH4 -> BIN2
PA2 / ADC1_IN3 <- ADC
```

AT8236 当前使用快速衰减控制：

| 动作 | IN1 | IN2 |
| --- | --- | --- |
| 正转 | PWM | 0 |
| 反转 | 0 | PWM |
| 滑行停止 | 0 | 0 |

PWM 为 20 kHz，TIM8 ARR 为 8499。换向前至少保持一个 10 ms 零输出周期；
同一电机两个输入不得同时输出有效 PWM。

整车逻辑方向还包含左右电机镜像安装的补偿，当前软件映射为：

| 逻辑命令 | TIM8 通道 | 驱动输入 |
| --- | --- | --- |
| 左轮正向 | CH1 / PC6 | AIN1 |
| 左轮反向 | CH2 / PC7 | AIN2 |
| 右轮正向 | CH4 / PC9 | BIN2 |
| 右轮反向 | CH3 / PC8 | BIN1 |

因此上面的 IN1/IN2 表描述 H 桥输入状态，不直接定义左右车轮的整车前进方向。

编码器：

```text
E1A/E1B -> PA0/PA1
E2A/E2B -> PD12/PD13
```

当前标定为 `1320 counts/rev`。更换减速比、编码器或计数配置后必须重新标定。

D157B ADC 为约 1/11 分压：

```text
vin_mv = adc_raw * 3300 * 11 / 4095
```

该值当前只用于观测，不构成过压或欠压保护。

## CAN FD

STM32 侧：

```text
PB6 / FDCAN2_TX -> MCP2562FD TX
PB5 / FDCAN2_RX <- MCP2562FD RX
5V              -> MCP2562FD 5V
GND             -> MCP2562FD GND
```

总线：

```text
CANH -> CANH
CANL -> CANL
GND  -> GND
```

- 两个物理端点各使用 120 Ω，断电测量 CANH-CANL 应约为 60 Ω。
- VIO 使用 3.3 V。
- 仲裁速率 500 kbit/s，数据速率 2 Mbit/s，开启 FD+BRS。
- STM32 数据段实际采样点为 82.35%，Jetson 配置为 80%。

调试命令和验收步骤见 `verification.md`。

## 急停

当前固件使用常开触点：

```text
PD2 -> COM
GND -> NO
```

松开时内部上拉为高，按下时 PD2 被拉低。按钮不连接 3.3 V 或 5 V。

## LCD

```text
PA5  -> RST
PA6  -> BLK
PA7  -> CS
PB13 -> SCK
PB14 -> DC
PB15 -> SDA/MOSI
GND  -> GND
```

LCD 使用 SPI2 TX DMA。当前屏幕没有触摸控制器，通过 `PB8/BOOT0` 按键切换页面。

## ICM45686

ICM45686 使用独立 SPI3，不与 LCD SPI2 共用总线。模块使用 3.3 V 供电：

```text
PC10 / SPI3_SCK  -> SCL/SCLK
PC11 / SPI3_MISO <- AD0/MISO
PC12 / SPI3_MOSI -> SDA/MOSI
PD0              -> CS
PD1 / EXTI1      <- INT1
3.3V             -> VCC
GND              -> GND
```

`INT2`、`SCX` 和 `SDX` 暂不连接。SPI3 使用 Mode 0、8-bit、MSB first、软件 NSS 和约
1.33 Mbit/s；`CS` 空闲为高电平。INT1 使用上升沿中断并触发 FIFO watermark。应用层
通过 STM32G4 HAL 的 SPI3 RX/TX DMA 批量读取最多 4 个 16 字节 FIFO 帧；DMA1 CH5 为 RX、
CH6 为 TX。当前传感器配置为100 Hz、加速度计 +/-4 g、陀螺仪 +/-500 dps、低噪声带宽
ODR/4和16 us timestamp；不启用20-bit或压缩FIFO。`.ioc` 是STM32通道配置的权威来源，
当前板上仍未验证通信、电气时序或FIFO连续性。

## 通用按钮

两个预留按钮暂不绑定业务功能，均使用常开触点：

```text
PD3 -> BUTTON_1 -> GND
PD4 -> BUTTON_2 -> GND
```

`PD3/PD4` 使用内部上拉，按下为低电平，分别触发 EXTI3/EXTI4 下降沿。ISR 只通知按钮
驱动，消抖和按下事件识别在非 ISR 上下文执行。

## HC-SR501

HC-SR501 使用 5 V 供电，数字输出接 STM32 普通输入：

```text
5V  -> VCC
GND -> GND
PD5 <- OUT
```

模块 OUT 高电平约 3.3 V，可直接接入 STM32；不得将 5 V 直接接到 PD5。PD5 配置为内部
下拉、高有效输入，不使用 EXTI。软件上电后忽略 60 秒预热期，对输入做 50 ms 稳定滤波，
只统计预热结束后的低到高人体感应事件。模块上的延时和灵敏度电位器先保持中间位置，重复
触发跳帽使用 H 模式；具体安装位置和业务用途后续确定。`.ioc` 已记录 PD5 配置，并已使用
CubeMX 6.18.1 将 `SR501_OUT` 引脚定义和输入下拉初始化同步到 `Core` 生成代码。当前模块已按
上述定义完成接线；b16 已确认 PD5 保持低电平时无误触发。模块指示灯和 OUT 均未观察到
高电平，高电平及运动事件排查按当前计划后置。

## RTC

- RTC 使用 LSE，IWDG 使用独立 LSI。
- 备用电池正极接 `VBAT/BAT`，负极接 `GND`，不得接 3V3 或 5V。
- 安装前先测量极性和约 3 V 输出。

## 外设资源

| 资源 | 用途 |
| --- | --- |
| TIM2 | 左编码器 |
| TIM3 | LCD 背光 PWM |
| TIM4 | 右编码器 |
| TIM6 | 10 ms 控制通知 |
| TIM7 | HAL 时基 |
| TIM8 | 四路 20 kHz 电机 PWM |
| DMA1 CH1 | USART1 RX circular |
| DMA1 CH2 | USART1 TX |
| DMA1 CH3 | SPI2 TX |
| DMA1 CH4 | QUADSPI |
| DMA1 CH5 | SPI3 RX |
| DMA1 CH6 | SPI3 TX |

ADC 每 100 ms 软件触发，不使用 DMA。QSPI 正式分区见 `bootloader_and_ota.md`。
