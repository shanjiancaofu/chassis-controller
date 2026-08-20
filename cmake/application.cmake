set(APP_ROOT "${CMAKE_SOURCE_DIR}/firmware/application/stm32g474")
include("${CMAKE_SOURCE_DIR}/cmake/application_config.cmake")

set(APP_COMMON_INCLUDES
  "${APP_ROOT}"
  "${APP_ROOT}/include"
  "${APP_GENERATED_DIR}"
  "${APP_ROOT}/Core/Inc"
  "${APP_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc"
  "${APP_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"
  "${APP_ROOT}/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
  "${APP_ROOT}/Drivers/CMSIS/Include"
  "${APP_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include"
  "${APP_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
  "${APP_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F")

function(chassis_target_setup target)
  target_include_directories(${target} PRIVATE ${APP_COMMON_INCLUDES})
  target_compile_features(${target} PRIVATE c_std_11)
  target_compile_definitions(${target} PRIVATE
    STM32G474xx
    USE_HAL_DRIVER
    USER_VECT_TAB_ADDRESS
    VECT_TAB_OFFSET=0x00008000U
    $<$<CONFIG:Debug>:APP_DEBUG_IWDG_FREEZE>)
  target_compile_options(${target} PRIVATE
    ${STM32G474_CPU_OPTIONS}
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>
    -ffunction-sections
    -fdata-sections
    -Wall
    -fstack-usage
    $<$<COMPILE_LANGUAGE:C>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>
    "-include${APP_AUTOCONF_HEADER}")
endfunction()

set(APP_VENDOR_SOURCES
  Application/User/Core/syscalls.c
  Application/User/Core/sysmem.c
  Application/User/Startup/startup_stm32g474vetx.s
  Core/Src/adc.c
  Core/Src/app_freertos.c
  Core/Src/dma.c
  Core/Src/fdcan.c
  Core/Src/gpio.c
  Core/Src/iwdg.c
  Core/Src/main.c
  Core/Src/quadspi.c
  Core/Src/rtc.c
  Core/Src/spi.c
  Core/Src/stm32g4xx_hal_msp.c
  Core/Src/stm32g4xx_hal_timebase_tim.c
  Core/Src/stm32g4xx_it.c
  Core/Src/system_stm32g4xx.c
  Core/Src/tim.c
  Core/Src/usart.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_adc_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_exti.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_fdcan.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_flash_ramfunc.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_iwdg.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_qspi.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rtc_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_spi_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_tim_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_uart_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_ll_adc.c
  Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c
  Middlewares/Third_Party/FreeRTOS/Source/croutine.c
  Middlewares/Third_Party/FreeRTOS/Source/event_groups.c
  Middlewares/Third_Party/FreeRTOS/Source/list.c
  Middlewares/Third_Party/FreeRTOS/Source/queue.c
  Middlewares/Third_Party/FreeRTOS/Source/stream_buffer.c
  Middlewares/Third_Party/FreeRTOS/Source/tasks.c
  Middlewares/Third_Party/FreeRTOS/Source/timers.c
  Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
  Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c)

set(APP_DRIVER_SOURCES
  boards/chassis_g474/board_devices.c
  drivers/button/button_stm32.c
  drivers/encoder/encoder_stm32.c
  drivers/safety/emergency_stop_stm32.c
  drivers/gpio/interrupts_stm32.c
  drivers/led/led_stm32.c
  drivers/display/lcd_stm32.c
  drivers/sensor/icm45686_stm32.c
  drivers/motor/motor_stm32.c
  drivers/adc/power_sample_stm32.c
  drivers/reset/reset_stm32.c
  drivers/sensor/sr501_stm32.c
  drivers/can/can_common.c
  drivers/can/can_stm32_fdcan.c
  drivers/uart/uart_common.c
  drivers/uart/uart_stm32.c
  drivers/uart/uart_stm32_device.c
  drivers/flash/flash_common.c
  drivers/flash/flash_stm32_qspi.c
  drivers/rtc/rtc_common.c
  drivers/rtc/rtc_stm32.c
  drivers/time/time_common.c
  drivers/time/time_stm32.c
  drivers/watchdog/watchdog_common.c
  drivers/watchdog/watchdog_stm32_iwdg.c)

set(APP_COMPONENT_SOURCES
  components/crc/crc32.c
  components/icm45686/icm45686.c
  components/imu_fusion/imu_fusion.c
  components/pid/speed_pid.c)

set(APP_COMMUNICATION_SOURCES
  communication/can_transport/can_transport.c
  communication/ota_transport/ota_can_transport.c
  communication/ota_transport/ota_confirmation.c
  communication/ota_transport/ota_metadata.c
  communication/ota_transport/ota_session.c
  communication/ota_transport/ota_uart_transport.c)

set(APP_SUBSYS_SOURCES
  infrastructure/console/console.c
  infrastructure/console/diagnostic_report.c
  infrastructure/parameter_storage/parameter_record.c
  infrastructure/parameter_storage/parameter_storage.c
  infrastructure/telemetry/telemetry.c
  infrastructure/uart_protocol/uart_protocol.c)

set(APP_MODULE_SOURCES
  modules/chassis/command_manager.c
  modules/chassis/differential_drive.c
  modules/chassis/odometry.c
  modules/chassis/wheel_controller.c
  modules/diagnostics/board_health.c
  modules/diagnostics/system_status.c
  modules/parameters/parameter_manager.c
  modules/safety/fault_manager.c
  modules/safety/safety_manager.c
  modules/sensors/imu_orientation.c)

set(APP_UI_SOURCES
  ui/lcd/lcd_ui.c
  ui/lcd/lcd_status_presenter.c)

set(APP_KERNEL_SOURCES
  kernel/device.c
  kernel/init.c)

set(APP_APP_SOURCES
  app/chassis_app.c
  app/chassis_console_commands.c
  app/chassis_maintenance.c
  app/system_status_collector.c)

set(APP_RTOS_SOURCES rtos/rtos_app.c)
set(APP_TARGET_TEST_SOURCES
  tests/target/iwdg_target_test.c
  tests/target/motor_target_test.c
  tests/target/qspi_target_test.c)

foreach(source_list
        APP_VENDOR_SOURCES APP_DRIVER_SOURCES APP_COMPONENT_SOURCES
        APP_COMMUNICATION_SOURCES APP_SUBSYS_SOURCES APP_MODULE_SOURCES
        APP_UI_SOURCES APP_KERNEL_SOURCES APP_APP_SOURCES APP_RTOS_SOURCES
        APP_TARGET_TEST_SOURCES)
  list(TRANSFORM ${source_list} PREPEND "${APP_ROOT}/")
endforeach()

add_library(chassis_vendor STATIC ${APP_VENDOR_SOURCES})
add_library(chassis_drivers STATIC ${APP_DRIVER_SOURCES})
add_library(chassis_components STATIC ${APP_COMPONENT_SOURCES})
add_library(chassis_communication STATIC ${APP_COMMUNICATION_SOURCES})
add_library(chassis_subsys STATIC ${APP_SUBSYS_SOURCES})
add_library(chassis_modules STATIC ${APP_MODULE_SOURCES})
add_library(chassis_ui STATIC ${APP_UI_SOURCES})
add_library(chassis_kernel STATIC ${APP_KERNEL_SOURCES})
add_library(chassis_app STATIC ${APP_APP_SOURCES})
add_library(chassis_rtos STATIC ${APP_RTOS_SOURCES})
add_library(chassis_target_tests STATIC ${APP_TARGET_TEST_SOURCES})

foreach(target
        chassis_vendor chassis_drivers chassis_components chassis_communication
        chassis_subsys chassis_modules chassis_ui chassis_kernel chassis_app
        chassis_rtos chassis_target_tests)
  chassis_target_setup(${target})
endforeach()

target_link_libraries(chassis_drivers PUBLIC chassis_kernel chassis_components)
target_link_libraries(chassis_communication PUBLIC chassis_drivers chassis_components)
target_link_libraries(chassis_subsys PUBLIC chassis_drivers chassis_communication)
target_link_libraries(chassis_modules PUBLIC chassis_drivers chassis_components chassis_subsys)
target_link_libraries(chassis_ui PUBLIC chassis_drivers chassis_modules)
target_link_libraries(chassis_app PUBLIC chassis_drivers chassis_components
                      chassis_communication chassis_subsys chassis_modules chassis_ui)
target_link_libraries(chassis_rtos PUBLIC chassis_app chassis_modules chassis_subsys)
target_link_libraries(chassis_target_tests PUBLIC chassis_drivers chassis_subsys)

add_executable(application)
target_sources(application PRIVATE
  $<TARGET_OBJECTS:chassis_vendor>)
target_link_libraries(application PRIVATE
  chassis_app
  chassis_rtos
  chassis_target_tests
  chassis_ui
  chassis_modules
  chassis_subsys
  chassis_communication
  chassis_components
  chassis_drivers
  chassis_kernel
  chassis_vendor)

chassis_target_setup(application)
stm32g474_target(application
  "${APP_ROOT}/STM32G474VETX_FLASH.ld" "application.map")
add_custom_command(TARGET application POST_BUILD
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/build/check_image_size.py"
          --size-tool "${CMAKE_SIZE}"
          --elf "$<TARGET_FILE:application>"
          --flash-limit 491520
          --ram-limit 131072
  VERBATIM)
