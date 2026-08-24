# 线协议契约

本目录保存 STM32、Jetson 和主机工具共同遵守的线上兼容契约，不参与固件编译。

- `chassis_canfd.yaml`：底盘 CAN FD 报文的机器可读定义。
- `canfd_protocol.md`：底盘 CAN FD 通道、状态机和兼容规则。
- `ota_canfd_protocol.md`：CAN FD OTA 传输协议。
- `uart_message_protocol.md`：UART 调试、响应和遥测消息格式。

协议在固件中的实现位于
`firmware/application/stm32g474/subsys/communication/`：`can/` 负责 CAN
传输及底盘报文编解码，`uart/` 负责结构化 UART 消息，`ota/` 负责跨 CAN/UART
的 OTA 会话。产品调试命令位于 `firmware/application/stm32g474/app/console/`，不属于
可复用通信协议实现。
