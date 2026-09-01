#include "drivers/crash/crash_context.h"

#include <string.h>

#include "stm32g4xx.h"

#define CRASH_CONTEXT_MAGIC 0x43524153U

typedef struct {
  uint32_t magic;
  CrashContext context;
} PersistentCrashContext;

__attribute__((section(".noinit"), used)) static volatile PersistentCrashContext
    persistent_context;

void CrashContext_CaptureFromException(const uint32_t *stack,
                                       uint32_t exc_return,
                                       uint32_t fault_id) {
  CrashContext captured = {
      .fault_id = fault_id,
      .exc_return = exc_return,
      .cfsr = SCB->CFSR,
      .hfsr = SCB->HFSR,
      .mmfar = SCB->MMFAR,
      .bfar = SCB->BFAR,
  };
  if (stack != NULL) {
    captured.r0 = stack[0];
    captured.r1 = stack[1];
    captured.r2 = stack[2];
    captured.r3 = stack[3];
    captured.r12 = stack[4];
    captured.lr = stack[5];
    captured.pc = stack[6];
    captured.xpsr = stack[7];
  }
  persistent_context.context = captured;
  __DMB();
  persistent_context.magic = CRASH_CONTEXT_MAGIC;
  __DSB();
}

bool CrashContext_Take(CrashContext *context) {
  if (context == NULL || persistent_context.magic != CRASH_CONTEXT_MAGIC) {
    return false;
  }
  *context = persistent_context.context;
  persistent_context.magic = 0U;
  __DMB();
  return true;
}
