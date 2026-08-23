#ifndef BUTTON_STM32_PRIVATE_H
#define BUTTON_STM32_PRIVATE_H

#include "drivers/button/button.h"
#include "drivers/gpio.h"

typedef struct {
  GpioSpec spec;
  bool debounce_pending;
  uint32_t debounce_started_ms;
} ButtonStm32State;

typedef struct {
  GpioSpec buttons[BUTTON_COUNT];
  GpioSpec display_key;
} ButtonStm32Config;

typedef struct {
  ButtonStm32State buttons[BUTTON_COUNT];
  volatile uint32_t pending_interrupts;
  volatile bool display_key_interrupt_pending;
  bool display_key_debounce_pending;
  bool display_key_pressed_event;
  uint32_t display_key_debounce_started_ms;
  uint32_t pressed_events;
  uint32_t pressed_counts[BUTTON_COUNT];
} ButtonStm32Data;

extern const ButtonDriverApi button_stm32_api;
int ButtonStm32_Init(const struct device *device);

#endif
