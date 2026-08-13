#ifndef OTA_UART_ARM_GUARD_H
#define OTA_UART_ARM_GUARD_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  uint32_t armed_ms;
  bool waiting_for_begin;
} OtaUartArmGuard;

static inline void OtaUartArmGuard_Init(OtaUartArmGuard *guard)
{
  guard->armed_ms = 0U;
  guard->waiting_for_begin = false;
}

static inline void OtaUartArmGuard_Arm(OtaUartArmGuard *guard,
                                      uint32_t now_ms)
{
  guard->armed_ms = now_ms;
  guard->waiting_for_begin = true;
}

static inline void OtaUartArmGuard_EndWait(OtaUartArmGuard *guard)
{
  guard->waiting_for_begin = false;
}

static inline bool OtaUartArmGuard_ShouldTimeout(
    const OtaUartArmGuard *guard, uint32_t now_ms,
    uint32_t timeout_ms, bool session_active, bool response_waiting)
{
  return guard->waiting_for_begin && !session_active &&
         !response_waiting && now_ms - guard->armed_ms >= timeout_ms;
}

#endif
