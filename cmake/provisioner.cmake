set(PROVISIONER_SOURCES
  Core/Src/gpio.c
  Core/Src/iwdg.c
  Core/Src/quadspi.c
  Core/Src/stm32g4xx_hal_msp.c
  Core/Src/stm32g4xx_it.c
  Core/Src/syscalls.c
  Core/Src/sysmem.c
  Core/Src/system_stm32g4xx.c
  Core/Startup/startup_stm32g474vetx.s
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_cortex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_dma_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_gpio.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_iwdg.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_pwr_ex.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_qspi.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc.c
  Drivers/STM32G4xx_HAL_Driver/Src/stm32g4xx_hal_rcc_ex.c
  boot/boot_trace.c
  boot/image_validator.c
  bsp/qspi/boot_qspi_flash.c
  components/crc/crc32.c)
list(TRANSFORM PROVISIONER_SOURCES PREPEND "${BOOT_ROOT}/")
list(APPEND PROVISIONER_SOURCES
  "${CMAKE_SOURCE_DIR}/tools/qspi_factory_provisioner/qspi_factory_provisioner.c")

add_executable(qspi_factory_provisioner ${PROVISIONER_SOURCES})
target_include_directories(qspi_factory_provisioner PRIVATE
  "${BOOT_ROOT}"
  "${BOOT_ROOT}/Core/Inc"
  "${BOOT_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc"
  "${BOOT_ROOT}/Drivers/STM32G4xx_HAL_Driver/Inc/Legacy"
  "${BOOT_ROOT}/Drivers/CMSIS/Device/ST/STM32G4xx/Include"
  "${BOOT_ROOT}/Drivers/CMSIS/Include")
stm32g474_target(qspi_factory_provisioner
  "${BOOT_ROOT}/STM32G474VETX_FLASH.ld" "qspi_factory_provisioner.map")
