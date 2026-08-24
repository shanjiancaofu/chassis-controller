#ifndef DIFFERENTIAL_DRIVE_H
#define DIFFERENTIAL_DRIVE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int32_t forward_count_sum;
  int32_t turn_count_difference;
} DifferentialDriveMotionDelta;

void DifferentialDrive_FromWheelDeltas(
    int32_t left_delta, int32_t right_delta,
    DifferentialDriveMotionDelta *motion_delta);
bool DifferentialDrive_ToWheelTargets(int32_t forward_component,
                                      int32_t turn_component,
                                      int32_t target_limit,
                                      int32_t *left_target,
                                      int32_t *right_target);

#endif
