#include <assert.h>
#include <stddef.h>

#include "drivers/crash/crash_frame.h"

#define EXC_RETURN_BASIC_PSP 0xFFFFFFFDU
#define EXC_RETURN_EXTENDED_FP_PSP 0xFFFFFFEDU

static void ExpectCoreRegisters(const CrashCoreFrame *core) {
  assert(core->r0 == 0x10U);
  assert(core->r1 == 0x11U);
  assert(core->r2 == 0x12U);
  assert(core->r3 == 0x13U);
  assert(core->r12 == 0x1CU);
  assert(core->lr == 0x08001235U);
  assert(core->pc == 0x08004567U);
  assert(core->xpsr == 0x01000000U);
}

int main(void) {
  const uint32_t core_words[8] = {
      0x10U, 0x11U, 0x12U, 0x13U, 0x1CU, 0x08001235U, 0x08004567U, 0x01000000U,
  };
  uint32_t extended_words[26] = {0};
  CrashCoreFrame core = {0};
  size_t index;

  assert(CrashFrame_TypeFromExcReturn(EXC_RETURN_BASIC_PSP) ==
         CRASH_FRAME_BASIC);
  assert(CrashFrame_DecodeCore(core_words, &core));
  ExpectCoreRegisters(&core);

  for (index = 0U; index < 8U; ++index) {
    extended_words[index] = core_words[index];
  }
  for (index = 8U; index < 26U; ++index) {
    extended_words[index] = 0xA5A5A5A5U;
  }
  assert(CrashFrame_TypeFromExcReturn(EXC_RETURN_EXTENDED_FP_PSP) ==
         CRASH_FRAME_EXTENDED_FP);
  assert(CrashFrame_DecodeCore(extended_words, &core));
  ExpectCoreRegisters(&core);

  assert(!CrashFrame_DecodeCore(NULL, &core));
  assert(!CrashFrame_DecodeCore(core_words, NULL));
  return 0;
}
