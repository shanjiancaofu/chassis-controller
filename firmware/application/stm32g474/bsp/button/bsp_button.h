#ifndef BSP_BUTTON_H
#define BSP_BUTTON_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BOARD_BUTTON_1 = 0,
  BOARD_BUTTON_2,
  BOARD_BUTTON_COUNT
} BspButtonId;

typedef struct {
  bool pressed[BOARD_BUTTON_COUNT];
  uint32_t pressed_count[BOARD_BUTTON_COUNT];
} BspButtonSnapshot;

void BspButton_Init(void);
void BspButton_OnInterrupt(uint16_t gpio_pin);
void BspButton_Run(uint32_t now_ms);
bool BspButton_TakePressed(BspButtonId button);
void BspButton_GetSnapshot(BspButtonSnapshot *snapshot);

#endif
