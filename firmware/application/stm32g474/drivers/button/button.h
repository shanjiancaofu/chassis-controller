#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BOARD_BUTTON_1 = 0,
  BOARD_BUTTON_2,
  BOARD_BUTTON_COUNT
} ButtonId;

typedef struct {
  bool pressed[BOARD_BUTTON_COUNT];
  uint32_t pressed_count[BOARD_BUTTON_COUNT];
} ButtonSnapshot;

void Button_Init(void);
void Button_OnInterrupt(uint16_t gpio_pin);
void Button_OnDisplayKeyInterrupt(void);
void Button_Run(uint32_t now_ms);
bool Button_TakePressed(ButtonId button);
bool Button_TakeDisplayKeyPressed(void);
void Button_GetSnapshot(ButtonSnapshot *snapshot);

#endif
