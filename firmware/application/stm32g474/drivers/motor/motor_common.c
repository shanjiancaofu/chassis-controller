#include "drivers/motor/motor.h"

static const MotorDriverApi *api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

int motor_start(const struct device *device)
{
  const MotorDriverApi *driver = api(device);
  return driver != NULL && driver->start != NULL ? driver->start(device) : -1;
}

void motor_set_signed_duty(const struct device *device, MotorId motor, int16_t duty)
{
  const MotorDriverApi *driver = api(device);
  if (driver != NULL && driver->set_signed_duty != NULL) {
    driver->set_signed_duty(device, motor, duty);
  }
}

void motor_set_signed_duty_both(const struct device *device, int16_t left_duty,
                                int16_t right_duty)
{
  const MotorDriverApi *driver = api(device);
  if (driver != NULL && driver->set_signed_duty_both != NULL) {
    driver->set_signed_duty_both(device, left_duty, right_duty);
  }
}

void motor_coast_all(const struct device *device)
{
  const MotorDriverApi *driver = api(device);
  if (driver != NULL && driver->coast_all != NULL) {
    driver->coast_all(device);
  }
}

void motor_emergency_stop(const struct device *device)
{
  const MotorDriverApi *driver = api(device);
  if (driver != NULL && driver->emergency_stop != NULL) {
    driver->emergency_stop(device);
  }
}

void motor_clear_emergency_stop(const struct device *device)
{
  const MotorDriverApi *driver = api(device);
  if (driver != NULL && driver->clear_emergency_stop != NULL) {
    driver->clear_emergency_stop(device);
  }
}

int16_t motor_get_applied_duty(const struct device *device, MotorId motor)
{
  const MotorDriverApi *driver = api(device);
  return driver != NULL && driver->get_applied_duty != NULL
             ? driver->get_applied_duty(device, motor)
             : 0;
}
