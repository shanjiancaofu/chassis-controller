add_library(chassis_product STATIC
  chassis/command_manager.c
  chassis/differential_drive.c
  chassis/odometry.c
  chassis/wheel_controller.c
  console/console.c
  diagnostics/board_health.c
  diagnostics/diagnostic_report.c
  diagnostics/system_status.c
  diagnostics/telemetry.c
  parameters/parameter_manager.c
  parameters/parameter_record.c
  parameters/parameter_storage.c
  safety/fault_manager.c
  safety/safety_manager.c
  sensors/imu_orientation.c)

if(CONFIG_SELF_TEST)
  target_sources(chassis_product PRIVATE
    maintenance/self_test/iwdg_self_test.c
    maintenance/self_test/motor_self_test.c
    maintenance/self_test/qspi_self_test.c)
else()
  target_sources(chassis_product PRIVATE
    maintenance/self_test/self_test_disabled.c)
endif()

chassis_target_setup(chassis_product)
target_link_libraries(chassis_product PUBLIC
  chassis_drivers
  chassis_components
  chassis_communication)
