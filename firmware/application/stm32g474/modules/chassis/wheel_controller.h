#ifndef WHEEL_CONTROLLER_H
#define WHEEL_CONTROLLER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  WHEEL_CONTROLLER_LEFT = 0,
  WHEEL_CONTROLLER_RIGHT
} WheelControllerSide;

typedef struct {
  int32_t left_target;
  int32_t right_target;
  int32_t left_measurement;
  int32_t right_measurement;
  int16_t left_output;
  int16_t right_output;
} WheelControllerSnapshot;

void WheelController_Init(void);
void WheelController_Stop(void);
void WheelController_Reset(void);
void WheelController_EmergencyStop(void);
bool WheelController_Update(int32_t left_target, int32_t right_target,
                            int32_t left_measurement,
                            int32_t right_measurement);
void WheelController_ApplyPidGains(WheelControllerSide side, uint16_t kp,
                                   uint16_t ki, uint16_t kd);
void WheelController_GetSnapshot(WheelControllerSnapshot *snapshot);

#endif
