add_library(chassis_components STATIC
  crc/crc32.c
  icm45686/icm45686.c
  imu_fusion/imu_fusion.c
  pid/speed_pid.c)

chassis_target_setup(chassis_components)
