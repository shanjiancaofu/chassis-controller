#include "drivers/gpio.h"

static const GpioStm32Config *config(const struct device *device)
{ return device != NULL ? device->config : NULL; }
static int Get(const struct device *device,uint16_t pin)
{ const GpioStm32Config *c=config(device); return c&&c->port ? HAL_GPIO_ReadPin(c->port,pin)==GPIO_PIN_SET : -1; }
static int Set(const struct device *device,uint16_t pin,bool value)
{ const GpioStm32Config *c=config(device); if(!c||!c->port)return -1; HAL_GPIO_WritePin(c->port,pin,value?GPIO_PIN_SET:GPIO_PIN_RESET); return 0; }
static int Toggle(const struct device *device,uint16_t pin)
{ const GpioStm32Config *c=config(device); if(!c||!c->port)return -1; HAL_GPIO_TogglePin(c->port,pin); return 0; }
const GpioDriverApi gpio_stm32_api={.get=Get,.set=Set,.toggle=Toggle};
int GpioStm32_Init(const struct device *device){const GpioStm32Config*c=config(device);return c&&c->port?0:-1;}
