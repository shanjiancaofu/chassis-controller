#include "device.h"

#include <errno.h>
#include <string.h>

extern const struct device __device_start[];
extern const struct device __device_end[];

int device_init(const struct device *device)
{
  int result;

  if (device == NULL || device->state == NULL) {
    return -EINVAL;
  }
  if (device->state->initialized) {
    return device->state->init_res == 0 ? -EALREADY
                                        : device->state->init_res;
  }
  result = device->ops.init != NULL ? device->ops.init(device) : 0;
  device->state->init_res = result;
  device->state->initialized = true;
  return result;
}

bool device_is_ready(const struct device *device)
{
  return device != NULL && device->state != NULL &&
         device->state->initialized && device->state->init_res == 0;
}

const struct device *device_get_binding(const char *name)
{
  if (name == NULL || name[0] == '\0') {
    return NULL;
  }
  for (const struct device *device = __device_start; device < __device_end; ++device) {
    if (device->name != NULL && strcmp(device->name, name) == 0 &&
        device_is_ready(device)) {
      return device;
    }
  }
  return NULL;
}

size_t device_get_all(const struct device **devices)
{
  if (devices != NULL) {
    *devices = __device_start;
  }
  return (size_t)(__device_end - __device_start);
}
