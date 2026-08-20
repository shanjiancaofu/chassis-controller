#ifndef BSP_LED_H
#define BSP_LED_H

#include <stdbool.h>

typedef enum {
  BSP_LED_BLUE = 0,
  BSP_LED_GREEN,
  BSP_LED_RED
} BspLed;

void BspLed_Set(BspLed led, bool on);
void BspLed_Toggle(BspLed led);

#endif
