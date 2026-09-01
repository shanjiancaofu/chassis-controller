add_library(chassis_drivers STATIC
  ../boards/chassis_g474/board_devices.c
  button/button_stm32.c
  encoder/encoder_stm32.c
  encoder/encoder_common.c
  safety/emergency_stop_stm32.c
  gpio/interrupts_stm32.c
  gpio/gpio_common.c
  gpio/gpio_stm32.c
  led/led_stm32.c
  display/lcd_stm32.c
  display/display_common.c
  sensor/sensor_common.c
  motor/motor_stm32.c
  motor/motor_common.c
  adc/power_sample_stm32.c
  adc/power_sample_common.c
  reset/reset_stm32.c
  crash/crash_context.c
  sensor/sr501_stm32.c
  can/can_common.c
  can/can_stm32_fdcan.c
  uart/uart_common.c
  uart/uart_stm32.c
  flash/flash_common.c
  flash/flash_stm32_qspi.c
  rtc/rtc_common.c
  rtc/rtc_stm32.c
  time/time_common.c
  time/time_stm32.c
  watchdog/watchdog_common.c
  watchdog/watchdog_stm32_iwdg.c)

if(CONFIG_ICM45686)
  target_sources(chassis_drivers PRIVATE sensor/icm45686_stm32.c)
endif()

chassis_target_setup(chassis_drivers)
target_link_libraries(chassis_drivers PUBLIC chassis_kernel chassis_components)
