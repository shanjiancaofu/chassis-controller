cmake_minimum_required(VERSION 3.22)

project(chassis_controller_firmware LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS_RELEASE "" CACHE STRING "" FORCE)

include(cmake/stm32g474.cmake)
include(cmake/application.cmake)
include(cmake/bootloader.cmake)
include(cmake/provisioner.cmake)
