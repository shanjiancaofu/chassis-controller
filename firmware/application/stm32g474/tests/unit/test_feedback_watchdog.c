#include <assert.h>

#include "app/chassis/feedback_watchdog.h"
#include "config/control_config.h"

static FeedbackWatchdogSample Sample(int16_t output, int32_t measurement) {
  return (FeedbackWatchdogSample){
      .left_target = output < 0 ? -100 : 100,
      .right_target = 0,
      .left_measurement = measurement,
      .left_output = output,
  };
}

static void ExpectFaultAfter500Ms(int16_t output) {
  FeedbackWatchdog watchdog = {0};
  FeedbackWatchdogSample sample = Sample(output, 0);
  unsigned int tick;

  for (tick = 0U; tick < 49U; ++tick) {
    assert(FeedbackWatchdog_Update(&watchdog, &sample, 10U));
  }
  assert(!FeedbackWatchdog_Update(&watchdog, &sample, 10U));
}

int main(void) {
  FeedbackWatchdog watchdog = {0};
  FeedbackWatchdogSample below =
      Sample(MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD - 1, 0);
  FeedbackWatchdogSample stalled =
      Sample(MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD, 0);
  FeedbackWatchdogSample moving =
      Sample(MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD, 1);
  unsigned int tick;

  ExpectFaultAfter500Ms(MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD);
  ExpectFaultAfter500Ms(-MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD);

  for (tick = 0U; tick < 100U; ++tick) {
    assert(FeedbackWatchdog_Update(&watchdog, &below, 10U));
  }
  assert(watchdog.left_loss_ms == 0U);

  for (tick = 0U; tick < 40U; ++tick) {
    assert(FeedbackWatchdog_Update(&watchdog, &stalled, 10U));
  }
  assert(watchdog.left_loss_ms == 400U);
  assert(FeedbackWatchdog_Update(&watchdog, &moving, 10U));
  assert(watchdog.left_loss_ms == 0U);

  for (tick = 0U; tick < 16U; ++tick) {
    assert(FeedbackWatchdog_Update(&watchdog, &stalled, 30U));
  }
  assert(watchdog.left_loss_ms == 480U);
  assert(!FeedbackWatchdog_Update(&watchdog, &stalled, 30U));
  return 0;
}
