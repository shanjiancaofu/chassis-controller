#ifndef BSP_FDCAN_H
#define BSP_FDCAN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BSP_FDCAN_MAX_DATA_SIZE 64U
#define BSP_FDCAN_CONTROL_DATA_SIZE 8U

#define BSP_FDCAN_ERROR_WARNING (1UL << 0)
#define BSP_FDCAN_ERROR_PASSIVE (1UL << 1)
#define BSP_FDCAN_ERROR_BUS_OFF (1UL << 2)
#define BSP_FDCAN_ERROR_PROTOCOL (1UL << 3)
#define BSP_FDCAN_ERROR_RX_FIFO_FULL (1UL << 4)
#define BSP_FDCAN_ERROR_RX_FIFO_LOST (1UL << 5)

typedef struct {
  uint32_t identifier;
  uint8_t length;
  uint8_t data[BSP_FDCAN_MAX_DATA_SIZE];
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

typedef struct {
  uint32_t warning_count;
  uint32_t error_passive_count;
  uint32_t bus_off_count;
  uint32_t protocol_error_count;
  uint32_t rx_fifo_full_count;
  uint32_t rx_fifo_lost_count;
} BspFdcanEventCounters;

bool BspFdcan_Start(const uint32_t *accepted_standard_ids,
                    size_t accepted_id_count);
bool BspFdcan_Restart(void);
bool BspFdcan_TakeRxFrame(BspFdcanFrame *frame);
bool BspFdcan_SendFrame(const BspFdcanFrame *frame);
bool BspFdcan_SendFrameWithTxEvent(const BspFdcanFrame *frame,
                                   uint8_t message_marker);
uint32_t BspFdcan_TakeErrorEvents(void);
void BspFdcan_GetEventCounters(BspFdcanEventCounters *counters);
bool BspFdcan_TakeTxEvent(uint8_t *message_marker);
bool BspFdcan_IsTxIdle(void);
bool BspFdcan_GetDiagnostics(BspFdcanDiagnostics *diagnostics);

#endif
