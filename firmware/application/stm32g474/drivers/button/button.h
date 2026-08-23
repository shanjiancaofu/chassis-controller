#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"
#include "drivers/gpio.h"

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

typedef struct { GpioSpec buttons[BUTTON_COUNT]; GpioSpec display_key; } ButtonStm32Config;
typedef struct {
  void (*run)(const struct device *, uint32_t);
  bool (*take_pressed)(const struct device *, ButtonId);
  bool (*take_display_key)(const struct device *);
  void (*get_snapshot)(const struct device *, ButtonSnapshot *);
  void (*on_interrupt)(const struct device *, uint16_t);
} ButtonDriverApi;
void button_run(const struct device *, uint32_t);
bool button_take_display_key(const struct device *);
void button_get_snapshot(const struct device *, ButtonSnapshot *);
void button_on_interrupt(const struct device *, uint16_t);
extern const ButtonDriverApi button_stm32_api;
int ButtonStm32_Init(const struct device *device);

#endif
