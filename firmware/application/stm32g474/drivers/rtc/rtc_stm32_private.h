#ifndef CHASSIS_RTC_STM32_PRIVATE_H
#define CHASSIS_RTC_STM32_PRIVATE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RtcStm32DateTime;

bool RtcStm32ReadDateTime(RtcStm32DateTime *date_time);

#endif
