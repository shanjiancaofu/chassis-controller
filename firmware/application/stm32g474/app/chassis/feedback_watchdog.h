#ifndef FEEDBACK_WATCHDOG_H
#define FEEDBACK_WATCHDOG_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t left_loss_ms;
  uint32_t right_loss_ms;
} FeedbackWatchdog;

typedef struct {
  int32_t left_target;
  int32_t right_target;
  int32_t left_measurement;
  int32_t right_measurement;
  int16_t left_output;
  int16_t right_output;
} FeedbackWatchdogSample;

void FeedbackWatchdog_Reset(FeedbackWatchdog *watchdog);
bool FeedbackWatchdog_Update(FeedbackWatchdog *watchdog,
                             const FeedbackWatchdogSample *sample,
                             uint32_t elapsed_ms);

#endif
