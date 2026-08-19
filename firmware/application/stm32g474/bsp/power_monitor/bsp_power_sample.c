#include "bsp/power_monitor/bsp_power_sample.h"

#include <stddef.h>

#include "board/board_config.h"

static volatile uint32_t latest_millivolts;
static volatile uint32_t latest_sample_timestamp_ms;
static volatile bool latest_valid;

bool BspPowerSample_Init(void)
{
  latest_millivolts = 0U;
  latest_sample_timestamp_ms = 0U;
  latest_valid = false;
  return HAL_ADCEx_Calibration_Start(&BOARD_POWER_ADC, ADC_SINGLE_ENDED) ==
         HAL_OK;
}

bool BspPowerSample_ReadRaw(uint16_t *raw)
{
  HAL_StatusTypeDef conversion_status;
  HAL_StatusTypeDef stop_status;

  if (raw == NULL || HAL_ADC_Start(&BOARD_POWER_ADC) != HAL_OK) {
    return false;
  }

  conversion_status =
      HAL_ADC_PollForConversion(&BOARD_POWER_ADC,
                                BOARD_POWER_ADC_TIMEOUT_MS);
  if (conversion_status == HAL_OK) {
    *raw = (uint16_t)HAL_ADC_GetValue(&BOARD_POWER_ADC);
  }
  stop_status = HAL_ADC_Stop(&BOARD_POWER_ADC);
  return conversion_status == HAL_OK && stop_status == HAL_OK;
}

bool BspPowerSample_ReadMillivolts(uint32_t *vin_mv)
{
  uint16_t raw;
  uint64_t scaled;

  if (vin_mv == NULL || !BspPowerSample_ReadRaw(&raw)) {
    return false;
  }

  scaled = (uint64_t)raw * BOARD_POWER_ADC_REFERENCE_MV *
           BOARD_POWER_DIVIDER_RATIO;
  *vin_mv = (uint32_t)(scaled / BOARD_POWER_ADC_MAX);
  latest_millivolts = *vin_mv;
  latest_sample_timestamp_ms = HAL_GetTick();
  latest_valid = true;
  return true;
}

bool BspPowerSample_GetLatestMillivolts(uint32_t *vin_mv)
{
  if (vin_mv == NULL || !latest_valid) {
    return false;
  }
  *vin_mv = latest_millivolts;
  return true;
}

void BspPowerSample_GetSnapshot(uint32_t now_ms,
                                BspPowerSampleSnapshot *snapshot)
{
  if (snapshot != NULL) {
    snapshot->valid = latest_valid;
    snapshot->millivolts = latest_millivolts;
    snapshot->sample_timestamp_ms = latest_sample_timestamp_ms;
    snapshot->sample_age_ms =
        snapshot->valid ? now_ms - snapshot->sample_timestamp_ms : 0U;
  }
}
