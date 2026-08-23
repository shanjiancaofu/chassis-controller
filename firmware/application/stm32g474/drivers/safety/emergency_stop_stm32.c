#include "drivers/safety/emergency_stop_stm32_private.h"

#include <errno.h>

#include "devicetree.h"
#include "drivers/motor/motor.h"

static const EmergencyStopStm32Config *Config(const struct device *device)
{
  return device != NULL ? device->config : NULL;
}

static EmergencyStopStm32Data *Data(const struct device *device)
{
  return device != NULL ? (EmergencyStopStm32Data *)device->data : NULL;
}

static bool Asserted(const struct device *device)
{
  const EmergencyStopStm32Config *config = Config(device);
  return config != NULL && gpio_get(&config->input) > 0;
}

static void SetCallback(const struct device *device,
                        EmergencyStopLatchedCallback callback)
{
  EmergencyStopStm32Data *data = Data(device);
  if (data != NULL) {
    data->latched_callback = callback;
  }
}

static void OnInterrupt(const struct device *device)
{
  const EmergencyStopStm32Data *data = Data(device);
  if (!Asserted(device)) {
    return;
  }
  motor_emergency_stop(DEVICE_DT_GET(DT_NODELABEL(drive0)));
  if (data != NULL && data->latched_callback != NULL) {
    data->latched_callback();
  }
}

const EmergencyStopDriverApi emergency_stop_stm32_api = {
    .asserted = Asserted,
    .set_callback = SetCallback,
    .on_interrupt = OnInterrupt,
};

int EmergencyStopStm32_Init(const struct device *device)
{
  EmergencyStopStm32Data *data = Data(device);
  if (Config(device) == NULL || data == NULL) {
    return -EINVAL;
  }
  data->latched_callback = NULL;
  return 0;
}

static const EmergencyStopDriverApi *Api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

bool emergency_stop_is_asserted(const struct device *device)
{
  const EmergencyStopDriverApi *api = Api(device);
  return api != NULL && api->asserted != NULL && api->asserted(device);
}

void emergency_stop_set_callback(const struct device *device,
                                 EmergencyStopLatchedCallback callback)
{
  const EmergencyStopDriverApi *api = Api(device);
  if (api != NULL && api->set_callback != NULL) {
    api->set_callback(device, callback);
  }
}

void emergency_stop_on_interrupt(const struct device *device)
{
  const EmergencyStopDriverApi *api = Api(device);
  if (api != NULL && api->on_interrupt != NULL) {
    api->on_interrupt(device);
  }
}
