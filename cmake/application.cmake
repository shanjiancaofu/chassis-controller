set(APP_ROOT "${CMAKE_SOURCE_DIR}/firmware/application/stm32g474")
set(APP_CUBEMX_ROOT "${APP_ROOT}/cubemx")
include("${CMAKE_SOURCE_DIR}/cmake/application_config.cmake")

set(APP_COMMON_INCLUDES
  "${APP_ROOT}"
  "${APP_ROOT}/include"
  "${APP_GENERATED_DIR}"
  "${APP_CUBEMX_ROOT}/Core/Inc"
  "${APP_CUBEMX_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc"
  "${APP_CUBEMX_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"
  "${APP_CUBEMX_ROOT}/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
  "${APP_CUBEMX_ROOT}/Drivers/CMSIS/Include"
  "${APP_CUBEMX_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include"
  "${APP_CUBEMX_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
  "${APP_CUBEMX_ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F")

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
list(TRANSFORM APP_VENDOR_SOURCES PREPEND "${APP_CUBEMX_ROOT}/")

add_library(chassis_vendor STATIC ${APP_VENDOR_SOURCES})
chassis_target_setup(chassis_vendor)

add_subdirectory("${APP_ROOT}/kernel" "${CMAKE_BINARY_DIR}/application/kernel")
add_subdirectory("${APP_ROOT}/lib" "${CMAKE_BINARY_DIR}/application/lib")
add_subdirectory("${APP_ROOT}/drivers" "${CMAKE_BINARY_DIR}/application/drivers")
add_subdirectory("${APP_ROOT}/subsys/communication" "${CMAKE_BINARY_DIR}/application/communication")
add_subdirectory("${APP_ROOT}/app" "${CMAKE_BINARY_DIR}/application/product")
add_subdirectory("${APP_ROOT}/app/ui" "${CMAKE_BINARY_DIR}/application/ui")
add_subdirectory("${APP_ROOT}/app/runtime" "${CMAKE_BINARY_DIR}/application/app")
add_subdirectory("${APP_ROOT}/kernel/freertos" "${CMAKE_BINARY_DIR}/application/rtos")

add_executable(application)
target_sources(application PRIVATE $<TARGET_OBJECTS:chassis_vendor>)
target_link_libraries(application PRIVATE
  chassis_app
  chassis_rtos
  chassis_ui
  chassis_product
  chassis_communication
  chassis_drivers
  chassis_components
  chassis_kernel
  chassis_vendor)

chassis_target_setup(application)
stm32g474_target(application
  "${APP_ROOT}/boards/chassis_g474/application.ld" "application.map")
set(APP_REQUIRED_HAL_CALLBACKS
  HAL_SPI_TxCpltCallback
  HAL_SPI_ErrorCallback)
if(CONFIG_ICM45686)
  list(APPEND APP_REQUIRED_HAL_CALLBACKS HAL_SPI_TxRxCpltCallback)
endif()
set(APP_REQUIRED_HAL_CALLBACK_ARGS)
foreach(callback IN LISTS APP_REQUIRED_HAL_CALLBACKS)
  list(APPEND APP_REQUIRED_HAL_CALLBACK_ARGS --symbol "${callback}")
endforeach()
add_custom_command(TARGET application POST_BUILD
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/build/check_required_symbols.py"
          --nm-tool "${CMAKE_NM}"
          --elf "$<TARGET_FILE:application>"
          ${APP_REQUIRED_HAL_CALLBACK_ARGS}
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/build/check_image_size.py"
          --size-tool "${CMAKE_SIZE}"
          --elf "$<TARGET_FILE:application>"
          --flash-limit 491520
          --ram-limit 131072
  VERBATIM)

add_custom_target(application-host-tests
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/build/run_host_tests.py"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Running Application C host tests"
  VERBATIM)
