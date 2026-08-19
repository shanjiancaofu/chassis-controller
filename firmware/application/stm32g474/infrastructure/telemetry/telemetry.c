#include "infrastructure/telemetry/telemetry.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>

#include "bsp/uart/uart_bsp.h"
#include "config/control_config.h"
#include "config/protocol_config.h"
#include "infrastructure/uart_protocol/uart_protocol.h"

static TelemetryMode telemetry_mode;
static uint32_t last_transmit_ms;
static char transmit_buffer[768];

static int32_t FixedFromFloat(float value, float scale)
{
  const float scaled = value * scale;

  if (!(scaled == scaled)) {
    return 0;
  }
  if (scaled >= 2147483647.0f) {
    return INT32_MAX;
  }
  if (scaled <= -2147483648.0f) {
    return INT32_MIN;
  }
  return (int32_t)(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

void Telemetry_Init(void)
{
  telemetry_mode = TELEMETRY_MODE_OFF;
  last_transmit_ms = 0U;
}

void Telemetry_SetMode(TelemetryMode mode)
{
  telemetry_mode = mode;
}

TelemetryMode Telemetry_GetMode(void)
{
  return telemetry_mode;
}

bool Telemetry_IsDue(uint32_t now_ms)
{
  const uint32_t period_ms =
      telemetry_mode == TELEMETRY_MODE_VOFA
          ? MOTOR_VOFA_TELEMETRY_PERIOD_MS
          : MOTOR_TELEMETRY_PERIOD_MS;

  return telemetry_mode != TELEMETRY_MODE_OFF &&
         now_ms - last_transmit_ms >= period_ms;
}

void Telemetry_Run(uint32_t now_ms, const TelemetrySnapshot *snapshot)
{
  int32_t left_rpm_x10;
  int32_t right_rpm_x10;
  uint32_t sequence;
  int length;
  char left_total[24];
  char right_total[24];

  if (snapshot == NULL || !Telemetry_IsDue(now_ms)) {
    return;
  }

  left_rpm_x10 =
      (int32_t)(((int64_t)snapshot->left_delta * 60000) /
                MOTOR_ENCODER_COUNTS_PER_REVOLUTION);
  right_rpm_x10 =
      (int32_t)(((int64_t)snapshot->right_delta * 60000) /
                MOTOR_ENCODER_COUNTS_PER_REVOLUTION);

  if (telemetry_mode == TELEMETRY_MODE_VOFA) {
    length = snprintf(
        transmit_buffer, sizeof(transmit_buffer),
        "%ld,%ld,%ld,%d,%ld,%ld,%ld,%d,%ld,%lu,%lu\r\n",
        (long)snapshot->left_target, (long)snapshot->left_delta,
        (long)left_rpm_x10, (int)snapshot->left_output,
        (long)snapshot->right_target, (long)snapshot->right_delta,
        (long)right_rpm_x10, (int)snapshot->right_output,
        (long)snapshot->supply_mv, (unsigned long)snapshot->control_state,
        (unsigned long)snapshot->fault_flags);
  } else {
    (void)UartProtocol_FormatSigned64(left_total, sizeof(left_total),
                                      snapshot->left_total);
    (void)UartProtocol_FormatSigned64(right_total, sizeof(right_total),
                                      snapshot->right_total);
    length = snprintf(
        transmit_buffer, sizeof(transmit_buffer),
        "supply_mv=%ld left_target=%ld left_delta=%ld left_rpm_x10=%ld "
        "left_total=%s left_pwm=%d right_target=%ld right_delta=%ld "
        "right_rpm_x10=%ld right_total=%s right_pwm=%d control=%lu "
        "fault=0x%08lx odom_valid=%u odom_ts_ms=%lu odom_age_ms=%lu "
        "odom_x_mm=%ld odom_y_mm=%ld odom_heading_mrad=%ld "
        "odom_linear_mm_s=%ld odom_angular_mrad_s=%ld",
        (long)snapshot->supply_mv, (long)snapshot->left_target,
        (long)snapshot->left_delta, (long)left_rpm_x10,
        left_total, (int)snapshot->left_output,
        (long)snapshot->right_target, (long)snapshot->right_delta,
        (long)right_rpm_x10, right_total,
        (int)snapshot->right_output,
        (unsigned long)snapshot->control_state,
        (unsigned long)snapshot->fault_flags,
        snapshot->odometry_valid ? 1U : 0U,
        (unsigned long)snapshot->odometry_sample_timestamp_ms,
        (unsigned long)snapshot->odometry_sample_age_ms,
        (long)FixedFromFloat(snapshot->odometry_x_m, 1000.0f),
        (long)FixedFromFloat(snapshot->odometry_y_m, 1000.0f),
        (long)FixedFromFloat(snapshot->odometry_heading_rad, 1000.0f),
        (long)FixedFromFloat(snapshot->odometry_linear_velocity_mps, 1000.0f),
        (long)FixedFromFloat(snapshot->odometry_angular_velocity_rad_s,
                             1000.0f));
  }

  if (telemetry_mode == TELEMETRY_MODE_VOFA) {
    if (length > 0 && (size_t)length < sizeof(transmit_buffer) &&
        BspUart_Write(transmit_buffer, (size_t)length)) {
      last_transmit_ms = now_ms;
    }
  } else if (length > 0 && (size_t)length < sizeof(transmit_buffer)) {
    sequence = UartProtocol_NextTelemetrySequence();
    if (UartProtocol_SendTelemetry(now_ms, sequence, "motor",
                                   transmit_buffer)) {
      last_transmit_ms = now_ms;
    }
  }
}
