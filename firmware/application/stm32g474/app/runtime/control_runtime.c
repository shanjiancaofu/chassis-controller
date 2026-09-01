#include "app/runtime/control_runtime.h"

#include <limits.h>
#include <stddef.h>

#include "app/chassis/odometry.h"
#include "app/chassis/feedback_watchdog.h"
#include "app/chassis/wheel_controller.h"
#include "app/diagnostics/board_health.h"
#include "app/maintenance/self_test/iwdg_self_test.h"
#include "app/maintenance/self_test/qspi_self_test.h"
#include "app/parameters/parameter_manager.h"
#include "app/parameters/parameter_storage.h"
#include "app/safety/fault_manager.h"
#include "app/safety/safety_manager.h"
#include "config/app_config.h"
#include "config/control_config.h"
#include "devicetree.h"
#include "drivers/adc/power_sample.h"
#include "drivers/encoder/encoder.h"
#include "drivers/motor/motor.h"
#include "drivers/safety/emergency_stop.h"
#include "drivers/time.h"
#include "kernel/critical.h"
#include "subsys/communication/ota/ota_confirmation.h"
#include "subsys/communication/ota/ota_session.h"
#include "subsys/communication/ota/ota_uart.h"

#if MOTOR_CONTROL_OUTPUT_LIMIT > MOTOR_COMPARE_MAX
#error "MOTOR_CONTROL_OUTPUT_LIMIT exceeds TIM8 compare range"
#endif

static const struct device *drive_device;
static const struct device *left_encoder_device;
static const struct device *right_encoder_device;
static uint32_t consecutive_control_overruns;
static volatile bool emergency_stop_event;
static FeedbackWatchdog feedback_watchdog;
static volatile bool inject_left_encoder_failure;
static volatile bool inject_right_encoder_failure;

static void ReleaseMotionOwner(void);
static void ApplyPendingControlParameters(void);
static bool PowerReady(uint32_t now_ms);

static int16_t GetLeftMotorAppliedDuty(void) {
  return motor_get_applied_duty(drive_device, MOTOR_LEFT);
}

static int16_t GetRightMotorAppliedDuty(void) {
  return motor_get_applied_duty(drive_device, MOTOR_RIGHT);
}

static void CoastMotors(void) { motor_coast_all(drive_device); }

static void EmergencyStopMotors(void) { motor_emergency_stop(drive_device); }

static void SetMotorDutyBoth(int16_t left_duty, int16_t right_duty) {
  motor_set_signed_duty_both(drive_device, left_duty, right_duty);
}

static const WheelControllerMotorPort wheel_controller_motor_port = {
    .coast_all = CoastMotors,
    .emergency_stop = EmergencyStopMotors,
    .set_signed_duty_both = SetMotorDutyBoth,
    .get_left_applied_duty = GetLeftMotorAppliedDuty,
    .get_right_applied_duty = GetRightMotorAppliedDuty,
};

bool ControlRuntime_BindDevices(const struct device *drive,
                                const struct device *left_encoder,
                                const struct device *right_encoder) {
  if (drive == NULL || left_encoder == NULL || right_encoder == NULL) {
    return false;
  }
  drive_device = drive;
  left_encoder_device = left_encoder;
  right_encoder_device = right_encoder;
  return true;
}

bool ControlRuntime_Init(void) {
  if (drive_device == NULL || left_encoder_device == NULL ||
      right_encoder_device == NULL) {
    return false;
  }
  consecutive_control_overruns = 0U;
  emergency_stop_event = false;
  FeedbackWatchdog_Reset(&feedback_watchdog);
  inject_left_encoder_failure = false;
  inject_right_encoder_failure = false;
  return WheelController_Init(&wheel_controller_motor_port);
}

void ControlRuntime_NotifyEmergencyStopFromIsr(void) {
  SafetyManager_StopFromEmergencyStopIsr();
  __atomic_store_n(&emergency_stop_event, true, __ATOMIC_RELEASE);
}

bool ControlRuntime_TakeEmergencyStopEvent(void) {
  return __atomic_exchange_n(&emergency_stop_event, false,
                             __ATOMIC_ACQ_REL);
}

void ControlRuntime_StopForEmergencyStopEvent(void) {
  kernel_critical_enter();
  CommandManager_ClearCommand();
  ReleaseMotionOwner();
  WheelController_Stop();
  SafetyManager_Stop();
  FeedbackWatchdog_Reset(&feedback_watchdog);
  kernel_critical_exit();
}

bool ControlRuntime_Start(void) {
  CommandManagerCommand command;
  bool accepted;

  if (!PowerReady(time_uptime_ms())) {
    return false;
  }
  kernel_critical_enter();
  accepted = CommandManager_Get(&command) && SafetyManager_RequestRun(true);
  kernel_critical_exit();
  return accepted;
}

void ControlRuntime_Stop(void) {
  kernel_critical_enter();
  CommandManager_ClearCommand();
  MotorSelfTest_Stop();
  WheelController_Stop();
  FeedbackWatchdog_Reset(&feedback_watchdog);
  SafetyManager_Stop();
  kernel_critical_exit();
}

static void ReleaseMotionOwner(void) {
  const CommandSource owner = CommandManager_GetOwner();

  if (owner == COMMAND_SOURCE_CAN_REMOTE || owner == COMMAND_SOURCE_CONSOLE ||
      owner == COMMAND_SOURCE_SELF_TEST) {
    CommandManager_Release(owner);
  }
}

static bool PowerReady(uint32_t now_ms) {
  PowerSampleSnapshot power_sample = {0};

  power_sample_get_snapshot(DEVICE_DT_GET(DT_CHOSEN(chassis_power)), now_ms,
                            &power_sample);
  return power_sample.valid &&
         power_sample.sample_age_ms <= MOTOR_CONTROL_MAX_SUPPLY_SAMPLE_AGE_MS &&
         power_sample.millivolts >= MOTOR_CONTROL_MIN_SUPPLY_MV;
}

void ControlRuntime_Run(uint32_t notification_count) {
  CommandManagerCommand command;
  int32_t left_delta;
  int32_t left_measurement;
  int32_t right_delta;
  int32_t right_measurement;
  int left_encoder_result;
  int right_encoder_result;
  PowerSampleSnapshot power_sample = {0};
  int64_t max_encoder_delta;
  int64_t left_abs_delta;
  int64_t right_abs_delta;
  bool command_available;
  uint32_t missed_ticks;
  const uint32_t now_ms = time_uptime_ms();

  if (notification_count == 0U) {
    return;
  }
  missed_ticks = notification_count - 1U;
  if (missed_ticks > 0U) {
    BoardHealth_RecordControlOverrun(missed_ticks);
    ++consecutive_control_overruns;
    if (missed_ticks > MOTOR_CONTROL_MAX_MISSED_TICKS ||
        consecutive_control_overruns >=
            MOTOR_CONTROL_MAX_CONSECUTIVE_OVERRUNS) {
      ControlRuntime_LatchInternalFault(CHASSIS_FAULT_CONTROL_OVERRUN);
      return;
    }
  } else {
    consecutive_control_overruns = 0U;
  }

  left_encoder_result =
      __atomic_exchange_n(&inject_left_encoder_failure, false,
                          __ATOMIC_ACQ_REL)
          ? -1
          : encoder_read_delta(left_encoder_device, &left_delta);
  right_encoder_result =
      __atomic_exchange_n(&inject_right_encoder_failure, false,
                          __ATOMIC_ACQ_REL)
          ? -1
          : encoder_read_delta(right_encoder_device, &right_delta);
  if (left_encoder_result < 0 || right_encoder_result < 0) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_ENCODER);
    return;
  }
  max_encoder_delta =
      (int64_t)MOTOR_ENCODER_MAX_DELTA_PER_TICK * notification_count;
  left_abs_delta = left_delta < 0 ? -(int64_t)left_delta : left_delta;
  right_abs_delta = right_delta < 0 ? -(int64_t)right_delta : right_delta;
  if (left_abs_delta > max_encoder_delta ||
      right_abs_delta > max_encoder_delta) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_ENCODER);
    return;
  }
  power_sample_get_snapshot(DEVICE_DT_GET(DT_CHOSEN(chassis_power)), now_ms,
                            &power_sample);
  if ((SafetyManager_IsRunning() || SafetyManager_IsOpenLoopTestRunning()) &&
      (!power_sample.valid ||
       power_sample.sample_age_ms > MOTOR_CONTROL_MAX_SUPPLY_SAMPLE_AGE_MS ||
       power_sample.millivolts < MOTOR_CONTROL_MIN_SUPPLY_MV)) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_UNDERVOLTAGE);
    return;
  }
  if (!Odometry_Update(left_delta, right_delta, now_ms,
                       MOTOR_CONTROL_PERIOD_MS * notification_count)) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
    return;
  }
  left_measurement = left_delta / (int32_t)notification_count;
  right_measurement = right_delta / (int32_t)notification_count;
  ApplyPendingControlParameters();
  if (SafetyManager_IsEmergencyStopLatched()) {
    kernel_critical_enter();
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
    kernel_critical_exit();
    WheelController_EmergencyStop();
    FeedbackWatchdog_Reset(&feedback_watchdog);
    return;
  }
  if (FaultManager_HasCritical()) {
    kernel_critical_enter();
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
    kernel_critical_exit();
    WheelController_EmergencyStop();
    FeedbackWatchdog_Reset(&feedback_watchdog);
    SafetyManager_LatchInternalFault();
    return;
  }
  if (SafetyManager_IsOpenLoopTestRunning()) {
    WheelController_Reset();
    return;
  }
  if (!SafetyManager_IsRunning()) {
    WheelController_Stop();
    FeedbackWatchdog_Reset(&feedback_watchdog);
    return;
  }

  kernel_critical_enter();
  command_available =
      !CommandManager_IsTimedOut(now_ms) && CommandManager_Get(&command);
  if (!command_available) {
    CommandManager_ClearCommand();
    ReleaseMotionOwner();
  }
  kernel_critical_exit();
  if (!command_available) {
    WheelController_Stop();
    FeedbackWatchdog_Reset(&feedback_watchdog);
    SafetyManager_EnterCommandTimeout();
    return;
  }
  if (!WheelController_Update(command.left_target, command.right_target,
                              left_measurement, right_measurement,
                              notification_count)) {
    ControlRuntime_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
    return;
  }
  {
    WheelControllerSnapshot wheels;
    WheelController_GetSnapshot(&wheels);
    const FeedbackWatchdogSample sample = {
        .left_target = wheels.left_target,
        .right_target = wheels.right_target,
        .left_measurement = wheels.left_measurement,
        .right_measurement = wheels.right_measurement,
        .left_output = wheels.left_output,
        .right_output = wheels.right_output,
    };
    if (!FeedbackWatchdog_Update(
            &feedback_watchdog, &sample,
            MOTOR_CONTROL_PERIOD_MS * notification_count)) {
      ControlRuntime_LatchInternalFault(CHASSIS_FAULT_ENCODER);
    }
  }
}

bool ControlRuntime_ArmEncoderReadFailure(bool left) {
  const int16_t output =
      left ? motor_get_applied_duty(drive_device, MOTOR_LEFT)
           : motor_get_applied_duty(drive_device, MOTOR_RIGHT);
  const int32_t magnitude = output < 0 ? -(int32_t)output : (int32_t)output;

  if (!SafetyManager_IsRunning() ||
      magnitude < MOTOR_ENCODER_STARTUP_OUTPUT_THRESHOLD) {
    return false;
  }
  if (left) {
    __atomic_store_n(&inject_left_encoder_failure, true, __ATOMIC_RELEASE);
  } else {
    __atomic_store_n(&inject_right_encoder_failure, true, __ATOMIC_RELEASE);
  }
  return true;
}

bool ControlRuntime_StartMotorSelfTest(MotorSelfTestAction action,
                                       uint32_t now_ms) {
  if (!PowerReady(now_ms) || !SafetyManager_RequestOpenLoopTest()) {
    return false;
  }
  kernel_critical_enter();
  WheelController_Stop();
  kernel_critical_exit();
  if (!MotorSelfTest_Start(action, now_ms)) {
    SafetyManager_Stop();
    return false;
  }
  return true;
}

bool ControlRuntime_AcquireSelfTestLock(void) {
  MotorSelfTestSnapshot motor_test;

  MotorSelfTest_GetSnapshot(&motor_test);
  if (IwdgSelfTest_IsResetRequested() ||
      QspiSelfTest_GetStatus() == QSPI_SELF_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() || ParameterStorage_IsUsingQspi() ||
      OtaSession_IsActive() || OtaUart_IsEnabled() ||
      motor_test.running) {
    return false;
  }
  ControlRuntime_Stop();
  motor_coast_all(drive_device);
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      motor_get_applied_duty(drive_device, MOTOR_LEFT) != 0 ||
      motor_get_applied_duty(drive_device, MOTOR_RIGHT) != 0) {
    return false;
  }
  kernel_critical_enter();
  ReleaseMotionOwner();
  const bool acquired = CommandManager_Acquire(COMMAND_SOURCE_SELF_TEST);
  kernel_critical_exit();
  return acquired;
}

bool ControlRuntime_AcquireOtaLock(void) {
  MotorSelfTestSnapshot motor_test;

  MotorSelfTest_GetSnapshot(&motor_test);
  if (IwdgSelfTest_IsResetRequested() ||
      QspiSelfTest_GetStatus() == QSPI_SELF_TEST_RUNNING ||
      OtaConfirmation_IsUsingQspi() || ParameterStorage_IsUsingQspi() ||
      OtaSession_IsActive() || motor_test.running) {
    return false;
  }
  ControlRuntime_Stop();
  motor_coast_all(drive_device);
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED ||
      motor_get_applied_duty(drive_device, MOTOR_LEFT) != 0 ||
      motor_get_applied_duty(drive_device, MOTOR_RIGHT) != 0) {
    return false;
  }
  kernel_critical_enter();
  ReleaseMotionOwner();
  const bool acquired = CommandManager_Acquire(COMMAND_SOURCE_OTA);
  kernel_critical_exit();
  return acquired;
}

void ControlRuntime_ReleaseOtaLock(void) {
  kernel_critical_enter();
  CommandManager_Release(COMMAND_SOURCE_OTA);
  kernel_critical_exit();
}

void ControlRuntime_ReleaseFinishedSelfTestLock(void) {
  MotorSelfTestSnapshot motor_test;

  if (IwdgSelfTest_IsResetRequested() ||
      QspiSelfTest_GetStatus() == QSPI_SELF_TEST_RUNNING) {
    return;
  }
  MotorSelfTest_GetSnapshot(&motor_test);
  if (motor_test.running) {
    return;
  }
  kernel_critical_enter();
  CommandManager_Release(COMMAND_SOURCE_SELF_TEST);
  kernel_critical_exit();
}

void ControlRuntime_LatchInternalFault(uint32_t fault) {
  FaultManager_Raise(fault);
  SafetyManager_LatchInternalFault();
  kernel_critical_enter();
  CommandManager_ClearCommand();
  ReleaseMotionOwner();
  WheelController_EmergencyStop();
  kernel_critical_exit();
}

static void ApplyPendingControlParameters(void) {
  ParameterSnapshot parameters;

  kernel_critical_enter();
  if (!ParameterManager_ApplyPending(&parameters)) {
    kernel_critical_exit();
    return;
  }
  kernel_critical_exit();
  WheelController_ApplyPidGains(WHEEL_CONTROLLER_LEFT, parameters.left_pid.kp,
                                parameters.left_pid.ki, parameters.left_pid.kd);
  WheelController_ApplyPidGains(WHEEL_CONTROLLER_RIGHT, parameters.right_pid.kp,
                                parameters.right_pid.ki,
                                parameters.right_pid.kd);
}

bool ControlRuntime_ResetOdometry(void) {
  if (SafetyManager_GetState() != CHASSIS_CONTROL_STOPPED) {
    return false;
  }
  kernel_critical_enter();
  Odometry_Reset();
  kernel_critical_exit();
  return true;
}

CommandManagerSubmitResult
ControlRuntime_SubmitMotionCommand(int32_t left_target, int32_t right_target,
                                   CommandSource source, uint32_t now_ms,
                                   bool has_sequence, uint8_t sequence) {
  const CommandManagerCommand command = {
      .left_target = left_target,
      .right_target = right_target,
      .received_ms = now_ms,
      .source = source,
      .sequence = sequence,
      .has_sequence = has_sequence,
  };
  CommandManagerSubmitResult result;

  kernel_critical_enter();
  result = CommandManager_Submit(&command);
  kernel_critical_exit();
  return result;
}

void ControlRuntime_EmergencyStopOutputs(void) {
  motor_emergency_stop(drive_device);
}

void ControlRuntime_CoastOutputs(void) { motor_coast_all(drive_device); }

void ControlRuntime_FatalError(void) {
  motor_emergency_stop(drive_device);
  ControlRuntime_LatchInternalFault(CHASSIS_FAULT_INTERNAL);
}

void ControlRuntime_PanicStopFromException(void) {
  kernel_interrupts_disable();
  motor_emergency_stop(drive_device);
}

bool ControlRuntime_ClearEmergencyStop(void) {
  const struct device *estop = DEVICE_DT_GET(DT_CHOSEN(chassis_estop));

  if (emergency_stop_is_asserted(estop)) {
    return false;
  }
  kernel_critical_enter();
  if (emergency_stop_is_asserted(estop)) {
    kernel_critical_exit();
    return false;
  }
  (void)SafetyManager_ClearEmergencyStop();
  motor_clear_emergency_stop(drive_device);
  kernel_critical_exit();
  ControlRuntime_Stop();
  return true;
}
