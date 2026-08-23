#ifdef CAN_TRANSPORT_HOST_TEST

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "subsys/communication/can_transport/can_transport.h"
#include "subsys/communication/ota_transport/ota_can_transport.h"
#include "drivers/can.h"
#include "drivers/time.h"
#include "../../../../shared/ota_protocol.h"

static struct can_frame queued_frame;
static bool frame_pending;
static uint32_t now_ms;
static uint32_t error_events;
static uint32_t send_count;
static uint32_t restart_count;
static uint32_t ota_frame_count;
static struct device_state fake_state = {.init_res = 0, .initialized = true};

bool device_is_ready(const struct device *device)
{
  return device != NULL && device->state == &fake_state;
}

static int FakeStart(const struct device *device, const struct can_filter *filters,
                     size_t count)
{
  (void)device;
  return filters != NULL && count == 3U ? 0 : -EINVAL;
}

static int FakeStop(const struct device *device)
{
  (void)device;
  return 0;
}

static int FakeRecover(const struct device *device)
{
  (void)device;
  ++restart_count;
  return 0;
}

static int FakeRecv(const struct device *device, struct can_frame *frame)
{
  (void)device;
  if (!frame_pending || frame == NULL) {
    return -EAGAIN;
  }
  *frame = queued_frame;
  frame_pending = false;
  return 0;
}

static int FakeSend(const struct device *device, const struct can_frame *frame)
{
  (void)device;
  assert(frame != NULL);
  ++send_count;
  return 0;
}

static uint32_t FakeTakeErrorEvents(const struct device *device)
{
  (void)device;
  const uint32_t events = error_events;

  error_events = 0U;
  return events;
}

static int FakeGetDiagnostics(const struct device *device,
                              struct can_diagnostics *diagnostics)
{
  (void)device;
  *diagnostics = (struct can_diagnostics){0};
  return 0;
}

static bool FakeIsTxIdle(const struct device *device)
{
  (void)device;
  return true;
}

static const struct can_driver_api fake_api = {
    .start = FakeStart,
    .stop = FakeStop,
    .send = FakeSend,
    .recv = FakeRecv,
    .recover = FakeRecover,
    .get_diagnostics = FakeGetDiagnostics,
    .take_error_events = FakeTakeErrorEvents,
    .is_tx_idle = FakeIsTxIdle,
};
static const struct device fake_device = {
    .name = "fake_can",
    .api = &fake_api,
    .state = &fake_state,
};

uint32_t time_uptime_ms(void)
{
  return now_ms;
}

bool OtaCanTransport_OnRxFrame(const struct can_frame *frame)
{
  assert(frame != NULL);
  ++ota_frame_count;
  return true;
}

static void QueueFrame(uint32_t identifier, const uint8_t data[8])
{
  queued_frame = (struct can_frame){
      .id = identifier,
      .dlc = 8U,
      .flags = CAN_FRAME_FDF | CAN_FRAME_BRS,
  };
  memcpy(queued_frame.data, data, 8U);
  frame_pending = true;
}

int main(void)
{
  static const uint8_t ping[8] = {'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U};
  static const uint8_t pass[8] = {'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U};
  static const uint8_t control[8] = {1U, 1U, 7U, 0U,
                                     10U, 0U, 0xF6U, 0xFFU};
  CanTransportControlCommand command;

  assert(CanTransport_Init(&fake_device) == 0);
  QueueFrame(0x720U, ping);
  CanTransport_Run();
  assert(send_count == 1U);

  QueueFrame(0x720U, pass);
  CanTransport_Run();
  assert(CanTransport_GetLinkStatus() == CAN_TRANSPORT_LINK_PASSED);

  QueueFrame(0x100U, control);
  CanTransport_Run();
  assert(CanTransport_TakeControlCommand(&command));
  assert(command.enabled && command.sequence == 7U);
  assert(command.left_target == 10 && command.right_target == -10);

  QueueFrame(OTA_CAN_REQUEST_ID, control);
  CanTransport_Run();
  assert(ota_frame_count == 1U);

  error_events = CAN_ERROR_BUS_OFF;
  CanTransport_Run();
  assert(CanTransport_TakeSessionInvalidated());
  now_ms = 100U;
  CanTransport_Run();
  assert(restart_count == 1U);
  assert(CanTransport_IsOperational());
  return 0;
}

#endif
