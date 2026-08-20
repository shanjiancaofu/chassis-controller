#include "bsp/rtc/bsp_rtc.h"

#include <stddef.h>

#include "board/board_config.h"

bool BspRtc_ReadDateTime(BspRtcDateTime *date_time)
{
  RTC_TimeTypeDef time = {0};
  RTC_DateTypeDef date = {0};

  if (date_time == NULL ||
      HAL_RTC_GetTime(&BOARD_RTC, &time, RTC_FORMAT_BIN) != HAL_OK ||
      HAL_RTC_GetDate(&BOARD_RTC, &date, RTC_FORMAT_BIN) != HAL_OK ||
      time.Hours > 23U || time.Minutes > 59U || time.Seconds > 59U ||
      date.Month < 1U || date.Month > 12U || date.Date < 1U ||
      date.Date > 31U) {
    return false;
  }
  date_time->year = date.Year;
  date_time->month = date.Month;
  date_time->date = date.Date;
  date_time->hours = time.Hours;
  date_time->minutes = time.Minutes;
  date_time->seconds = time.Seconds;
  return true;
}
