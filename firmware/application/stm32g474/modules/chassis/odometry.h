#ifndef ODOMETRY_H
#define ODOMETRY_H

#include <stdint.h>

typedef struct {
  int32_t left_delta;
  int32_t right_delta;
  int64_t left_total;
  int64_t right_total;
  int32_t forward_count_sum_delta;
  int32_t turn_count_difference_delta;
  int64_t forward_count_sum_total;
  int64_t turn_count_difference_total;
} OdometrySnapshot;

void Odometry_Init(void);
void Odometry_Update(int32_t left_delta, int32_t right_delta);
void Odometry_Reset(void);
void Odometry_GetSnapshot(OdometrySnapshot *snapshot);

#endif
