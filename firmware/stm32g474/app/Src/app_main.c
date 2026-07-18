#include "app_main.h"

#include "fdcan_driver.h"
#include "main.h"
#include "usart.h"

void App_Init(void)
{
  static const uint8_t startup_message[] = "chassis-controller started\r\n";

  HAL_UART_Transmit(&huart1, startup_message, sizeof(startup_message) - 1U,
                    HAL_MAX_DELAY);
  if (FdcanDriver_Init() != HAL_OK) {
    Error_Handler();
  }
}

void App_Run(void)
{
  static uint32_t last_heartbeat_ms;
  const uint32_t now_ms = HAL_GetTick();
  const FdcanLoopbackStatus loopback_status =
      FdcanDriver_GetLoopbackStatus();

  if (now_ms - last_heartbeat_ms >= 500U) {
    last_heartbeat_ms = now_ms;
    HAL_GPIO_TogglePin(LED_B_GPIO_Port, LED_B_Pin);
  }

  if (loopback_status == FDCAN_LOOPBACK_PASSED) {
    HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_RESET);
  } else if (loopback_status == FDCAN_LOOPBACK_FAILED) {
    HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_RESET);
  }
}
