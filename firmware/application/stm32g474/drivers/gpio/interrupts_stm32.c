#include "drivers/button/button.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/display/lcd.h"
#include "boards/chassis_g474/board_config.h"

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == BOARD_BUTTON_1_GPIO_PIN || gpio_pin == BOARD_BUTTON_2_GPIO_PIN) {
    Button_OnInterrupt(gpio_pin);
  } else if (gpio_pin == BOARD_IMU_INTERRUPT_GPIO_PIN) {
    Icm45686Stm32_OnDataReadyInterrupt();
  } else if (gpio_pin == BOARD_DISPLAY_KEY_GPIO_PIN) {
    Button_OnDisplayKeyInterrupt();
  } else if (gpio_pin == BOARD_EMERGENCY_STOP_GPIO_PIN) {
    EmergencyStop_OnInterrupt();
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI2) {
    Lcd_OnSpiTxComplete();
  }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI3) {
    Icm45686Stm32_OnSpiTransferComplete();
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL) {
    return;
  }
  if (hspi->Instance == SPI2) {
    Lcd_OnSpiError();
  } else if (hspi->Instance == SPI3) {
    Icm45686Stm32_OnSpiTransferError();
  }
}
