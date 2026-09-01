#ifndef CRASH_FRAME_H
#define CRASH_FRAME_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CRASH_FRAME_BASIC = 0,
  CRASH_FRAME_EXTENDED_FP = 1,
} CrashFrameType;

typedef struct {
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
} CrashCoreFrame;

CrashFrameType CrashFrame_TypeFromExcReturn(uint32_t exc_return);
bool CrashFrame_DecodeCore(const uint32_t *exception_stack,
                           CrashCoreFrame *core);

#endif
