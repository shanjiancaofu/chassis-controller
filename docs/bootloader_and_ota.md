# Bootloader 与 OTA

本文是 Bootloader、Flash/QSPI 布局、OTA 状态机和恢复策略的权威设计文档。当前实现与
待验证项见 [`current_status.md`](current_status.md)，线协议见
[`../protocol/ota_canfd_protocol.md`](../protocol/ota_canfd_protocol.md)，证据见
[`verification.md`](verification.md)。

## 方案

STM32G474VET6 使用“内部单 Application + QSPI 双镜像槽”：

- 内部 Flash：32 KiB Bootloader + 480 KiB Application。
- 板载 8 MiB QSPI：双副本元数据、物理 Slot A/B、参数、日志和受控测试扇区。
- Application 通过 UART 或 CAN FD 接收镜像并写入候选槽。
- Bootloader 校验候选镜像，安装到内部 Application 区并负责试运行计数和回滚；
  Application 负责在健康窗口通过后提交确认。
- Slot A/B 是物理位置；confirmed/candidate 是元数据角色，可以互换。
- OTA V1 使用 CRC32 检测传输和存储损坏，不把 CRC32 当成身份认证。
- OTA V2 再增加数字签名、防回滚和 Bootloader CAN FD Recovery。

## OTA V1 传输

OTA V1 支持两个 Application 侧入口：

- CAN FD：Jetson 正式升级通道。
- USART1：本地维护和生产调试通道。

两种入口复用 `BEGIN/DATA/END/ABORT/STATUS` 语义、会话状态机、镜像校验和
QSPI 写入逻辑。任一时刻只能有一个传输源持有 OTA 会话。

UART 直接复用当前 BSP：

```text
USART1 RX
  -> circular DMA + IDLE
  -> software ring buffer
  -> non-real-time OTA receiver
  -> frame and sequence validation
  -> QSPI candidate-slot writer
```

现有 UART BSP 已具备 circular DMA、IDLE 接收、RX 环形缓冲、TX DMA 队列和
错误恢复。OTA 响应使用发送 token 跟踪 DMA 完成结果，异步失败时重试同一响应；只有
完成回调确认成功后，最终 `STAGED` 响应才允许触发复位。ISR 不解析 OTA 帧、不计算整包
CRC、不写 QSPI、不打印日志。OTA 接收和写入不得进入 `control_task`。

Application 使用 stop-and-wait 会话，DATA 必须按 `next_offset` 连续提交。UART 模式
通过 `ota uart confirm` 显式进入，30 秒未收到 BEGIN 自动退出。UART/CAN 响应发送
失败会重试；CAN FD 响应通过 Tx Event 确认实际传输完成，`STAGED` 最终响应确认完成后
才允许复位，不能只依据 TX FIFO 空闲。失败或 ABORT 发生在
扇区擦除期间时，维护锁保持到 Flash 内部 busy 清除，不假装已经取消擦除。

5 秒会话超时只用于 `RECEIVING` 阶段的主机通信空闲检测。`PREPARING` 使用独立的
120 秒总时限和 6 秒内部进度时限；每完成一个扇区擦除都会刷新进度。单次 DMA、页编程
和扇区擦除仍分别使用自己的操作 deadline。合法同会话 `STATUS` 会刷新主机活动时间。

## 内部 Flash

| 区域 | 起始地址 | 大小 | 说明 |
| --- | ---: | ---: | --- |
| Bootloader | `0x08000000` | 32 KiB | 启动、校验、安装、确认和回滚 |
| Application | `0x08008000` | 480 KiB | FreeRTOS 底盘应用 |

Bootloader 只能擦写 Application 区，不得擦写自身。Application 的链接地址和
VTOR 已同时迁移到 `0x08008000`，并与打包工具和镜像头保持一致。

## 启动时钟与看门狗

- Bootloader 使用内部 16 MHz HSI，不启用 HSE/PLL；QSPI 输入为 16 MHz，分频后工作在
  约 8 MHz。Application 跳转后自行配置 170 MHz 业务时钟。
- 普通启动路径不启动 Bootloader IWDG，避免将不可停止的看门狗状态直接继承给
  Application。
- Application 正常运行时配置约 10 秒 IWDG；OTA 请求复位前临时切换到约 30 秒并刷新。
- Bootloader 不主动启动或重配置 IWDG，只在 QSPI、内部 Flash 擦写、编程和校验循环中
  刷新从 Application 继承的实例；内部 Application 按页擦除并在页间刷新。冷启动且
  IWDG 未运行时，reload 键不会启动它。
- `bootloader.ioc` 当前仍保留 IWDG 为启用状态，但生成的 `MX_IWDG_Init()` 在 USER CODE
  区域立即返回。实际运行行为以上一条为准；CubeMX 重新生成后必须检查该保护仍然存在。
- 安装或回滚完成后提交元数据并执行系统复位；Application 随后重新配置正常周期并负责
  持续刷新。Recovery 停止刷新，使继承的 IWDG 复位；无有效 Application 时停留在明确的
  Recovery 状态。

Bootloader 的功能版本和构建号定义在 `config/build_info.h`。启动串口输出格式为
`BOOT: VERSION=0.1.0 BUILD=22`：功能或兼容行为变化时更新语义版本，同一版本下的不同
构建递增 build 号。

## QSPI

| 区域 | 起始地址 | 大小 | 说明 |
| --- | ---: | ---: | --- |
| 参数 | `0x00000000` | 64 KiB | 参数预留 |
| 日志 | `0x00010000` | 4032 KiB | 日志预留 |
| Metadata A | `0x00400000` | 4 KiB | 元数据副本 A |
| Metadata B | `0x00401000` | 4 KiB | 元数据副本 B |
| Firmware Slot A | `0x00402000` | 2040 KiB | confirmed 或 candidate |
| Firmware Slot B | `0x00600000` | 2044 KiB | confirmed 或 candidate |
| 人工擦写测试 | `0x007FF000` | 4 KiB | 受控板上测试扇区 |

两个镜像槽都能容纳完整 480 KiB Application 和 64 字节镜像头。元数据使用
sequence 和 CRC32 选择最新有效副本；更新时先写非当前副本，校验成功后才生效。

## 共享 ABI

以下文件必须由 Bootloader、Application 和主机工具共同遵守：

- `firmware/shared/flash_layout.h`
- `firmware/shared/firmware_image.h`
- `firmware/shared/ota_metadata.h`
- `firmware/shared/ota_protocol.h`
- `firmware/shared/qspi_flash_identity.h`

镜像头和元数据记录均固定为 64 字节。元数据记录：

- 当前状态；
- confirmed/candidate 对应的物理槽；
- 镜像长度和 CRC32；
- 安装尝试次数、试运行次数和最后错误；
- sequence、记录 CRC32 和保留字段。

共享 ABI 不放驱动和状态机。兼容性字段发生变化时必须提升格式版本。

## 状态机

```text
EMPTY -> RECEIVING -> STAGED -> INSTALLING -> TRIAL -> CONFIRMED
            |           |          |           |
            +-> FAILED <-+----------+           +-> ROLLBACK_PENDING
                                                   |
                                                   +-> INSTALLING -> CONFIRMED
```

1. Application 仅在停车、PWM 为零、无运动控制所有权时建立 OTA 会话。
2. 数据写入非 confirmed 的物理槽，完成后校验头部和 payload。
3. 校验成功后提交 `STAGED` 元数据并安全复位。
4. Bootloader 读取双副本元数据，再次校验候选镜像。
5. 标记 `INSTALLING`，擦写内部 Application 并进行写后校验。
6. 安装完成后标记 `TRIAL` 并执行系统复位；下一次启动按候选槽镜像头对内部
   Application 执行完整 CRC 和向量表校验后才跳转。
7. Application 完成最小启动自检后显式确认，候选槽才成为 confirmed 槽。
8. 试运行未确认或连续异常复位时，从旧 confirmed 槽重新安装。
9. candidate、rollback 和 confirmed repair 每次擦除内部 Flash 前都先持久化增加
   `install_attempts`，最多执行 3 次破坏性安装。每次从已开始的安装状态恢复时，都先验证
   目标 slot 的镜像头、完整 QSPI payload CRC、内部 Application CRC 和向量表；candidate
   还要求 slot header 的 size/CRC 与 metadata 一致。若上一次安装已完成但最终 metadata
   未提交，则直接补交 `TRIAL/CONFIRMED`，不消耗下一次破坏性安装；验证失败且次数未耗尽
   才重新安装，次数耗尽后 candidate 转 rollback/FAILED，confirmed 转 FAILED/Recovery。
10. 候选安装失败且存在 confirmed 槽时提交 `ROLLBACK_PENDING`。进入该状态后无论
    `install_attempts` 是否为零，都先完整验证 QSPI confirmed payload 与内部 Application；
    内部已是 confirmed 时直接补交 `CONFIRMED`，只有验证失败才执行破坏性重装。无 confirmed
    槽时进入 `FAILED`。`BOOT_INSTALL_ERROR_FLASH_LAYOUT` 表示 Bootloader/设备布局不兼容，
    candidate 与 confirmed 都无法安装，因此直接进入 Recovery，不执行无效 rollback。

confirmed 恢复验证返回 `MATCH`、`INTERNAL_MISMATCH`、`SOURCE_INVALID` 或 `IO_ERROR`，而不是
单一 bool。QSPI header、payload 向量表和完整 payload CRC 全部有效后，才比较内部 Application；
只有 `INTERNAL_MISMATCH` 允许开始破坏性重装。QSPI 读取错误进行最多 3 次非破坏性重试，持续
失败进入 Recovery；source header、向量或 CRC 无效时写入 `FAILED` 并进入 Recovery。confirmed
重装遇到全局 fatal 错误同样立即进入 `FAILED/Recovery`，不执行无意义的复位重试。

`STAGED/INSTALLING/TRIAL` 的 metadata `image_size/image_crc32` 描述 candidate；
`CONFIRMED` 状态描述 confirmed。rollback、repair 或掉电 salvage 成功后，Bootloader 从
confirmed slot 的已校验镜像头回填这两个字段，再提交 `CONFIRMED`。

`CONFIRMED` 普通启动同样按 confirmed 槽镜像头校验内部 Application 的完整 CRC；校验
失败时从 confirmed 槽重新安装。Application 与 Bootloader 均通过共享的
`qspi_flash_identity.h` 严格接受 W25Q64 JEDEC `EF 40 17`。

`FAILED`、QSPI 识别失败、metadata 读取失败和非空损坏 metadata 一律进入 Recovery，
不能使用前 8 字节向量表跳转。只有两份 metadata 都为擦除态的 factory 设备可使用向量表
fallback。`EMPTY/RECEIVING` 没有 confirmed 槽时进入 Recovery；存在 confirmed 槽时必须
通过完整安装镜像校验。`EMPTY` 只允许两个槽都为 `NONE`；`RECEIVING` 必须有 candidate。

OTA、试运行确认和人工 QSPI 测试的终止清理最多等待 WIP 10 秒。持续无法读取状态时保留
维护锁并锁存 critical fault，停车且停止刷新 IWDG。试运行确认失败会间隔 1 秒重试，最多
3 次；持续失败后同样触发看门狗复位，让 Bootloader 按 TRIAL 次数执行回滚策略。

恢复后不得自动继续旧的运动命令或旧 OTA 会话。

Application 确认运行在非实时 Application task。当前实现要求 QSPI JEDEC 检查、
Application task、`control_task` 和关键故障状态连续健康 5 秒；随后通过 DMA 读取
Metadata A/B，只接受最新合法 `TRIAL`，擦除并写入非当前副本，再回读校验。确认记录将
`confirmed_slot` 设为原 `candidate_slot`，清空 candidate、trial count 和 last error。
该流程与人工 QSPI 擦写测试互斥，不依赖 CAN 外部节点在线，也不进入 `control_task`。
因此当前自动确认只能覆盖启动和关键任务健康类故障；Jetson 显式验收授权仍属于后续发布策略。

## 打包

```powershell
python tools/ota/package_firmware.py `
  firmware/application/stm32g474/Release/chassis_controller.bin `
  _output/application/app-v<version>-b<app-build>.ota `
  --version <version> --build <app-build>
```

打包器检查向量表、链接地址、长度、头部 CRC32 和 payload CRC32。仍链接到
`0x08000000` 的旧 Application 必须被拒绝。当前构建产物、打包结果和实物状态记录在
`verification.md`；正式 factory 产物和 UART OTA 主链均已完成实物验证。

## 首次组合镜像

第一次引入 Bootloader 时不能只烧录 relocated Application。先从两个 Release ELF
生成 BIN，再创建组合镜像：

```powershell
arm-none-eabi-objcopy -O binary `
  firmware/bootloader/stm32g474/Release/bootloader.elf `
  firmware/bootloader/stm32g474/Release/bootloader.bin

arm-none-eabi-objcopy -O binary `
  firmware/application/stm32g474/Release/chassis_controller.elf `
  firmware/application/stm32g474/Release/chassis_controller.bin

python tools/ota/create_factory_image.py `
  _output/bootloader/boot-v0.1.0-b22.bin `
  _output/application/app-v0.1.0-b12.bin `
  _output/factory/factory-a12-b22.bin `
  --ota _output/application/app-v0.1.0-b12.ota `
  --qspi-output _output/factory/qspi-a12-confirmed.bin
```

生成器校验两个向量表、区域上限和 Reset Handler 地址，并将 Application 固定放到
组合 BIN 的 `0x8000` 偏移。该 BIN 在 STM32CubeProgrammer 中以
`0x08000000` 为下载地址。QSPI raw 镜像使用同一次构建生成的 `.ota`，将完整 package
写入 Slot A，并生成 `CONFIRMED/SLOT_A` Metadata A；Metadata B 保持擦除态。生产初始化
必须使用 External Loader 或等价工具同时写入内部 factory BIN 和 QSPI raw 镜像，才能建立
首次 OTA 可回滚基线。该 provisioning 流程尚未完成目标板验收。
工具默认缺少 `--ota/--qspi-output` 会失败。仅用于内部 Flash 诊断时可显式传入
`--internal-only`；该产物没有 QSPI confirmed 基线，不得作为正式 factory 发布。

## 主机发送

UART 需要 Python 3 和 `pyserial`：

```powershell
python -m pip install pyserial
python tools/ota/send_uart.py COM8 `
  _output/application/app-v<version>-b<app-build>.ota
```

工具先发送 `telemetry off` 和 `ota uart confirm`，收到 READY 后切换为二进制协议。
不要同时打开串口助手或 VOFA+，否则串口会被其他进程占用。

Jetson 先按 `verification.md` 配置 `can0`，再发送：

```bash
python3 tools/ota/send_canfd.py can0 \
  _output/application/app-v<version>-b<app-build>.ota
```

两个工具都等待 `PREPARING -> RECEIVING`，逐块等待 ACK，并只在最终收到 `STAGED`
后报告成功。脚本成功仅表示 Application 已暂存镜像；仍需观察复位、Bootloader 安装、
TRIAL 启动和 CONFIRMED 提交。

## 安全边界

- CRC32 只能检测损坏，不能证明镜像来源。
- Bootloader 不运行 FreeRTOS，不接管底盘控制业务。
- OTA 接收、CRC 整包校验和 QSPI 擦写不得阻塞 100 Hz 控制任务。
- 下载、提交、安装和复位前都必须满足停车及危险操作互斥规则。
- V1 Bootloader 不直接接收完整 UART/CAN FD 镜像；Recovery 放到 V2。
- 数字签名落地前，不启用强制防回滚策略，也不宣称 OTA 具备安全认证。

实施顺序见 `roadmap.md`，构建和实物验收见 `verification.md`。
