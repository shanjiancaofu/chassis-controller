#include "device.h"

#include "devicetree_generated.h"
#include "drivers/can/can_stm32_fdcan.h"
#include "drivers/uart/uart_stm32.h"
#include "fdcan.h"
#include "usart.h"

static const CanStm32FdcanConfig can0_config = {
    .handle = &hfdcan2,
    .filter_capacity = DT_PROP_CAN0_STANDARD_FILTER_COUNT,
};
static CanStm32FdcanData can0_data;

DEVICE_DT_DEFINE(can0, CanStm32Fdcan_Init, &can0_data, &can0_config,
                 PRE_KERNEL_2, 50, &can_stm32_fdcan_api);

DEVICE_DT_DEFINE(uart0, UartStm32_Init, NULL, NULL,
                 PRE_KERNEL_2, 60, &uart_stm32_api);
