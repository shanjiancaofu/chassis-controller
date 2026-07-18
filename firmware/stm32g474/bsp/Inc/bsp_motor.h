#ifndef BSP_MOTOR_H
#define BSP_MOTOR_H

#include <stdbool.h>
#include <stdint.h>

bool BspMotor_Init(void);
void BspMotor_Set(int16_t left_per_mille, int16_t right_per_mille);
void BspMotor_Coast(void);
void BspMotor_ReadEncoderDelta(int16_t *left_count, int16_t *right_count);

#endif
