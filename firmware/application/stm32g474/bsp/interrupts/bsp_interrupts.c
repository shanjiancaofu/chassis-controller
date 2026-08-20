#include "bsp/button/bsp_button.h"
#include "bsp/emergency_stop/bsp_emergency_stop.h"
#include "bsp/imu/bsp_icm45686.h"
#include "bsp/lcd/bsp_lcd.h"
#include "board/board_config.h"

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == BOARD_BUTTON_1_GPIO_PIN || gpio_pin == BOARD_BUTTON_2_GPIO_PIN) {
    BspButton_OnInterrupt(gpio_pin);
  } else if (gpio_pin == BOARD_IMU_INTERRUPT_GPIO_PIN) {
    BspIcm45686_OnDataReadyInterrupt();
  } else if (gpio_pin == BOARD_DISPLAY_KEY_GPIO_PIN) {
    BspButton_OnDisplayKeyInterrupt();
  } else if (gpio_pin == BOARD_EMERGENCY_STOP_GPIO_PIN) {
    BspEmergencyStop_OnInterrupt();
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI2) {
    BspLcd_OnSpiTxComplete();
  }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI3) {
    BspIcm45686_OnSpiTransferComplete();
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL) {
    return;
  }
  if (hspi->Instance == SPI2) {
    BspLcd_OnSpiError();
  } else if (hspi->Instance == SPI3) {
    BspIcm45686_OnSpiTransferError();
  }
}
