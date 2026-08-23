#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "drivers/display/lcd.h"
#include "ui/lcd/lcd_ui.h"
#include "ui/lcd/lcd_ui_layout.h"

static struct device_state preview_state = {.init_res = 0, .initialized = true};
static const struct device preview_device = {
    .name = "preview_display", .state = &preview_state,
};

bool device_is_ready(const struct device *device)
{
  return device == &preview_device;
}

bool display_begin_frame(const struct device *device)
{
  return device_is_ready(device);
}

bool display_transmit_row(const struct device *device, const uint8_t *data,
                          uint16_t size)
{
  (void)data;
  return device_is_ready(device) && size == LCD_UI_WIDTH * 2U;
}

bool display_row_complete(const struct device *device)
{
  return device_is_ready(device);
}

bool display_has_error(const struct device *device)
{
  return !device_is_ready(device);
}

void display_end_frame(const struct device *device)
{
  (void)device;
}

LcdStatus display_get_status(const struct device *device)
{
  return device_is_ready(device) ? LCD_READY : LCD_FAILED;
}

bool Lcd_Init(void)
{
  return true;
}

bool Lcd_BeginFrame(void)
{
  return true;
}

bool Lcd_TransmitRow(const uint8_t *row_data, uint16_t size)
{
  (void)row_data;
  (void)size;
  return true;
}

bool Lcd_IsRowTransferComplete(void)
{
  return true;
}

bool Lcd_HasTransferError(void)
{
  return false;
}

void Lcd_EndFrame(void)
{
}

LcdStatus Lcd_GetStatus(void)
{
  return LCD_READY;
}

void Lcd_OnSpiTxComplete(void)
{
}

void Lcd_OnSpiError(void)
{
}

static bool WritePage(const char *directory, const char *name,
                      LcdUiPage page, const LcdUiStatusData *data)
{
  uint8_t row[LCD_UI_WIDTH * 2U];
  char path[512];
  FILE *output;

  if (snprintf(path, sizeof(path), "%s/%s.rgb565", directory, name) < 0) {
    return false;
  }
  output = fopen(path, "wb");
  if (output == NULL) {
    return false;
  }

  LcdUi_PreviewPrepare(page, data);
  for (uint16_t y = 0U; y < LCD_UI_HEIGHT; ++y) {
    LcdUi_PreviewRenderRow(y, row, sizeof(row));
    if (fwrite(row, 1U, sizeof(row), output) != sizeof(row)) {
      fclose(output);
      return false;
    }
  }
  return fclose(output) == 0;
}

int main(int argc, char **argv)
{
  const LcdUiStatusData data = {
      .control_state = LCD_UI_CONTROL_STOPPED,
      .can_state = LCD_UI_VALUE_PASS,
      .qspi_state = LCD_UI_VALUE_PASS,
      .rtc_state = LCD_UI_VALUE_PASS,
      .supply_state = LCD_UI_VALUE_PASS,
      .imu_state = LCD_UI_SENSOR_READY,
      .sr501_state = LCD_UI_SENSOR_READY,
      .left_target = 0,
      .right_target = 0,
      .left_measurement = 0,
      .right_measurement = 0,
      .left_encoder_delta = 0,
      .right_encoder_delta = 0,
      .left_output = 0,
      .right_output = 0,
      .odometry_valid = true,
      .odometry_x_mm = 0,
      .odometry_y_mm = 0,
      .odometry_heading_mrad = 0,
      .imu_orientation_valid = false,
      .sr501_motion = false,
      .sr501_event_count = 0U,
      .rtc_hours = 3U,
      .rtc_minutes = 35U,
      .rtc_seconds = 20U,
      .qspi_capacity_mib = 8U,
      .adc_mv = 12520U,
      .battery_percent = 93U,
      .battery_percent_valid = true,
      .fault_flags = 0U,
      .uptime_ms = 148000U,
      .reset_cause = LCD_UI_RESET_SOFTWARE,
      .critical_tasks_healthy = true,
      .service_task_healthy = true,
      .control_task_healthy = true,
      .diagnostics_task_healthy = true,
      .display_task_healthy = true,
      .service_stack_free_words = 388U,
      .control_stack_free_words = 440U,
      .diagnostics_stack_free_words = 490U,
      .display_stack_free_words = 320U,
  };

  if (argc != 2) {
    fprintf(stderr, "usage: %s OUTPUT_DIRECTORY\n", argv[0]);
    return 2;
  }
  if (!WritePage(argv[1], "overview", LCD_UI_PAGE_OVERVIEW, &data) ||
      !WritePage(argv[1], "motor", LCD_UI_PAGE_MOTOR, &data) ||
      !WritePage(argv[1], "sensors", LCD_UI_PAGE_SENSORS, &data) ||
      !WritePage(argv[1], "system", LCD_UI_PAGE_SYSTEM, &data)) {
    fprintf(stderr, "failed to write LCD preview frames\n");
    return 1;
  }
  return 0;
}
