# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

## 代码基线

- Application：`0.1.0-b13`。
- Bootloader：`0.1.0 build22`。
- 当前工作树已升到 Application `0.1.0-b13`；b12/build22 仍是已上板冻结产物，不得覆盖。
- 当前阶段：ICM45686 已后置为 `DEFERRED`，不再阻塞 OTA V1 主线；当前默认不启动或轮询 ICM45686，IMU 代码保持现状，后续仅在硬件可接入时再单独验证。

## 当前实现

- Application 的独立 CubeMX 工程已加入 SPI3（PC10/PC11/PC12）和两个预留按钮（PD3/PD4）配置；CubeMX 生成结果包含 `hspi3`、`MX_SPI3_Init()`、SPI3 DMA1 CH5/CH6 以及 EXTI1/3/4。
- 已加入 `bsp/button` 通用按钮事件驱动。按钮目前只产生消抖后的 pressed event，不绑定业务。
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

- CMake 3.22 + Ninja + GNU Arm Embedded 14.3.rel1 当前 Release clean build 通过：Application
  `text=198060 data=120 bss=35232`，Bootloader `text=13480 data=48 bss=1656`，QSPI
  provisioner `text=9272 data=12 bss=1644`。ICM45686 FIFO/MREG 和 IMU fusion 纯组件测试
  均使用 MSVC `/W4 /WX` 编译并执行通过。

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
- 最近完成实物 UART OTA 闭环的是 Application b6 + Bootloader build15。
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

- b12/build22 普通按键/断电复位启动和上电四路 PWM 电气零输出。
- b12/build22 UART OTA 与 CAN FD OTA。
- Application 安装过程中断电恢复。
- TRIAL 不确认后的自动回滚。
- rollback 安装过程中断电恢复。

confirmation 持续失败、QSPI terminal cleanup 和其他 recovery 边角不属于当前冻结门槛；仅在
上述实测暴露 P0/P1 时处理。

## 下一步

1. 先确认无 ICM45686 时 Application 仍能正常启动，不触发 critical fault / IWDG 复位。
2. 完成普通按键/断电复位启动和四路 PWM 上电电气零输出验收。
3. 完成 UART OTA 和 CAN FD OTA 实物验收。
4. 只执行三个故障测试：Application 安装中断电、TRIAL 不确认自动回滚、rollback 安装中断电。
5. 上述 OTA 项目通过后冻结 OTA V1；签名、防回滚和 Bootloader CAN Recovery 延后到 OTA V2。

当前路线：

```text
OTA 实物验证 -> OTA V1 冻结 -> PID -> 左右轮标定/加减速 -> 里程计
-> 安全保护 -> Fault/Health/Reset 诊断 -> 正式 CAN FD 协议
```

## 对话交接要求

新对话先读本文件和任务对应的权威文档。完成工作后只更新真实发生变化的内容；不要根据旧聊天
推断硬件通过，也不要因为合并来源中缺少某段内容而删除现有文档。
