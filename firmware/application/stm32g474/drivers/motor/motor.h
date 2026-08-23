#ifndef CHASSIS_MOTOR_H
#define CHASSIS_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

#include "device.h"

#define MOTOR_COMPARE_MAX 8499

typedef enum {
  MOTOR_LEFT = 0,
  MOTOR_RIGHT
} MotorId;

typedef struct {
  int (*start)(const struct device *device);
  void (*set_signed_duty)(const struct device *device, MotorId motor,
                          int16_t duty);
  void (*set_signed_duty_both)(const struct device *device,
                               int16_t left_duty, int16_t right_duty);
  void (*coast_all)(const struct device *device);
  void (*emergency_stop)(const struct device *device);
  void (*clear_emergency_stop)(const struct device *device);
  int16_t (*get_applied_duty)(const struct device *device, MotorId motor);
} MotorDriverApi;

int motor_start(const struct device *device);
void motor_set_signed_duty(const struct device *device, MotorId motor,
                           int16_t duty);
void motor_set_signed_duty_both(const struct device *device,
                                int16_t left_duty, int16_t right_duty);
void motor_coast_all(const struct device *device);
void motor_emergency_stop(const struct device *device);
void motor_clear_emergency_stop(const struct device *device);
int16_t motor_get_applied_duty(const struct device *device, MotorId motor);

#endif
