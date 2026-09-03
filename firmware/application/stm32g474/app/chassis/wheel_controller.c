#include "app/chassis/wheel_controller.h"

#include <stddef.h>
#include <stdlib.h>

#include "config/control_config.h"
#include "lib/pid/speed_pid.h"

static SpeedPid left_pid;
static SpeedPid right_pid;
static WheelControllerSnapshot controller_snapshot;
static int32_t left_ramped_target;
static int32_t right_ramped_target;
static uint32_t left_startup_boost_ms;
static uint32_t right_startup_boost_ms;
static uint32_t left_slew_remainder;
static uint32_t right_slew_remainder;
static uint32_t left_direction_hold_ms;
static uint32_t right_direction_hold_ms;
static WheelControllerMotorPort controller_motor_port;
static bool controller_motor_port_ready;

static int16_t UpdateWheel(SpeedPid *pid, int32_t target, int32_t measurement,
                           int16_t current_duty, uint32_t elapsed_ticks,
                           uint32_t *startup_boost_ms);
static int32_t SlewTarget(int32_t current, int32_t requested,
                          uint32_t elapsed_ticks, uint32_t *remainder,
                          uint32_t *direction_hold_ms);
static float ScaleDerivativeGain(float kd);

bool WheelController_Init(const WheelControllerMotorPort *motor_port) {
  controller_motor_port_ready = false;
  if (motor_port == NULL || motor_port->coast_all == NULL ||
      motor_port->emergency_stop == NULL ||
      motor_port->set_signed_duty_both == NULL ||
      motor_port->get_left_applied_duty == NULL ||
      motor_port->get_right_applied_duty == NULL) {
    return false;
  }
  controller_motor_port = *motor_port;
  controller_motor_port_ready = true;
  controller_snapshot = (WheelControllerSnapshot){0};
  left_ramped_target = 0;
  right_ramped_target = 0;
  left_startup_boost_ms = 0U;
  right_startup_boost_ms = 0U;
  left_slew_remainder = 0U;
  right_slew_remainder = 0U;
  left_direction_hold_ms = 0U;
  right_direction_hold_ms = 0U;
  SpeedPid_Init(&left_pid, MOTOR_LEFT_PID_KP, MOTOR_LEFT_PID_KI,
                ScaleDerivativeGain(MOTOR_LEFT_PID_KD),
                (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
  SpeedPid_Init(&right_pid, MOTOR_RIGHT_PID_KP, MOTOR_RIGHT_PID_KI,
                ScaleDerivativeGain(MOTOR_RIGHT_PID_KD),
                (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
  return true;
}

void WheelController_Stop(void) {
  if (controller_motor_port_ready) {
    controller_motor_port.coast_all();
  }
  controller_snapshot.left_target = 0;
  controller_snapshot.right_target = 0;
  controller_snapshot.left_output = 0;
  controller_snapshot.right_output = 0;
  left_ramped_target = 0;
  right_ramped_target = 0;
  left_startup_boost_ms = 0U;
  right_startup_boost_ms = 0U;
  left_slew_remainder = 0U;
  right_slew_remainder = 0U;
  left_direction_hold_ms = 0U;
  right_direction_hold_ms = 0U;
  WheelController_Reset();
}

void WheelController_Reset(void) {
  SpeedPid_Reset(&left_pid);
  SpeedPid_Reset(&right_pid);
}

void WheelController_EmergencyStop(void) {
  if (controller_motor_port_ready) {
    controller_motor_port.emergency_stop();
  }
  controller_snapshot.left_target = 0;
  controller_snapshot.right_target = 0;
  controller_snapshot.left_output = 0;
  controller_snapshot.right_output = 0;
  left_ramped_target = 0;
  right_ramped_target = 0;
  left_startup_boost_ms = 0U;
  right_startup_boost_ms = 0U;
  left_slew_remainder = 0U;
  right_slew_remainder = 0U;
  left_direction_hold_ms = 0U;
  right_direction_hold_ms = 0U;
  WheelController_Reset();
}

bool WheelController_Update(int32_t left_target, int32_t right_target,
                            int32_t left_measurement, int32_t right_measurement,
                            uint32_t elapsed_ticks) {
  int32_t effective_left_target;
  int32_t effective_right_target;
  int16_t left_duty;
  int16_t right_duty;

  if (!controller_motor_port_ready || elapsed_ticks == 0U ||
      left_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      left_target > MOTOR_CONTROL_TARGET_LIMIT ||
      right_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      right_target > MOTOR_CONTROL_TARGET_LIMIT) {
    return false;
  }

  effective_left_target =
      SlewTarget(left_ramped_target, left_target, elapsed_ticks,
                 &left_slew_remainder, &left_direction_hold_ms);
  effective_right_target =
      SlewTarget(right_ramped_target, right_target, elapsed_ticks,
                 &right_slew_remainder, &right_direction_hold_ms);
  left_ramped_target = effective_left_target;
  right_ramped_target = effective_right_target;

  left_duty = UpdateWheel(&left_pid, effective_left_target, left_measurement,
                          controller_motor_port.get_left_applied_duty(),
                          elapsed_ticks, &left_startup_boost_ms);
  right_duty =
      UpdateWheel(&right_pid, effective_right_target, right_measurement,
                  controller_motor_port.get_right_applied_duty(), elapsed_ticks,
                  &right_startup_boost_ms);
  controller_motor_port.set_signed_duty_both(left_duty, right_duty);
  controller_snapshot.left_target = effective_left_target;
  controller_snapshot.right_target = effective_right_target;
  controller_snapshot.left_measurement = left_measurement;
  controller_snapshot.right_measurement = right_measurement;
  controller_snapshot.left_output = left_duty;
  controller_snapshot.right_output = right_duty;
  return true;
}

void WheelController_ApplyPidGains(WheelControllerSide side, uint16_t kp,
                                   uint16_t ki, uint16_t kd) {
  SpeedPid *pid;

  if (side != WHEEL_CONTROLLER_LEFT && side != WHEEL_CONTROLLER_RIGHT) {
    return;
  }

  pid = side == WHEEL_CONTROLLER_LEFT ? &left_pid : &right_pid;
  SpeedPid_Init(pid, (float)kp, (float)ki, ScaleDerivativeGain((float)kd),
                (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
}

static float ScaleDerivativeGain(float kd) {
  return kd * (float)MOTOR_CONTROL_PERIOD_MS /
         (float)MOTOR_CONTROL_REFERENCE_PERIOD_MS;
}

void WheelController_GetSnapshot(WheelControllerSnapshot *snapshot) {
  if (snapshot != NULL) {
    *snapshot = controller_snapshot;
  }
}

static int16_t UpdateWheel(SpeedPid *pid, int32_t target, int32_t measurement,
                           int16_t current_duty, uint32_t elapsed_ticks,
                           uint32_t *startup_boost_ms) {
  int16_t duty;
  const float dt_seconds =
      (float)(MOTOR_CONTROL_PERIOD_MS * elapsed_ticks) / 1000.0f;

  if (target == 0) {
    *startup_boost_ms = 0U;
    SpeedPid_Reset(pid);
    return 0;
  }
  if (startup_boost_ms != NULL && abs(measurement) <= 1 &&
      *startup_boost_ms < MOTOR_STARTUP_BOOST_MAX_MS) {
    *startup_boost_ms += MOTOR_CONTROL_PERIOD_MS * elapsed_ticks;
    return target < 0 ? -MOTOR_STARTUP_BOOST_DUTY : MOTOR_STARTUP_BOOST_DUTY;
  }
  if (startup_boost_ms != NULL && abs(measurement) > 1) {
    *startup_boost_ms = MOTOR_STARTUP_BOOST_MAX_MS;
  }

  duty = (int16_t)SpeedPid_Update(pid, (float)target, (float)measurement,
                                  dt_seconds);
  if ((current_duty > 0 && duty < 0) || (current_duty < 0 && duty > 0)) {
    SpeedPid_Reset(pid);
    return 0;
  }
  return duty;
}

static int32_t SlewTarget(int32_t current, int32_t requested,
                          uint32_t elapsed_ticks, uint32_t *remainder,
                          uint32_t *direction_hold_ms) {
  const uint32_t elapsed_ms = MOTOR_CONTROL_PERIOD_MS * elapsed_ticks;
  uint64_t budget;
  uint32_t max_step;
  int32_t delta;

  if (remainder == NULL || direction_hold_ms == NULL) {
    return current;
  }
  if (requested == 0) {
    *remainder = 0U;
    *direction_hold_ms = 0U;
    return 0;
  }
  if ((current > 0 && requested < 0) || (current < 0 && requested > 0)) {
    *remainder = 0U;
    *direction_hold_ms = elapsed_ms >= MOTOR_CONTROL_REFERENCE_PERIOD_MS
                             ? 0U
                             : MOTOR_CONTROL_REFERENCE_PERIOD_MS - elapsed_ms;
    return 0;
  }
  if (*direction_hold_ms > 0U) {
    *direction_hold_ms =
        elapsed_ms >= *direction_hold_ms ? 0U : *direction_hold_ms - elapsed_ms;
    return 0;
  }
  if (requested == current) {
    *remainder = 0U;
    return current;
  }
  budget = (uint64_t)MOTOR_CONTROL_TARGET_SLEW_COUNTS_PER_REFERENCE_PERIOD *
               MOTOR_CONTROL_PERIOD_MS * elapsed_ticks +
           *remainder;
  max_step = (uint32_t)(budget / MOTOR_CONTROL_REFERENCE_PERIOD_MS);
  *remainder = (uint32_t)(budget % MOTOR_CONTROL_REFERENCE_PERIOD_MS);
  if (max_step > MOTOR_CONTROL_TARGET_LIMIT) {
    max_step = MOTOR_CONTROL_TARGET_LIMIT;
  }
  delta = requested - current;
  if (delta > 0) {
    return delta <= (int32_t)max_step ? requested : current + (int32_t)max_step;
  }
  return -delta <= (int32_t)max_step ? requested : current - (int32_t)max_step;
}
