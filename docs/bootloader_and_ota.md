# Bootloader 与 OTA

## 方案

STM32G474VET6 使用“内部单 Application + QSPI 双镜像槽”：

- 内部 Flash：32 KiB Bootloader + 480 KiB Application。
- 板载 8 MiB QSPI：双副本元数据、物理 Slot A/B、参数、日志和受控测试扇区。
- Application 通过 UART 或 CAN FD 接收镜像并写入候选槽。
- Bootloader 校验候选镜像，安装到内部 Application 区并负责试运行确认和回滚。
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

## 内部 Flash

| 区域 | 起始地址 | 大小 | 说明 |
| --- | ---: | ---: | --- |
| Bootloader | `0x08000000` | 32 KiB | 启动、校验、安装、确认和回滚 |
| Application | `0x08008000` | 480 KiB | FreeRTOS 底盘应用 |

Bootloader 只能擦写 Application 区，不得擦写自身。Application 的链接地址和
VTOR 必须同时迁移到 `0x08008000`，并与打包工具和镜像头保持一致。

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
6. 安装完成后标记 `TRIAL`，严格检查向量表后跳转。
7. Application 完成最小启动自检后显式确认，候选槽才成为 confirmed 槽。
8. 试运行未确认或连续异常复位时，从旧 confirmed 槽重新安装。
9. 安装中断电时，根据 `INSTALLING` 状态从完整 QSPI 镜像重新开始安装。

恢复后不得自动继续旧的运动命令或旧 OTA 会话。

## 打包

```powershell
python tools/ota/package_firmware.py `
  firmware/application/stm32g474/Release/chassis_controller.bin `
  _output/chassis-controller-0.2.0.ota `
  --version 0.2.0 --build 1
```

打包器检查向量表、链接地址、长度、头部 CRC32 和 payload CRC32。仍链接到
`0x08000000` 的旧 Application 必须被拒绝。

## 安全边界

- CRC32 只能检测损坏，不能证明镜像来源。
- Bootloader 不运行 FreeRTOS，不接管底盘控制业务。
- OTA 接收、CRC 整包校验和 QSPI 擦写不得阻塞 100 Hz 控制任务。
- 下载、提交、安装和复位前都必须满足停车及危险操作互斥规则。
- V1 Bootloader 不直接接收完整 UART/CAN FD 镜像；Recovery 放到 V2。
- 数字签名落地前，不启用强制防回滚策略，也不宣称 OTA 具备安全认证。

实施顺序见 `roadmap.md`，构建和实物验收见 `verification.md`。
