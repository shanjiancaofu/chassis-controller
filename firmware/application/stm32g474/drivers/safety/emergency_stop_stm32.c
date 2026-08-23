#include "drivers/safety/emergency_stop.h"
#include "device.h"
#include "devicetree.h"
#include "drivers/motor/motor.h"

#include "drivers/gpio.h"

static EmergencyStopLatchedCallback latched_callback;
static GpioSpec estop_input;

void EmergencyStop_Init(EmergencyStopLatchedCallback callback)
{
  latched_callback = callback;
}

bool EmergencyStop_IsAsserted(void)
{
  return gpio_get(&estop_input) > 0;
}
static bool Asserted(const struct device*d){(void)d;return EmergencyStop_IsAsserted();}
static void Callback(const struct device*d,EmergencyStopLatchedCallback c){(void)d;EmergencyStop_Init(c);}
static void Interrupt(const struct device*d){(void)d;EmergencyStop_OnInterrupt();}
const EmergencyStopDriverApi emergency_stop_stm32_api={.asserted=Asserted,.set_callback=Callback,.on_interrupt=Interrupt};
int EmergencyStopStm32_Init(const struct device*d){const EmergencyStopStm32Config*c=d?d->config:NULL;if(!c)return -1;estop_input=c->input;latched_callback=NULL;return 0;}
static const EmergencyStopDriverApi *Api(const struct device*d){return device_is_ready(d)?d->api:NULL;}
bool emergency_stop_is_asserted(const struct device*d){const EmergencyStopDriverApi*a=Api(d);return a&&a->asserted&&a->asserted(d);}
void emergency_stop_set_callback(const struct device*d,EmergencyStopLatchedCallback c){const EmergencyStopDriverApi*a=Api(d);if(a&&a->set_callback)a->set_callback(d,c);}
void emergency_stop_on_interrupt(const struct device*d){const EmergencyStopDriverApi*a=Api(d);if(a&&a->on_interrupt)a->on_interrupt(d);}

void EmergencyStop_OnInterrupt(void)
{
  if (!EmergencyStop_IsAsserted()) {
    return;
  }
  motor_emergency_stop(DEVICE_DT_GET(DT_NODELABEL(drive0)));
  if (latched_callback != NULL) {
    latched_callback();
  }
}
