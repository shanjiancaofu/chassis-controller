#ifndef STATUS_DISPLAY_H
#define STATUS_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>

bool StatusDisplay_Init(void);
void StatusDisplay_Run(uint32_t now_ms);
void StatusDisplay_OnKeyInterrupt(void);

#endif
