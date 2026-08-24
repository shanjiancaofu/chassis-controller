#ifdef CAN_TRANSPORT_HOST_TEST
#include "subsys/communication/can/can_transport.h"
#include <assert.h>
#include <errno.h>
static struct can_frame queued_frame;
static bool frame_pending;
static uint32_t now_ms, error_events, send_count, recovery_count;
static struct device_state fake_state = {.init_res = 0, .initialized = true};
bool device_is_ready(const struct device *d) {
  return d && d->state == &fake_state;
}
uint32_t time_uptime_ms(void) { return now_ms; }
static int Start(const struct device *d, const struct can_filter *f, size_t n) {
  (void)d;
  return f && n == 1 ? 0 : -EINVAL;
}
static int Stop(const struct device *d) {
  (void)d;
  return 0;
}
static int Send(const struct device *d, const struct can_frame *f) {
  (void)d;
  assert(f);
  send_count++;
  return 0;
}
static int Recv(const struct device *d, struct can_frame *f) {
  (void)d;
  if (!frame_pending)
    return -EAGAIN;
  *f = queued_frame;
  frame_pending = false;
  return 0;
}
static int Recover(const struct device *d) {
  (void)d;
  recovery_count++;
  return 0;
}
static int Diag(const struct device *d, struct can_diagnostics *x) {
  (void)d;
  *x = (struct can_diagnostics){0};
  return 0;
}
static uint32_t Errors(const struct device *d) {
  (void)d;
  uint32_t e = error_events;
  error_events = 0;
  return e;
}
static bool Idle(const struct device *d) {
  (void)d;
  return true;
}
static const struct can_driver_api api = {.start = Start,
                                          .stop = Stop,
                                          .send = Send,
                                          .recv = Recv,
                                          .recover = Recover,
                                          .get_diagnostics = Diag,
                                          .take_error_events = Errors,
                                          .is_tx_idle = Idle};
static const struct device device = {
    .name = "fake_can", .api = &api, .state = &fake_state};
int main(void) {
  const struct can_filter filter = {.id = 0x100U, .mask = 0x7FFU};
  struct can_frame frame = {.id = 0x100U, .dlc = 8U};
  assert(CanTransport_Init(&device, &filter, 1U) == 0);
  queued_frame = frame;
  frame_pending = true;
  assert(CanTransport_Receive(&frame) == 0);
  assert(CanTransport_Receive(&frame) == -EAGAIN);
  assert(CanTransport_Send(&frame) == 0 && send_count == 1U);
  error_events = CAN_ERROR_BUS_OFF;
  CanTransport_Run();
  assert(CanTransport_TakeSessionInvalidated());
  now_ms = 100U;
  CanTransport_Run();
  assert(recovery_count == 1U);
  assert(CanTransport_TakeRecovered());
  assert(CanTransport_IsOperational());
  return 0;
}
#endif
