#include "drivers/can.h"

#include <errno.h>

static const struct can_driver_api *Api(const struct device *device)
{
  return device != NULL && device_is_ready(device) ? device->api : NULL;
}

int can_start(const struct device *device, const struct can_filter *filters,
              size_t filter_count)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->start != NULL
             ? api->start(device, filters, filter_count)
             : -ENODEV;
}

int can_stop(const struct device *device)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->stop != NULL ? api->stop(device) : -ENODEV;
}

int can_send(const struct device *device, const struct can_frame *frame)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->send != NULL
             ? api->send(device, frame)
             : -ENODEV;
}

int can_recv(const struct device *device, struct can_frame *frame)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->recv != NULL ? api->recv(device, frame) : -ENODEV;
}

int can_recover(const struct device *device)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->recover != NULL ? api->recover(device) : -ENODEV;
}

int can_get_diagnostics(const struct device *device, struct can_diagnostics *diagnostics)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->get_diagnostics != NULL
             ? api->get_diagnostics(device, diagnostics)
             : -ENODEV;
}

uint32_t can_take_error_events(const struct device *device)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->take_error_events != NULL
             ? api->take_error_events(device)
             : 0U;
}

bool can_is_tx_idle(const struct device *device)
{
  const struct can_driver_api *api = Api(device);
  return api != NULL && api->is_tx_idle != NULL && api->is_tx_idle(device);
}
