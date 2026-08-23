#ifndef CHASSIS_RTC_H
#define CHASSIS_RTC_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef struct {
  uint8_t year;
  uint8_t month;
  uint8_t date;
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} RtcDateTime;

bool rtc_read_datetime(RtcDateTime *date_time);
typedef struct { bool (*read_datetime)(const struct device *device, RtcDateTime *date_time); } RtcDriverApi;
extern const RtcDriverApi rtc_stm32_api;
int RtcStm32_Init(const struct device *device);

#endif
