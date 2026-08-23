#include "drivers/led/led.h"

#include "drivers/gpio.h"

static const LedStm32Config *led_config;

void Led_Set(Led led, bool on)
{
  if (led <= LED_RED) {
    (void)gpio_set(&led_config->leds[led], on);
  }
}

void Led_Toggle(Led led)
{
  if (led <= LED_RED) {
    (void)gpio_toggle(&led_config->leds[led]);
  }
}

static void Set(const struct device*d,Led l,bool on){(void)d;Led_Set(l,on);}
static void Toggle(const struct device*d,Led l){(void)d;Led_Toggle(l);}
const LedDriverApi led_stm32_api={.set=Set,.toggle=Toggle};
int LedStm32_Init(const struct device*d){led_config=d?d->config:NULL;if(!led_config)return -1;for(int i=0;i<3;i++)Led_Set((Led)i,false);return 0;}
static const LedDriverApi *Api(const struct device*d){return device_is_ready(d)?d->api:NULL;}
void led_set(const struct device*d,Led l,bool on){const LedDriverApi*a=Api(d);if(a&&a->set)a->set(d,l,on);}
void led_toggle(const struct device*d,Led l){const LedDriverApi*a=Api(d);if(a&&a->toggle)a->toggle(d,l);}
