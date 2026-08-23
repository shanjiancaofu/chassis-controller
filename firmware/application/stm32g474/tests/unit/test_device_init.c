#ifdef DEVICE_INIT_HOST_TEST

#include <assert.h>
#include <errno.h>

#include "device.h"
#include "init.h"

static int success_calls;
static int failure_calls;
static int system_calls;

static int SuccessInit(const struct device *device)
{
  (void)device;
  ++success_calls;
  return 0;
}

static int FailureInit(const struct device *device)
{
  (void)device;
  ++failure_calls;
  return -EIO;
}

static int SystemFailure(void)
{
  ++system_calls;
  return -EPERM;
}

int main(void)
{
  struct device_state failed_state = {0};
  struct device_state ready_state = {0};
  const struct device failed_device = {
      .name = "optional",
      .state = &failed_state,
      .ops = {.init = FailureInit},
  };
  const struct device ready_device = {
      .name = "required",
      .state = &ready_state,
      .ops = {.init = SuccessInit},
  };
  const InitEntry entries[] = {
      {.device = &failed_device},
      {.device = &ready_device},
      {.init_fn = SystemFailure},
      {.device = &ready_device},
  };

  assert(SystemInit_RunEntries(NULL, entries) == -EINVAL);
  assert(SystemInit_RunEntries(entries, entries + 4) == -EPERM);
  assert(failure_calls == 1);
  assert(success_calls == 1);
  assert(system_calls == 1);
  assert(!device_is_ready(&failed_device));
  assert(device_is_ready(&ready_device));
  assert(device_init(&ready_device) == -EALREADY);
  assert(success_calls == 1);
  assert(failed_state.initialized && failed_state.init_res == -EIO);
  return 0;
}

#endif
