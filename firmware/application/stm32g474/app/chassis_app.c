#include "app/chassis_app.h"

#include <errno.h>

#include "app/runtime/app_bootstrap.h"
#include "app/runtime/control_runtime.h"
#include "app/runtime/diagnostics_runtime.h"
#include "app/runtime/display_runtime.h"
#include "app/runtime/service_runtime.h"
#include "init.h"

bool ChassisApp_Init(void)
{
  return AppBootstrap_Init();
}

static int ChassisAppSystemInit(void)
{
  return ChassisApp_Init() ? 0 : -EIO;
}

SYS_INIT(ChassisAppSystemInit, APPLICATION, 90);

void ChassisApp_RunServiceCycle(void)
{
  ServiceRuntime_Run();
}

void ChassisApp_RunDiagnosticsCycle(void)
{
  DiagnosticsRuntime_Run();
}

void ChassisApp_RunDisplayCycle(void)
{
  DisplayRuntime_Run();
}

void ChassisApp_RunControlCycle(uint32_t notification_count)
{
  ControlRuntime_Run(notification_count);
}

void ChassisApp_FatalError(void)
{
  ControlRuntime_FatalError();
}

void ChassisApp_PanicStopFromException(void)
{
  ControlRuntime_PanicStopFromException();
}

bool ChassisApp_ClearEmergencyStop(void)
{
  return ControlRuntime_ClearEmergencyStop();
}
