add_library(chassis_modules STATIC
  chassis/command_manager.c
  chassis/differential_drive.c
  chassis/odometry.c
  chassis/wheel_controller.c
  diagnostics/board_health.c
  diagnostics/system_status.c
  parameters/parameter_manager.c
  safety/fault_manager.c
  safety/safety_manager.c
  sensors/imu_orientation.c)

chassis_target_setup(chassis_modules)
target_link_libraries(chassis_modules PUBLIC chassis_drivers chassis_components chassis_subsys)
