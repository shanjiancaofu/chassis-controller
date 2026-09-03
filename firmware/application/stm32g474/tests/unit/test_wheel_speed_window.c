#ifdef WHEEL_SPEED_WINDOW_HOST_TEST

#include <assert.h>
#include <stddef.h>

#include "app/chassis/wheel_speed_window.h"

int main(void) {
  WheelSpeedWindow window = {0};
  int32_t left;
  int32_t right;
  unsigned int tick;

  assert(!WheelSpeedWindow_Update(NULL, 0, 0, 1U, &left, &right));
  WheelSpeedWindow_Reset(&window);
  for (tick = 0U; tick < WHEEL_SPEED_WINDOW_SAMPLES; ++tick) {
    assert(WheelSpeedWindow_Update(&window, 3, -2, 1U, &left, &right));
  }
  assert(left == 30);
  assert(right == -20);

  assert(WheelSpeedWindow_Update(&window, 12, -8, 4U, &left, &right));
  assert(left == 30);
  assert(right == -20);

  WheelSpeedWindow_Reset(&window);
  assert(WheelSpeedWindow_Update(&window, 5, 7, 1U, &left, &right));
  assert(left == 5);
  assert(right == 7);
  return 0;
}

#endif
