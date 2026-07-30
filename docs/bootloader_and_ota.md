# Bootloader 与 OTA

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
错误恢复，不再复制一套 UART 驱动。ISR 不解析 OTA 帧、不计算整包 CRC、不写
QSPI、不打印日志。OTA 接收和写入不得进入 `control_task`。

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
  刷新从 Application 继承的实例；冷启动且 IWDG 未运行时，reload 键不会启动它。
- 安装或回滚完成后提交元数据并执行系统复位；Application 随后重新配置正常周期并负责
  持续刷新。Recovery 停止刷新，使继承的 IWDG 复位；无有效 Application 时停留在明确的
  Recovery 状态。

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
9. 安装中断电时，根据 `INSTALLING` 状态从完整 QSPI 镜像重新开始安装。
10. 候选安装任一步失败时，有 confirmed 槽则提交 `ROLLBACK_PENDING` 并自动重装旧版；
    无 confirmed 槽才进入 `FAILED` 等待恢复。

`CONFIRMED` 普通启动同样按 confirmed 槽镜像头校验内部 Application 的完整 CRC；校验
失败时从 confirmed 槽重新安装。Application 与 Bootloader 均通过共享的
`qspi_flash_identity.h` 严格接受 W25Q64 JEDEC `EF 40 17`。

恢复后不得自动继续旧的运动命令或旧 OTA 会话。

Application 确认运行在非实时 Application task。当前实现要求 QSPI JEDEC 检查、
Application task、`control_task` 和关键故障状态连续健康 5 秒；随后通过 DMA 读取
Metadata A/B，只接受最新合法 `TRIAL`，擦除并写入非当前副本，再回读校验。确认记录将
`confirmed_slot` 设为原 `candidate_slot`，清空 candidate、trial count 和 last error。
该流程与人工 QSPI 擦写测试互斥，不依赖 CAN 外部节点在线，也不进入 `control_task`。

## 打包

```powershell
python tools/ota/package_firmware.py `
  firmware/application/stm32g474/Release/chassis_controller.bin `
  _output/application/chassis-controller-0.2.0-build1-20260730-120000.ota `
  --version 0.2.0 --build 1
```

打包器检查向量表、链接地址、长度、头部 CRC32 和 payload CRC32。仍链接到
`0x08000000` 的旧 Application 必须被拒绝。2026-07-28 的构建打包结果记录在
`verification.md`；正式 factory 产物已完成 DFU 烧录、校验和普通复位启动验证。

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
  firmware/bootloader/stm32g474/Release/bootloader.bin `
  firmware/application/stm32g474/Release/chassis_controller.bin `
  _output/factory/chassis-controller-app-0.2.0-build1_bootloader-0.1.0-build15-20260730-120000-factory.bin
```

生成器校验两个向量表、区域上限和 Reset Handler 地址，并将 Application 固定放到
组合 BIN 的 `0x8000` 偏移。该 BIN 在 STM32CubeProgrammer 中以
`0x08000000` 为下载地址，只用于首次安装或完整恢复。

## 主机发送

UART 需要 Python 3 和 `pyserial`：

```powershell
python -m pip install pyserial
python tools/ota/send_uart.py COM7 `
  _output/application/chassis-controller-0.1.0-build6-20260730-201234.ota
```

工具先发送 `telemetry off` 和 `ota uart confirm`，收到 READY 后切换为二进制协议。
不要同时打开串口助手或 VOFA+，否则串口会被其他进程占用。

Jetson 先按 `verification.md` 配置 `can0`，再发送：

```bash
python3 tools/ota/send_canfd.py can0 \
  _output/application/chassis-controller-0.1.0-build6-20260730-201234.ota
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
