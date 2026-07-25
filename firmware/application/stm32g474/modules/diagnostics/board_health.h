#ifndef BOARD_HEALTH_H
#define BOARD_HEALTH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
  bool qspi_read_ok;
  bool qspi_id_valid;
  uint8_t qspi_jedec_id[3];
  uint32_t qspi_capacity_bytes;
  bool iwdg_reset_test_passed;
  bool button_test_passed;
} BoardHealthSnapshot;

void BoardHealth_Init(void);
void BoardHealth_NotifyButtonPressed(void);
void BoardHealth_GetSnapshot(BoardHealthSnapshot *snapshot);

#endif
