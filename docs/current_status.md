# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

## 代码基线

- 当前已提交固件实现基线：`b86b96e [fix]: avoid destructive rollback for healthy firmware`。
- 当前工作树包含可诊断 confirmed 恢复校验、非破坏性 QSPI I/O 重试、confirmed fatal 处理和
  ARM GCC `offsetof` ABI 静态断言。
- Application：`0.1.0-b12`。
- Bootloader：`0.1.0 build22`。
- 当前阶段：Bootloader 与 OTA V1 实物验证。

## 当前实现

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

- Application b12 Release clean build：`text=183492 data=96 bss=34224`。
- Bootloader build22 Release clean build：`text=13400 data=48 bss=1656`。
- OTA Python 共 9 项通过，包含字段顺序/offset 级共享 C/Python ABI 对照；两个目标 clean build
  已由 ARM GCC 验证共享结构体的逐字段 `offsetof` 静态断言。既有 factory、
  UART arm guard、Application metadata 和 Bootloader core 主机测试记录保持有效；本轮新增的
  rollback attempts=0/fatal 分类断言已通过目标 GCC `-Werror` 编译，尚未在宿主机执行。
- 最近完成实物 UART OTA 闭环的是 Application b6 + Bootloader build15。
- factory 普通启动和 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` 已实物通过。

## 尚未验证

- Application b12 + Bootloader build22 目标板回归。
- QSPI External Loader 写入 factory confirmed 基线。
- CAN FD OTA 完整升级。
- QSPI 暂存和内部安装期间断电恢复。
- TRIAL 失败回滚、rollback 安装中断和 Recovery 故障注入。
- confirmation 持续失败和 QSPI terminal cleanup 超时的板上行为。

## 下一步

1. 生成并烧录匹配的 b12/build22 内部 factory BIN 与 QSPI confirmed raw 镜像。
2. 验证普通启动、版本输出和零 PWM。
3. 完成 UART 回归，再完成 CAN FD OTA。
4. 按 `verification.md` 执行安装断电、rollback、确认失败和 Recovery 故障注入。
5. 实物结果确认后再更新 `HARDWARE PASS` 和正式产物清单。

## 对话交接要求

新对话先读本文件和任务对应的权威文档。完成工作后只更新真实发生变化的内容；不要根据旧聊天
推断硬件通过，也不要因为合并来源中缺少某段内容而删除现有文档。
