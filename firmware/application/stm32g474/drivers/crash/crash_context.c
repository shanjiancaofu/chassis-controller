#include "drivers/crash/crash_context.h"

#include <string.h>

#include "stm32g4xx.h"

#define CRASH_CONTEXT_MAGIC 0x43524153U
#define EXC_RETURN_BASIC_FRAME_BIT (1UL << 4)
#define FP_EXTENDED_FRAME_WORDS 18U

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
    const uint32_t *core_frame = stack;

    if ((exc_return & EXC_RETURN_BASIC_FRAME_BIT) == 0U) {
      core_frame += FP_EXTENDED_FRAME_WORDS;
    }
    captured.r0 = core_frame[0];
    captured.r1 = core_frame[1];
    captured.r2 = core_frame[2];
    captured.r3 = core_frame[3];
    captured.r12 = core_frame[4];
    captured.lr = core_frame[5];
    captured.pc = core_frame[6];
    captured.xpsr = core_frame[7];
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
