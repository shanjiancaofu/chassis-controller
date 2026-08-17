#include "bsp/reset/bsp_reset.h"

#include "rtc.h"

#define BSP_RESET_BACKUP_REGISTER_COUNT 32U

bool BspReset_WasIndependentWatchdog(void)
{
  return __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
}

uint32_t BspReset_GetCauseFlags(void)
{
  uint32_t flags = 0U;

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET) {
    flags |= BSP_RESET_CAUSE_PIN;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST) != RESET) {
    flags |= BSP_RESET_CAUSE_BOR;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET) {
    flags |= BSP_RESET_CAUSE_SOFTWARE;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET) {
    flags |= BSP_RESET_CAUSE_IWDG;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST) != RESET) {
    flags |= BSP_RESET_CAUSE_WWDG;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST) != RESET) {
    flags |= BSP_RESET_CAUSE_LOW_POWER;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_OBLRST) != RESET) {
    flags |= BSP_RESET_CAUSE_OPTION_BYTE;
  }
  return flags;
}

void BspReset_ClearCauseFlags(void)
{
  __HAL_RCC_CLEAR_RESET_FLAGS();
}

uint32_t BspReset_ReadBackupRegister(uint32_t index)
{
  if (index >= BSP_RESET_BACKUP_REGISTER_COUNT) {
    return 0U;
  }
  return HAL_RTCEx_BKUPRead(&hrtc, index);
}

void BspReset_WriteBackupRegister(uint32_t index, uint32_t value)
{
  if (index < BSP_RESET_BACKUP_REGISTER_COUNT) {
    HAL_RTCEx_BKUPWrite(&hrtc, index, value);
  }
}
