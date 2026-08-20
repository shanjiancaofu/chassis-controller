#include "init.h"

#include <errno.h>

#include "device.h"

extern const InitEntry __init_EARLY_start[];
extern const InitEntry __init_EARLY_end[];
extern const InitEntry __init_PRE_KERNEL_1_start[];
extern const InitEntry __init_PRE_KERNEL_1_end[];
extern const InitEntry __init_PRE_KERNEL_2_start[];
extern const InitEntry __init_PRE_KERNEL_2_end[];
extern const InitEntry __init_POST_KERNEL_start[];
extern const InitEntry __init_POST_KERNEL_end[];
extern const InitEntry __init_APPLICATION_start[];
extern const InitEntry __init_APPLICATION_end[];

typedef struct {
  const InitEntry *start;
  const InitEntry *end;
} InitRange;

int SystemInit_RunLevel(InitLevel level)
{
  static const InitRange ranges[INIT_LEVEL_COUNT] = {
      [INIT_LEVEL_EARLY] = {__init_EARLY_start, __init_EARLY_end},
      [INIT_LEVEL_PRE_KERNEL_1] = {__init_PRE_KERNEL_1_start,
                                   __init_PRE_KERNEL_1_end},
      [INIT_LEVEL_PRE_KERNEL_2] = {__init_PRE_KERNEL_2_start,
                                   __init_PRE_KERNEL_2_end},
      [INIT_LEVEL_POST_KERNEL] = {__init_POST_KERNEL_start,
                                  __init_POST_KERNEL_end},
      [INIT_LEVEL_APPLICATION] = {__init_APPLICATION_start,
                                  __init_APPLICATION_end},
  };

  if (level >= INIT_LEVEL_COUNT) {
    return -EINVAL;
  }
  for (const InitEntry *entry = ranges[level].start;
       entry < ranges[level].end; ++entry) {
    int result = entry->device != NULL
                     ? device_init(entry->device)
                     : entry->init_fn != NULL ? entry->init_fn() : 0;

    if (result < 0 && result != -EALREADY) {
      return result;
    }
  }
  return 0;
}
