#ifndef BSP_FDCAN_H
#define BSP_FDCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

#define BSP_FDCAN_DATA_SIZE 8U

typedef struct {
  uint32_t identifier;
  uint8_t data[BSP_FDCAN_DATA_SIZE];
} BspFdcanFrame;

typedef struct {
  uint32_t last_error_code;
  uint32_t data_last_error_code;
  uint32_t activity;
  uint32_t tx_error_count;
  uint32_t rx_error_count;
  uint32_t error_passive;
  uint32_t warning;
  uint32_t bus_off;
  uint32_t restricted_mode;
  uint32_t rx_fifo_fill;
  uint32_t tx_fifo_free;
} BspFdcanDiagnostics;

HAL_StatusTypeDef BspFdcan_Start(const uint32_t *accepted_standard_ids,
                                 size_t accepted_id_count);
HAL_StatusTypeDef BspFdcan_Restart(void);
HAL_StatusTypeDef BspFdcan_ReadRxFrame(BspFdcanFrame *frame);
HAL_StatusTypeDef BspFdcan_SendFrame(const BspFdcanFrame *frame);
bool BspFdcan_GetDiagnostics(BspFdcanDiagnostics *diagnostics);

#endif
