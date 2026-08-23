#include "app/modules/diagnostics/board_health.h"

#include <stddef.h>

#include "drivers/flash.h"
#include "drivers/reset/reset.h"
#include "config/storage_layout.h"
#include "config/target_test_config.h"
#include "../../../../../shared/qspi_flash_identity.h"

static BoardHealthSnapshot board_health;
static uint32_t control_overrun_count;
static uint32_t control_missed_tick_count;

void BoardHealth_Init(void)
{
  const bool marker_present =
      Reset_ReadBackupRegister(IWDG_TEST_BACKUP_REGISTER) ==
      IWDG_TEST_MARKER;

  board_health = (BoardHealthSnapshot){0};
  board_health.reset_cause_flags = Reset_GetCauseFlags();
  __atomic_store_n(&control_overrun_count, 0U, __ATOMIC_RELAXED);
  __atomic_store_n(&control_missed_tick_count, 0U, __ATOMIC_RELAXED);
  board_health.iwdg_reset_test_passed =
      Reset_WasIndependentWatchdog() && marker_present;
  if (marker_present) {
    Reset_WriteBackupRegister(IWDG_TEST_BACKUP_REGISTER, 0U);
  }
  Reset_ClearCauseFlags();

  board_health.qspi_read_ok =
      flash_read_jedec_id(board_health.qspi_jedec_id);
  if (board_health.qspi_read_ok &&
      board_health.qspi_jedec_id[2] >= 0x10U &&
      board_health.qspi_jedec_id[2] <= 0x1FU) {
    board_health.qspi_capacity_bytes =
        1UL << board_health.qspi_jedec_id[2];
  }
  board_health.qspi_id_valid =
      board_health.qspi_read_ok &&
      QspiFlashIdentity_IsSupported(board_health.qspi_jedec_id) &&
      board_health.qspi_capacity_bytes == QSPI_FLASH_CAPACITY_BYTES;
}

void BoardHealth_NotifyButtonPressed(void)
{
  board_health.button_test_passed = true;
}

void BoardHealth_RecordControlOverrun(uint32_t missed_ticks)
{
  (void)__atomic_fetch_add(&control_overrun_count, 1U, __ATOMIC_RELAXED);
  (void)__atomic_fetch_add(&control_missed_tick_count, missed_ticks,
                           __ATOMIC_RELAXED);
}

void BoardHealth_GetSnapshot(BoardHealthSnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = board_health;
    snapshot->control_overrun_count =
        __atomic_load_n(&control_overrun_count, __ATOMIC_RELAXED);
    snapshot->control_missed_tick_count =
        __atomic_load_n(&control_missed_tick_count, __ATOMIC_RELAXED);
  }
}
