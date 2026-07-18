# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策，STM32 负责电机控制、
状态采集和 CAN FD 通信。

## 当前状态

正式固件工程已建立在 `firmware/stm32g474/`，使用 STM32CubeMX 6.18、
STM32CubeG4 V1.6.3 和 STM32CubeIDE GCC 工具链。`example/` 只保存本地商家和外设参考例程，
不作为产品源码继续开发，也不提交到仓库。

当前固件已完成 170 MHz 系统时钟、SWD、USART1、RGB LED 和 FDCAN2 内部回环配置。启动后通过
USART1 输出启动信息，蓝灯每 500 ms 翻转；FDCAN2 以 500 kbps 仲裁速率、2 Mbps 数据速率发送
CAN FD+BRS 回环帧，接收校验成功亮绿灯，失败亮红灯。CubeIDE Debug 配置已使用 GCC 完整编译，
结果为 0 errors、0 warnings；烧录和真实收发器通信仍待硬件到位后验证。

可直接参考的 STM32G474VET6 例程：

- GPIO：板载 RGB LED 和按键引脚。
- USART1：`PA9/PA10`，用于早期日志和调试。
- Timer PWM：TIM1 PWM 初始化方式。
- ADC：ADC1 多通道 DMA 采样方式。
- FreeRTOS、DMA、独立/窗口看门狗：用于后续运行框架和故障保护。

现有 CAN FD 例程使用 STM32G474CBT6/LQFP48，不是目标 STM32G474VET6/LQFP100。它只能用于参考
FDCAN filter、FIFO、中断和收发流程；正式工程的实例、引脚、收发器控制和位时序必须根据底盘板
原理图重新配置。

## 已确认的开发板配置

以下配置由商家资料包和多份 STM32G474VET6 CubeMX 例程交叉核对，正式工程仍以新的
`.ioc` 为唯一配置入口：

- MCU：STM32G474VET6，LQFP100。
- 主时钟：`PF0/PF1` 外接 8 MHz HSE，PLL 后 SYSCLK 170 MHz。
- 板载 RGB LED：`PC0/PC1/PC2`。
- 板载按键：`PB8/BOOT0`，示例使用 EXTI8。
- 调试串口 USART1：`PA9/PA10`，115200 baud。
- USB FS：`PA11/PA12`。
- 板载 QSPI Flash：`PE10` 至 `PE15`。

商家 Timer PWM 例程把 `PC0/PC1/PC2` 复用成 TIM1 CH1/CH2/CH3，这与板载 RGB LED 冲突，
只能用于演示，不能作为底盘电机 PWM 引脚。CAN FD 使用外接 MCP2562FD 模块，连接方式见下文。

## CAN FD 收发器

外接模块使用 Microchip `MCP2562FD-E/SN`，支持 CAN FD，商家标称最高数据速率 8 Mbps。首版
接线如下：

| STM32G474VET6 | 收发器模块 | 方向 |
|---|---|---|
| 5V | 5V | 开发板向模块供电 |
| GND | GND | 必须共地 |
| FDCAN_TX | TX | MCU 到收发器 |
| FDCAN_RX | RX | 收发器到 MCU |
| - | CANH/CANL | 接 CAN 总线双绞线 |

模块的 VIO 选择必须设为 3.3V，不能让 5V 逻辑电平进入 STM32。板载 120 欧终端电阻只在模块位于
总线物理端点时启用；总线中间节点关闭，整条总线只保留两处终端。

现有参考例程使用 FDCAN2：`PB6=FDCAN2_TX`、`PB5=FDCAN2_RX`。这组引脚可作为正式工程候选，
但创建 `.ioc` 前仍需确认开发板排针确实引出了 PB5/PB6。模块没有向 MCU 暴露 STBY/EN，首版不做
软件休眠控制。联调先使用 500 kbps 仲裁速率和 2 Mbps 数据速率，稳定后再提高，不直接从 8 Mbps
开始。

## 开发约定

- 正式硬件配置以 CubeMX `.ioc` 为准。
- CubeMX 生成代码只在 `USER CODE BEGIN/END` 区域内修改。
- `Core/` 只放 CubeMX 生成代码，程序流程放在 `app/`。
- 业务目录有实际代码时再细分，不提前创建空实现和转调 helper。
- 不直接复制完整商家工程，只吸收经过引脚、时钟和外设核对的配置。
- CubeMX 选择 STM32CubeIDE，并启用 Generate Under Root。
- 编译、烧录和调试使用 STM32CubeIDE GCC 与 ST-Link。

## 下一批任务

1. 硬件到位后核对 PB5/PB6、RGB LED、急停、电机接口与原理图，修正 `.ioc` 中的最终引脚。
2. 通过 ST-Link 烧录，验证 170 MHz 时钟、蓝灯心跳、USART1 启动日志和调试链路。
3. 先验证 FDCAN2 内部回环，再连接 MCP2562FD 做双节点 500 kbps/2 Mbps 实总线收发。
4. 明确电机驱动器接口后配置 PWM、方向、使能、编码器和 ADC，不沿用商家 LED PWM 引脚。
5. 完成急停、通信超时、输出归零和看门狗后，再评估是否需要 FreeRTOS。
