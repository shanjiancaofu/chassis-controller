#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

#define LCD_WIDTH 320U
#define LCD_HEIGHT 240U

typedef enum
{
  LCD_DISABLED = 0,
  LCD_READY,
  LCD_TRANSMITTING,
  LCD_FAILED
} LcdStatus;

typedef struct {
  bool (*begin_frame)(const struct device *device);
  bool (*transmit_row)(const struct device *device, const uint8_t *data, uint16_t size);
  bool (*row_complete)(const struct device *device);
  bool (*has_error)(const struct device *device);
  void (*end_frame)(const struct device *device);
  LcdStatus (*get_status)(const struct device *device);
  void (*on_tx_complete)(const struct device *device);
  void (*on_error)(const struct device *device);
} DisplayDriverApi;

bool display_begin_frame(const struct device *device);
bool display_transmit_row(const struct device *device, const uint8_t *data, uint16_t size);
bool display_row_complete(const struct device *device);
bool display_has_error(const struct device *device);
void display_end_frame(const struct device *device);
LcdStatus display_get_status(const struct device *device);
void display_on_tx_complete(const struct device *device);
void display_on_error(const struct device *device);

#endif
