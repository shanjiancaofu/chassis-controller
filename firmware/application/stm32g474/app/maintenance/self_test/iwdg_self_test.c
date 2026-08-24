#include "app/maintenance/self_test/iwdg_self_test.h"

#include "drivers/reset/reset.h"
#include "config/self_test_config.h"

static bool reset_requested;

void IwdgSelfTest_Init(void)
{
  reset_requested = false;
}

bool IwdgSelfTest_Start(void)
{
  if (reset_requested) {
    return false;
  }

  Reset_WriteBackupRegister(IWDG_TEST_BACKUP_REGISTER,
                               IWDG_TEST_MARKER);
  reset_requested = true;
  return true;
}

bool IwdgSelfTest_IsResetRequested(void)
{
  return reset_requested;
}
