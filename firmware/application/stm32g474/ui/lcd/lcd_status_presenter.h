#ifndef LCD_STATUS_PRESENTER_H
#define LCD_STATUS_PRESENTER_H

#include <stdbool.h>
#include <stdint.h>

bool LcdStatusPresenter_Init(void);
void LcdStatusPresenter_Run(uint32_t now_ms);

#endif
