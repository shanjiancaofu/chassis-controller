#ifndef CHASSIS_FLASH_H
#define CHASSIS_FLASH_H

#include <stdbool.h>
#include <stdint.h>
#include "device.h"

typedef enum {
  FLASH_TRANSFER_IDLE = 0,
  FLASH_TRANSFER_BUSY,
  FLASH_TRANSFER_COMPLETE,
  FLASH_TRANSFER_FAILED,
} FlashTransferStatus;

bool flash_read_jedec_id(uint8_t jedec_id[3]);
bool flash_read(uint32_t address, uint8_t *data, uint32_t size);
bool flash_read_dma(uint32_t address, uint8_t *data, uint32_t size);
bool flash_erase_sector(uint32_t address);
bool flash_program_page_dma(uint32_t address, const uint8_t *data,
                            uint32_t size);
bool flash_is_busy(bool *busy);
FlashTransferStatus flash_get_transfer_status(void);
void flash_abort(void);

typedef struct {
  bool (*read_jedec_id)(const struct device *device, uint8_t id[3]);
  bool (*read)(const struct device *device, uint32_t address, uint8_t *data, uint32_t size);
  bool (*read_dma)(const struct device *device, uint32_t address, uint8_t *data, uint32_t size);
  bool (*erase_sector)(const struct device *device, uint32_t address);
  bool (*program_page_dma)(const struct device *device, uint32_t address, const uint8_t *data, uint32_t size);
  bool (*is_busy)(const struct device *device, bool *busy);
  FlashTransferStatus (*get_status)(const struct device *device);
  void (*abort)(const struct device *device);
} FlashDriverApi;

#endif
