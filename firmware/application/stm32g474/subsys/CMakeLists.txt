add_library(chassis_subsys STATIC
  console/console.c
  console/diagnostic_report.c
  settings/parameter_record.c
  settings/parameter_storage.c
  telemetry/telemetry.c)

chassis_target_setup(chassis_subsys)
target_link_libraries(chassis_subsys PUBLIC chassis_drivers chassis_communication)
