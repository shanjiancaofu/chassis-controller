#ifndef INTERRUPTS_STM32_PRIVATE_H
#define INTERRUPTS_STM32_PRIVATE_H

#include <stdint.h>

static inline uint16_t InterruptsStm32_PinNumber(uint16_t pin_mask)
{
  uint16_t pin = 0U;

  if (pin_mask == 0U || (pin_mask & (uint16_t)(pin_mask - 1U)) != 0U) {
    return UINT16_MAX;
  }
  while (pin_mask > 1U) {
    pin_mask >>= 1U;
    ++pin;
  }
  return pin;
}

int InterruptsStm32_Link(void);

#endif
