# chassis-controller

基于 Jetson Orin Nano 和 STM32G474VET6 的底盘控制器。Jetson 负责上层决策，STM32 负责电机控制、
状态采集和 CAN FD 通信。

## 当前状态

仓库目前处于硬件例程盘点阶段，尚未建立正式固件工程。`example/` 只保存开发板商家和外设参考
例程，不作为产品源码继续开发。

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
只能用于演示，不能作为底盘电机 PWM 引脚。现有资料也没有确认板载 CAN FD 收发器，因此 FDCAN
实例、TX/RX、STB/EN 和位时序必须结合底盘扩展板或外接收发器重新确定。

## 开发约定

- 正式硬件配置以 CubeMX `.ioc` 为准。
- CubeMX 生成代码只在 `USER CODE BEGIN/END` 区域内修改。
- 业务代码放在独立源文件中，不把控制逻辑堆进生成的 `main.c`。
- 不直接复制完整商家工程，只吸收经过引脚、时钟和外设核对的配置。
- 编译、烧录和调试使用 VS Code 的 STM32CubeIDE 插件。

## 第一批任务

1. 确认 FDCAN 收发器、急停和电机接口的实际连接与引脚。
2. 用 CubeMX 创建 STM32G474VET6 正式工程，先启用 170 MHz 时钟、SWD、USART1 和状态 LED。
3. 在空闲循环中完成 LED 心跳和串口启动信息，验证编译、烧录、复位和调试链路。
4. 再加入 FDCAN loopback，确认位时序和中断收发后切换到真实收发器。
5. 通信稳定后再接 PWM、编码器、ADC 和看门狗，不提前加入 FreeRTOS。
