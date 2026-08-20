add_library(chassis_app STATIC
  chassis_app.c
  chassis_console_commands.c
  chassis_maintenance.c
  system_status_collector.c)

chassis_target_setup(chassis_app)
target_link_libraries(chassis_app PUBLIC chassis_drivers chassis_components
                      chassis_communication chassis_subsys chassis_modules chassis_ui)
