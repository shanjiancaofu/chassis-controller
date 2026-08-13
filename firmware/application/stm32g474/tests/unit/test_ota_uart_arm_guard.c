#ifdef OTA_UART_ARM_GUARD_HOST_TEST

#include <assert.h>

#include "communication/ota_transport/ota_uart_arm_guard.h"

int main(void)
{
  OtaUartArmGuard guard;

  OtaUartArmGuard_Init(&guard);
  OtaUartArmGuard_Arm(&guard, 100U);
  assert(!OtaUartArmGuard_ShouldTimeout(
      &guard, 30100U, 30000U, false, true));
  assert(!OtaUartArmGuard_ShouldTimeout(
      &guard, 30100U, 30000U, true, false));
  OtaUartArmGuard_EndWait(&guard);
  assert(!OtaUartArmGuard_ShouldTimeout(
      &guard, 60100U, 30000U, false, false));

  OtaUartArmGuard_Arm(&guard, 100U);
  assert(OtaUartArmGuard_ShouldTimeout(
      &guard, 30100U, 30000U, false, false));
  return 0;
}

#endif
