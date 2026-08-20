find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_program(DTC_EXECUTABLE dtc REQUIRED)

set(APP_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(APP_AUTOCONF_HEADER "${APP_GENERATED_DIR}/autoconf.h")
set(APP_CONFIG_CMAKE "${APP_GENERATED_DIR}/config.cmake")
set(APP_CONFIG_FILE "${APP_GENERATED_DIR}/.config")
set(APP_DTB "${APP_GENERATED_DIR}/chassis_g474.dtb")
set(APP_DTS_HEADER "${APP_GENERATED_DIR}/devicetree_generated.h")
set(APP_DTS_MANIFEST "${APP_GENERATED_DIR}/devicetree.json")
set(APP_DTS_BINDINGS "${APP_ROOT}/dts/bindings")
file(GLOB APP_DTS_BINDING_FILES CONFIGURE_DEPENDS
  "${APP_DTS_BINDINGS}/*.yaml")
set(APP_KCONFIG "${APP_ROOT}/config/Kconfig")
set(APP_PRJ_CONF "${APP_ROOT}/config/prj.conf")
set(APP_DTS "${APP_ROOT}/boards/chassis_g474/chassis_g474.dts")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${APP_KCONFIG}" "${APP_PRJ_CONF}" "${APP_DTS}"
  "${CMAKE_SOURCE_DIR}/tools/config/generate_config.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/__init__.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/config.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/evaluator.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/genconfig.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/lexer.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/model.py"
  "${CMAKE_SOURCE_DIR}/tools/kconfig/parser.py"
  "${CMAKE_SOURCE_DIR}/tools/dts/generate_devicetree.py"
  "${CMAKE_SOURCE_DIR}/tools/dts/verify_hw_config.py"
  "${CMAKE_SOURCE_DIR}/tools/dts/verify_bindings.py"
  "${CMAKE_SOURCE_DIR}/tools/build/check_architecture.py"
  ${APP_DTS_BINDING_FILES})
file(MAKE_DIRECTORY "${APP_GENERATED_DIR}")

execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/config/generate_config.py"
          --kconfig "${APP_KCONFIG}"
          --config "${APP_PRJ_CONF}"
          --header "${APP_AUTOCONF_HEADER}"
          --cmake "${APP_CONFIG_CMAKE}"
          --out-config "${APP_CONFIG_FILE}"
  COMMAND_ERROR_IS_FATAL ANY)
include("${APP_CONFIG_CMAKE}")

execute_process(
  COMMAND "${DTC_EXECUTABLE}" -@ -I dts -O dtb -o "${APP_DTB}" "${APP_DTS}"
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
execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/dts/verify_bindings.py"
          --manifest "${APP_DTS_MANIFEST}"
          --bindings "${APP_DTS_BINDINGS}"
  COMMAND_ERROR_IS_FATAL ANY)
execute_process(
  COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/tools/build/check_architecture.py"
          --root "${APP_ROOT}"
          --scope app
          --scope modules
          --scope communication
          --scope infrastructure
          --scope ui
          --scope rtos
  COMMAND_ERROR_IS_FATAL ANY)
