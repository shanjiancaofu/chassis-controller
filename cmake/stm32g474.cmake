set(STM32G474_CPU_OPTIONS
  -mcpu=cortex-m4
  -mfpu=fpv4-sp-d16
  -mfloat-abi=hard
  -mthumb)

function(stm32g474_target target linker_script map_name)
  target_compile_features(${target} PRIVATE c_std_11)
  target_compile_definitions(${target} PRIVATE USE_HAL_DRIVER STM32G474xx)
  target_compile_options(${target} PRIVATE
    ${STM32G474_CPU_OPTIONS}
    -Os
    -ffunction-sections
    -fdata-sections
    -Wall
    -fstack-usage
    -fcyclomatic-complexity
    $<$<COMPILE_LANGUAGE:C>:--specs=nano.specs>
    $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>)
  target_link_options(${target} PRIVATE
    ${STM32G474_CPU_OPTIONS}
    -T${linker_script}
    --specs=nosys.specs
    --specs=nano.specs
    -static
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${map_name})
  target_link_libraries(${target} PRIVATE c m)
  set_target_properties(${target} PROPERTIES SUFFIX ".elf")

  add_custom_command(TARGET ${target} POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O binary
            "$<TARGET_FILE:${target}>"
            "${CMAKE_CURRENT_BINARY_DIR}/${target}.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:${target}>"
    VERBATIM)
endfunction()
