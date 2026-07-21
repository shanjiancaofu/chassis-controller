#ifndef BOARD_SELF_TEST_H
#define BOARD_SELF_TEST_H

#include <stdbool.h>
#include <stdint.h>

bool BoardSelfTest_Init(void);
bool BoardSelfTest_Run(uint32_t now_ms);

#endif
