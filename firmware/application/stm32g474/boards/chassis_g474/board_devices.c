#include "device.h"

#include "adc.h"
#include "devicetree.h"
#include "drivers/adc/power_sample_stm32_private.h"
#include "drivers/button/button_stm32_private.h"
#include "drivers/can/can_stm32_fdcan.h"
#include "drivers/display/lcd_stm32_private.h"
#include "drivers/encoder/encoder_stm32_private.h"
#include "drivers/flash/flash_stm32_qspi_private.h"
#include "drivers/gpio.h"
#include "drivers/led/led_stm32_private.h"
#include "drivers/motor/motor_stm32_private.h"
#include "drivers/rtc.h"
#include "drivers/safety/emergency_stop_stm32_private.h"
#include "drivers/sensor/icm45686_stm32_private.h"
#include "drivers/sensor/sr501_stm32_private.h"
#include "drivers/time.h"
#include "drivers/uart/uart_stm32_private.h"
#include "drivers/watchdog.h"
#include "fdcan.h"
#include "main.h"
#include "quadspi.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

static const CanStm32FdcanConfig can0_config = {
    .handle = &hfdcan2,
    .filter_capacity = DT_PROP(DT_NODELABEL(can0), standard_filter_count),
};
static CanStm32FdcanData can0_data;

DEVICE_DT_DEFINE(can0, CanStm32Fdcan_Init, &can0_data, &can0_config,
                 PRE_KERNEL_2, 50, &can_stm32_fdcan_api);

static const UartStm32Config uart0_config = {.handle = &huart1};
static UartStm32Data uart0_data;
DEVICE_DT_DEFINE(uart0, UartStm32_Init, &uart0_data, &uart0_config,
                 PRE_KERNEL_2, 60, &uart_stm32_api);

static const FlashStm32QspiConfig flash0_config = {.handle = &hqspi1};
static FlashStm32QspiData flash0_data;
DEVICE_DT_DEFINE(flash0, FlashStm32Qspi_Init, &flash0_data, &flash0_config,
                 PRE_KERNEL_2, 70, &flash_stm32_qspi_api);
DEVICE_DT_DEFINE(watchdog0, WatchdogStm32_Init, NULL, NULL, PRE_KERNEL_2, 71,
                 &watchdog_stm32_api);
DEVICE_DT_DEFINE(rtc0, RtcStm32_Init, NULL, NULL, PRE_KERNEL_2, 72,
                 &rtc_stm32_api);
DEVICE_DT_DEFINE(time0, TimeStm32_Init, NULL, NULL, PRE_KERNEL_2, 73,
                 &time_stm32_api);

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

static PowerSampleStm32Data power0_data;
DEVICE_DT_DEFINE(power0, PowerSampleStm32_Init, &power0_data, NULL,
                 PRE_KERNEL_2, 83, &power_sample_stm32_api);
static DisplayStm32Data display0_data;
DEVICE_DT_DEFINE(display0, DisplayStm32_Init, &display0_data, NULL,
                 PRE_KERNEL_2, 84, &display_stm32_api);
#if CONFIG_ICM45686
static const Icm45686Stm32Config imu0_config = {
    .spi = &hspi3,
    .cs_port = IMU_CS_GPIO_Port,
    .cs_pin = IMU_CS_Pin,
};
static Icm45686Stm32Data imu0_data;
DEVICE_DT_DEFINE(imu0, Icm45686Device_Init, &imu0_data, &imu0_config,
                 PRE_KERNEL_2, 85, &icm45686_stm32_api);
#endif

static const GpioStm32Config gpioa_config = {.port = GPIOA};
static const GpioStm32Config gpiob_config = {.port = GPIOB};
static const GpioStm32Config gpioc_config = {.port = GPIOC};
static const GpioStm32Config gpiod_config = {.port = GPIOD};
DEVICE_DT_DEFINE(gpioa, GpioStm32_Init, NULL, &gpioa_config, PRE_KERNEL_1, 20,
                 &gpio_stm32_api);
DEVICE_DT_DEFINE(gpiob, GpioStm32_Init, NULL, &gpiob_config, PRE_KERNEL_1, 21,
                 &gpio_stm32_api);
DEVICE_DT_DEFINE(gpioc, GpioStm32_Init, NULL, &gpioc_config, PRE_KERNEL_1, 22,
                 &gpio_stm32_api);
DEVICE_DT_DEFINE(gpiod, GpioStm32_Init, NULL, &gpiod_config, PRE_KERNEL_1, 23,
                 &gpio_stm32_api);

static const ButtonStm32Config buttons0_config = {
    .buttons = {GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(buttons0), gpios, 0),
                GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(buttons0), gpios, 1)},
    .display_key = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(buttons0), gpios, 2),
};
static ButtonStm32Data buttons0_data;
DEVICE_DT_DEFINE(buttons0, ButtonStm32_Init, &buttons0_data, &buttons0_config,
                 PRE_KERNEL_2, 90, &button_stm32_api);
static const LedStm32Config leds0_config = {
    .leds = {GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(leds0), gpios, 0),
             GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(leds0), gpios, 1),
             GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(leds0), gpios, 2)}};
DEVICE_DT_DEFINE(leds0, LedStm32_Init, NULL, &leds0_config, PRE_KERNEL_2, 91,
                 &led_stm32_api);
static const Sr501Stm32Config sr5010_config = {
    .input = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(sr5010), gpios, 0)};
static Sr501Stm32Data sr5010_data;
DEVICE_DT_DEFINE(sr5010, Sr501Stm32_Init, &sr5010_data, &sr5010_config,
                 PRE_KERNEL_2, 92, &sr501_stm32_api);
static const EmergencyStopStm32Config estop0_config = {
    .input = GPIO_DT_SPEC_GET_BY_IDX(DT_NODELABEL(estop0), gpios, 0)};
static EmergencyStopStm32Data estop0_data;
DEVICE_DT_DEFINE(estop0, EmergencyStopStm32_Init, &estop0_data, &estop0_config,
                 PRE_KERNEL_2, 93, &emergency_stop_stm32_api);
