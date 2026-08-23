#include "drivers/button/button.h"

#include <stddef.h>

#include "main.h"

#define BUTTON_DEBOUNCE_MS 20U

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  bool debounce_pending;
  uint32_t debounce_started_ms;
} ButtonState;

static ButtonState buttons[BUTTON_COUNT] = {
    [BUTTON_1] = {BUTTON_1_GPIO_Port, BUTTON_1_Pin, false, 0U},
    [BUTTON_2] = {BUTTON_2_GPIO_Port, BUTTON_2_Pin, false, 0U},
};
static volatile uint32_t pending_interrupts;
static volatile bool display_key_interrupt_pending;
static bool display_key_debounce_pending;
static bool display_key_pressed_event;
static uint32_t display_key_debounce_started_ms;
static uint32_t pressed_events;
static uint32_t pressed_counts[BUTTON_COUNT];

void Button_Init(void)
{
  pending_interrupts = 0U;
  display_key_interrupt_pending = false;
  display_key_debounce_pending = false;
  display_key_pressed_event = false;
  display_key_debounce_started_ms = 0U;
  pressed_events = 0U;
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    buttons[index].debounce_pending = false;
    buttons[index].debounce_started_ms = 0U;
    pressed_counts[index] = 0U;
  }
}

void Button_OnInterrupt(uint16_t gpio_pin)
{
  if (gpio_pin == BUTTON_1_Pin) {
    (void)__atomic_fetch_or(&pending_interrupts, 1UL << BUTTON_1,
                            __ATOMIC_RELAXED);
  } else if (gpio_pin == BUTTON_2_Pin) {
    (void)__atomic_fetch_or(&pending_interrupts, 1UL << BUTTON_2,
                            __ATOMIC_RELAXED);
  }
}

void Button_OnDisplayKeyInterrupt(void)
{
  __atomic_store_n(&display_key_interrupt_pending, true, __ATOMIC_RELEASE);
}

void Button_Run(uint32_t now_ms)
{
  const uint32_t interrupts =
      __atomic_exchange_n(&pending_interrupts, 0U, __ATOMIC_RELAXED);

  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    ButtonState *button = &buttons[index];

    if ((interrupts & (1UL << index)) != 0U) {
      button->debounce_pending = true;
      button->debounce_started_ms = now_ms;
    }
    if (button->debounce_pending &&
        now_ms - button->debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
      button->debounce_pending = false;
      if (HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET) {
        pressed_events |= 1UL << index;
        (void)__atomic_fetch_add(&pressed_counts[index], 1U,
                                 __ATOMIC_RELAXED);
      }
    }
  }
  if (__atomic_exchange_n(&display_key_interrupt_pending, false,
                          __ATOMIC_ACQUIRE)) {
    display_key_debounce_pending = true;
    display_key_debounce_started_ms = now_ms;
  }
  if (display_key_debounce_pending &&
      now_ms - display_key_debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
    display_key_debounce_pending = false;
    if (HAL_GPIO_ReadPin(KEY_GPIO_Port, KEY_Pin) == GPIO_PIN_SET) {
      display_key_pressed_event = true;
    }
  }
}

bool Button_TakePressed(ButtonId button)
{
  uint32_t mask;

  if (button >= BUTTON_COUNT) {
    return false;
  }
  mask = 1UL << (uint32_t)button;
  if ((pressed_events & mask) == 0U) {
    return false;
  }
  pressed_events &= ~mask;
  return true;
}

bool Button_TakeDisplayKeyPressed(void)
{
  if (!display_key_pressed_event) {
    return false;
  }
  display_key_pressed_event = false;
  return true;
}

void Button_GetSnapshot(ButtonSnapshot *snapshot)
{
  if (snapshot == NULL) {
    return;
  }
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    snapshot->pressed[index] =
        HAL_GPIO_ReadPin(buttons[index].port, buttons[index].pin) ==
        GPIO_PIN_RESET;
    snapshot->pressed_count[index] =
        __atomic_load_n(&pressed_counts[index], __ATOMIC_RELAXED);
  }
}
