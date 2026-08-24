add_library(chassis_communication STATIC
  can/can_transport.c
  can/chassis_protocol.c
  ota/ota_can.c
  ota/ota_confirmation.c
  ota/ota_metadata.c
  ota/ota_session.c
  ota/ota_uart.c
  uart/uart_messages.c)

chassis_target_setup(chassis_communication)
target_link_libraries(chassis_communication PUBLIC chassis_drivers chassis_components)
