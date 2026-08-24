#include "app/runtime/display_runtime.h"

#include "app/ui/lcd/lcd_status_presenter.h"
#include "devicetree.h"
#include "drivers/button/button.h"
#include "drivers/time.h"

bool DisplayRuntime_Init(void) { return LcdStatusPresenter_Init(); }

void DisplayRuntime_Run(void) {
  const uint32_t now_ms = time_uptime_ms();

  button_run(DEVICE_DT_GET(DT_CHOSEN(chassis_buttons)), now_ms);
  LcdStatusPresenter_Run(now_ms);
}
