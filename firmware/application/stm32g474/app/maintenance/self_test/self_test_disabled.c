#include "app/maintenance/self_test/iwdg_self_test.h"
#include "app/maintenance/self_test/motor_self_test.h"
#include "app/maintenance/self_test/qspi_self_test.h"

void IwdgSelfTest_Init(void) {}
bool IwdgSelfTest_Start(void) { return false; }
bool IwdgSelfTest_IsResetRequested(void) { return false; }

void MotorSelfTest_Init(void) {}
bool MotorSelfTest_Start(MotorSelfTestAction action, uint32_t now_ms) {
  (void)action;
  (void)now_ms;
  return false;
}
void MotorSelfTest_Run(uint32_t now_ms) { (void)now_ms; }
void MotorSelfTest_Stop(void) {}
bool MotorSelfTest_SetDuty(uint16_t duty) {
  (void)duty;
  return false;
}
uint16_t MotorSelfTest_GetDuty(void) { return 0U; }
void MotorSelfTest_GetSnapshot(MotorSelfTestSnapshot *snapshot) {
  if (snapshot != 0)
    *snapshot = (MotorSelfTestSnapshot){0};
}

void QspiSelfTest_Init(void) {}
bool QspiSelfTest_Start(void) { return false; }
void QspiSelfTest_Run(uint32_t now_ms) { (void)now_ms; }
QspiSelfTestStatus QspiSelfTest_GetStatus(void) { return QSPI_SELF_TEST_IDLE; }
bool QspiSelfTest_TakeCompletion(void) { return false; }
bool QspiSelfTest_HasCriticalFault(void) { return false; }
