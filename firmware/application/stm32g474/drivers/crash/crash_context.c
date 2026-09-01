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
      .frame_type = CrashFrame_TypeFromExcReturn(exc_return),
      .cfsr = SCB->CFSR,
      .hfsr = SCB->HFSR,
      .mmfar = SCB->MMFAR,
      .bfar = SCB->BFAR,
  };
  CrashCoreFrame core;
  if (CrashFrame_DecodeCore(stack, &core)) {
    captured.r0 = core.r0;
    captured.r1 = core.r1;
    captured.r2 = core.r2;
    captured.r3 = core.r3;
    captured.r12 = core.r12;
    captured.lr = core.lr;
    captured.pc = core.pc;
    captured.xpsr = core.xpsr;
  }
  persistent_context.context = captured;
  __DMB();
  persistent_context.magic = CRASH_CONTEXT_MAGIC;
  __DSB();
}

__attribute__((noreturn)) void CrashContext_TriggerHardFault(void) {
  SCB->SHCSR &= ~SCB_SHCSR_USGFAULTENA_Msk;
  __DSB();
  __asm volatile("udf #0");
  __builtin_unreachable();
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
