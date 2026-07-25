#include "bsp/reset/bsp_reset.h"

#include "rtc.h"

#define BSP_RESET_BACKUP_REGISTER_COUNT 32U

bool BspReset_WasIndependentWatchdog(void)
{
  return __HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET;
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
