#ifndef DIAGNOSTIC_REPORT_H
#define DIAGNOSTIC_REPORT_H

#include <stdbool.h>
#include <stdint.h>

void DiagnosticReport_Init(uint32_t now_ms);
void DiagnosticReport_Run(uint32_t now_ms);
void DiagnosticReport_RequestSelfTest(void);
void DiagnosticReport_RequestQspiTest(void);
void DiagnosticReport_RequestIwdgArmed(void);

#endif
