#include "infrastructure/telemetry/telemetry.h"

#include <stddef.h>
#include <stdio.h>

#include "bsp/uart/uart_bsp.h"
#include "config/control_config.h"
#include "config/protocol_config.h"

static TelemetryMode telemetry_mode;
static uint32_t last_transmit_ms;
static char transmit_buffer[256];

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
  int length;

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
    length = snprintf(
        transmit_buffer, sizeof(transmit_buffer),
        "vin_mv=%ld lt=%ld ld=%ld lrpm_x10=%ld lc=%ld lo=%d rt=%ld rd=%ld rrpm_x10=%ld rc=%ld ro=%d state=%lu fault=0x%08lx\r\n",
        (long)snapshot->supply_mv, (long)snapshot->left_target,
        (long)snapshot->left_delta, (long)left_rpm_x10,
        (long)snapshot->left_total, (int)snapshot->left_output,
        (long)snapshot->right_target, (long)snapshot->right_delta,
        (long)right_rpm_x10, (long)snapshot->right_total,
        (int)snapshot->right_output,
        (unsigned long)snapshot->control_state,
        (unsigned long)snapshot->fault_flags);
  }

  if (length > 0 && (size_t)length < sizeof(transmit_buffer) &&
      BspUart_Write(transmit_buffer, (size_t)length)) {
    last_transmit_ms = now_ms;
  }
}
