#ifndef CHASSIS_DEVICE_H
#define CHASSIS_DEVICE_H

#include <stdbool.h>
#include <stddef.h>

#include "init.h"

struct device_state {
  int init_res;
  bool initialized;
};

struct device_ops {
  int (*init)(const struct device *device);
};

struct device {
  const char *name;
  const void *config;
  const void *api;
  struct device_state *state;
  void *data;
  struct device_ops ops;
};

int device_init(const struct device *device);
bool device_is_ready(const struct device *device);
const struct device *device_get_binding(const char *name);
size_t device_get_all(const struct device **devices);

#define DEVICE_SYMBOL_INNER(name) device_##name
#define DEVICE_SYMBOL(name) DEVICE_SYMBOL_INNER(name)
#define DEVICE_GET(name) (&DEVICE_SYMBOL(name))
#define DEVICE_DECLARE(name) extern const struct device DEVICE_SYMBOL(name)

#define DEVICE_DEFINE(dev_id, device_name, init_function, data_ptr, config_ptr, \
                      level, priority, api_ptr)                              \
  static struct device_state device_state_##dev_id;                         \
  const struct device DEVICE_SYMBOL(dev_id)                                 \
      __attribute__((section(".device"), used, aligned(4))) = {            \
          .name = (device_name),                                            \
          .config = (config_ptr),                                           \
          .api = (api_ptr),                                                 \
          .state = &device_state_##dev_id,                                  \
          .data = (data_ptr),                                               \
          .ops = {.init = (init_function)},                                 \
      };                                                                    \
  INIT_ENTRY_DEFINE(dev_id, 0, DEVICE_GET(dev_id), level, priority)

#define DEVICE_DT_DEFINE(node_id, init_function, data_ptr, config_ptr, level, \
                         priority, api_ptr)                                  \
  DEVICE_DEFINE(node_id, #node_id, init_function, data_ptr, config_ptr,      \
                level, priority, api_ptr)

#define DEVICE_DT_GET(node_id) DEVICE_GET(node_id)

#endif
