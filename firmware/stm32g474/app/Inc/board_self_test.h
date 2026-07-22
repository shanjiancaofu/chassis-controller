#ifndef BOARD_SELF_TEST_H
#define BOARD_SELF_TEST_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  bool qspi_id_valid;
  uint32_t qspi_capacity_bytes;
  bool iwdg_reset_test_passed;
} BoardSelfTestStatus;

typedef enum
{
  BOARD_MOTOR_TEST_NONE = 0,
  BOARD_MOTOR_TEST_STOP,
  BOARD_MOTOR_TEST_LEFT_FORWARD,
  BOARD_MOTOR_TEST_LEFT_REVERSE,
  BOARD_MOTOR_TEST_RIGHT_FORWARD,
  BOARD_MOTOR_TEST_RIGHT_REVERSE
} BoardMotorTestRequest;

bool BoardSelfTest_Init(void);
bool BoardSelfTest_Run(uint32_t now_ms);
void BoardSelfTest_GetStatus(BoardSelfTestStatus *status);
void BoardSelfTest_NotifyButtonPressed(void);
bool BoardSelfTest_IsIwdgResetRequested(void);
BoardMotorTestRequest BoardSelfTest_TakeMotorTestRequest(void);
void BoardSelfTest_ReportMotorTestResult(BoardMotorTestRequest request,
                                         bool accepted);

#endif
