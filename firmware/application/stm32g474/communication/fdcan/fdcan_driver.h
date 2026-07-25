#ifndef FDCAN_DRIVER_H
#define FDCAN_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "stm32g4xx_hal.h"

typedef enum {
  FDCAN_LINK_READY = 0,
  FDCAN_LINK_PASSED,
  FDCAN_LINK_FAILED
} FdcanLinkStatus;

typedef struct {
  bool enabled;
  uint8_t sequence;
  int16_t left_target;
  int16_t right_target;
} FdcanControlCommand;

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
} FdcanDiagnostics;

HAL_StatusTypeDef FdcanDriver_Init(void);
void FdcanDriver_Run(void);
FdcanLinkStatus FdcanDriver_GetLinkStatus(void);
bool FdcanDriver_TakeControlCommand(FdcanControlCommand *command);
bool FdcanDriver_GetDiagnostics(FdcanDiagnostics *diagnostics);
void FdcanDriver_RequestDiagnosticTransmit(void);

#endif
