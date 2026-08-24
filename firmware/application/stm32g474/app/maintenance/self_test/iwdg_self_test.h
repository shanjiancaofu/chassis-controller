#ifndef IWDG_SELF_TEST_H
#define IWDG_SELF_TEST_H

#include <stdbool.h>

void IwdgSelfTest_Init(void);
bool IwdgSelfTest_Start(void);
bool IwdgSelfTest_IsResetRequested(void);

#endif
