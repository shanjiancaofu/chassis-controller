#ifndef CHASSIS_FLASH_H
#define CHASSIS_FLASH_H

#include <stdbool.h>
#include <stdint.h>

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

#endif
