#ifndef SPEED_PID_H
#define SPEED_PID_H

#include <stdbool.h>

typedef struct {
  float kp;
  float ki;
  float kd;
  float integral;
  float previous_measurement;
  float output_limit;
  float integral_limit;
  bool initialized;
} SpeedPid;

void SpeedPid_Init(SpeedPid *pid, float kp, float ki, float kd,
                   float output_limit, float integral_limit);
void SpeedPid_Reset(SpeedPid *pid);
float SpeedPid_Update(SpeedPid *pid, float target, float measurement,
                      float dt_seconds);

#endif
