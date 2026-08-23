#include "drivers/uart.h"

#include <errno.h>

#include "device.h"
#include "devicetree.h"

static const struct device *DefaultDevice(void)
{
  return DEVICE_DT_GET(DT_CHOSEN(chassis_uart));
}

static const struct uart_driver_api *Api(void)
{
  const struct device *device = DefaultDevice();
  return device_is_ready(device) ? device->api : NULL;
}

int uart_start(void)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->start != NULL ? api->start(DefaultDevice()) : -ENODEV;
}

void uart_run(void)
{
  const struct uart_driver_api *api = Api();
  if (api != NULL && api->run != NULL) {
    api->run(DefaultDevice());
  }
}

size_t uart_read(uint8_t *data, size_t capacity)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->read != NULL ? api->read(DefaultDevice(), data, capacity) : 0U;
}

bool uart_write(const void *data, size_t length)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->write != NULL && api->write(DefaultDevice(), data, length);
}

bool uart_write_tracked(const void *data, size_t length, uint32_t *token)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->write_tracked != NULL &&
         api->write_tracked(DefaultDevice(), data, length, token);
}

bool uart_get_tracked_completion(uint32_t token, bool *completed, bool *success)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->get_tracked_completion != NULL &&
         api->get_tracked_completion(DefaultDevice(), token, completed, success);
}

bool uart_is_tx_idle(void)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->is_tx_idle != NULL && api->is_tx_idle(DefaultDevice());
}

uint8_t uart_get_tx_slots_available(void)
{
  const struct uart_driver_api *api = Api();
  return api != NULL && api->get_tx_slots_available != NULL
             ? api->get_tx_slots_available(DefaultDevice())
             : 0U;
}

void uart_get_diagnostics(UartDiagnostics *diagnostics)
{
  const struct uart_driver_api *api = Api();
  if (api != NULL && api->get_diagnostics != NULL) {
    api->get_diagnostics(DefaultDevice(), diagnostics);
  }
}
