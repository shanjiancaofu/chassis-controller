#include "drivers/button/button.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/sensor/icm45686.h"
#include "drivers/display/lcd.h"
#include "main.h"
#include "devicetree.h"

void HAL_GPIO_EXTI_Callback(uint16_t gpio_pin)
{
  if (gpio_pin == BUTTON_1_Pin || gpio_pin == BUTTON_2_Pin) {
    button_on_interrupt(DEVICE_DT_GET(DT_CHOSEN(chassis_buttons)), gpio_pin);
  } else if (gpio_pin == IMU_INT1_Pin) {
    sensor_on_data_ready(DEVICE_DT_GET(DT_NODELABEL(imu0)));
  } else if (gpio_pin == KEY_Pin) {
    button_on_interrupt(DEVICE_DT_GET(DT_CHOSEN(chassis_buttons)), gpio_pin);
  } else if (gpio_pin == E_STOP_Pin) {
    emergency_stop_on_interrupt(DEVICE_DT_GET(DT_CHOSEN(chassis_estop)));
  }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI2) {
    display_on_tx_complete(DEVICE_DT_GET(DT_CHOSEN(chassis_display)));
  }
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi != NULL && hspi->Instance == SPI3) {
    sensor_on_transfer_complete(DEVICE_DT_GET(DT_NODELABEL(imu0)));
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL) {
    return;
  }
  if (hspi->Instance == SPI2) {
    display_on_error(DEVICE_DT_GET(DT_CHOSEN(chassis_display)));
  } else if (hspi->Instance == SPI3) {
    sensor_on_transfer_error(DEVICE_DT_GET(DT_NODELABEL(imu0)));
  }
}
