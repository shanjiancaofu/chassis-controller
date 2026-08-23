#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef enum {
  BUTTON_1 = 0,
  BUTTON_2,
  BUTTON_COUNT
} ButtonId;

typedef struct {
  bool pressed[BUTTON_COUNT];
  uint32_t pressed_count[BUTTON_COUNT];
} ButtonSnapshot;

typedef struct {
  void (*run)(const struct device *, uint32_t);
  bool (*take_pressed)(const struct device *, ButtonId);
  bool (*take_display_key)(const struct device *);
  void (*get_snapshot)(const struct device *, ButtonSnapshot *);
  void (*on_interrupt)(const struct device *, uint16_t);
} ButtonDriverApi;
void button_run(const struct device *, uint32_t);
bool button_take_pressed(const struct device *, ButtonId);
bool button_take_display_key(const struct device *);
void button_get_snapshot(const struct device *, ButtonSnapshot *);
void button_on_interrupt(const struct device *, uint16_t);

#endif
