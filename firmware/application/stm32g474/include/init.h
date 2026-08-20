#ifndef CHASSIS_INIT_H
#define CHASSIS_INIT_H

#include <stdint.h>

struct device;

typedef enum {
  INIT_LEVEL_EARLY = 0,
  INIT_LEVEL_PRE_KERNEL_1,
  INIT_LEVEL_PRE_KERNEL_2,
  INIT_LEVEL_POST_KERNEL,
  INIT_LEVEL_APPLICATION,
  INIT_LEVEL_COUNT
} InitLevel;

typedef int (*InitFunction)(void);

typedef struct {
  InitFunction init_fn;
  const struct device *device;
} InitEntry;

int SystemInit_RunLevel(InitLevel level);

#define INIT_STRINGIFY_INNER(value) #value
#define INIT_STRINGIFY(value) INIT_STRINGIFY_INNER(value)
#define INIT_CONCAT_INNER(left, right) left##right
#define INIT_CONCAT(left, right) INIT_CONCAT_INNER(left, right)
#define INIT_SECTION(level, priority, sub_priority)                         \
  ".z_init_" #level "_P_" INIT_STRINGIFY(priority) "_SUB_"             \
      INIT_STRINGIFY(sub_priority)

#define INIT_ENTRY_DEFINE_IMPL(name, function, device_ptr, level, priority,  \
                               sub_priority)                                 \
  static const InitEntry INIT_CONCAT(init_entry_, name)                     \
      __attribute__((section(INIT_SECTION(level, priority, sub_priority)),  \
                     used,                                                   \
                     aligned(4))) = {                                      \
          .init_fn = (function),                                           \
          .device = (device_ptr),                                          \
      }

#define INIT_ENTRY_DEFINE(name, function, device_ptr, level, priority)       \
  INIT_ENTRY_DEFINE_IMPL(name, function, device_ptr, level, priority,        \
                         __COUNTER__)

#define SYS_INIT(function, level, priority) \
  INIT_ENTRY_DEFINE(function, function, 0, level, priority)

#endif
