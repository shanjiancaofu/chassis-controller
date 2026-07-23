#include "chassis_control.h"

#include <stddef.h>

#include "bsp_encoder.h"
#include "bsp_motor.h"
#include "main.h"
#include "project_config.h"
#include "speed_pid.h"

#if MOTOR_CONTROL_OUTPUT_LIMIT > BSP_MOTOR_COMPARE_MAX
#error "MOTOR_CONTROL_OUTPUT_LIMIT exceeds TIM8 compare range"
#endif

#if MOTOR_OPEN_LOOP_TEST_DUTY > MOTOR_CONTROL_OUTPUT_LIMIT
#error "MOTOR_OPEN_LOOP_TEST_DUTY exceeds bring-up output limit"
#endif

static SpeedPid left_pid;
static SpeedPid right_pid;
static ChassisControlStatus control_status;
static volatile bool emergency_stop_latched;
static bool running;
static bool open_loop_test_running;
static int16_t open_loop_left_duty;
static int16_t open_loop_right_duty;
static uint32_t open_loop_started_ms;
static uint32_t open_loop_duration_ms;
static bool has_command;
static uint32_t last_command_ms;

void ChassisControl_Init(void)
{
  control_status = (ChassisControlStatus){0};
  SpeedPid_Init(&left_pid, MOTOR_LEFT_PID_KP, MOTOR_LEFT_PID_KI,
                MOTOR_LEFT_PID_KD, (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
  SpeedPid_Init(&right_pid, MOTOR_RIGHT_PID_KP, MOTOR_RIGHT_PID_KI,
                MOTOR_RIGHT_PID_KD, (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
  running = false;
  open_loop_test_running = false;
  has_command = false;
  last_command_ms = 0U;

  if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    emergency_stop_latched = true;
  }
  if (emergency_stop_latched) {
    BspMotor_EmergencyStop();
    control_status.state = CHASSIS_CONTROL_EMERGENCY_STOP;
    control_status.fault_flags = CHASSIS_FAULT_EMERGENCY_STOP;
  }
}

bool ChassisControl_Start(void)
{
  if (emergency_stop_latched || ChassisControl_HasInternalFault() ||
      open_loop_test_running || !has_command) {
    return false;
  }

  running = true;
  control_status.state = CHASSIS_CONTROL_RUNNING;
  return true;
}

bool ChassisControl_StartOpenLoopTest(int16_t left_duty,
                                      int16_t right_duty,
                                      uint32_t now_ms,
                                      uint32_t duration_ms)
{
  if (emergency_stop_latched || ChassisControl_HasInternalFault() ||
      running || open_loop_test_running || duration_ms == 0U ||
      (left_duty == 0 && right_duty == 0) ||
      (left_duty != 0 && right_duty != 0) ||
      left_duty > MOTOR_CONTROL_OUTPUT_LIMIT ||
      left_duty < -MOTOR_CONTROL_OUTPUT_LIMIT ||
      right_duty > MOTOR_CONTROL_OUTPUT_LIMIT ||
      right_duty < -MOTOR_CONTROL_OUTPUT_LIMIT) {
    return false;
  }

  has_command = false;
  control_status.left_target = 0;
  control_status.right_target = 0;
  open_loop_left_duty = left_duty;
  open_loop_right_duty = right_duty;
  open_loop_started_ms = now_ms;
  open_loop_duration_ms = duration_ms;
  open_loop_test_running = true;
  control_status.state = CHASSIS_CONTROL_OPEN_LOOP_TEST;
  BspMotor_CoastAll();
  SpeedPid_Reset(&left_pid);
  SpeedPid_Reset(&right_pid);
  return true;
}

void ChassisControl_Stop(void)
{
  running = false;
  open_loop_test_running = false;
  open_loop_left_duty = 0;
  open_loop_right_duty = 0;
  has_command = false;
  control_status.left_target = 0;
  control_status.right_target = 0;
  control_status.left_output = 0;
  control_status.right_output = 0;
  BspMotor_CoastAll();
  SpeedPid_Reset(&left_pid);
  SpeedPid_Reset(&right_pid);
  if (emergency_stop_latched) {
    control_status.state = CHASSIS_CONTROL_EMERGENCY_STOP;
  } else if (ChassisControl_HasInternalFault()) {
    control_status.state = CHASSIS_CONTROL_INTERNAL_FAULT;
  } else {
    control_status.state = CHASSIS_CONTROL_STOPPED;
  }
}

void ChassisControl_SetTargetSpeed(int32_t left_counts_per_tick,
                                   int32_t right_counts_per_tick)
{
  control_status.left_target = left_counts_per_tick;
  control_status.right_target = right_counts_per_tick;
}

void ChassisControl_NotifyCommandReceived(uint32_t now_ms)
{
  last_command_ms = now_ms;
  has_command = true;
  control_status.fault_flags &= ~CHASSIS_FAULT_COMMAND_TIMEOUT;
  if (control_status.state == CHASSIS_CONTROL_COMMAND_TIMEOUT) {
    control_status.state = CHASSIS_CONTROL_STOPPED;
  }
}

void ChassisControl_Tick10ms(uint32_t now_ms)
{
  float left_output;
  float right_output;
  int16_t left_duty;
  int16_t right_duty;
  const int16_t current_left =
      BspMotor_GetAppliedDuty(BSP_MOTOR_LEFT);
  const int16_t current_right =
      BspMotor_GetAppliedDuty(BSP_MOTOR_RIGHT);

  BspEncoder_ReadDelta(&control_status.left_delta,
                       &control_status.right_delta);
  control_status.left_total += control_status.left_delta;
  control_status.right_total += control_status.right_delta;

  if (emergency_stop_latched) {
    running = false;
    open_loop_test_running = false;
    has_command = false;
    control_status.left_target = 0;
    control_status.right_target = 0;
    control_status.left_output = 0;
    control_status.right_output = 0;
    control_status.state = CHASSIS_CONTROL_EMERGENCY_STOP;
    control_status.fault_flags |= CHASSIS_FAULT_EMERGENCY_STOP;
    BspMotor_EmergencyStop();
    SpeedPid_Reset(&left_pid);
    SpeedPid_Reset(&right_pid);
    return;
  }

  if (ChassisControl_HasInternalFault()) {
    running = false;
    open_loop_test_running = false;
    control_status.left_output = 0;
    control_status.right_output = 0;
    control_status.state = CHASSIS_CONTROL_INTERNAL_FAULT;
    BspMotor_CoastAll();
    SpeedPid_Reset(&left_pid);
    SpeedPid_Reset(&right_pid);
    return;
  }

  if (open_loop_test_running) {
    if (now_ms - open_loop_started_ms >= open_loop_duration_ms) {
      ChassisControl_Stop();
      return;
    }
    BspMotor_SetSignedDutyBoth(open_loop_left_duty,
                               open_loop_right_duty);
    control_status.left_output = open_loop_left_duty;
    control_status.right_output = open_loop_right_duty;
    control_status.state = CHASSIS_CONTROL_OPEN_LOOP_TEST;
    return;
  }

  if (!running) {
    BspMotor_CoastAll();
    control_status.left_output = 0;
    control_status.right_output = 0;
    return;
  }

  if (!has_command ||
      now_ms - last_command_ms >= MOTOR_COMMAND_TIMEOUT_MS) {
    running = false;
    has_command = false;
    control_status.left_target = 0;
    control_status.right_target = 0;
    control_status.left_output = 0;
    control_status.right_output = 0;
    control_status.state = CHASSIS_CONTROL_COMMAND_TIMEOUT;
    control_status.fault_flags |= CHASSIS_FAULT_COMMAND_TIMEOUT;
    BspMotor_CoastAll();
    SpeedPid_Reset(&left_pid);
    SpeedPid_Reset(&right_pid);
    return;
  }

  if (control_status.left_target == 0 && control_status.right_target == 0) {
    BspMotor_CoastAll();
    control_status.left_output = 0;
    control_status.right_output = 0;
    SpeedPid_Reset(&left_pid);
    SpeedPid_Reset(&right_pid);
    return;
  }

  if (control_status.left_target == 0) {
    left_output = 0.0f;
    SpeedPid_Reset(&left_pid);
  } else {
    left_output = SpeedPid_Update(
        &left_pid, (float)control_status.left_target,
        (float)control_status.left_delta,
        MOTOR_CONTROL_PERIOD_MS / 1000.0f);
  }
  if (control_status.right_target == 0) {
    right_output = 0.0f;
    SpeedPid_Reset(&right_pid);
  } else {
    right_output = SpeedPid_Update(
        &right_pid, (float)control_status.right_target,
        (float)control_status.right_delta,
        MOTOR_CONTROL_PERIOD_MS / 1000.0f);
  }
  left_duty = (int16_t)left_output;
  right_duty = (int16_t)right_output;

  if ((current_left > 0 && left_duty < 0) ||
      (current_left < 0 && left_duty > 0)) {
    left_duty = 0;
    SpeedPid_Reset(&left_pid);
  }
  if ((current_right > 0 && right_duty < 0) ||
      (current_right < 0 && right_duty > 0)) {
    right_duty = 0;
    SpeedPid_Reset(&right_pid);
  }

  BspMotor_SetSignedDutyBoth(left_duty, right_duty);
  control_status.left_output = left_duty;
  control_status.right_output = right_duty;
  control_status.state = CHASSIS_CONTROL_RUNNING;
}

bool ChassisControl_ResetEncoderTotals(void)
{
  if (control_status.state != CHASSIS_CONTROL_STOPPED) {
    return false;
  }

  control_status.left_total = 0;
  control_status.right_total = 0;
  return true;
}

bool ChassisControl_SetPidGains(ChassisPidSide side, uint16_t kp,
                                uint16_t ki, uint16_t kd)
{
  SpeedPid *pid;

  if ((side != CHASSIS_PID_LEFT && side != CHASSIS_PID_RIGHT) ||
      kp > MOTOR_PID_KP_MAX || ki > MOTOR_PID_KI_MAX ||
      kd > MOTOR_PID_KD_MAX) {
    return false;
  }

  pid = side == CHASSIS_PID_LEFT ? &left_pid : &right_pid;
  SpeedPid_Init(pid, (float)kp, (float)ki, (float)kd,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT,
                (float)MOTOR_CONTROL_OUTPUT_LIMIT);
  return true;
}

void ChassisControl_GetPidGains(ChassisPidSide side, uint16_t *kp,
                                uint16_t *ki, uint16_t *kd)
{
  const SpeedPid *pid;

  if ((side != CHASSIS_PID_LEFT && side != CHASSIS_PID_RIGHT) ||
      kp == NULL || ki == NULL || kd == NULL) {
    return;
  }

  pid = side == CHASSIS_PID_LEFT ? &left_pid : &right_pid;
  *kp = (uint16_t)pid->kp;
  *ki = (uint16_t)pid->ki;
  *kd = (uint16_t)pid->kd;
}

void ChassisControl_EmergencyStopFromIsr(void)
{
  emergency_stop_latched = true;
  BspMotor_EmergencyStop();
}

bool ChassisControl_ClearEmergencyStop(void)
{
  uint32_t primask;

  if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    return false;
  }

  primask = __get_PRIMASK();
  __disable_irq();
  if (HAL_GPIO_ReadPin(E_STOP_GPIO_Port, E_STOP_Pin) == GPIO_PIN_RESET) {
    __set_PRIMASK(primask);
    return false;
  }
  emergency_stop_latched = false;
  BspMotor_ClearEmergencyStop();
  __set_PRIMASK(primask);

  control_status.fault_flags &= ~CHASSIS_FAULT_EMERGENCY_STOP;
  ChassisControl_Stop();
  return true;
}

void ChassisControl_LatchInternalFault(uint32_t fault)
{
  if (fault == CHASSIS_FAULT_NONE) {
    fault = CHASSIS_FAULT_INTERNAL;
  }
  control_status.fault_flags |= fault;
  control_status.state = CHASSIS_CONTROL_INTERNAL_FAULT;
  running = false;
  open_loop_test_running = false;
  has_command = false;
  control_status.left_target = 0;
  control_status.right_target = 0;
  control_status.left_output = 0;
  control_status.right_output = 0;
  BspMotor_CoastAll();
  SpeedPid_Reset(&left_pid);
  SpeedPid_Reset(&right_pid);
}

bool ChassisControl_HasInternalFault(void)
{
  return (control_status.fault_flags &
          (CHASSIS_FAULT_CONTROL_OVERRUN | CHASSIS_FAULT_INTERNAL)) != 0U;
}

void ChassisControl_GetStatus(ChassisControlStatus *status)
{
  if (status != NULL) {
    *status = control_status;
  }
}
