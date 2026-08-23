add_library(chassis_kernel STATIC
  device.c
  init.c
  freertos/critical.c)

chassis_target_setup(chassis_kernel)
