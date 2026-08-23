#include "drivers/encoder/encoder_stm32_private.h"

static const EncoderStm32Config *config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static EncoderStm32Data *data(const struct device *device)
{
  return device != NULL ? device->data : NULL;
}

static int Start(const struct device *device)
{
  const EncoderStm32Config *cfg = config(device);
  EncoderStm32Data *state = data(device);
  if (cfg == NULL || state == NULL || cfg->timer == NULL ||
      HAL_TIM_Encoder_Start(cfg->timer, TIM_CHANNEL_ALL) != HAL_OK) {
    return -1;
  }
  __HAL_TIM_SET_COUNTER(cfg->timer, 0U);
  state->previous_count = 0U;
  return 0;
}

static int ReadDelta(const struct device *device, int32_t *delta)
{
  const EncoderStm32Config *cfg = config(device);
  EncoderStm32Data *state = data(device);
  uint16_t current;
  if (cfg == NULL || state == NULL || cfg->timer == NULL || delta == NULL) {
    return -1;
  }
  current = (uint16_t)__HAL_TIM_GET_COUNTER(cfg->timer);
  *delta = (int32_t)(int16_t)(current - state->previous_count) * cfg->direction;
  state->previous_count = current;
  return 0;
}

const EncoderDriverApi encoder_stm32_api = {
    .start = Start,
    .read_delta = ReadDelta,
};

int EncoderStm32_Init(const struct device *device)
{
  EncoderStm32Data *state = data(device);
  if (state == NULL) return -1;
  state->previous_count = 0U;
  return 0;
}
