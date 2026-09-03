#include "app/chassis/wheel_speed_window.h"

#include <limits.h>
#include <stddef.h>

_Static_assert(MOTOR_CONTROL_REFERENCE_PERIOD_MS % MOTOR_CONTROL_PERIOD_MS ==
                   0U,
               "wheel speed window requires an integral period ratio");
_Static_assert(WHEEL_SPEED_WINDOW_SAMPLES > 0U,
               "wheel speed window must contain samples");

void WheelSpeedWindow_Reset(WheelSpeedWindow *window) {
  uint32_t index;

  if (window == NULL) {
    return;
  }
  for (index = 0U; index < WHEEL_SPEED_WINDOW_SAMPLES; ++index) {
    window->left_samples[index] = 0;
    window->right_samples[index] = 0;
  }
  window->left_sum = 0;
  window->right_sum = 0;
  window->next_sample = 0U;
}

bool WheelSpeedWindow_Update(WheelSpeedWindow *window, int32_t left_delta,
                             int32_t right_delta, uint32_t elapsed_ticks,
                             int32_t *left_measurement,
                             int32_t *right_measurement) {
  uint32_t tick;

  if (window == NULL || left_measurement == NULL || right_measurement == NULL ||
      elapsed_ticks == 0U || elapsed_ticks > WHEEL_SPEED_WINDOW_SAMPLES) {
    return false;
  }
  for (tick = 0U; tick < elapsed_ticks; ++tick) {
    const int32_t next_left = tick + 1U == elapsed_ticks ? left_delta : 0;
    const int32_t next_right = tick + 1U == elapsed_ticks ? right_delta : 0;

    window->left_sum -= window->left_samples[window->next_sample];
    window->right_sum -= window->right_samples[window->next_sample];
    window->left_samples[window->next_sample] = next_left;
    window->right_samples[window->next_sample] = next_right;
    window->left_sum += next_left;
    window->right_sum += next_right;
    window->next_sample =
        (window->next_sample + 1U) % WHEEL_SPEED_WINDOW_SAMPLES;
  }
  if (window->left_sum < INT32_MIN || window->left_sum > INT32_MAX ||
      window->right_sum < INT32_MIN || window->right_sum > INT32_MAX) {
    return false;
  }
  *left_measurement = (int32_t)window->left_sum;
  *right_measurement = (int32_t)window->right_sum;
  return true;
}
