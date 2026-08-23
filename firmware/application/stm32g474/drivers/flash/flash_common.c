#include "drivers/flash.h"

#include "drivers/flash/flash_stm32_qspi_private.h"
#include "device.h"
#include "devicetree.h"

static const FlashDriverApi *Api(const struct device **selected)
{
  const struct device *device = DEVICE_DT_GET(DT_CHOSEN(chassis_flash));
  if (selected != NULL) *selected = device;
  return device_is_ready(device) ? device->api : NULL;
}

bool flash_read_jedec_id(uint8_t jedec_id[3])
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->read_jedec_id && api->read_jedec_id(dev, jedec_id);
}

bool flash_read(uint32_t address, uint8_t *data, uint32_t size)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->read && api->read(dev, address, data, size);
}

bool flash_read_dma(uint32_t address, uint8_t *data, uint32_t size)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->read_dma && api->read_dma(dev, address, data, size);
}

bool flash_erase_sector(uint32_t address)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->erase_sector && api->erase_sector(dev, address);
}

bool flash_program_page_dma(uint32_t address, const uint8_t *data, uint32_t size)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->program_page_dma && api->program_page_dma(dev, address, data, size);
}

bool flash_is_busy(bool *busy)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->is_busy && api->is_busy(dev, busy);
}

FlashTransferStatus flash_get_transfer_status(void)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  return api && api->get_status ? api->get_status(dev) : FLASH_TRANSFER_FAILED;
}

void flash_abort(void)
{
  const struct device *dev; const FlashDriverApi *api = Api(&dev);
  if (api && api->abort) api->abort(dev);
}
