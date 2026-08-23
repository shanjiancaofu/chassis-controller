#include "drivers/button/button.h"

#include <stddef.h>

#include "drivers/gpio.h"

#define BUTTON_DEBOUNCE_MS 20U

typedef struct {
  GpioSpec spec;
  bool debounce_pending;
  uint32_t debounce_started_ms;
} ButtonState;

static ButtonState buttons[BUTTON_COUNT];
static GpioSpec display_key;
static volatile uint32_t pending_interrupts;
static volatile bool display_key_interrupt_pending;
static bool display_key_debounce_pending;
static bool display_key_pressed_event;
static uint32_t display_key_debounce_started_ms;
static uint32_t pressed_events;
static uint32_t pressed_counts[BUTTON_COUNT];

void Button_Init(void)
{
  pending_interrupts = 0U;
  display_key_interrupt_pending = false;
  display_key_debounce_pending = false;
  display_key_pressed_event = false;
  display_key_debounce_started_ms = 0U;
  pressed_events = 0U;
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    buttons[index].debounce_pending = false;
    buttons[index].debounce_started_ms = 0U;
    pressed_counts[index] = 0U;
  }
}

void Button_OnInterrupt(uint16_t gpio_pin)
{
  if (gpio_pin == buttons[BUTTON_1].spec.pin) {
    (void)__atomic_fetch_or(&pending_interrupts, 1UL << BUTTON_1,
                            __ATOMIC_RELAXED);
  } else if (gpio_pin == buttons[BUTTON_2].spec.pin) {
    (void)__atomic_fetch_or(&pending_interrupts, 1UL << BUTTON_2,
                            __ATOMIC_RELAXED);
  } else if (gpio_pin == display_key.pin) {
    Button_OnDisplayKeyInterrupt();
  }
}

void Button_OnDisplayKeyInterrupt(void)
{
  __atomic_store_n(&display_key_interrupt_pending, true, __ATOMIC_RELEASE);
}

void Button_Run(uint32_t now_ms)
{
  const uint32_t interrupts =
      __atomic_exchange_n(&pending_interrupts, 0U, __ATOMIC_RELAXED);

  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    ButtonState *button = &buttons[index];

    if ((interrupts & (1UL << index)) != 0U) {
      button->debounce_pending = true;
      button->debounce_started_ms = now_ms;
    }
    if (button->debounce_pending &&
        now_ms - button->debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
      button->debounce_pending = false;
      if (gpio_get(&button->spec) > 0) {
        pressed_events |= 1UL << index;
        (void)__atomic_fetch_add(&pressed_counts[index], 1U,
                                 __ATOMIC_RELAXED);
      }
    }
  }
  if (__atomic_exchange_n(&display_key_interrupt_pending, false,
                          __ATOMIC_ACQUIRE)) {
    display_key_debounce_pending = true;
    display_key_debounce_started_ms = now_ms;
  }
  if (display_key_debounce_pending &&
      now_ms - display_key_debounce_started_ms >= BUTTON_DEBOUNCE_MS) {
    display_key_debounce_pending = false;
    if (gpio_get(&display_key) > 0) {
      display_key_pressed_event = true;
    }
  }
}

bool Button_TakePressed(ButtonId button)
{
  uint32_t mask;

  if (button >= BUTTON_COUNT) {
    return false;
  }
  mask = 1UL << (uint32_t)button;
  if ((pressed_events & mask) == 0U) {
    return false;
  }
  pressed_events &= ~mask;
  return true;
}

bool Button_TakeDisplayKeyPressed(void)
{
  if (!display_key_pressed_event) {
    return false;
  }
  display_key_pressed_event = false;
  return true;
}

void Button_GetSnapshot(ButtonSnapshot *snapshot)
{
  if (snapshot == NULL) {
    return;
  }
  for (uint32_t index = 0U; index < BUTTON_COUNT; ++index) {
    snapshot->pressed[index] =
        gpio_get(&buttons[index].spec) > 0;
    snapshot->pressed_count[index] =
        __atomic_load_n(&pressed_counts[index], __ATOMIC_RELAXED);
  }
}

static void ApiRun(const struct device*d,uint32_t n){(void)d;Button_Run(n);}
static bool ApiTake(const struct device*d,ButtonId b){(void)d;return Button_TakePressed(b);}
static bool ApiKey(const struct device*d){(void)d;return Button_TakeDisplayKeyPressed();}
static void ApiSnapshot(const struct device*d,ButtonSnapshot*s){(void)d;Button_GetSnapshot(s);}
static void ApiInterrupt(const struct device*d,uint16_t p){(void)d;Button_OnInterrupt(p);}
const ButtonDriverApi button_stm32_api={.run=ApiRun,.take_pressed=ApiTake,.take_display_key=ApiKey,.get_snapshot=ApiSnapshot,.on_interrupt=ApiInterrupt};
int ButtonStm32_Init(const struct device *device){const ButtonStm32Config*c=device?device->config:NULL;if(!c)return -1;for(unsigned i=0;i<BUTTON_COUNT;i++)buttons[i].spec=c->buttons[i];display_key=c->display_key;Button_Init();return 0;}

static const ButtonDriverApi *Api(const struct device*d){return device_is_ready(d)?d->api:NULL;}
void button_run(const struct device*d,uint32_t n){const ButtonDriverApi*a=Api(d);if(a&&a->run)a->run(d,n);}
bool button_take_display_key(const struct device*d){const ButtonDriverApi*a=Api(d);return a&&a->take_display_key&&a->take_display_key(d);}
void button_get_snapshot(const struct device*d,ButtonSnapshot*s){const ButtonDriverApi*a=Api(d);if(a&&a->get_snapshot)a->get_snapshot(d,s);}
void button_on_interrupt(const struct device*d,uint16_t p){const ButtonDriverApi*a=Api(d);if(a&&a->on_interrupt)a->on_interrupt(d,p);}
