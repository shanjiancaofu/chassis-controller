#ifndef CHASSIS_ENCODER_H
#define CHASSIS_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

#include "device.h"

typedef struct {
  int (*start)(const struct device *device);
  int (*read_delta)(const struct device *device, int32_t *delta);
} EncoderDriverApi;

int encoder_start(const struct device *device);
int encoder_read_delta(const struct device *device, int32_t *delta);
#endif
