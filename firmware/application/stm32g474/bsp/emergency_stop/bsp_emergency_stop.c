#include "bsp/emergency_stop/bsp_emergency_stop.h"

#include "bsp/motor/bsp_motor.h"
#include "board/board_config.h"

static BspEmergencyStopLatchedCallback latched_callback;

void BspEmergencyStop_Init(BspEmergencyStopLatchedCallback callback)
{
  latched_callback = callback;
}

bool BspEmergencyStop_IsAsserted(void)
{
  return HAL_GPIO_ReadPin(BOARD_EMERGENCY_STOP_GPIO_PORT, BOARD_EMERGENCY_STOP_GPIO_PIN) == GPIO_PIN_RESET;
}

void BspEmergencyStop_OnInterrupt(void)
{
  if (!BspEmergencyStop_IsAsserted()) {
    return;
  }
  BspMotor_EmergencyStop();
  if (latched_callback != NULL) {
    latched_callback();
  }
}
