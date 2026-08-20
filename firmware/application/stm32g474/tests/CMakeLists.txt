add_library(chassis_target_tests STATIC
  target/iwdg_target_test.c
  target/motor_target_test.c
  target/qspi_target_test.c)

chassis_target_setup(chassis_target_tests)
target_link_libraries(chassis_target_tests PUBLIC chassis_drivers chassis_subsys)
