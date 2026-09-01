#include "drivers/crash/crash_frame.h"

#include <stddef.h>

#define EXC_RETURN_BASIC_FRAME_BIT (1UL << 4)

CrashFrameType CrashFrame_TypeFromExcReturn(uint32_t exc_return) {
  return (exc_return & EXC_RETURN_BASIC_FRAME_BIT) != 0U
             ? CRASH_FRAME_BASIC
             : CRASH_FRAME_EXTENDED_FP;
}

bool CrashFrame_DecodeCore(const uint32_t *exception_stack,
                           CrashCoreFrame *core) {
  if (exception_stack == NULL || core == NULL) {
    return false;
  }
  *core = (CrashCoreFrame){
      .r0 = exception_stack[0],
      .r1 = exception_stack[1],
      .r2 = exception_stack[2],
      .r3 = exception_stack[3],
      .r12 = exception_stack[4],
      .lr = exception_stack[5],
      .pc = exception_stack[6],
      .xpsr = exception_stack[7],
  };
  return true;
}
