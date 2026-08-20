#ifdef CAN_TRANSPORT_HOST_TEST

#include <assert.h>
#include <string.h>

#include "bsp/fdcan/fdcan_bsp.h"
#include "communication/can_transport/can_transport.h"
#include "communication/ota_transport/ota_can_transport.h"
#include "../../../../shared/ota_protocol.h"

static BspFdcanFrame queued_frame;
static bool frame_pending;
static uint32_t now_ms;
static uint32_t error_events;
static uint32_t send_count;
static uint32_t restart_count;
static uint32_t ota_frame_count;

bool BspFdcan_Start(const uint32_t *ids, size_t count)
{
  return ids != NULL && count == 3U;
}

bool BspFdcan_Restart(void)
{
  ++restart_count;
  return true;
}

bool BspFdcan_TakeRxFrame(BspFdcanFrame *frame)
{
  if (!frame_pending || frame == NULL) {
    return false;
  }
  *frame = queued_frame;
  frame_pending = false;
  return true;
}

bool BspFdcan_SendFrame(const BspFdcanFrame *frame)
{
  assert(frame != NULL);
  ++send_count;
  return true;
}

uint32_t BspFdcan_TakeErrorEvents(void)
{
  const uint32_t events = error_events;

  error_events = 0U;
  return events;
}

void BspFdcan_GetEventCounters(BspFdcanEventCounters *counters)
{
  *counters = (BspFdcanEventCounters){0};
}

bool BspFdcan_GetDiagnostics(BspFdcanDiagnostics *diagnostics)
{
  *diagnostics = (BspFdcanDiagnostics){0};
  return true;
}

uint32_t BspTime_GetUptimeMs(void)
{
  return now_ms;
}

bool OtaCanTransport_OnRxFrame(const BspFdcanFrame *frame)
{
  assert(frame != NULL);
  ++ota_frame_count;
  return true;
}

static void QueueFrame(uint32_t identifier, const uint8_t data[8])
{
  queued_frame = (BspFdcanFrame){
      .identifier = identifier,
      .length = BSP_FDCAN_CONTROL_DATA_SIZE,
  };
  memcpy(queued_frame.data, data, BSP_FDCAN_CONTROL_DATA_SIZE);
  frame_pending = true;
}

int main(void)
{
  static const uint8_t ping[8] = {'P', 'I', 'N', 'G', 1U, 0U, 0U, 0U};
  static const uint8_t pass[8] = {'P', 'A', 'S', 'S', 1U, 0U, 0U, 0U};
  static const uint8_t control[8] = {1U, 1U, 7U, 0U,
                                     10U, 0U, 0xF6U, 0xFFU};
  CanTransportControlCommand command;

  assert(CanTransport_Init());
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

  error_events = BSP_FDCAN_ERROR_BUS_OFF;
  CanTransport_Run();
  assert(CanTransport_TakeSessionInvalidated());
  now_ms = 100U;
  CanTransport_Run();
  assert(restart_count == 1U);
  assert(CanTransport_IsOperational());
  return 0;
}

#endif
