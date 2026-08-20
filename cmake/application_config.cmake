find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(DTC_EXECUTABLE dtc REQUIRED)

set(APP_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(APP_AUTOCONF_HEADER "${APP_GENERATED_DIR}/autoconf.h")
set(APP_CONFIG_CMAKE "${APP_GENERATED_DIR}/config.cmake")
set(APP_DTB "${APP_GENERATED_DIR}/chassis_g474.dtb")
set(APP_DTS_HEADER "${APP_GENERATED_DIR}/devicetree_generated.h")
set(APP_DTS_MANIFEST "${APP_GENERATED_DIR}/devicetree.json")
set(APP_KCONFIG "${APP_ROOT}/config/Kconfig")
set(APP_PRJ_CONF "${APP_ROOT}/config/prj.conf")
set(APP_DTS "${APP_ROOT}/boards/chassis_g474/chassis_g474.dts")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${APP_KCONFIG}" "${APP_PRJ_CONF}" "${APP_DTS}"
  "${CMAKE_SOURCE_DIR}/tools/config/generate_config.py"
  "${CMAKE_SOURCE_DIR}/tools/dts/generate_devicetree.py"
  "${CMAKE_SOURCE_DIR}/tools/dts/verify_hw_config.py")
file(MAKE_DIRECTORY "${APP_GENERATED_DIR}")

execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/config/generate_config.py"
          --kconfig "${APP_KCONFIG}"
          --config "${APP_PRJ_CONF}"
          --header "${APP_AUTOCONF_HEADER}"
          --cmake "${APP_CONFIG_CMAKE}"
  COMMAND_ERROR_IS_FATAL ANY)
include("${APP_CONFIG_CMAKE}")

execute_process(
  COMMAND "${DTC_EXECUTABLE}" -I dts -O dtb -o "${APP_DTB}" "${APP_DTS}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/dts/generate_devicetree.py"
          --dtb "${APP_DTB}"
          --header "${APP_DTS_HEADER}"
          --json "${APP_DTS_MANIFEST}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/dts/verify_hw_config.py"
          --ioc "${APP_ROOT}/chassis_controller.ioc"
          --manifest "${APP_DTS_MANIFEST}"
  COMMAND_ERROR_IS_FATAL ANY)
