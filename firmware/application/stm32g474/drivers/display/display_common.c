#include "drivers/display/lcd.h"

static const DisplayDriverApi *api(const struct device *device)
{
  return device_is_ready(device) ? device->api : NULL;
}

bool display_begin_frame(const struct device *device) { const DisplayDriverApi *d=api(device); return d && d->begin_frame && d->begin_frame(device); }
bool display_transmit_row(const struct device *device,const uint8_t *data,uint16_t size) { const DisplayDriverApi *d=api(device); return d && d->transmit_row && d->transmit_row(device,data,size); }
bool display_row_complete(const struct device *device) { const DisplayDriverApi *d=api(device); return d && d->row_complete && d->row_complete(device); }
bool display_has_error(const struct device *device) { const DisplayDriverApi *d=api(device); return !d || !d->has_error || d->has_error(device); }
void display_end_frame(const struct device *device) { const DisplayDriverApi *d=api(device); if(d&&d->end_frame)d->end_frame(device); }
LcdStatus display_get_status(const struct device *device) { const DisplayDriverApi *d=api(device); return d&&d->get_status?d->get_status(device):LCD_FAILED; }
