#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include "device.h"
#include "drivers/gpio.h"

typedef enum {
  LED_BLUE = 0,
  LED_GREEN,
  LED_RED
} Led;

typedef struct { GpioSpec leds[3]; } LedStm32Config;
typedef struct { void (*set)(const struct device*,Led,bool); void (*toggle)(const struct device*,Led); } LedDriverApi;
void led_set(const struct device*,Led,bool);
void led_toggle(const struct device*,Led);
extern const LedDriverApi led_stm32_api;
int LedStm32_Init(const struct device*);

void Led_Set(Led led, bool on);
void Led_Toggle(Led led);

#endif
