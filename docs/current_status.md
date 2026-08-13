# 当前开发状态

本文件是新对话的最小交接上下文。它只保存当前结论，不替代设计、协议和完整验证记录。
具体细节按 [`README.md`](README.md) 的索引读取。

## 代码基线

- 当前已提交固件实现基线：`f496cd5 [fix]: verify recovery source before reinstall`。
- Application：`0.1.0-b12`。
- Bootloader：`0.1.0 build22`。
- 当前阶段：OTA V1 最终实物验证；除实测发现的 P0/P1 外，不再增加 recovery 细节。

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
- CMake + Ninja 已使用 GNU Arm Embedded 14.3.rel1 完成 Application、Bootloader 和 QSPI
  provisioner clean build；三个 BIN 的 SHA-256 分别与既有 b12、build22 和已上板 provisioner
  基线完全一致。CMake 作为主命令行构建入口，CubeIDE 暂留作对照和调试。
- OTA Python 共 9 项通过，包含字段顺序/offset 级共享 C/Python ABI 对照；两个目标 clean build
  已由 ARM GCC 验证共享结构体的逐字段 `offsetof` 静态断言。既有 factory、
  UART arm guard、Application metadata 和 Bootloader core 主机测试记录保持有效；本轮新增的
  rollback attempts=0/fatal 分类断言已通过目标 GCC `-Werror` 编译，尚未在宿主机执行。
- 最近完成实物 UART OTA 闭环的是 Application b6 + Bootloader build15。
- factory 普通启动和 UART `STAGED -> INSTALLING -> TRIAL -> CONFIRMED` 已实物通过。
- 已从当前 Release ELF 生成匹配的 b12/build22 Application BIN、OTA、内部 factory BIN 和
  8 MiB QSPI confirmed raw；OTA Python 9 项重新通过。产物生成不代表已烧录或实物通过。
- 已通过独立 DFU provisioner 将匹配的 b12 OTA package 和 `CONFIRMED/SLOT_A` metadata
  写入 W25Q64 并完成全包读回校验；随后恢复内部 factory BIN，实物确认 build22 读取
  `CONFIRMED` 并启动 b12。Application 稳定报告 QSPI `EF4017`、`OTA_CONFIRM: NOT_REQUIRED`
  和 `MOTOR: DISABLED`。

## 尚未验证

- b12/build22 普通按键/断电复位启动和上电四路 PWM 电气零输出。
- b12/build22 UART OTA 与 CAN FD OTA。
- Application 安装过程中断电恢复。
- TRIAL 不确认后的自动回滚。
- rollback 安装过程中断电恢复。

confirmation 持续失败、QSPI terminal cleanup 和其他 recovery 边角不属于当前冻结门槛；仅在
上述实测暴露 P0/P1 时处理。

## 下一步

1. 在已完成 CMake 产物等价验证的基线上接入 ICM45686；外设配置仍由 `.ioc`/CubeMX 管理。
2. 执行普通按键/断电复位启动和四路 PWM 上电电气零输出验收。
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
