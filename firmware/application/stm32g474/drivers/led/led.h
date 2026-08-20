#ifndef LED_H
#define LED_H

#include <stdbool.h>

typedef enum {
  LED_BLUE = 0,
  LED_GREEN,
  LED_RED
} Led;

void Led_Set(Led led, bool on);
void Led_Toggle(Led led);

#endif
