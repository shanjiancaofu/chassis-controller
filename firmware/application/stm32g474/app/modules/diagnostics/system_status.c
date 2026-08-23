#include "app/modules/diagnostics/system_status.h"

#include <stddef.h>

static SystemStatusSnapshot system_status_buffers[2];
static volatile uint32_t active_buffer;

void SystemStatus_Init(void)
{
  system_status_buffers[0] = (SystemStatusSnapshot){0};
  system_status_buffers[1] = (SystemStatusSnapshot){0};
  __atomic_store_n(&active_buffer, 0U, __ATOMIC_RELAXED);
}

void SystemStatus_Update(const SystemStatusSnapshot *snapshot)
{
  if (snapshot != NULL) {
    const uint32_t current =
        __atomic_load_n(&active_buffer, __ATOMIC_RELAXED);
    const uint32_t next = current ^ 1U;

    system_status_buffers[next] = *snapshot;
    __atomic_store_n(&active_buffer, next, __ATOMIC_RELEASE);
  }
}

void SystemStatus_GetSnapshot(SystemStatusSnapshot *snapshot)
{
  if (snapshot != NULL) {
    uint32_t first;
    uint32_t second;

    do {
      first = __atomic_load_n(&active_buffer, __ATOMIC_ACQUIRE);
      *snapshot = system_status_buffers[first];
      second = __atomic_load_n(&active_buffer, __ATOMIC_ACQUIRE);
    } while (first != second);
  }
}
