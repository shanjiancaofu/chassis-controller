#include "bsp/sr501/bsp_sr501.h"

#include <stddef.h>

#include "board/board_config.h"

#define SR501_WARMUP_MS 60000U
#define SR501_STABLE_FILTER_MS 50U

static BspSr501Status status;
static bool raw_high;
static bool stable_high;
static bool candidate_high;
static bool candidate_countable;
static uint32_t initialized_ms;
static uint32_t candidate_started_ms;
static uint32_t current_ms;
static uint32_t event_count;
static uint32_t last_motion_ms;

void BspSr501_Init(uint32_t now_ms)
{
  raw_high = HAL_GPIO_ReadPin(BOARD_SR501_GPIO_PORT,
                             BOARD_SR501_GPIO_PIN) == GPIO_PIN_SET;
  stable_high = raw_high;
  candidate_high = raw_high;
  candidate_countable = false;
  status = BSP_SR501_WARMING_UP;
  initialized_ms = now_ms;
  candidate_started_ms = now_ms;
  current_ms = now_ms;
  event_count = 0U;
  last_motion_ms = 0U;
}

void BspSr501_Run(uint32_t now_ms)
{
  bool previous_stable_high;

  current_ms = now_ms;
  raw_high = HAL_GPIO_ReadPin(BOARD_SR501_GPIO_PORT,
                             BOARD_SR501_GPIO_PIN) == GPIO_PIN_SET;

  if (raw_high != candidate_high) {
    candidate_high = raw_high;
    candidate_countable = status == BSP_SR501_READY;
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

  if (status == BSP_SR501_WARMING_UP &&
      now_ms - initialized_ms >= SR501_WARMUP_MS) {
    status = BSP_SR501_READY;
  }
}

void BspSr501_GetSnapshot(BspSr501Snapshot *snapshot)
{
  const uint32_t warmup_elapsed_ms = current_ms - initialized_ms;

  if (snapshot == NULL) {
    return;
  }

  snapshot->status = status;
  snapshot->raw_high = raw_high;
  snapshot->motion_detected = status == BSP_SR501_READY && stable_high;
  snapshot->event_count = event_count;
  snapshot->last_motion_ms = last_motion_ms;
  snapshot->warmup_remaining_ms =
      status == BSP_SR501_WARMING_UP && warmup_elapsed_ms < SR501_WARMUP_MS
          ? SR501_WARMUP_MS - warmup_elapsed_ms
          : 0U;
}
