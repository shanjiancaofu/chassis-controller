# Bootloader 与 OTA

## 当前结论

项目从阶段 5 开始优先建设 Bootloader 与 OTA，IMU、里程计、ROS 2 Bridge 和复杂运动学后移。

STM32G474VET6 只有 512 KiB 内部 Flash，当前 Release Application 已超过 170 KiB，
因此不照搬 STM32F411 示例的内部双 Application Bank。当前方案采用：

- 内部 Flash：独立 Bootloader + 单 Application。
- 板载 8 MiB QSPI：双副本升级元数据、已确认镜像、候选镜像和升级日志。
- 升级期间断电：Bootloader 保留，重新上电后根据双副本元数据重新安装。
- OTA V1 支持试运行确认；候选 Application 未确认时，Bootloader 从 QSPI
  已确认镜像恢复，不在内部 Flash 伪造双 Application Bank。
- 第一版只使用 CRC32 做传输和存储损坏检测，不把 CRC32 描述成安全认证。
- 第二版先加入数字签名与防回滚，再评估是否需要镜像加密。

参考仓库的 UART DMA、状态机、SHA-256 和签名分层可作为设计输入，但芯片地址、
Flash 擦除单位、向量表和密钥方案必须以当前 STM32G474 工程为准。

参考取舍：

- [`freertos_uart_output`](https://github.com/ParacosmYy/GS_ETERNALCHIP_Mcu/tree/freertos_uart_output)：
  保留 DMA/IDLE 中断只投递数据、任务上下文解析的原则；当前 UART DMA 已具备同类边界。
- [`OTA_Bootloader`](https://github.com/ParacosmYy/GS_ETERNALCHIP_Mcu/tree/OTA_Bootloader)：
  参考镜像校验、状态记录和跳转顺序，不复制 F411 分区，也不接受校验失败后的宽松跳转。
- [`OTA_Encrypted_Upgrade`](https://github.com/ParacosmYy/GS_ETERNALCHIP_Mcu/tree/OTA_Encrypted_Upgrade)：
  参考验证、传输、平台和实现层的职责划分；AES、ECDSA 和密钥配置放到阶段 6。

## OTA V1 传输

OTA V1 正式支持两个 Application 侧传输入口：

- CAN FD：Jetson 正式升级通道。
- USART1：本地维护、生产烧录和调试升级通道，使用 DMA、IDLE 检测和软件环形缓冲区。

两个入口复用同一套 `BEGIN/DATA/END/ABORT` 分包语义、会话状态机、镜像校验和 QSPI
写入逻辑，不分别实现两套 OTA。UART 和 CAN FD 都是 V1 正式能力，不存在由一个替代
另一个的关系。任一时刻只能有一个传输源持有 OTA 会话；UART 会话活动时拒绝新的
CAN FD `BEGIN`，反之亦然。会话释放前必须保持底盘停车、PWM 为零且运动控制所有权
已撤销。

UART 接收路径固定为：

```text
USART1 RX
  -> DMA circular buffer
  -> IDLE / half-transfer / transfer-complete ISR
  -> software ring buffer
  -> OTA receive task
  -> frame parser and sequence check
  -> QSPI chunk writer
```

约束：

- DMA 与 UART ISR 只计算新到达字节范围、推进生产者位置并通知任务。
- ISR 不解析 OTA 帧，不计算整包 CRC，不擦写 QSPI，不打印日志。
- 软件环形缓冲区采用单生产者、单消费者模型；不得覆盖尚未消费的数据。
- 环形缓冲区空间不足、帧超长、序号跳变、接收超时或 CRC 错误时中止本次候选镜像。
- QSPI 擦写按块在非实时任务中执行，不得进入 `control_task`。
- UART TX 使用 DMA 返回 ACK、NAK、当前偏移和错误码，发送路径不得阻塞控制任务。
- CAN FD 与 UART 只负责可靠分块传输；完整镜像仍由 Application 和 Bootloader
  分别校验一次。

OTA V2 再增加 Bootloader CAN FD Recovery。V1 的 Bootloader 不直接通过 CAN FD
或 UART 接收完整固件，避免第一版 Bootloader 膨胀。

## 存储布局

### 内部 Flash

| 区域 | 起始地址 | 大小 | 说明 |
|---|---:|---:|---|
| Bootloader | `0x08000000` | 32 KiB | 启动、校验、安装和跳转 |
| Application | `0x08008000` | 480 KiB | FreeRTOS 底盘应用 |

内部 Flash 页大小按当前 STM32G4 HAL 定义为 2 KiB。Bootloader 只能擦写 Application
区域，不得擦写自身。Application 在 Bootloader 可独立构建和烧录前仍保持原链接地址，
避免破坏当前开发板启动；切换链接地址必须与首次 Bootloader 烧录在同一批完成。

### QSPI

| 区域 | 起始地址 | 大小 | 说明 |
|---|---:|---:|---|
| 参数 | `0x00000000` | 64 KiB | 现有预留 |
| 日志 | `0x00010000` | 4032 KiB | 现有预留 |
| Metadata A | `0x00400000` | 4 KiB | 元数据副本 A |
| Metadata B | `0x00401000` | 4 KiB | 元数据副本 B |
| Firmware Slot A | `0x00402000` | 2040 KiB | 已确认或候选镜像槽 A |
| Firmware Slot B | `0x00600000` | 2044 KiB | 已确认或候选镜像槽 B |
| 人工擦写测试 | `0x007FF000` | 4 KiB | 现有受控测试扇区 |

元数据使用序号和 CRC32 选择最新有效副本。写入新状态时先写非当前副本，校验成功后
再把它作为最新记录，不能原地覆盖唯一有效记录。两个 QSPI 固件槽不等于内部双 Bank：
STM32 始终只从内部 Application 区运行，Bootloader 根据元数据选择 QSPI 镜像并安装。

## 镜像格式

`firmware/shared/ota/ota_image.h` 是 Bootloader、Application 和主机打包工具共同遵守的
存储 ABI。第一版头部固定为 64 字节，包含：

- magic 和格式版本；
- payload 长度与 CRC32；
- 加载地址和向量表地址；
- 固件版本、构建号和防回滚计数；
- flags、保留字段和头部 CRC32。

第一版 `flags` 必须为 0。未知格式、未知 flags、越界长度、非法初始栈地址、非 Thumb
Reset Handler、错误 CRC 或错误目标地址都必须拒绝安装和跳转。

打包命令：

```powershell
python tools/ota/package_firmware.py `
  firmware/application/stm32g474/Release/chassis_controller.bin `
  _output/chassis-controller-0.2.0.ota `
  --version 0.2.0 --build 1
```

打包器会拒绝仍链接到 `0x08000000` 的旧 Application，这是防止烧入错误镜像的预期行为。

## 安装状态机

第一版保留安装和试运行确认所需状态：

```text
EMPTY -> STAGED -> INSTALLING -> TRIAL
                    |             |
                    |             +-> CONFIRMED
                    |             |
                    |             +-> ROLLBACK_PENDING
                    |                    |
                    +-> FAILED <---------+-> INSTALLING -> CONFIRMED
```

流程：

1. Application 在停车、零 PWM、无运动控制所有权时接收升级包。
2. 包完整写入 QSPI 非活动固件槽后，Application 校验头和 payload，再提交 `STAGED`
   元数据；当前已确认槽保持不变。
3. Application 安全停车并复位。
4. Bootloader 读取两份元数据，选择序号更新且 CRC 正确的记录。
5. Bootloader 再次校验镜像，标记 `INSTALLING`，按页擦除并按 double-word 写入 Application。
6. Bootloader 对内部 Application 重新计算 CRC；成功后标记 `TRIAL`。
7. Bootloader 严格检查向量表，关闭中断和 SysTick，设置 VTOR/MSP 后跳转。
8. 新 Application 完成最小启动自检后显式写入 `CONFIRMED`，候选槽才成为新的
   已确认槽。
9. 试运行未确认、连续异常复位或超过允许启动次数时，Bootloader 标记
   `ROLLBACK_PENDING`，从旧的已确认 QSPI 槽重新安装。
10. 任一步失败都保持在 Bootloader；没有可校验的已确认镜像时，不允许宽松跳转。

安装过程中断电后，`INSTALLING` 状态必须触发从所选 QSPI 槽重新完整安装。这里的
“切换和回滚”是切换 QSPI 固件槽并重新安装内部单 Application，不是直接从 QSPI
执行，也不是内部 Flash 双 Application Bank。

## 安全边界

- CRC32 只能检测损坏，不能证明镜像来源。
- 第二版使用 Bootloader 内置公钥验证签名；私钥只保留在构建/发布环境。
- 加密不能替代签名。若后续增加 AES，密钥不得以明文常量放进 Application。
- 防回滚计数只有在签名验证落地后才作为强制策略启用。
- Bootloader 不运行 FreeRTOS，不接管底盘控制业务。
- OTA 接收不进入 `control_task`，不得阻塞 100 Hz 控制。
- 下载、校验、提交和复位前都必须满足停车与危险操作互斥规则。

## 实施批次

### 批次 1：格式与布局

- [ ] 将已确认/候选 QSPI 双固件槽同步到共享布局。
- [ ] 将试运行、确认和回滚状态同步到共享元数据 ABI。
- [x] 建立主机打包与基本合法性检查。
- [x] 将 IMU 后移并把 OTA 提升为阶段 5。

### 批次 2：独立 Bootloader

- [ ] 使用 CubeMX 建立 `firmware/bootloader/stm32g474/` 独立 CubeIDE 工程。
- [ ] 只启用时钟、GPIO、USART1、QUADSPI 和内部 Flash 所需 HAL。
- [ ] 实现双副本元数据、CRC32、内部 Flash 安装和严格跳转。
- [ ] 完成 Debug/Release 构建，不与 Application 共用链接脚本。

### 批次 3：Application OTA 接收

- [ ] 将 Application 链接到 `0x08008000` 并同步 VTOR。
- [ ] 增加非实时 OTA 接收状态机和分块写入。
- [ ] 使用 USART1 RX DMA circular + IDLE + 软件环形缓冲区打通本地 OTA。
- [ ] 使用 USART1 TX DMA 返回流控、进度和错误响应。
- [ ] 同时完成 UART 和 CAN FD 两种正式传输，两种入口复用同一状态机并分别验收。
- [ ] 下载期间允许底盘保持停车诊断，但不允许运动控制和危险目标测试。

### 批次 4：实物验证

- [ ] 正常升级。
- [ ] 错误 magic、地址、长度和 CRC 均拒绝。
- [ ] QSPI 写入、内部擦除和内部写入阶段分别断电后可恢复安装。
- [ ] 候选镜像未确认、连续异常复位后可恢复已确认镜像。
- [ ] UART 环形缓冲区溢出、乱序、超时和传输切换均安全中止。
- [ ] UART 和 CAN FD 均可独立完成同一固件包的接收、校验、提交和安装。
- [ ] 无有效 Application 时停留 Bootloader。
- [ ] 有效 Application 可稳定跳转，复位原因和版本可诊断。

签名、防回滚和可选加密在上述基础链路稳定后单独实施，不与第一版混在一起。
