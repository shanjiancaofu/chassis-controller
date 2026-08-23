#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BUTTON_1 = 0,
  BUTTON_2,
  BUTTON_COUNT
} ButtonId;

typedef struct {
  bool pressed[BUTTON_COUNT];
  uint32_t pressed_count[BUTTON_COUNT];
} ButtonSnapshot;

void Button_Init(void);
void Button_OnInterrupt(uint16_t gpio_pin);
void Button_OnDisplayKeyInterrupt(void);
void Button_Run(uint32_t now_ms);
bool Button_TakePressed(ButtonId button);
bool Button_TakeDisplayKeyPressed(void);
void Button_GetSnapshot(ButtonSnapshot *snapshot);

#endif
