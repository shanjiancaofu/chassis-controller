#ifndef QSPI_SELF_TEST_H
#define QSPI_SELF_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  QSPI_SELF_TEST_IDLE = 0,
  QSPI_SELF_TEST_RUNNING,
  QSPI_SELF_TEST_PASSED,
  QSPI_SELF_TEST_FAILED
} QspiSelfTestStatus;

void QspiSelfTest_Init(void);
bool QspiSelfTest_Start(void);
void QspiSelfTest_Run(uint32_t now_ms);
QspiSelfTestStatus QspiSelfTest_GetStatus(void);
bool QspiSelfTest_TakeCompletion(void);
bool QspiSelfTest_HasCriticalFault(void);

#endif
