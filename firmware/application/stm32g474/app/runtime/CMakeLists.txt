add_library(chassis_app STATIC
  ../chassis_app.c
  ../console/commands.c
  ../diagnostics/system_status_collector.c
  ../maintenance/chassis_maintenance.c
  app_bootstrap.c
  control_runtime.c
  diagnostics_runtime.c
  display_runtime.c
  service_runtime.c)

chassis_target_setup(chassis_app)
target_link_libraries(chassis_app PUBLIC
  chassis_drivers
  chassis_components
  chassis_communication
  chassis_product
  chassis_ui)
