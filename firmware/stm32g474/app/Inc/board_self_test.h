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

bool BoardSelfTest_Init(void);
bool BoardSelfTest_Run(uint32_t now_ms);
void BoardSelfTest_GetStatus(BoardSelfTestStatus *status);
void BoardSelfTest_NotifyButtonPressed(void);
bool BoardSelfTest_IsIwdgResetRequested(void);

#endif
