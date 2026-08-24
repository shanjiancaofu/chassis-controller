add_library(chassis_communication STATIC
  can_transport/can_transport.c
  chassis_protocol/chassis_protocol.c
  ota_transport/ota_can_transport.c
  ota_transport/ota_confirmation.c
  ota_transport/ota_metadata.c
  ota_transport/ota_session.c
  ota_transport/ota_uart_transport.c
  uart_protocol/uart_protocol.c)

chassis_target_setup(chassis_communication)
target_link_libraries(chassis_communication PUBLIC chassis_drivers chassis_components)
