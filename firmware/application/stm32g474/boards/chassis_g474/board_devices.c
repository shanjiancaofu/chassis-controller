#include "device.h"

#include "devicetree_generated.h"
#include "drivers/can/can_stm32_fdcan.h"
#include "drivers/uart/uart_stm32.h"
#include "fdcan.h"
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
