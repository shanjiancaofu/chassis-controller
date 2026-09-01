#include "app/chassis/feedback_watchdog.h"

#include <stddef.h>

#include "config/control_config.h"

static int32_t OutputMagnitude(int16_t output) {
  return output < 0 ? -(int32_t)output : (int32_t)output;
}

static uint32_t AccumulateLoss(uint32_t current, bool feedback_implausible,
                               uint32_t elapsed_ms) {
  if (!feedback_implausible) {
    return 0U;
  }
  if (UINT32_MAX - current < elapsed_ms) {
    return UINT32_MAX;
  }
  return current + elapsed_ms;
}

void FeedbackWatchdog_Reset(FeedbackWatchdog *watchdog) {
  if (watchdog != NULL) {
    *watchdog = (FeedbackWatchdog){0};
  }
}

bool FeedbackWatchdog_Update(FeedbackWatchdog *watchdog,
                             const FeedbackWatchdogSample *sample,
                             uint32_t elapsed_ms) {
  bool left_implausible;
  bool right_implausible;

  if (watchdog == NULL || sample == NULL) {
    return false;
  }
  left_implausible =
      sample->left_target != 0 && sample->left_measurement == 0 &&
      OutputMagnitude(sample->left_output) >=
          MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD;
  right_implausible =
      sample->right_target != 0 && sample->right_measurement == 0 &&
      OutputMagnitude(sample->right_output) >=
          MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD;
  watchdog->left_loss_ms =
      AccumulateLoss(watchdog->left_loss_ms, left_implausible, elapsed_ms);
  watchdog->right_loss_ms =
      AccumulateLoss(watchdog->right_loss_ms, right_implausible, elapsed_ms);
  return watchdog->left_loss_ms < MOTOR_ENCODER_FEEDBACK_LOSS_TIMEOUT_MS &&
         watchdog->right_loss_ms < MOTOR_ENCODER_FEEDBACK_LOSS_TIMEOUT_MS;
}
