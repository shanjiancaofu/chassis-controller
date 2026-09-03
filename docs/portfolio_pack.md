# Chassis Controller V1 求职材料包

## 简历一行版

基于 STM32G474 + FreeRTOS 实现 1kHz 双轮闭环与 CAN FD 安全控制器，覆盖编码器反馈丢失、
命令/心跳超时、HardFault 持久化诊断和 UART OTA；目标板实测 ControlTask WCET max 33us、
零 deadline miss，并建立 source/ELF/BIN/OTA/硬件证据链。

## 简历项目版

- 将双轮 ControlTask 从 100Hz 提升至 1kHz；通过10样本滑动测速窗口和Kd离散时间缩放保持旧
  命令/PID单位，解决1ms encoder量化导致的PWM间歇归零，架空`±50`双轮稳定收敛。
- 设计 CAN FD command、sequence、peer heartbeat 和 motion authority 状态机；200ms command
  timeout、300ms heartbeat timeout及任何critical fault均清除命令并同时关闭两轮PWM。
- 实现正反转统一的encoder feedback-loss watchdog和四象限软件故障注入，验证fault锁存、owner
  释放及同一telemetry snapshot中PWM=0/0。
- 实现Cortex-M4F HardFault fail-close与`.noinit` crash context，区分basic/extended FP frame，
  经IWDG复位后使用同一Release ELF完成`addr2line`定位。
- 建立CMake/Kconfig/DTS、21项host tests、ARM Debug/Release CI和自动release manifest；固件
  `1.0.9 build3`完成UART OTA、CONFIRMED、OpenOCD复位及hash一致性验证。

## 三分钟讲解

这个项目是Jetson上层计算机对应的STM32底盘安全控制器。STM32负责CAN FD命令接收、双轮速度闭环、
编码器和电源采集，以及所有必须在上层失联后仍能独立工作的停车逻辑。

控制链使用TIM6每1ms通知最高业务优先级ControlTask。ISR只发notification，不解析协议、不运行PID。
ControlTask读取encoder，经过10个1ms样本的滑动窗口形成与旧系统一致的10ms等效速度，再运行PID、
startup boost和feedback watchdog。这样既得到1kHz安全响应，也不破坏已经验证的CAN命令和调参单位。

安全上，我把command timeout、peer heartbeat、欠压、encoder API failure和HardFault都收敛到同一个
fail-close结果：清命令、释放owner、两轮PWM归零、fault锁存。HardFault还会保存PC/LR/xPSR和CFSR，
由IWDG复位后输出previous exception，再用同一ELF定位源码。

最终目标板跑了约30万次1ms周期和正反转运动，motion WCET最大33us，没有deadline miss。发布侧用
manifest自动绑定source commit、GCC版本以及ELF/BIN/OTA hash，避免“板上的固件到底来自哪个提交”
无法回答。

## 十分钟技术展开

1. 从Jetson CAN FD帧讲到codec、ordering、heartbeat和CommandManager ownership。
2. 解释ISR只通知、ControlTask消费累计notification，以及missed tick如何进入fail-close。
3. 展示100Hz升级1kHz时遇到的单位、encoder量化、Kd和换向dead-time问题。
4. 解释10ms滑动测速窗口为何比单个1ms delta乘10稳定，同时仍每1ms更新控制和安全状态。
5. 说明startup boost、低输出不启动feedback watchdog、500ms loss timeout的物理依据。
6. 展示HardFault basic/extended frame、`.noinit`、IWDG和`addr2line`完整链路。
7. 展示DWT WCET、deadline miss、missed tick和为何它不等于ISR→task wake jitter。
8. 展示OTA metadata、TRIAL/CONFIRMED和release manifest provenance。
9. 明确V1没有验证物理拔线、带负载PID和精确odom标定，避免扩大结论。

## 高频面试问答

### 为什么控制任务要1kHz，但测速窗口仍是10ms？

低速encoder在1ms内经常只有0或1 count，直接换算会产生0/10跳变。10ms滑动窗口降低量化噪声，
但窗口每1ms滑动一次，因此控制和故障检测仍以1kHz更新，而不是退回100Hz批处理。

### 为什么不能只把PID的dt从0.01改为0.001？

数学积分项能自动适配dt，但离散encoder噪声和既有Kd调参语义不会自动适配。实测曾导致单轮PWM
间歇归零，因此需要同时处理measurement窗口和内部Kd比例。

### 为什么encoder失败要停两轮？

差速底盘单侧失控会产生不可预测角速度。继续驱动健康侧不能保持原运动意图，所以critical encoder
fault必须同时停两轮并锁存，恢复需要显式重新进入安全状态机。

### 为什么HardFault中不能打印完整日志？

异常现场的栈、RTOS和外设状态都不可信。Handler只做有界PWM清零、保存固定长度context并等待IWDG，
格式化和符号化放到下一次正常启动以及主机工具完成。

### WCET 33us代表调度延迟也是33us吗？

不是。当前DWT从ControlTask真正开始执行到控制周期结束，只测execution time。ISR notification到任务
开始之间的wake latency没有时间戳，因此不能把该数据描述成RTOS jitter。

## 可演示顺序

```text
status / 1ms task metrics
→ +50 forward
→ -50 reverse
→ pid stop / PWM=0
→ encoder software injection / fail-close
→ previous HardFault + addr2line evidence
→ release manifest and artifact hashes
```

演示只使用架空轮和软件故障注入，不做物理拔线。
