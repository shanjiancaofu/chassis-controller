#ifndef APP_DIAGNOSTICS_CRASH_CONTEXT_H
#define APP_DIAGNOSTICS_CRASH_CONTEXT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint32_t fault_id;
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
  uint32_t exc_return;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t mmfar;
  uint32_t bfar;
} CrashContext;

void CrashContext_CaptureFromException(const uint32_t *stack,
                                       uint32_t exc_return,
                                       uint32_t fault_id);
bool CrashContext_Take(CrashContext *context);

#endif
