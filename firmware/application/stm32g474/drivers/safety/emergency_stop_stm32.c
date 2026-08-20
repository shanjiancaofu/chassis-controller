#include "drivers/safety/emergency_stop.h"

#include "drivers/motor/motor.h"
#include "boards/chassis_g474/board_config.h"

static EmergencyStopLatchedCallback latched_callback;

void EmergencyStop_Init(EmergencyStopLatchedCallback callback)
{
  latched_callback = callback;
}

bool EmergencyStop_IsAsserted(void)
{
  return HAL_GPIO_ReadPin(BOARD_EMERGENCY_STOP_GPIO_PORT, BOARD_EMERGENCY_STOP_GPIO_PIN) == GPIO_PIN_RESET;
}

void EmergencyStop_OnInterrupt(void)
{
  if (!EmergencyStop_IsAsserted()) {
    return;
  }
  Motor_EmergencyStop();
  if (latched_callback != NULL) {
    latched_callback();
  }
}
