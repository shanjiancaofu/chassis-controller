#ifndef PARAMETER_MANAGER_H
#define PARAMETER_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  PARAMETER_WHEEL_LEFT = 0,
  PARAMETER_WHEEL_RIGHT
} ParameterWheel;

typedef struct {
  uint16_t kp;
  uint16_t ki;
  uint16_t kd;
} ParameterPidGains;

typedef struct {
  ParameterPidGains left_pid;
  ParameterPidGains right_pid;
} ParameterSnapshot;

void ParameterManager_Init(const ParameterSnapshot *initial_parameters);
bool ParameterManager_StagePidGains(ParameterWheel wheel, uint16_t kp,
                                    uint16_t ki, uint16_t kd);
bool ParameterManager_ApplyPending(ParameterSnapshot *active_parameters);
void ParameterManager_GetRequested(ParameterSnapshot *parameters);
void ParameterManager_GetActive(ParameterSnapshot *parameters);
void ParameterManager_RestoreDefaults(void);

#endif
