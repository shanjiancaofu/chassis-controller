#include "device.h"

#include "devicetree_generated.h"
#include "drivers/can/can_stm32_fdcan.h"
#include "drivers/uart/uart_stm32.h"
#include "drivers/motor/motor.h"
#include "drivers/encoder/encoder.h"
#include "fdcan.h"
#include "tim.h"
#include "usart.h"

static int PeripheralDevice_Init(const struct device *device)
{
  (void)device;
  return 0;
}

static const CanStm32FdcanConfig can0_config = {
    .handle = &hfdcan2,
    .filter_capacity = DT_PROP_CAN0_STANDARD_FILTER_COUNT,
};
static CanStm32FdcanData can0_data;

DEVICE_DT_DEFINE(can0, CanStm32Fdcan_Init, &can0_data, &can0_config,
                 PRE_KERNEL_2, 50, &can_stm32_fdcan_api);

DEVICE_DT_DEFINE(uart0, UartStm32_Init, NULL, NULL,
                 PRE_KERNEL_2, 60, &uart_stm32_api);

DEVICE_DT_DEFINE(flash0, PeripheralDevice_Init, NULL, NULL,
                 PRE_KERNEL_2, 70, NULL);
DEVICE_DT_DEFINE(watchdog0, PeripheralDevice_Init, NULL, NULL,
                 PRE_KERNEL_2, 71, NULL);
DEVICE_DT_DEFINE(rtc0, PeripheralDevice_Init, NULL, NULL,
                 PRE_KERNEL_2, 72, NULL);
DEVICE_DT_DEFINE(time0, PeripheralDevice_Init, NULL, NULL,
                 PRE_KERNEL_2, 73, NULL);

static const MotorStm32Config motor0_config = {
    .timer = &htim8,
    .left_positive_channel = TIM_CHANNEL_1,
    .left_negative_channel = TIM_CHANNEL_2,
    .right_positive_channel = TIM_CHANNEL_4,
    .right_negative_channel = TIM_CHANNEL_3,
};
static MotorStm32Data motor0_data;

DEVICE_DT_DEFINE(drive0, MotorStm32_Init, &motor0_data, &motor0_config,
                 PRE_KERNEL_2, 80, &motor_stm32_api);

static const EncoderStm32Config left_encoder_config = {
    .timer = &htim2,
    .direction = -1,
};
static EncoderStm32Data left_encoder_data;
static const EncoderStm32Config right_encoder_config = {
    .timer = &htim4,
    .direction = 1,
};
static EncoderStm32Data right_encoder_data;

DEVICE_DT_DEFINE(left_encoder, EncoderStm32_Init, &left_encoder_data,
                 &left_encoder_config, PRE_KERNEL_2, 81, &encoder_stm32_api);
DEVICE_DT_DEFINE(right_encoder, EncoderStm32_Init, &right_encoder_data,
                 &right_encoder_config, PRE_KERNEL_2, 82, &encoder_stm32_api);
