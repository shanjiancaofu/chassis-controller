#include "drivers/button/button_stm32_private.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#define BUTTON_DEBOUNCE_MS 20U

static ButtonStm32Data *Data(const struct device *device)
{
  return device != NULL ? (ButtonStm32Data *)device->data : NULL;
}

static void Run(const struct device *device, uint32_t now_ms)
{
  ButtonStm32Data *data = Data(device);
  uint32_t interrupts;
  if (data == NULL) {
    return;
  }
  interrupts = __atomic_exchange_n(&data->pending_interrupts, 0U,
                                    __ATOMIC_RELAXED);
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    ButtonStm32State *button = &data->buttons[index];
    if ((interrupts & (1UL << index)) != 0U) {
      button->debounce_pending = true;
      button->debounce_started_ms = now_ms;
    }
    if (button->debounce_pending &&
        now_ms - button->debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
      button->debounce_pending = false;
      if (gpio_get(&button->spec) > 0) {
        data->pressed_events |= 1UL << index;
        (void)__atomic_fetch_add(&data->pressed_counts[index], 1U,
                                 __ATOMIC_RELAXED);
      }
    }
  }
  if (__atomic_exchange_n(&data->display_key_interrupt_pending, false,
                          __ATOMIC_ACQUIRE)) {
    data->display_key_debounce_pending = true;
    data->display_key_debounce_started_ms = now_ms;
  }
  if (data->display_key_debounce_pending &&
      now_ms - data->display_key_debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
    const ButtonStm32Config *config = device->config;
    data->display_key_debounce_pending = false;
    if (gpio_get(&config->display_key) > 0) {
      data->display_key_pressed_event = true;
    }
  }
}

static bool TakePressed(const struct device *device, ButtonId button)
{
  ButtonStm32Data *data = Data(device);
  uint32_t mask;
  if (data == NULL || button >= BUTTON_COUNT) {
    return false;
  }
  mask = 1UL << (uint32_t)button;
  if ((data->pressed_events & mask) == 0U) {
    return false;
  }
  data->pressed_events &= ~mask;
  return true;
}

static bool TakeDisplayKey(const struct device *device)
{
  ButtonStm32Data *data = Data(device);
  if (data == NULL || !data->display_key_pressed_event) {
    return false;
  }
  data->display_key_pressed_event = false;
  return true;
}

static void GetSnapshot(const struct device *device, ButtonSnapshot *snapshot)
{
  const ButtonStm32Data *data = Data(device);
  if (data == NULL || snapshot == NULL) {
    return;
  }
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    snapshot->pressed[index] = gpio_get(&data->buttons[index].spec) > 0;
    snapshot->pressed_count[index] =
        __atomic_load_n(&data->pressed_counts[index], __ATOMIC_RELAXED);
  }
}

static void OnInterrupt(const struct device *device, uint16_t gpio_pin)
{
  ButtonStm32Data *data = Data(device);
  const ButtonStm32Config *config = device != NULL ? device->config : NULL;
  if (data == NULL || config == NULL) {
    return;
  }
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    if (gpio_pin == config->buttons[index].pin) {
      (void)__atomic_fetch_or(&data->pending_interrupts, 1UL << index,
                              __ATOMIC_RELAXED);
      return;
    }
  }
  if (gpio_pin == config->display_key.pin) {
    __atomic_store_n(&data->display_key_interrupt_pending, true,
                     __ATOMIC_RELEASE);
  }
}

const ButtonDriverApi button_stm32_api = {
    .run = Run,
    .take_pressed = TakePressed,
    .take_display_key = TakeDisplayKey,
    .get_snapshot = GetSnapshot,
    .on_interrupt = OnInterrupt,
};

int ButtonStm32_Init(const struct device *device)
{
  const ButtonStm32Config *config = device != NULL ? device->config : NULL;
  ButtonStm32Data *data = Data(device);
  if (config == NULL || data == NULL) {
    return -EINVAL;
  }
  memset(data, 0, sizeof(*data));
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    data->buttons[index].spec = config->buttons[index];
  }
  return 0;
}

static const ButtonDriverApi *Api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

void button_run(const struct device *device, uint32_t now_ms)
{
  const ButtonDriverApi *api = Api(device);
  if (api != NULL && api->run != NULL) {
    api->run(device, now_ms);
  }
}

bool button_take_pressed(const struct device *device, ButtonId button)
{
  const ButtonDriverApi *api = Api(device);
  return api != NULL && api->take_pressed != NULL &&
         api->take_pressed(device, button);
}

bool button_take_display_key(const struct device *device)
{
  const ButtonDriverApi *api = Api(device);
  return api != NULL && api->take_display_key != NULL &&
         api->take_display_key(device);
}

void button_get_snapshot(const struct device *device, ButtonSnapshot *snapshot)
{
  const ButtonDriverApi *api = Api(device);
  if (api != NULL && api->get_snapshot != NULL) {
    api->get_snapshot(device, snapshot);
  }
}

void button_on_interrupt(const struct device *device, uint16_t gpio_pin)
{
  const ButtonDriverApi *api = Api(device);
  if (api != NULL && api->on_interrupt != NULL) {
    api->on_interrupt(device, gpio_pin);
  }
}
