#include "drivers/button/button.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/display/lcd.h"
#include "main.h"

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == BUTTON_1_Pin || gpio_pin == BUTTON_2_Pin) {
    Button_OnInterrupt(gpio_pin);
  } else if (gpio_pin == IMU_INT1_Pin) {
    Icm45686Stm32_OnDataReadyInterrupt();
  } else if (gpio_pin == KEY_Pin) {
    Button_OnDisplayKeyInterrupt();
  } else if (gpio_pin == E_STOP_Pin) {
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
