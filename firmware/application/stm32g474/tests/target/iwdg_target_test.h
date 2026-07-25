#ifndef IWDG_TARGET_TEST_H
#define IWDG_TARGET_TEST_H

#include <stdbool.h>

void IwdgTargetTest_Init(void);
bool IwdgTargetTest_Start(void);
bool IwdgTargetTest_IsResetRequested(void);

#endif
