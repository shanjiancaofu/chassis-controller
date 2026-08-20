#ifndef CHASSIS_ENCODER_H
#define CHASSIS_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

void Encoder_Init(void);
bool Encoder_Start(void);
void Encoder_ReadDelta(int32_t *left_delta, int32_t *right_delta);

#endif
