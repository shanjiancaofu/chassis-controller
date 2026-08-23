#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include "device.h"

typedef enum {
  LED_BLUE = 0,
  LED_GREEN,
  LED_RED
} Led;

typedef struct { void (*set)(const struct device*,Led,bool); void (*toggle)(const struct device*,Led); } LedDriverApi;
void led_set(const struct device*,Led,bool);
void led_toggle(const struct device*,Led);

#endif
