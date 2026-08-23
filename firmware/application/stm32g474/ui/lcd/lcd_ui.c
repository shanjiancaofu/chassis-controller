#include "ui/lcd/lcd_ui.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "drivers/display/lcd.h"
#include "drivers/display/lcd_logo_image.h"
#include "config/build_info.h"
#include "ui/lcd/lcd_ui_layout.h"

#define LCD_TEXT_LINE_COUNT 24U
#define LCD_TEXT_LINE_LENGTH 32U

_Static_assert(LCD_UI_WIDTH == LCD_WIDTH &&
                   LCD_UI_HEIGHT == LCD_HEIGHT,
               "LCD UI canvas must match the physical panel");

typedef struct
{
  char character;
  uint8_t columns[5];
} LcdGlyph;

static const LcdGlyph lcd_glyphs[] = {
    {'-', {0x08U, 0x08U, 0x08U, 0x08U, 0x08U}},
    {'.', {0x00U, 0x60U, 0x60U, 0x00U, 0x00U}},
    {'%', {0x63U, 0x13U, 0x08U, 0x64U, 0x63U}},
    {'/', {0x20U, 0x10U, 0x08U, 0x04U, 0x02U}},
    {':', {0x00U, 0x36U, 0x36U, 0x00U, 0x00U}},
    {'0', {0x3EU, 0x51U, 0x49U, 0x45U, 0x3EU}},
    {'1', {0x00U, 0x42U, 0x7FU, 0x40U, 0x00U}},
    {'2', {0x42U, 0x61U, 0x51U, 0x49U, 0x46U}},
    {'3', {0x21U, 0x41U, 0x45U, 0x4BU, 0x31U}},
    {'4', {0x18U, 0x14U, 0x12U, 0x7FU, 0x10U}},
    {'5', {0x27U, 0x45U, 0x45U, 0x45U, 0x39U}},
    {'6', {0x3CU, 0x4AU, 0x49U, 0x49U, 0x30U}},
    {'7', {0x01U, 0x71U, 0x09U, 0x05U, 0x03U}},
    {'8', {0x36U, 0x49U, 0x49U, 0x49U, 0x36U}},
    {'9', {0x06U, 0x49U, 0x49U, 0x29U, 0x1EU}},
    {'A', {0x7EU, 0x11U, 0x11U, 0x11U, 0x7EU}},
    {'B', {0x7FU, 0x49U, 0x49U, 0x49U, 0x36U}},
    {'C', {0x3EU, 0x41U, 0x41U, 0x41U, 0x22U}},
    {'D', {0x7FU, 0x41U, 0x41U, 0x22U, 0x1CU}},
    {'E', {0x7FU, 0x49U, 0x49U, 0x49U, 0x41U}},
    {'F', {0x7FU, 0x09U, 0x09U, 0x09U, 0x01U}},
    {'G', {0x3EU, 0x41U, 0x49U, 0x49U, 0x7AU}},
    {'H', {0x7FU, 0x08U, 0x08U, 0x08U, 0x7FU}},
    {'I', {0x00U, 0x41U, 0x7FU, 0x41U, 0x00U}},
    {'J', {0x20U, 0x40U, 0x41U, 0x3FU, 0x01U}},
    {'K', {0x7FU, 0x08U, 0x14U, 0x22U, 0x41U}},
    {'L', {0x7FU, 0x40U, 0x40U, 0x40U, 0x40U}},
    {'M', {0x7FU, 0x02U, 0x0CU, 0x02U, 0x7FU}},
    {'N', {0x7FU, 0x04U, 0x08U, 0x10U, 0x7FU}},
    {'O', {0x3EU, 0x41U, 0x41U, 0x41U, 0x3EU}},
    {'P', {0x7FU, 0x09U, 0x09U, 0x09U, 0x06U}},
    {'Q', {0x3EU, 0x41U, 0x51U, 0x21U, 0x5EU}},
    {'R', {0x7FU, 0x09U, 0x19U, 0x29U, 0x46U}},
    {'S', {0x46U, 0x49U, 0x49U, 0x49U, 0x31U}},
    {'T', {0x01U, 0x01U, 0x7FU, 0x01U, 0x01U}},
    {'U', {0x3FU, 0x40U, 0x40U, 0x40U, 0x3FU}},
    {'V', {0x1FU, 0x20U, 0x40U, 0x20U, 0x1FU}},
    {'W', {0x3FU, 0x40U, 0x38U, 0x40U, 0x3FU}},
    {'X', {0x63U, 0x14U, 0x08U, 0x14U, 0x63U}},
    {'Y', {0x07U, 0x08U, 0x70U, 0x08U, 0x07U}},
    {'Z', {0x61U, 0x51U, 0x49U, 0x45U, 0x43U}},
};

static uint8_t lcd_line_buffer[LCD_UI_WIDTH * 2U];
static uint16_t lcd_next_row;
static LcdUiStatus lcd_status;
static const struct device *lcd_display;
static LcdUiPage lcd_requested_page;
static bool lcd_redraw_requested;
static LcdUiStatusData lcd_status_data;
static char lcd_text_lines[LCD_TEXT_LINE_COUNT][LCD_TEXT_LINE_LENGTH];
static uint16_t lcd_text_x[LCD_TEXT_LINE_COUNT];
static uint16_t lcd_text_y[LCD_TEXT_LINE_COUNT];
static uint16_t lcd_text_colors[LCD_TEXT_LINE_COUNT];
static uint8_t lcd_text_scale[LCD_TEXT_LINE_COUNT];
static char lcd_page_number[8];

static const uint8_t *LcdFindGlyph(char character)
{
  if (character == ' ') {
    return NULL;
  }
  for (size_t index = 0U;
       index < sizeof(lcd_glyphs) / sizeof(lcd_glyphs[0]); ++index) {
    if (lcd_glyphs[index].character == character) {
      return lcd_glyphs[index].columns;
    }
  }
  return NULL;
}

static uint16_t LcdValueColor(LcdUiValueState state)
{
  switch (state) {
    case LCD_UI_VALUE_PASS:
      return LCD_UI_COLOR_PASS;
    case LCD_UI_VALUE_FAIL:
      return LCD_UI_COLOR_FAIL;
    case LCD_UI_VALUE_DISABLED:
      return LCD_UI_COLOR_WEAK;
    default:
      return LCD_UI_COLOR_PASS;
  }
}

static const char *LcdValueText(LcdUiValueState state)
{
  switch (state) {
    case LCD_UI_VALUE_PASS:
      return "PASS";
    case LCD_UI_VALUE_FAIL:
      return "FAIL";
    case LCD_UI_VALUE_DISABLED:
      return "DISABLED";
    default:
      return "READY";
  }
}

static const char *LcdControlText(LcdUiControlState state)
{
  switch (state) {
    case LCD_UI_CONTROL_RUNNING:
      return "RUNNING";
    case LCD_UI_CONTROL_TIMEOUT:
      return "TIMEOUT";
    case LCD_UI_CONTROL_EMERGENCY_STOP:
      return "E-STOP";
    case LCD_UI_CONTROL_FAULT:
      return "FAULT";
    case LCD_UI_CONTROL_TEST:
      return "TEST";
    default:
      return "STOPPED";
  }
}

static const char *LcdSensorText(LcdUiSensorState state)
{
  switch (state) {
    case LCD_UI_SENSOR_WARMING:
      return "WARMING";
    case LCD_UI_SENSOR_READY:
      return "READY";
    case LCD_UI_SENSOR_DEGRADED:
      return "DEGRADED";
    case LCD_UI_SENSOR_FAILED:
      return "FAILED";
    default:
      return "DISABLED";
  }
}

static uint16_t LcdSensorColor(LcdUiSensorState state)
{
  switch (state) {
    case LCD_UI_SENSOR_READY:
      return LCD_UI_COLOR_PASS;
    case LCD_UI_SENSOR_FAILED:
      return LCD_UI_COLOR_FAIL;
    case LCD_UI_SENSOR_DISABLED:
      return LCD_UI_COLOR_WEAK;
    default:
      return LCD_UI_COLOR_WARNING;
  }
}

static uint16_t LcdControlColor(LcdUiControlState state)
{
  switch (state) {
    case LCD_UI_CONTROL_RUNNING:
      return LCD_UI_COLOR_PASS;
    case LCD_UI_CONTROL_EMERGENCY_STOP:
    case LCD_UI_CONTROL_FAULT:
      return LCD_UI_COLOR_FAIL;
    default:
      return LCD_UI_COLOR_WARNING;
  }
}

static const char *LcdResetText(LcdUiResetCause cause)
{
  switch (cause) {
    case LCD_UI_RESET_PIN:
      return "PIN";
    case LCD_UI_RESET_BROWNOUT:
      return "BOR";
    case LCD_UI_RESET_SOFTWARE:
      return "SW";
    case LCD_UI_RESET_IWDG:
      return "IWDG";
    case LCD_UI_RESET_WWDG:
      return "WWDG";
    case LCD_UI_RESET_LOW_POWER:
      return "LOWPOWER";
    case LCD_UI_RESET_OPTION_BYTE:
      return "OPTION";
    default:
      return "NONE";
  }
}

static const char *LcdPageTitle(LcdUiPage page)
{
  switch (page) {
    case LCD_UI_PAGE_MOTOR:
      return "MOTOR";
    case LCD_UI_PAGE_SENSORS:
      return "SENSORS";
    case LCD_UI_PAGE_SYSTEM:
      return "SYSTEM";
    default:
      return "OVERVIEW";
  }
}

static void LcdDrawPageIndicatorOnRow(uint16_t row)
{
  if (row < LCD_UI_HEADER_INDICATOR_Y ||
      row >= LCD_UI_HEADER_INDICATOR_Y + 3U) {
    return;
  }
  for (uint16_t page = 0U; page < LCD_UI_PAGE_COUNT; ++page) {
    const uint16_t start_x =
        LCD_UI_HEADER_INDICATOR_X +
        page * (LCD_UI_HEADER_INDICATOR_WIDTH +
                LCD_UI_HEADER_INDICATOR_GAP);
    const uint16_t color = page == (uint16_t)lcd_requested_page
                               ? LCD_UI_COLOR_ACCENT
                               : LCD_UI_COLOR_DIVIDER;
    for (uint16_t column = start_x;
         column < start_x + LCD_UI_HEADER_INDICATOR_WIDTH;
         ++column) {
      lcd_line_buffer[column * 2U] = (uint8_t)(color >> 8U);
      lcd_line_buffer[column * 2U + 1U] = (uint8_t)color;
    }
  }
}

static void LcdDrawPanelDividerOnRow(uint16_t row, uint16_t start_y,
                                     uint16_t end_y)
{
  if (row < start_y || row >= end_y) {
    return;
  }
  lcd_line_buffer[LCD_UI_DIVIDER_X * 2U] =
      (uint8_t)(LCD_UI_COLOR_DIVIDER >> 8U);
  lcd_line_buffer[LCD_UI_DIVIDER_X * 2U + 1U] =
      (uint8_t)LCD_UI_COLOR_DIVIDER;
}

static uint16_t LcdBatteryColor(void);

static void LcdFormatVoltage(char *buffer, size_t buffer_size,
                             uint32_t millivolts)
{
  (void)snprintf(buffer, buffer_size, "%lu.%02luV",
                 (unsigned long)(millivolts / 1000U),
                 (unsigned long)((millivolts % 1000U) / 10U));
}

static int16_t LcdClampPoseValue(int32_t value)
{
  if (value > 9999) {
    return 9999;
  }
  if (value < -9999) {
    return -9999;
  }
  return (int16_t)value;
}

static uint16_t LcdCenteredTextX(const char *text, uint16_t start_x,
                                 uint16_t width, uint8_t scale)
{
  const size_t length = strlen(text);
  const uint16_t text_width = length == 0U
                                  ? 0U
                                  : (uint16_t)(length * 6U * scale - scale);

  return text_width >= width ? start_x
                             : (uint16_t)(start_x + (width - text_width) / 2U);
}

static void LcdPrepareOverviewPage(void)
{
  if (lcd_status_data.supply_state == LCD_UI_VALUE_PASS) {
    LcdFormatVoltage(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH,
                     lcd_status_data.adc_mv);
    (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH,
                   "%u%%", (unsigned int)lcd_status_data.battery_percent);
  } else {
    (void)snprintf(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH, "NO DATA");
    (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "--");
  }
  (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH, "POWER");
  (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH, "CONTROL");
  (void)snprintf(lcd_text_lines[5], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdControlText(lcd_status_data.control_state));
  (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH, "CAN");
  (void)snprintf(lcd_text_lines[7], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdValueText(lcd_status_data.can_state));
  (void)snprintf(lcd_text_lines[8], LCD_TEXT_LINE_LENGTH, "QSPI");
  (void)snprintf(lcd_text_lines[9], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdValueText(lcd_status_data.qspi_state));
  (void)snprintf(lcd_text_lines[10], LCD_TEXT_LINE_LENGTH, "IMU");
  (void)snprintf(lcd_text_lines[11], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdSensorText(lcd_status_data.imu_state));
  (void)snprintf(lcd_text_lines[12], LCD_TEXT_LINE_LENGTH, "FAULTS");
  (void)snprintf(lcd_text_lines[13], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.fault_flags == 0U ? "0" : "ERROR");

  lcd_text_x[1] = 12U;
  lcd_text_y[1] = 45U;
  lcd_text_scale[1] = 1U;
  lcd_text_colors[1] = LCD_UI_COLOR_MUTED;
  lcd_text_x[2] = 12U;
  lcd_text_y[2] = 58U;
  lcd_text_scale[2] = 3U;
  lcd_text_colors[2] = lcd_status_data.supply_state == LCD_UI_VALUE_PASS
                           ? LCD_UI_COLOR_TEXT
                           : LCD_UI_COLOR_FAIL;
  lcd_text_x[3] = 236U;
  lcd_text_y[3] = 61U;
  lcd_text_scale[3] = 2U;
  lcd_text_colors[3] = LcdBatteryColor();
  lcd_text_x[4] = 12U;
  lcd_text_y[4] = 124U;
  lcd_text_scale[4] = 1U;
  lcd_text_colors[4] = LCD_UI_COLOR_MUTED;
  lcd_text_x[5] = 12U;
  lcd_text_y[5] = 143U;
  lcd_text_scale[5] = 1U;
  lcd_text_colors[5] = LcdControlColor(lcd_status_data.control_state);
  lcd_text_x[6] = 170U;
  lcd_text_y[6] = 124U;
  lcd_text_scale[6] = 1U;
  lcd_text_colors[6] = LCD_UI_COLOR_MUTED;
  lcd_text_x[7] = 232U;
  lcd_text_y[7] = 124U;
  lcd_text_scale[7] = 1U;
  lcd_text_colors[7] = LcdValueColor(lcd_status_data.can_state);
  lcd_text_x[8] = 170U;
  lcd_text_y[8] = 143U;
  lcd_text_scale[8] = 1U;
  lcd_text_colors[8] = LCD_UI_COLOR_MUTED;
  lcd_text_x[9] = 232U;
  lcd_text_y[9] = 143U;
  lcd_text_scale[9] = 1U;
  lcd_text_colors[9] = LcdValueColor(lcd_status_data.qspi_state);
  lcd_text_x[10] = 170U;
  lcd_text_y[10] = 162U;
  lcd_text_scale[10] = 1U;
  lcd_text_colors[10] = LCD_UI_COLOR_MUTED;
  lcd_text_x[11] = 232U;
  lcd_text_y[11] = 162U;
  lcd_text_scale[11] = 1U;
  lcd_text_colors[11] = LcdSensorColor(lcd_status_data.imu_state);
  lcd_text_x[12] = 12U;
  lcd_text_y[12] = 178U;
  lcd_text_scale[12] = 1U;
  lcd_text_colors[12] = LCD_UI_COLOR_WEAK;
  lcd_text_x[13] = 96U;
  lcd_text_y[13] = 178U;
  lcd_text_scale[13] = 1U;
  lcd_text_colors[13] = lcd_status_data.fault_flags == 0U
                           ? LCD_UI_COLOR_WEAK
                           : LCD_UI_COLOR_FAIL;
}

static void LcdPrepareMotorPage(void)
{
  uint8_t left_speed_scale;
  uint8_t right_speed_scale;

  (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH, "LEFT");
  (void)snprintf(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH, "%ld",
                 (long)lcd_status_data.left_measurement);
  (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "SPEED");
  (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH, "RIGHT");
  (void)snprintf(lcd_text_lines[5], LCD_TEXT_LINE_LENGTH, "%ld",
                 (long)lcd_status_data.right_measurement);
  (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH, "SPEED");
  (void)snprintf(lcd_text_lines[7], LCD_TEXT_LINE_LENGTH, "TARGET %ld",
                 (long)lcd_status_data.left_target);
  (void)snprintf(lcd_text_lines[8], LCD_TEXT_LINE_LENGTH, "PWM %d",
                 (int)lcd_status_data.left_output);
  (void)snprintf(lcd_text_lines[9], LCD_TEXT_LINE_LENGTH, "ENC %ld",
                 (long)lcd_status_data.left_encoder_delta);
  (void)snprintf(lcd_text_lines[10], LCD_TEXT_LINE_LENGTH, "TARGET %ld",
                 (long)lcd_status_data.right_target);
  (void)snprintf(lcd_text_lines[11], LCD_TEXT_LINE_LENGTH, "PWM %d",
                 (int)lcd_status_data.right_output);
  (void)snprintf(lcd_text_lines[12], LCD_TEXT_LINE_LENGTH, "ENC %ld",
                 (long)lcd_status_data.right_encoder_delta);
  (void)snprintf(lcd_text_lines[13], LCD_TEXT_LINE_LENGTH, "POSE");
  if (lcd_status_data.odometry_valid) {
    (void)snprintf(lcd_text_lines[14], LCD_TEXT_LINE_LENGTH,
                   "X%hd Y%hd H%hd",
                   LcdClampPoseValue(lcd_status_data.odometry_x_mm),
                   LcdClampPoseValue(lcd_status_data.odometry_y_mm),
                   LcdClampPoseValue(lcd_status_data.odometry_heading_mrad));
  } else {
    (void)snprintf(lcd_text_lines[14], LCD_TEXT_LINE_LENGTH, "ODOM N/R");
  }
  (void)snprintf(lcd_text_lines[15], LCD_TEXT_LINE_LENGTH, "CONTROL");
  (void)snprintf(lcd_text_lines[16], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdControlText(lcd_status_data.control_state));

  left_speed_scale = strlen(lcd_text_lines[2]) > 7U ? 2U : 3U;
  right_speed_scale = strlen(lcd_text_lines[5]) > 7U ? 2U : 3U;

  lcd_text_x[1] = 12U; lcd_text_y[1] = 45U; lcd_text_scale[1] = 1U;
  lcd_text_x[2] = LcdCenteredTextX(lcd_text_lines[2], 12U, 136U,
                                   left_speed_scale);
  lcd_text_y[2] = 57U; lcd_text_scale[2] = left_speed_scale;
  lcd_text_x[3] = 108U; lcd_text_y[3] = 79U; lcd_text_scale[3] = 1U;
  lcd_text_x[4] = 170U; lcd_text_y[4] = 45U; lcd_text_scale[4] = 1U;
  lcd_text_x[5] = LcdCenteredTextX(lcd_text_lines[5], 170U, 136U,
                                   right_speed_scale);
  lcd_text_y[5] = 57U; lcd_text_scale[5] = right_speed_scale;
  lcd_text_x[6] = 266U; lcd_text_y[6] = 79U; lcd_text_scale[6] = 1U;
  lcd_text_x[7] = 12U; lcd_text_y[7] = 120U; lcd_text_scale[7] = 1U;
  lcd_text_x[8] = 12U; lcd_text_y[8] = 136U; lcd_text_scale[8] = 1U;
  lcd_text_x[9] = 12U; lcd_text_y[9] = 152U; lcd_text_scale[9] = 1U;
  lcd_text_x[10] = 170U; lcd_text_y[10] = 120U; lcd_text_scale[10] = 1U;
  lcd_text_x[11] = 170U; lcd_text_y[11] = 136U; lcd_text_scale[11] = 1U;
  lcd_text_x[12] = 170U; lcd_text_y[12] = 152U; lcd_text_scale[12] = 1U;
  lcd_text_x[13] = 12U; lcd_text_y[13] = 174U; lcd_text_scale[13] = 1U;
  lcd_text_x[14] = 12U; lcd_text_y[14] = 188U; lcd_text_scale[14] = 1U;
  lcd_text_x[15] = 190U; lcd_text_y[15] = 174U; lcd_text_scale[15] = 1U;
  lcd_text_x[16] = 190U; lcd_text_y[16] = 188U; lcd_text_scale[16] = 1U;
  for (uint8_t line = 1U; line < LCD_TEXT_LINE_COUNT; ++line) {
    lcd_text_colors[line] = LCD_UI_COLOR_TEXT;
  }
  lcd_text_colors[1] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[3] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[4] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[6] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[7] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[10] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[13] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[15] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[16] = LcdControlColor(lcd_status_data.control_state);
}

static void LcdPrepareSensorsPage(void)
{
  (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH, "IMU");
  (void)snprintf(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdSensorText(lcd_status_data.imu_state));
  if (lcd_status_data.imu_orientation_valid) {
    (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "R %d P %d",
                   (int)lcd_status_data.roll_deg,
                   (int)lcd_status_data.pitch_deg);
    (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH, "Y %d",
                   (int)lcd_status_data.yaw_deg);
  } else {
    (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "RPY N/A");
  }
  (void)snprintf(lcd_text_lines[5], LCD_TEXT_LINE_LENGTH, "SR501");
  if (lcd_status_data.sr501_state == LCD_UI_SENSOR_WARMING) {
    (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH, "WARM %lus",
                   (unsigned long)(lcd_status_data.sr501_warmup_remaining_ms /
                                   1000U));
  } else {
    (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH, "%s",
                   LcdSensorText(lcd_status_data.sr501_state));
  }
  (void)snprintf(lcd_text_lines[7], LCD_TEXT_LINE_LENGTH, "MOTION %s",
                 lcd_status_data.sr501_motion ? "YES" : "NO");
  (void)snprintf(lcd_text_lines[8], LCD_TEXT_LINE_LENGTH, "EVENTS %lu",
                 (unsigned long)lcd_status_data.sr501_event_count);
  (void)snprintf(lcd_text_lines[9], LCD_TEXT_LINE_LENGTH, "POWER");
  if (lcd_status_data.supply_state == LCD_UI_VALUE_PASS) {
    LcdFormatVoltage(lcd_text_lines[10], LCD_TEXT_LINE_LENGTH,
                     lcd_status_data.adc_mv);
  } else {
    (void)snprintf(lcd_text_lines[10], LCD_TEXT_LINE_LENGTH, "FAIL");
  }
  (void)snprintf(lcd_text_lines[11], LCD_TEXT_LINE_LENGTH, "RTC");
  if (lcd_status_data.rtc_state == LCD_UI_VALUE_PASS) {
    (void)snprintf(lcd_text_lines[12], LCD_TEXT_LINE_LENGTH, "%02u:%02u:%02u",
                   (unsigned int)lcd_status_data.rtc_hours,
                   (unsigned int)lcd_status_data.rtc_minutes,
                   (unsigned int)lcd_status_data.rtc_seconds);
  } else {
    (void)snprintf(lcd_text_lines[12], LCD_TEXT_LINE_LENGTH, "%s",
                   LcdValueText(lcd_status_data.rtc_state));
  }

  lcd_text_x[1] = 12U; lcd_text_y[1] = 45U; lcd_text_scale[1] = 1U;
  lcd_text_x[2] = 12U; lcd_text_y[2] = 60U; lcd_text_scale[2] = 1U;
  lcd_text_x[3] = 12U; lcd_text_y[3] = 80U; lcd_text_scale[3] = 1U;
  lcd_text_x[4] = 12U; lcd_text_y[4] = 96U; lcd_text_scale[4] = 1U;
  lcd_text_x[5] = 170U; lcd_text_y[5] = 45U; lcd_text_scale[5] = 1U;
  lcd_text_x[6] = 170U; lcd_text_y[6] = 60U; lcd_text_scale[6] = 1U;
  lcd_text_x[7] = 170U; lcd_text_y[7] = 80U; lcd_text_scale[7] = 1U;
  lcd_text_x[8] = 170U; lcd_text_y[8] = 96U; lcd_text_scale[8] = 1U;
  lcd_text_x[9] = 12U; lcd_text_y[9] = 124U; lcd_text_scale[9] = 1U;
  lcd_text_x[10] = 12U; lcd_text_y[10] = 142U; lcd_text_scale[10] = 2U;
  lcd_text_x[11] = 170U; lcd_text_y[11] = 124U; lcd_text_scale[11] = 1U;
  lcd_text_x[12] = 170U; lcd_text_y[12] = 142U; lcd_text_scale[12] = 2U;
  for (uint8_t line = 1U; line < LCD_TEXT_LINE_COUNT; ++line) {
    lcd_text_colors[line] = LCD_UI_COLOR_TEXT;
  }
  lcd_text_colors[1] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[2] = LcdSensorColor(lcd_status_data.imu_state);
  lcd_text_colors[3] = lcd_status_data.imu_orientation_valid
                           ? LCD_UI_COLOR_TEXT
                           : LCD_UI_COLOR_WARNING;
  lcd_text_colors[4] = lcd_text_colors[3];
  lcd_text_colors[5] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[6] = LcdSensorColor(lcd_status_data.sr501_state);
  lcd_text_colors[7] = LCD_UI_COLOR_TEXT;
  lcd_text_colors[9] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[11] = LCD_UI_COLOR_MUTED;
}

static void LcdPrepareSystemPage(void)
{
  const uint32_t uptime_seconds = lcd_status_data.uptime_ms / 1000U;

  (void)snprintf(lcd_text_lines[1], LCD_TEXT_LINE_LENGTH, "HEALTH");
  (void)snprintf(lcd_text_lines[2], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.critical_tasks_healthy ? "OK" : "FAIL");
  (void)snprintf(lcd_text_lines[3], LCD_TEXT_LINE_LENGTH, "STORAGE");
  (void)snprintf(lcd_text_lines[4], LCD_TEXT_LINE_LENGTH, "QSPI %uM",
                 (unsigned int)lcd_status_data.qspi_capacity_mib);
  (void)snprintf(lcd_text_lines[5], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdValueText(lcd_status_data.qspi_state));
  (void)snprintf(lcd_text_lines[6], LCD_TEXT_LINE_LENGTH, "TASKS");
  (void)snprintf(lcd_text_lines[7], LCD_TEXT_LINE_LENGTH, "SERVICE");
  (void)snprintf(lcd_text_lines[8], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.service_task_healthy ? "RUN" : "FAIL");
  (void)snprintf(lcd_text_lines[9], LCD_TEXT_LINE_LENGTH, "CONTROL");
  (void)snprintf(lcd_text_lines[10], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.control_task_healthy ? "RUN" : "FAIL");
  (void)snprintf(lcd_text_lines[11], LCD_TEXT_LINE_LENGTH, "DIAG");
  (void)snprintf(lcd_text_lines[12], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.diagnostics_task_healthy ? "RUN" : "FAIL");
  (void)snprintf(lcd_text_lines[13], LCD_TEXT_LINE_LENGTH, "DISPLAY");
  (void)snprintf(lcd_text_lines[14], LCD_TEXT_LINE_LENGTH, "%s",
                 lcd_status_data.display_task_healthy ? "RUN" : "FAIL");
  (void)snprintf(lcd_text_lines[15], LCD_TEXT_LINE_LENGTH, "STACK FREE");
  (void)snprintf(lcd_text_lines[16], LCD_TEXT_LINE_LENGTH, "SVC %lu CTRL %lu",
                 (unsigned long)lcd_status_data.service_stack_free_words,
                 (unsigned long)lcd_status_data.control_stack_free_words);
  (void)snprintf(lcd_text_lines[17], LCD_TEXT_LINE_LENGTH, "DIAG %lu LCD %lu",
                 (unsigned long)lcd_status_data.diagnostics_stack_free_words,
                 (unsigned long)lcd_status_data.display_stack_free_words);
  (void)snprintf(lcd_text_lines[18], LCD_TEXT_LINE_LENGTH, "UPTIME");
  (void)snprintf(lcd_text_lines[19], LCD_TEXT_LINE_LENGTH, "%02lu:%02lu:%02lu",
                 (unsigned long)(uptime_seconds / 3600U),
                 (unsigned long)((uptime_seconds / 60U) % 60U),
                 (unsigned long)(uptime_seconds % 60U));
  (void)snprintf(lcd_text_lines[20], LCD_TEXT_LINE_LENGTH, "RESET");
  (void)snprintf(lcd_text_lines[21], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdResetText(lcd_status_data.reset_cause));
  lcd_text_x[1] = 12U; lcd_text_y[1] = 44U; lcd_text_scale[1] = 1U;
  lcd_text_x[2] = 90U; lcd_text_y[2] = 44U; lcd_text_scale[2] = 1U;
  lcd_text_x[3] = 170U; lcd_text_y[3] = 44U; lcd_text_scale[3] = 1U;
  lcd_text_x[4] = 170U; lcd_text_y[4] = 61U; lcd_text_scale[4] = 1U;
  lcd_text_x[5] = 260U; lcd_text_y[5] = 61U; lcd_text_scale[5] = 1U;
  lcd_text_x[6] = 12U; lcd_text_y[6] = 82U; lcd_text_scale[6] = 1U;
  lcd_text_x[7] = 12U; lcd_text_y[7] = 98U; lcd_text_scale[7] = 1U;
  lcd_text_x[8] = 104U; lcd_text_y[8] = 98U; lcd_text_scale[8] = 1U;
  lcd_text_x[9] = 12U; lcd_text_y[9] = LCD_UI_SYSTEM_TASK_ROW_2_Y;
  lcd_text_scale[9] = 1U;
  lcd_text_x[10] = 104U; lcd_text_y[10] = LCD_UI_SYSTEM_TASK_ROW_2_Y;
  lcd_text_scale[10] = 1U;
  lcd_text_x[11] = 170U; lcd_text_y[11] = LCD_UI_SYSTEM_TASK_ROW_1_Y;
  lcd_text_scale[11] = 1U;
  lcd_text_x[12] = 250U; lcd_text_y[12] = LCD_UI_SYSTEM_TASK_ROW_1_Y;
  lcd_text_scale[12] = 1U;
  lcd_text_x[13] = 170U; lcd_text_y[13] = LCD_UI_SYSTEM_TASK_ROW_2_Y;
  lcd_text_scale[13] = 1U;
  lcd_text_x[14] = 250U; lcd_text_y[14] = LCD_UI_SYSTEM_TASK_ROW_2_Y;
  lcd_text_scale[14] = 1U;
  lcd_text_x[15] = 12U; lcd_text_y[15] = 140U; lcd_text_scale[15] = 1U;
  lcd_text_x[16] = 12U; lcd_text_y[16] = 155U; lcd_text_scale[16] = 1U;
  lcd_text_x[17] = 170U; lcd_text_y[17] = 155U; lcd_text_scale[17] = 1U;
  lcd_text_x[18] = 12U; lcd_text_y[18] = 174U; lcd_text_scale[18] = 1U;
  lcd_text_x[19] = 12U; lcd_text_y[19] = 190U; lcd_text_scale[19] = 1U;
  lcd_text_x[20] = 190U; lcd_text_y[20] = 174U; lcd_text_scale[20] = 1U;
  lcd_text_x[21] = 190U; lcd_text_y[21] = 190U; lcd_text_scale[21] = 1U;
  for (uint8_t line = 1U; line < LCD_TEXT_LINE_COUNT; ++line) {
    lcd_text_colors[line] = LCD_UI_COLOR_TEXT;
  }
  lcd_text_colors[1] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[2] = lcd_status_data.critical_tasks_healthy
                           ? LCD_UI_COLOR_PASS
                           : LCD_UI_COLOR_FAIL;
  lcd_text_colors[3] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[5] = LcdValueColor(lcd_status_data.qspi_state);
  lcd_text_colors[6] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[7] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[8] = lcd_status_data.service_task_healthy
                           ? LCD_UI_COLOR_PASS : LCD_UI_COLOR_FAIL;
  lcd_text_colors[9] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[10] = lcd_status_data.control_task_healthy
                            ? LCD_UI_COLOR_PASS : LCD_UI_COLOR_FAIL;
  lcd_text_colors[11] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[12] = lcd_status_data.diagnostics_task_healthy
                            ? LCD_UI_COLOR_PASS : LCD_UI_COLOR_FAIL;
  lcd_text_colors[13] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[14] = lcd_status_data.display_task_healthy
                            ? LCD_UI_COLOR_PASS : LCD_UI_COLOR_FAIL;
  lcd_text_colors[15] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[18] = LCD_UI_COLOR_MUTED;
  lcd_text_colors[20] = LCD_UI_COLOR_MUTED;
}

static void LcdPreparePage(LcdUiPage page)
{
  (void)snprintf(lcd_text_lines[0], LCD_TEXT_LINE_LENGTH, "%s",
                 LcdPageTitle(page));
  (void)snprintf(lcd_page_number, sizeof(lcd_page_number), "%02u/04",
                 (unsigned int)page + 1U);
  for (uint8_t line = 1U; line < LCD_TEXT_LINE_COUNT; ++line) {
    lcd_text_lines[line][0] = '\0';
    lcd_text_x[line] = 12U;
    lcd_text_y[line] = 44U;
    lcd_text_scale[line] = 2U;
    lcd_text_colors[line] = LCD_UI_COLOR_TEXT;
  }
  lcd_text_x[0] = 12U;
  lcd_text_y[0] = 8U;
  lcd_text_scale[0] = 2U;
  lcd_text_colors[0] = LCD_UI_COLOR_ACCENT;

  switch (page) {
    case LCD_UI_PAGE_MOTOR:
      LcdPrepareMotorPage();
      break;
    case LCD_UI_PAGE_SENSORS:
      LcdPrepareSensorsPage();
      break;
    case LCD_UI_PAGE_SYSTEM:
      LcdPrepareSystemPage();
      break;
    default:
      LcdPrepareOverviewPage();
      break;
  }
  (void)snprintf(lcd_text_lines[23], LCD_TEXT_LINE_LENGTH, "FW V%s B%s",
                 CHASSIS_FIRMWARE_VERSION, CHASSIS_FIRMWARE_BUILD_STRING);
  lcd_text_x[23] = 12U;
  lcd_text_y[23] = 220U;
  lcd_text_scale[23] = 1U;
  lcd_text_colors[23] = LCD_UI_COLOR_WEAK;
}

static uint16_t LcdBackgroundColor(uint16_t row, uint16_t column)
{
  if (row < LCD_UI_HEADER_HEIGHT) {
    if (row >= LCD_UI_HEADER_ACCENT_START_Y &&
        column >= LCD_UI_MARGIN_X &&
        column < LCD_UI_HEADER_ACCENT_END_X) {
      return LCD_UI_COLOR_ACCENT;
    }
    return LCD_UI_COLOR_BACKGROUND;
  }
  if (row == LCD_UI_HEADER_RULE_Y || row == LCD_UI_FOOTER_RULE_Y) {
    return LCD_UI_COLOR_DIVIDER;
  }
  switch (lcd_requested_page) {
    case LCD_UI_PAGE_MOTOR:
      if (row >= LCD_UI_MOTOR_MAIN_Y &&
          row < LCD_UI_MOTOR_MAIN_END_Y) {
        return LCD_UI_COLOR_PANEL;
      }
      if (row >= LCD_UI_MOTOR_SUMMARY_Y &&
          row < LCD_UI_MOTOR_SUMMARY_END_Y) {
        return LCD_UI_COLOR_PANEL_ALT;
      }
      break;
    case LCD_UI_PAGE_SENSORS:
      if (row >= LCD_UI_SENSORS_MAIN_Y &&
          row < LCD_UI_SENSORS_MAIN_END_Y) {
        return LCD_UI_COLOR_PANEL;
      }
      if (row >= LCD_UI_SENSORS_SUMMARY_Y &&
          row < LCD_UI_SENSORS_SUMMARY_END_Y) {
        return LCD_UI_COLOR_PANEL_ALT;
      }
      break;
    case LCD_UI_PAGE_SYSTEM:
      if ((row >= LCD_UI_SYSTEM_HEALTH_Y &&
           row < LCD_UI_SYSTEM_HEALTH_END_Y) ||
          (row >= LCD_UI_SYSTEM_DIAGNOSTICS_Y &&
           row < LCD_UI_SYSTEM_DIAGNOSTICS_END_Y)) {
        return LCD_UI_COLOR_PANEL;
      }
      if (row >= LCD_UI_SYSTEM_TASKS_Y &&
          row < LCD_UI_SYSTEM_TASKS_END_Y) {
        return LCD_UI_COLOR_PANEL_ALT;
      }
      break;
    default:
      if (row >= LCD_UI_PANEL_TOP_Y && row < LCD_UI_PANEL_TOP_END_Y) {
        return LCD_UI_COLOR_PANEL;
      }
      if (row >= LCD_UI_PANEL_BOTTOM_Y &&
          row < LCD_UI_PANEL_BOTTOM_END_Y) {
        return LCD_UI_COLOR_PANEL_ALT;
      }
      break;
  }
  if (row >= LCD_UI_FOOTER_START_Y) {
    return LCD_UI_COLOR_BACKGROUND;
  }
  return LCD_UI_COLOR_BACKGROUND;
}

static uint16_t LcdBatteryColor(void)
{
  if (!lcd_status_data.battery_percent_valid ||
      lcd_status_data.battery_percent <= 20U) {
    return LCD_UI_COLOR_FAIL;
  }
  if (lcd_status_data.battery_percent <= 40U) {
    return LCD_UI_COLOR_WARNING;
  }
  return LCD_UI_COLOR_ACCENT;
}

static void LcdDrawBatteryBarOnRow(uint16_t row)
{
  const uint16_t bar_end = LCD_UI_BATTERY_X + LCD_UI_BATTERY_WIDTH;
  const uint16_t fill_width = lcd_status_data.battery_percent_valid
                                   ? (uint16_t)((LCD_UI_BATTERY_WIDTH - 4U) *
                                                lcd_status_data.battery_percent /
                                                100U)
                                   : 0U;
  const uint16_t fill_end = LCD_UI_BATTERY_X + 2U + fill_width;
  const uint16_t color = LcdBatteryColor();

  if (lcd_requested_page != LCD_UI_PAGE_OVERVIEW ||
      row < LCD_UI_BATTERY_Y ||
      row >= LCD_UI_BATTERY_Y + LCD_UI_BATTERY_HEIGHT) {
    return;
  }
  for (uint16_t column = LCD_UI_BATTERY_X; column <= bar_end; ++column) {
    if (row == LCD_UI_BATTERY_Y ||
        row == LCD_UI_BATTERY_Y + LCD_UI_BATTERY_HEIGHT - 1U ||
        column == LCD_UI_BATTERY_X || column == bar_end) {
      lcd_line_buffer[column * 2U] = (uint8_t)(LCD_UI_COLOR_MUTED >> 8U);
      lcd_line_buffer[column * 2U + 1U] = (uint8_t)LCD_UI_COLOR_MUTED;
    } else if (column < fill_end) {
      lcd_line_buffer[column * 2U] = (uint8_t)(color >> 8U);
      lcd_line_buffer[column * 2U + 1U] = (uint8_t)color;
    }
  }
  if (row > LCD_UI_BATTERY_Y &&
      row < LCD_UI_BATTERY_Y + LCD_UI_BATTERY_HEIGHT - 1U) {
    for (uint16_t column = bar_end + 1U; column <= bar_end + 4U; ++column) {
      lcd_line_buffer[column * 2U] = (uint8_t)(LCD_UI_COLOR_MUTED >> 8U);
      lcd_line_buffer[column * 2U + 1U] = (uint8_t)LCD_UI_COLOR_MUTED;
    }
  }
}

static void LcdDrawTextOnRow(uint16_t row, uint16_t y, const char *text,
                             uint16_t x, uint16_t color, uint8_t scale)
{
  uint8_t font_row;

  if (row < y || row >= y + 7U * scale) {
    return;
  }
  font_row = (uint8_t)((row - y) / scale);

  while (*text != '\0' && x + 5U * scale <= LCD_UI_WIDTH) {
    const uint8_t *glyph = LcdFindGlyph(*text++);

    if (glyph != NULL) {
      for (uint8_t column = 0U; column < 5U; ++column) {
        if ((glyph[column] & (1U << font_row)) != 0U) {
          for (uint8_t pixel = 0U; pixel < scale; ++pixel) {
            const uint16_t draw_x = x + column * scale + pixel;

            lcd_line_buffer[draw_x * 2U] = (uint8_t)(color >> 8U);
            lcd_line_buffer[draw_x * 2U + 1U] = (uint8_t)color;
          }
        }
      }
    }
    x += 6U * scale;
  }
}

static void LcdRenderRow(uint16_t row)
{
  for (uint16_t column = 0U; column < LCD_UI_WIDTH; ++column) {
    const uint16_t color = LcdBackgroundColor(row, column);

    if (row >= LCD_UI_LOGO_Y &&
        row < LCD_UI_LOGO_Y + LCD_UI_LOGO_SIZE &&
        column >= LCD_UI_WIDTH - LCD_UI_LOGO_SIZE -
                      LCD_UI_LOGO_RIGHT_MARGIN &&
        column < LCD_UI_WIDTH - LCD_UI_LOGO_RIGHT_MARGIN) {
      const uint16_t source_row =
          (uint16_t)((row - LCD_UI_LOGO_Y) * LCD_LOGO_IMAGE_HEIGHT /
                     LCD_UI_LOGO_SIZE);
      const uint16_t source_column =
          (uint16_t)((column - (LCD_UI_WIDTH - LCD_UI_LOGO_SIZE -
                               LCD_UI_LOGO_RIGHT_MARGIN)) *
                     LCD_LOGO_IMAGE_WIDTH /
                     LCD_UI_LOGO_SIZE);
      const uint32_t image_index =
          (source_row * LCD_LOGO_IMAGE_WIDTH + source_column) * 2U;

      const uint32_t mask_index =
          source_row * LCD_LOGO_IMAGE_WIDTH + source_column;
      if (LCD_LOGO_IMAGE_MASK[mask_index] != 0U) {
        lcd_line_buffer[column * 2U] =
            LCD_LOGO_IMAGE_DATA[image_index];
        lcd_line_buffer[column * 2U + 1U] =
            LCD_LOGO_IMAGE_DATA[image_index + 1U];
        continue;
      }
    }

    lcd_line_buffer[column * 2U] = (uint8_t)(color >> 8U);
    lcd_line_buffer[column * 2U + 1U] = (uint8_t)color;
  }

  LcdDrawBatteryBarOnRow(row);
  LcdDrawPageIndicatorOnRow(row);
  LcdDrawTextOnRow(row, LCD_UI_HEADER_PAGE_Y, lcd_page_number,
                   LCD_UI_HEADER_PAGE_X, LCD_UI_COLOR_MUTED, 1U);
  if (lcd_requested_page == LCD_UI_PAGE_OVERVIEW) {
    LcdDrawPanelDividerOnRow(row, LCD_UI_PANEL_BOTTOM_Y,
                             LCD_UI_PANEL_BOTTOM_END_Y);
  } else if (lcd_requested_page == LCD_UI_PAGE_MOTOR) {
    LcdDrawPanelDividerOnRow(row, LCD_UI_MOTOR_MAIN_Y,
                             LCD_UI_MOTOR_MAIN_END_Y);
  } else if (lcd_requested_page == LCD_UI_PAGE_SENSORS) {
    LcdDrawPanelDividerOnRow(row, LCD_UI_SENSORS_MAIN_Y,
                             LCD_UI_SENSORS_MAIN_END_Y);
    LcdDrawPanelDividerOnRow(row, LCD_UI_SENSORS_SUMMARY_Y,
                             LCD_UI_SENSORS_SUMMARY_END_Y);
  } else if (lcd_requested_page == LCD_UI_PAGE_SYSTEM) {
    LcdDrawPanelDividerOnRow(row, LCD_UI_SYSTEM_HEALTH_Y,
                             LCD_UI_SYSTEM_HEALTH_END_Y);
    LcdDrawPanelDividerOnRow(row, LCD_UI_SYSTEM_TASKS_Y,
                             LCD_UI_SYSTEM_TASKS_END_Y);
    LcdDrawPanelDividerOnRow(row, LCD_UI_SYSTEM_DIAGNOSTICS_Y,
                             LCD_UI_SYSTEM_DIAGNOSTICS_END_Y);
  }

  for (uint8_t line = 0U; line < LCD_TEXT_LINE_COUNT; ++line) {
    LcdDrawTextOnRow(row, lcd_text_y[line], lcd_text_lines[line],
                     lcd_text_x[line], lcd_text_colors[line],
                     lcd_text_scale[line]);
  }
}

static bool LcdTransmitRow(uint16_t row)
{
  LcdRenderRow(row);
  return display_transmit_row(lcd_display, lcd_line_buffer, sizeof(lcd_line_buffer));
}

static bool LcdStartDrawing(void)
{
  lcd_redraw_requested = false;
  LcdPreparePage(lcd_requested_page);
  if (!display_begin_frame(lcd_display)) {
    return false;
  }

  lcd_next_row = 1U;
  lcd_status = LCD_UI_DRAWING;
  return LcdTransmitRow(0U);
}

bool LcdUi_Init(const struct device *display)
{
  lcd_display = display;
  lcd_status = LCD_UI_FAILED;
  lcd_requested_page = LCD_UI_PAGE_OVERVIEW;
  lcd_redraw_requested = true;
  lcd_status_data = (LcdUiStatusData){0};
  if (!device_is_ready(lcd_display)) {
    return false;
  }
  if (!LcdStartDrawing()) {
    lcd_status = LCD_UI_FAILED;
    return false;
  }
  return true;
}

void LcdUi_Run(void)
{
  if (lcd_status == LCD_UI_READY && lcd_redraw_requested) {
    if (!LcdStartDrawing()) {
      lcd_status = LCD_UI_FAILED;
    }
    return;
  }
  if (lcd_status != LCD_UI_DRAWING) {
    return;
  }
  if (display_has_error(lcd_display)) {
    lcd_status = LCD_UI_FAILED;
    return;
  }
  if (!display_row_complete(lcd_display)) {
    return;
  }
  if (lcd_next_row >= LCD_UI_HEIGHT) {
    display_end_frame(lcd_display);
    lcd_status = display_get_status(lcd_display) == LCD_READY
                     ? LCD_UI_READY
                     : LCD_UI_FAILED;
    return;
  }
  if (!LcdTransmitRow(lcd_next_row++)) {
    lcd_status = LCD_UI_FAILED;
  }
}

void LcdUi_SetPage(LcdUiPage page)
{
  if (page >= LCD_UI_PAGE_COUNT) {
    return;
  }
  if (lcd_requested_page != page) {
    lcd_requested_page = page;
    lcd_redraw_requested = true;
  }
}

void LcdUi_SetStatusData(const LcdUiStatusData *data)
{
  if (data == NULL) {
    return;
  }
  lcd_status_data = *data;
  lcd_redraw_requested = true;
}

LcdUiStatus LcdUi_GetStatus(void)
{
  return lcd_status;
}

LcdUiPage LcdUi_GetPage(void)
{
  return lcd_requested_page;
}

#if defined(LCD_UI_HOST_PREVIEW)
void LcdUi_PreviewPrepare(LcdUiPage page, const LcdUiStatusData *data)
{
  lcd_requested_page = page < LCD_UI_PAGE_COUNT
                           ? page
                           : LCD_UI_PAGE_OVERVIEW;
  lcd_status_data = data != NULL ? *data : (LcdUiStatusData){0};
  LcdPreparePage(lcd_requested_page);
}

void LcdUi_PreviewRenderRow(uint16_t row, uint8_t *row_data,
                           uint16_t row_size)
{
  if (row >= LCD_UI_HEIGHT || row_data == NULL ||
      row_size < sizeof(lcd_line_buffer)) {
    return;
  }
  LcdRenderRow(row);
  memcpy(row_data, lcd_line_buffer, sizeof(lcd_line_buffer));
}
#endif
