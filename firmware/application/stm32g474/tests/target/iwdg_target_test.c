#include "tests/target/iwdg_target_test.h"

#include "drivers/reset/reset.h"
#include "config/target_test_config.h"

static bool reset_requested;

void IwdgTargetTest_Init(void)
{
  reset_requested = false;
}

bool IwdgTargetTest_Start(void)
{
  if (reset_requested) {
    return false;
  }

  Reset_WriteBackupRegister(IWDG_TEST_BACKUP_REGISTER,
                               IWDG_TEST_MARKER);
  reset_requested = true;
  return true;
}

bool IwdgTargetTest_IsResetRequested(void)
{
  return reset_requested;
}
