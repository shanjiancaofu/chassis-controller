# 开发路线

本文保留阶段完成项和后续范围。新对话需要的短状态见
[`current_status.md`](current_status.md)，可复现构建和硬件证据见 [`verification.md`](verification.md)。

## 当前基线

阶段 4 已完成代码实现：

- FreeRTOS 静态 Application task 和高优先级 `control_task`
- TIM6 100 Hz 任务通知，过期控制周期不连续补跑
- 控制源所有权、CAN 200 ms 超时和危险测试互斥
- FDCAN error-passive、bus-off 处理与会话撤销
- 关键任务健康汇总和条件喂狗
- `chassis`、`safety`、`parameters`、`diagnostics` 业务域

Application 与 Bootloader 的 Debug 和 Release 已在 OTA 代码落地后重新 clean build。
构建尺寸和未完成的实物回归见 `verification.md`。

## 当前结构到最终结构

`architecture.md` 中的完整目录树是最终设计。只有对应逻辑落地时才创建目录和文件：

| 当前实际状态 | 最终位置或变化 | 时机 |
| --- | --- | --- |
| 五个 `firmware/shared/*.h` | Bootloader、Application 和主机工具共用固定 ABI/硬件契约 | 已完成首版，后续兼容修改必须提升格式版本 |
| Application `components/pid/`、`components/crc/` | Bootloader 保持独立 CRC 实现 | 已按独立工程边界拆分 |
| `communication/can_transport/`、`communication/ota_transport/` | 增加 `chassis_protocol/` | 正式底盘协议编解码落地时 |
| `infrastructure/` | 保留现有命名，后续增加参数存储 | 参数持久化实现时 |
| `modules/chassis/` 等业务域分组 | 保留当前高内聚组织 | 不再反向平铺 |
| `rtos/rtos_app.c/h` 两任务模型 | 仅按真实阻塞或周期需求新增任务 | OTA 接收需要独立非实时任务时 |
| `tests/target/` 和 `tests/unit/` | 按风险补目标板测试和无 HAL 主机测试 | CommandManager 首批主机测试已落地 |
| 独立 Bootloader 工程 | 继续与 Application 保持独立 CubeMX、链接脚本和构建配置 | 已完成首版 |

迁移不改变底盘行为，不批量创建空目录，不建立只转发调用的空层。

## 当前阶段：Bootloader 与 OTA

状态：`IN PROGRESS`

已完成：

- 决定内部 32 KiB Bootloader + 480 KiB 单 Application
- 建立 64 字节镜像头和双副本 OTA 元数据 ABI
- 将板载 QSPI OTA 区划分为物理 Slot A 和 Slot B
- 由元数据分配 confirmed/candidate 角色，不把角色写死到物理地址
- 建立主机固件打包工具
- 确定 UART 与 CAN FD 都是 OTA V1 正式传输
- UART 已具备 circular DMA、IDLE 接收、软件环形缓冲和 TX DMA 队列
- 建立 Bootloader 纯逻辑首批代码：CRC32、镜像头/载荷校验、向量表校验、
  OTA 元数据双副本校验和 sequence 选择
- 建立独立裸机 Bootloader CubeMX/CubeIDE 工程，运行时使用 QSPI、SWD、直接寄存器
  USART1 trace 和时钟；`.ioc` 仍保留 IWDG 外设，但 `MX_IWDG_Init()` 在 USER CODE 中
  直接返回，Bootloader 只刷新继承实例
- Bootloader 链接区限制为 `0x08000000` 起始的 32 KiB
- 接入 W25Q64 JEDEC 检查、元数据双副本读取和交替提交
- 实现候选镜像流式 CRC 校验、内部 Flash 双 Bank 擦写、写后校验和严格跳转
- 实现 `STAGED -> INSTALLING -> TRIAL`、试运行计数和 confirmed 镜像回滚路径
- Debug 与 Release 均完成 clean build，结果见 `verification.md`
- Application 链接区迁移到 `0x08008000`、大小限制为 480 KiB，Debug/Release 的
  VTOR 均经 ELF 反汇编确认写入 `0x08008000`
- Application 增加非实时试运行确认状态机：连续 5 秒关键任务健康后读取双副本元数据，
  仅将合法 `TRIAL` 交替提交为 `CONFIRMED`，并与人工 QSPI 擦写测试互斥
- 当前 Release BIN 已通过 OTA 打包器的向量表、地址、长度和 CRC32 校验
- Application 已实现统一 OTA 会话、停车维护锁、UART 二进制帧解析、CAN FD 64 字节
  分块传输、QSPI DMA 写入、整包/镜像校验和 `STAGED` 元数据提交
- UART 进入二进制模式后仅在等待 BEGIN 的阶段执行 30 秒超时；会话开始或 tracked 响应
  尚未完成时不会禁用 transport 或混发文本
- 失败或中止时等待 QSPI 内部擦写结束后才释放维护锁；清理超过 10 秒则锁存 critical fault、
  保持停车并由 IWDG 复位，不会无限等待或假定 Flash 空闲
- UART OTA 响应使用 token 等待对应 DMA 完成，失败时重试同一帧；最终响应确认成功后才复位
- 已提供 UART 和 SocketCAN CAN FD stop-and-wait 主机发送工具
- 已生成并校验 Bootloader + relocated Application 首次组合烧录产物
- 已使用 DFU 烧录正式组合镜像，验证 Bootloader 普通启动可跳转到 relocated Application
- 已使用 UART 完成 build6 镜像的真实传输、QSPI 暂存、候选安装、TRIAL 启动和
  CONFIRMED 提交，UART OTA 主链达到 `HARDWARE PASS`
- Bootloader 固定使用 16 MHz HSI；不主动启动或重配置 IWDG，只刷新 Application 继承的实例；
  Application 正常周期约 10 秒，OTA 复位前约 30 秒，Recovery 停止刷新
- 运动命令与维护 owner 已拆分；`pid stop` 不能释放 OTA 锁，Console 目标持续到明确停止，
  CAN 目标仍使用 200 ms heartbeat timeout
- OTA 擦除阶段使用独立总时限和内部进度时限，人工 QSPI 测试与试运行确认失败后均等待
  DMA 终止和 Flash WIP 清除
- CAN OTA 响应使用 Tx Event 确认和软件重试；候选安装失败时自动回滚 confirmed 槽
- `FAILED` 禁止向量表 fallback；仅双 metadata 擦除态允许 factory fallback，QSPI/metadata
  故障进入 Recovery；`EMPTY/RECEIVING` 无 confirmed 时也进入 Recovery
- candidate、rollback 和 confirmed repair 的内部安装均使用持久化三次上限
- 三次上限耗尽时先验证内部镜像；若最后一次安装已成功但 metadata 提交前掉电，则补交
  `TRIAL/CONFIRMED`，避免把完整镜像误判为 FAILED；confirmed 成功后同步回填 image size/CRC
- factory 工具可从同一 `.ota` 生成 Slot A + `CONFIRMED` Metadata A 的 8 MiB QSPI raw
  provisioning 镜像；默认缺少 QSPI 参数会失败，仅诊断可显式 `--internal-only`
- 试运行确认最多重试 3 次，持续失败时锁存 critical fault 并由 IWDG 复位；内部 Flash
  改为逐页擦除并在页间刷新继承的 IWDG
- TRIAL/CONFIRMED 启动执行完整 Application CRC 校验，Application/Bootloader 共用
  W25Q64 JEDEC `EF 40 17` 准入契约
- `test_ota_transfer.py`、`test_factory_image.py`、`test_shared_abi.py`、Application OTA metadata
  和 Bootloader core 宿主机测试覆盖安装上限、每次中断后的完整 QSPI/internal salvage、
  rollback attempts=0 健康镜像恢复、状态槽约束、factory fail-closed、QSPI 布局和字段
  offset 级共享 C/Python ABI；共享 C 头同时使用 ARM GCC `offsetof` 静态断言，本轮 Python
  9 项于 2026-08-13 通过
- confirmed 恢复已区分 source invalid、I/O error 和 internal mismatch；只有 source 完整且
  internal mismatch 才允许擦写，I/O 先有限非破坏性重试，confirmed global fatal 直接 Recovery
- Bootloader 已嵌入 `0.1.0` 功能版本和 build22 构建号，启动串口输出两者；build22
  Release 已 clean build，尚未烧板回归
- 已清除 Git 跟踪的 `tools/ota/__pycache__/*.pyc`，并增加通用 Python 缓存忽略规则
- Bootloader `.ioc` 有意保留 IWDG；生成的 `MX_IWDG_Init()` 继续在 USER CODE 中提前返回，
  从而保留 CubeMX 工程结构但不在 Bootloader 主动启动或重配置 IWDG

OTA V1 最终验收按以下顺序执行，不再继续扩展 recovery 边角：

1. 使用 b12/build22 验证普通启动和上电 PWM 为零。
2. 完成 UART OTA 和 Jetson SocketCAN CAN FD OTA 闭环。
3. 只做三个关键故障测试：Application 安装过程中断电、TRIAL 不确认自动回滚、rollback
   安装过程中断电。
4. 全部通过后冻结 OTA V1。之后只修实测发现的 P0/P1，不主动增加 OTA 功能。

OTA V1 冻结门槛：

```text
普通启动 + 零 PWM + UART OTA + CAN FD OTA
+ 安装断电恢复 + TRIAL 未确认回滚 + rollback 断电恢复
```

## 后续阶段

### 底盘功能

OTA V1 冻结后立即进入底盘功能，顺序如下：

1. PID 实物闭环调通；先固定速度单位、正负方向、心跳超时和停车语义。
2. 左右轮标定和加减速限制。
3. 里程计累计、速度和转角验证。
4. 堵转、编码器异常和电压保护。
5. Fault、Health 和 Reset 诊断闭环。
6. 在已验证行为基础上完善并冻结正式 CAN FD 底盘协议。

### 发布安全

以下内容属于 OTA V2，不进入 OTA V1 冻结范围：

- 固件数字签名
- 防回滚计数
- Jetson 发布流程和版本兼容检查
- 签名验收后再决定是否需要镜像加密
- Bootloader CAN FD Recovery

### IMU 与里程计

STM32端ICM45686 SPI3/DMA、16字节FIFO、timestamp、掉线恢复、RAM零偏标定和六轴Mahony
已经实现，当前等待实物确认 `WHO_AM_I`、连续采样、零偏收敛和安装轴向。硬件确认后再决定
是否持久化零偏；20-bit、压缩FIFO和自检不属于当前上板门槛。

STM32轮式里程计仍属于当前底盘阶段。Jetson后续消费轮式里程计和IMU输出并负责ROS 2
定位融合，不在STM32重复实现导航级融合。

## 暂不实施

- 位置环
- 复杂底盘运动学
- 超出当前六轴Mahony的STM32端复杂姿态/导航融合
- 内部 Flash 双 Application Bank
- Bootloader FreeRTOS
- 无人值守远程发布
