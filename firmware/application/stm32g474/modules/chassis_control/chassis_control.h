#ifndef CHASSIS_CONTROL_H
#define CHASSIS_CONTROL_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CHASSIS_CONTROL_STOPPED = 0,
  CHASSIS_CONTROL_RUNNING,
  CHASSIS_CONTROL_COMMAND_TIMEOUT,
  CHASSIS_CONTROL_EMERGENCY_STOP,
  CHASSIS_CONTROL_INTERNAL_FAULT,
  CHASSIS_CONTROL_OPEN_LOOP_TEST
} ChassisControlState;

typedef enum {
  CHASSIS_PID_LEFT = 0,
  CHASSIS_PID_RIGHT
} ChassisPidSide;

enum {
  CHASSIS_FAULT_NONE = 0U,
  CHASSIS_FAULT_COMMAND_TIMEOUT = 1U << 0,
  CHASSIS_FAULT_EMERGENCY_STOP = 1U << 1,
  CHASSIS_FAULT_CONTROL_OVERRUN = 1U << 2,
  CHASSIS_FAULT_INTERNAL = 1U << 3
};

typedef struct {
  int32_t left_target;
  int32_t right_target;
  int32_t left_delta;
  int32_t right_delta;
  int32_t left_total;
  int32_t right_total;
  int16_t left_output;
  int16_t right_output;
  ChassisControlState state;
  uint32_t fault_flags;
} ChassisControlStatus;

void ChassisControl_Init(void);
bool ChassisControl_Start(void);
bool ChassisControl_StartOpenLoopTest(int16_t left_duty,
                                      int16_t right_duty,
                                      uint32_t now_ms,
                                      uint32_t duration_ms);
void ChassisControl_Stop(void);
void ChassisControl_SetTargetSpeed(int32_t left_counts_per_tick,
                                   int32_t right_counts_per_tick);
void ChassisControl_NotifyCommandReceived(uint32_t now_ms);
void ChassisControl_Tick10ms(uint32_t now_ms);
bool ChassisControl_ResetEncoderTotals(void);
bool ChassisControl_SetPidGains(ChassisPidSide side, uint16_t kp,
                                uint16_t ki, uint16_t kd);
void ChassisControl_GetPidGains(ChassisPidSide side, uint16_t *kp,
                                uint16_t *ki, uint16_t *kd);
void ChassisControl_EmergencyStopFromIsr(void);
bool ChassisControl_ClearEmergencyStop(void);
void ChassisControl_LatchInternalFault(uint32_t fault);
bool ChassisControl_HasInternalFault(void);
void ChassisControl_GetStatus(ChassisControlStatus *status);

#endif
