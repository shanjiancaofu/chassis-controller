#include "drivers/sensor/sr501.h"

#include <stddef.h>

#include "drivers/gpio.h"

#define SR501_WARMUP_MS 60000U
#define SR501_STABLE_FILTER_MS 50U

static Sr501Status status;
static bool raw_high;
static bool stable_high;
static bool candidate_high;
static bool candidate_countable;
static uint32_t initialized_ms;
static uint32_t candidate_started_ms;
static uint32_t current_ms;
static uint32_t event_count;
static uint32_t last_motion_ms;
static GpioSpec sr501_input;

void Sr501_Init(uint32_t now_ms)
{
  raw_high = gpio_get(&sr501_input) > 0;
  stable_high = raw_high;
  candidate_high = raw_high;
  candidate_countable = false;
  status = SR501_WARMING_UP;
  initialized_ms = now_ms;
  candidate_started_ms = now_ms;
  current_ms = now_ms;
  event_count = 0U;
  last_motion_ms = 0U;
}

void Sr501_Run(uint32_t now_ms)
{
  bool previous_stable_high;

  current_ms = now_ms;
  raw_high = gpio_get(&sr501_input) > 0;

  if (raw_high != candidate_high) {
    candidate_high = raw_high;
    candidate_countable = status == SR501_READY;
    candidate_started_ms = now_ms;
  }
  if (candidate_high != stable_high &&
      now_ms - candidate_started_ms >= SR501_STABLE_FILTER_MS) {
    previous_stable_high = stable_high;
    stable_high = candidate_high;
    if (candidate_countable && !previous_stable_high && stable_high) {
      ++event_count;
      last_motion_ms = now_ms;
    }
    candidate_countable = false;
  }

  if (status == SR501_WARMING_UP &&
      now_ms - initialized_ms >= SR501_WARMUP_MS) {
    status = SR501_READY;
  }
}
static void ApiRun(const struct device*d,uint32_t n){(void)d;Sr501_Run(n);}
static void ApiSnapshot(const struct device*d,Sr501Snapshot*s){(void)d;Sr501_GetSnapshot(s);}
const Sr501DriverApi sr501_stm32_api={.run=ApiRun,.snapshot=ApiSnapshot};
int Sr501Stm32_Init(const struct device*d){const Sr501Stm32Config*c=d?d->config:NULL;if(!c)return -1;sr501_input=c->input;Sr501_Init(0);return 0;}
static const Sr501DriverApi *Api(const struct device*d){return device_is_ready(d)?d->api:NULL;}
void sr501_run(const struct device*d,uint32_t n){const Sr501DriverApi*a=Api(d);if(a&&a->run)a->run(d,n);}
void sr501_get_snapshot(const struct device*d,Sr501Snapshot*s){const Sr501DriverApi*a=Api(d);if(a&&a->snapshot)a->snapshot(d,s);}

void Sr501_GetSnapshot(Sr501Snapshot *snapshot)
{
  const uint32_t warmup_elapsed_ms = current_ms - initialized_ms;

  if (snapshot == NULL) {
    return;
  }

  snapshot->status = status;
  snapshot->raw_high = raw_high;
  snapshot->motion_detected = status == SR501_READY && stable_high;
  snapshot->event_count = event_count;
  snapshot->last_motion_ms = last_motion_ms;
  snapshot->warmup_remaining_ms =
      status == SR501_WARMING_UP && warmup_elapsed_ms < SR501_WARMUP_MS
          ? SR501_WARMUP_MS - warmup_elapsed_ms
          : 0U;
}
