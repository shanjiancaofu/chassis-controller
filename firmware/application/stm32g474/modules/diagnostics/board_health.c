#include "modules/diagnostics/board_health.h"

#include <stddef.h>

#include "bsp/qspi/bsp_qspi_flash.h"
#include "bsp/reset/bsp_reset.h"
#include "config/storage_layout.h"
#include "config/target_test_config.h"

static BoardHealthSnapshot board_health;

void BoardHealth_Init(void)
{
  const bool marker_present =
      BspReset_ReadBackupRegister(IWDG_TEST_BACKUP_REGISTER) ==
      IWDG_TEST_MARKER;

  board_health = (BoardHealthSnapshot){0};
  board_health.iwdg_reset_test_passed =
      BspReset_WasIndependentWatchdog() && marker_present;
  if (marker_present) {
    BspReset_WriteBackupRegister(IWDG_TEST_BACKUP_REGISTER, 0U);
  }
  BspReset_ClearCauseFlags();

  board_health.qspi_read_ok =
      BspQspiFlash_ReadJedecId(board_health.qspi_jedec_id);
  if (board_health.qspi_read_ok &&
      board_health.qspi_jedec_id[2] >= 0x10U &&
      board_health.qspi_jedec_id[2] <= 0x1FU) {
    board_health.qspi_capacity_bytes =
        1UL << board_health.qspi_jedec_id[2];
  }
  board_health.qspi_id_valid =
      board_health.qspi_read_ok &&
      board_health.qspi_capacity_bytes == QSPI_FLASH_CAPACITY_BYTES;
}

void BoardHealth_NotifyButtonPressed(void)
{
  board_health.button_test_passed = true;
}

void BoardHealth_GetSnapshot(BoardHealthSnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = board_health;
  }
}
