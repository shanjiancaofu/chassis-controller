add_library(chassis_rtos STATIC rtos_app.c)

chassis_target_setup(chassis_rtos)
target_link_libraries(chassis_rtos PUBLIC chassis_app chassis_modules chassis_subsys)
