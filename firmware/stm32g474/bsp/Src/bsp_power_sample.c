#include "bsp_power_sample.h"

#include <stddef.h>

#include "adc.h"
#include "project_config.h"

bool BspPowerSample_Init(void)
{
  return HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) == HAL_OK;
}

bool BspPowerSample_ReadRaw(uint16_t *raw)
{
  HAL_StatusTypeDef conversion_status;
  HAL_StatusTypeDef stop_status;

  if (raw == NULL || HAL_ADC_Start(&hadc1) != HAL_OK) {
    return false;
  }

  conversion_status =
      HAL_ADC_PollForConversion(&hadc1, MOTOR_SUPPLY_ADC_TIMEOUT_MS);
  if (conversion_status == HAL_OK) {
    *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }
  stop_status = HAL_ADC_Stop(&hadc1);
  return conversion_status == HAL_OK && stop_status == HAL_OK;
}

bool BspPowerSample_ReadMillivolts(uint32_t *vin_mv)
{
  uint16_t raw;
  uint64_t scaled;

  if (vin_mv == NULL || !BspPowerSample_ReadRaw(&raw)) {
    return false;
  }

  scaled = (uint64_t)raw * MOTOR_SUPPLY_ADC_REFERENCE_MV *
           MOTOR_SUPPLY_DIVIDER_RATIO;
  *vin_mv = (uint32_t)(scaled / MOTOR_SUPPLY_ADC_MAX);
  return true;
}
