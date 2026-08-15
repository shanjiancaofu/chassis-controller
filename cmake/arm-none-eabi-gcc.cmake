set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(_tool_suffix ".exe")
else()
  set(_tool_suffix "")
endif()

set(ARM_GNU_TOOLCHAIN_ROOT "$ENV{ARM_GNU_TOOLCHAIN_ROOT}" CACHE PATH
    "GNU Arm Embedded toolchain root containing bin/arm-none-eabi-gcc")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_GNU_TOOLCHAIN_ROOT)

if(ARM_GNU_TOOLCHAIN_ROOT)
  set(_arm_bin "${ARM_GNU_TOOLCHAIN_ROOT}/bin")
else()
  find_program(_arm_gcc arm-none-eabi-gcc)
  if(_arm_gcc)
    get_filename_component(_arm_bin "${_arm_gcc}" DIRECTORY)
  endif()
endif()

if(NOT EXISTS "${_arm_bin}/arm-none-eabi-gcc${_tool_suffix}")
  message(FATAL_ERROR
    "GNU Arm Embedded toolchain not found. Put arm-none-eabi-gcc on PATH or set ARM_GNU_TOOLCHAIN_ROOT.")
endif()

set(CMAKE_C_COMPILER "${_arm_bin}/arm-none-eabi-gcc${_tool_suffix}")
set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
set(CMAKE_OBJCOPY "${_arm_bin}/arm-none-eabi-objcopy${_tool_suffix}" CACHE FILEPATH "")
set(CMAKE_OBJDUMP "${_arm_bin}/arm-none-eabi-objdump${_tool_suffix}" CACHE FILEPATH "")
set(CMAKE_SIZE "${_arm_bin}/arm-none-eabi-size${_tool_suffix}" CACHE FILEPATH "")

execute_process(
  COMMAND "${CMAKE_C_COMPILER}" -dumpfullversion
  OUTPUT_VARIABLE ARM_GNU_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT ARM_GNU_VERSION VERSION_EQUAL "14.3.1")
  message(FATAL_ERROR
    "Expected Arm GNU Toolchain 14.3.Rel1 (GCC 14.3.1), found ${ARM_GNU_VERSION}")
endif()

set(CMAKE_EXECUTABLE_SUFFIX ".elf")
