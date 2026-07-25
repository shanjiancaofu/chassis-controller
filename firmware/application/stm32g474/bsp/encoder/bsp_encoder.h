#ifndef BSP_ENCODER_H
#define BSP_ENCODER_H

#include <stdbool.h>
#include <stdint.h>

void BspEncoder_Init(void);
bool BspEncoder_Start(void);
void BspEncoder_ReadDelta(int32_t *left_delta, int32_t *right_delta);

#endif
