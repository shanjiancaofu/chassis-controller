#ifndef QSPI_TARGET_TEST_H
#define QSPI_TARGET_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  QSPI_TARGET_TEST_IDLE = 0,
  QSPI_TARGET_TEST_RUNNING,
  QSPI_TARGET_TEST_PASSED,
  QSPI_TARGET_TEST_FAILED
} QspiTargetTestStatus;

void QspiTargetTest_Init(void);
bool QspiTargetTest_Start(void);
void QspiTargetTest_Run(uint32_t now_ms);
QspiTargetTestStatus QspiTargetTest_GetStatus(void);
bool QspiTargetTest_TakeCompletion(void);
bool QspiTargetTest_HasCriticalFault(void);

#endif
