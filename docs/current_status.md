# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

## 代码基线

- Application：`0.1.0-b16`。
- Bootloader：`0.1.0 build22`。
- 当前 SR501 工作树和板上 confirmed 镜像均为 Application `0.1.0-b16`；b12/build22 仍是
  已上板冻结 factory 基线，历史 b13 和当前产物不得覆盖该基线。
- 当前阶段：OTA V1 代码基线已冻结，CAN FD OTA、断电恢复、回滚和电气验收统一后置为
  `DEFERRED`。SR501 代码、接线、60 秒预热和低电平零误计数已上板；模块指示灯和 OUT
  均未观察到高电平，剩余实物排查已 `DEFERRED`。当前主线进入 PID，ICM45686 继续后置。

## 当前实现

- Application 的独立 CubeMX 工程已加入 SPI3（PC10/PC11/PC12）、两个预留按钮（PD3/PD4）
  和 SR501 输入（PD5）；CubeMX 生成结果包含 `hspi3`、`MX_SPI3_Init()`、SPI3 DMA1 CH5/CH6、
  EXTI1/3/4，以及 PD5 普通输入和内部下拉初始化。
- 已加入 `bsp/button` 通用按钮事件驱动。按钮目前只产生消抖后的 pressed event，不绑定业务。
- 已加入 `bsp/sr501` 轮询驱动。PD5 由 CubeMX 生成代码配置为内部下拉普通输入；驱动忽略 60 秒预热期，
  使用 50 ms 稳定滤波，只统计 READY 后的稳定低到高事件，并通过 `status` 输出原始电平、
  稳定运动状态、事件计数、最近事件时间和剩余预热时间。当前不绑定电机、安全、LCD 或业务。
- ICM45686 已拆分为 HAL 无关的 `components/icm45686` 寄存器/FIFO 驱动、HAL 无关的
  `components/imu_fusion` 六轴融合组件，以及 `bsp/imu` STM32 HAL SPI3/DMA 适配层。
  当前支持 WHO_AM_I、软复位、MREG 字节序、量程/ODR、ODR/4 内部低通、FIFO watermark、
  16 字节帧、DMA批量读取、FIFO full/非法帧/传输失败flush恢复、16位timestamp动态采样周期、
  静止窗口零偏标定、Mahony 四元数和 roll/pitch/yaw 诊断输出。当前不启用20-bit、压缩FIFO
  或自检；零偏仅保存在RAM，安装方向和轴映射等待实物确认。

- 内部 Flash 使用 32 KiB Bootloader + 480 KiB 单 Application。
- QSPI 使用双 metadata 和 Slot A/B，confirmed/candidate 由 metadata 分配。
- UART 与 CAN FD 共用 OTA 会话、QSPI 暂存、校验和 `STAGED` 提交流程。
- Bootloader 支持安装、TRIAL、CONFIRMED、confirmed repair 和 rollback。
- 只有双 metadata 全擦除的 factory 场景允许 vector fallback。
- candidate、rollback、confirmed repair 每次擦除内部 Flash 前持久化增加尝试次数，最多 3 次；
  任意一次安装中断后都会先校验 QSPI slot payload 和内部镜像，若上次安装已成功则直接补交
  `TRIAL/CONFIRMED`，不再执行下一次破坏性擦除。
- 进入 `ROLLBACK_PENDING` 时即使 `install_attempts=0` 也先完整验证 confirmed；内部已是健康
  confirmed 时直接提交 `CONFIRMED`，不执行无意义擦除。Flash 布局不支持属于全局 fatal，
  直接进入 Recovery，不尝试同样无法成功的 rollback。
- confirmed 恢复校验区分 `MATCH / INTERNAL_MISMATCH / SOURCE_INVALID / IO_ERROR`。只有 source
  的 header、向量表和完整 payload CRC 均有效，且明确证明内部镜像不匹配时才允许擦写；
  QSPI I/O 最多非破坏性重试 3 次，持续失败或 source 无效均进入 Recovery。
- rollback/repair 成功后从 confirmed slot header 回填 metadata `image_size/image_crc32`。
- UART 最终响应等待对应 DMA token 成功；30 秒超时只作用于等待 BEGIN 阶段。
- factory 工具默认必须同时生成内部组合 BIN 和 QSPI confirmed raw；只生成内部镜像必须显式
  使用 `--internal-only`，且不构成生产回滚基线。
- Application 正常使用约 10 秒 IWDG，OTA 复位前切换约 30 秒；Bootloader 只刷新继承实例。

## 已验证

- CMake 3.22 + Ninja + GNU Arm Embedded 14.3.rel1 当前 SR501 工作树构建通过：Application
  Debug `text=194364 data=120 bss=42272`，Release `text=185948 data=120 bss=42264`。
  b16 Release BIN 为 186076 字节、payload CRC32 `0x995BF0B5`，OTA 包为 186140 字节；
  OTA Python 9 项通过。b16 已完成 `STAGED -> INSTALLING -> TRIAL -> CONFIRMED`。
  上板发现新增 SR501 行使 `status` 达到 1251 字节，超过原 UART 1200 字节消息限制；b16
  将诊断缓冲和 UART 消息上限统一为 2048 字节并增加编译期约束，完整报告已实测恢复。
  已完成实物闭环的较早 b13 Release 为 `text=185388 data=120 bss=34944`，对应 OTA payload
  185516 字节、CRC32 `0x6FD23D35`；不得把当前新增 SR501 的构建视为同一上板产物。
  Bootloader build22 和 QSPI provisioner 的既有构建结果保持有效。ICM45686 FIFO/MREG 和
  IMU fusion 纯组件测试均使用 MSVC `/W4 /WX` 编译并执行通过。

- 已写入目标板的 b12/build22 factory 产物尺寸仍为 Application `text=183492 data=96 bss=34224`、
  Bootloader `text=13400 data=48 bss=1656`；这是冻结产物证据，不等于当前工作树构建。
- `arm-debug` 已使用 `-Og -g3` 构建，ELF 含源码调试信息和 FreeRTOS 内核符号；VS Code
  Cortex-Debug/OpenOCD 配置已就绪。2026-08-15 已在目标板验证无 `sudo` OpenOCD、GDB
  烧写 Debug ELF、停在 `main`、源码断点、调用栈、FreeRTOS `pxCurrentTCB` 和 Debug IWDG
  冻结；VS Code 图形界面 F5 已于 2026-08-15 人工确认通过。
- 在引入当前 ICM45686/调试改动之前，CMake 生成的三个 BIN 曾与已上板 b12/build22/provisioner
  冻结产物逐哈希一致；当前工作树的 clean build 已变化，不能沿用旧哈希或直接覆盖 `_output/`。
  CMake 作为主命令行构建入口，CubeIDE 暂留作对照和调试。
- OTA Python 共 9 项通过，包含字段顺序/offset 级共享 C/Python ABI 对照；两个目标 clean build
  已由 ARM GCC 验证共享结构体的逐字段 `offsetof` 静态断言。既有 factory、
  UART arm guard、Application metadata 和 Bootloader core 主机测试记录保持有效；本轮新增的
  rollback attempts=0/fatal 分类断言已通过目标 GCC `-Werror` 编译，尚未在宿主机执行。
- 2026-08-17 已使用 build22 从 confirmed b12 通过 UART OTA 安装 b13：发送工具完成
  185580 字节传输并收到 `STAGED`，Bootloader 依次报告 `INSTALL VERIFIED`、
  `TRIAL COMMITTED`、`TRIAL VERIFIED`，b13 健康窗口后报告 `OTA_CONFIRM: CONFIRMED`。
- b13 在没有 ICM45686 的情况下正常启动，串口报告 `ICM45686: NOT_INITIALIZED`、
  `LCD: READY`、`MOTOR: DISABLED`、`CONTROL_OVERRUN: count=0 missed=0`；confirmed 普通
  复位后 metadata state 为 `0x5`，随后报告 `OTA_CONFIRM: NOT_REQUIRED`，未出现 critical
  fault 或 IWDG 复位循环。
- factory 普通启动和 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` 已实物通过。
- 已从冻结 Release ELF 生成匹配的 b12/build22 Application BIN、OTA、内部 factory BIN 和
  8 MiB QSPI confirmed raw；OTA Python 9 项重新通过。产物生成不代表已烧录或实物通过。
- 已通过独立 DFU provisioner 将匹配的 b12 OTA package 和 `CONFIRMED/SLOT_A` metadata
  写入 W25Q64 并完成全包读回校验；随后恢复内部 factory BIN，实物确认 build22 读取
  `CONFIRMED` 并启动 b12。Application 稳定报告 QSPI `EF4017`、`OTA_CONFIRM: NOT_REQUIRED`
  和 `MOTOR: DISABLED`。

## 尚未验证

- VS Code 图形界面 F5 已于 2026-08-15 人工确认通过；底层 OpenOCD/GDB 自动烧写、停在
  `main`、源码断点、调用栈、FreeRTOS 符号和 Debug IWDG 冻结也已完成命令行等价目标板验证。

- ICM45686 SPI3 实物接线、`WHO_AM_I=0xE9`、FIFO/DMA连续性、静止零偏收敛、姿态轴向和两个预留按钮的机械消抖尚未上板验证；诊断可显示 `NOT_FOUND / STARTING / CALIBRATING / READY / DEGRADED` 及FIFO恢复计数，但任何软件状态均不得据此标记硬件PASS。

- SR501 已按 5 V、共地、OUT 接 PD5 完成接线。b16 实测 `warmup_ms` 递减并在 60 秒后进入
  `READY`，预热期间和 READY 后持续低电平均保持 `motion=0 raw=0 count=0`。模块指示灯未亮，
  OUT 高电平、50 ms 稳定滤波、单次上升沿计数和持续高电平不重复计数均为 `DEFERRED`。

- confirmed b16 普通按键/断电复位启动和上电四路 PWM 电气零输出：`DEFERRED`。
- CAN FD OTA：`DEFERRED`，后续在启用 Jetson OTA 前单独验收。
- Application 安装过程中断电恢复、TRIAL 不确认自动回滚和 rollback 安装中断电：`DEFERRED`。

confirmation 持续失败、QSPI terminal cleanup 和其他 recovery 边角不属于当前冻结门槛；仅在
上述实测暴露 P0/P1 时处理。

## 下一步

1. 进入 PID 实物闭环主线，先确认速度单位、方向、心跳超时和停车语义。
2. SR501 高电平/事件计数、CAN FD OTA、断电恢复、回滚和电气验收保留为后置回归，
   不阻塞当前开发。

当前路线：

```text
OTA 实物验证 -> OTA V1 冻结 -> SR501 -> PID -> 左右轮标定/加减速
-> 里程计 -> 安全保护 -> Fault/Health/Reset 诊断 -> 正式 CAN FD 协议
```

## 对话交接要求

新对话先读本文件和任务对应的权威文档。完成工作后只更新真实发生变化的内容；不要根据旧聊天
推断硬件通过，也不要因为合并来源中缺少某段内容而删除现有文档。
