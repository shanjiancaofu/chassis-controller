#ifndef DIAGNOSTIC_REPORT_H
#define DIAGNOSTIC_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "tests/target/motor_target_test.h"

void DiagnosticReport_Init(uint32_t now_ms);
void DiagnosticReport_Run(uint32_t now_ms);
void DiagnosticReport_RequestSelfTest(void);
void DiagnosticReport_RequestQspiTest(void);
void DiagnosticReport_RequestIwdgArmed(void);
void DiagnosticReport_MotorTestResult(MotorTargetTestAction action,
                                      bool accepted);

#endif
