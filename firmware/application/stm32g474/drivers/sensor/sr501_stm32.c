#include "drivers/sensor/sr501_stm32_private.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#define SR501_WARMUP_MS 60000U
#define SR501_STABLE_FILTER_MS 50U

static Sr501Stm32Data *Data(const struct device *device)
{
  return device != NULL ? (Sr501Stm32Data *)device->data : NULL;
}

static void Run(const struct device *device, uint32_t now_ms)
{
  const Sr501Stm32Config *config = device != NULL ? device->config : NULL;
  Sr501Stm32Data *data = Data(device);
  bool previous_stable_high;
  if (config == NULL || data == NULL) {
    return;
  }

  data->current_ms = now_ms;
  data->raw_high = gpio_get(&config->input) > 0;
  if (data->raw_high != data->candidate_high) {
    data->candidate_high = data->raw_high;
    data->candidate_countable = data->status == SR501_READY;
    data->candidate_started_ms = now_ms;
  }
  if (data->candidate_high != data->stable_high &&
      now_ms - data->candidate_started_ms >= SR501_STABLE_FILTER_MS) {
    previous_stable_high = data->stable_high;
    data->stable_high = data->candidate_high;
    if (data->candidate_countable && !previous_stable_high &&
        data->stable_high) {
      ++data->event_count;
      data->last_motion_ms = now_ms;
    }
    data->candidate_countable = false;
  }
  if (data->status == SR501_WARMING_UP &&
      now_ms - data->initialized_ms >= SR501_WARMUP_MS) {
    data->status = SR501_READY;
  }
}

static void GetSnapshot(const struct device *device, Sr501Snapshot *snapshot)
{
  const Sr501Stm32Data *data = Data(device);
  uint32_t warmup_elapsed_ms;
  if (data == NULL || snapshot == NULL) {
    return;
  }
  warmup_elapsed_ms = data->current_ms - data->initialized_ms;
  snapshot->status = data->status;
  snapshot->raw_high = data->raw_high;
  snapshot->motion_detected =
      data->status == SR501_READY && data->stable_high;
  snapshot->event_count = data->event_count;
  snapshot->last_motion_ms = data->last_motion_ms;
  snapshot->warmup_remaining_ms =
      data->status == SR501_WARMING_UP && warmup_elapsed_ms < SR501_WARMUP_MS
          ? SR501_WARMUP_MS - warmup_elapsed_ms
          : 0U;
}

const Sr501DriverApi sr501_stm32_api = {
    .run = Run,
    .snapshot = GetSnapshot,
};

int Sr501Stm32_Init(const struct device *device)
{
  const Sr501Stm32Config *config = device != NULL ? device->config : NULL;
  Sr501Stm32Data *data = Data(device);
  if (config == NULL || data == NULL) {
    return -EINVAL;
  }
  memset(data, 0, sizeof(*data));
  data->raw_high = gpio_get(&config->input) > 0;
  data->stable_high = data->raw_high;
  data->candidate_high = data->raw_high;
  data->status = SR501_WARMING_UP;
  return 0;
}

static const Sr501DriverApi *Api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

void sr501_run(const struct device *device, uint32_t now_ms)
{
  const Sr501DriverApi *api = Api(device);
  if (api != NULL && api->run != NULL) {
    api->run(device, now_ms);
  }
}

void sr501_get_snapshot(const struct device *device, Sr501Snapshot *snapshot)
{
  const Sr501DriverApi *api = Api(device);
  if (api != NULL && api->snapshot != NULL) {
    api->snapshot(device, snapshot);
  }
}
