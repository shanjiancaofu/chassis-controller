#include "drivers/adc/power_sample.h"

#include <stddef.h>

#include "adc.h"

#define POWER_ADC_REFERENCE_MV 3300ULL
#define POWER_DIVIDER_RATIO 11ULL
#define POWER_ADC_MAX 4095ULL
#define POWER_ADC_TIMEOUT_MS 2U

typedef struct {
  volatile uint32_t latest_millivolts;
  volatile uint32_t latest_sample_timestamp_ms;
  volatile bool latest_valid;
} PowerSampleStm32Data;

static PowerSampleStm32Data power_data;

static int Init(const struct device *device)
{
  (void)device;
  power_data.latest_millivolts = 0U;
  power_data.latest_sample_timestamp_ms = 0U;
  power_data.latest_valid = false;
  return HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) ==
         HAL_OK ? 0 : -1;
}

static int ReadRaw(const struct device *device, uint16_t *raw)
{
  HAL_StatusTypeDef conversion_status;
  HAL_StatusTypeDef stop_status;

  if (raw == NULL || HAL_ADC_Start(&hadc1) != HAL_OK) {
    return false;
  }

  conversion_status =
      HAL_ADC_PollForConversion(&hadc1,
                                POWER_ADC_TIMEOUT_MS);
  if (conversion_status == HAL_OK) {
    *raw = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }
  stop_status = HAL_ADC_Stop(&hadc1);
  return conversion_status == HAL_OK && stop_status == HAL_OK ? 0 : -1;
}

static int ReadMillivolts(const struct device *device, uint32_t *vin_mv)
{
  uint16_t raw;
  uint64_t scaled;

  if (vin_mv == NULL || ReadRaw(device, &raw) < 0) {
    return -1;
  }

  scaled = (uint64_t)raw * POWER_ADC_REFERENCE_MV *
           POWER_DIVIDER_RATIO;
  *vin_mv = (uint32_t)(scaled / POWER_ADC_MAX);
  power_data.latest_millivolts = *vin_mv;
  power_data.latest_sample_timestamp_ms = HAL_GetTick();
  power_data.latest_valid = true;
  return 0;
}

static int GetLatest(const struct device *device, uint32_t *vin_mv)
{
  (void)device;
  if (vin_mv == NULL || !power_data.latest_valid) {
    return -1;
  }
  *vin_mv = power_data.latest_millivolts;
  return 0;
}

static void GetSnapshot(const struct device *device, uint32_t now_ms,
                        PowerSampleSnapshot *snapshot)
{
  (void)device;
  if (snapshot != NULL) {
    snapshot->valid = power_data.latest_valid;
    snapshot->millivolts = power_data.latest_millivolts;
    snapshot->sample_timestamp_ms = power_data.latest_sample_timestamp_ms;
    snapshot->sample_age_ms =
        snapshot->valid ? now_ms - snapshot->sample_timestamp_ms : 0U;
  }
}

const PowerSampleDriverApi power_sample_stm32_api = {
    .read_raw = ReadRaw,
    .read_millivolts = ReadMillivolts,
    .get_latest_millivolts = GetLatest,
    .get_snapshot = GetSnapshot,
};

int PowerSampleStm32_Init(const struct device *device)
{
  return Init(device);
}
