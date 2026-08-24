#include "subsys/communication/ota/ota_confirmation.h"

#include <stddef.h>
#include <string.h>

#include "drivers/flash.h"
#include "subsys/communication/ota/ota_metadata.h"
#include "config/storage_layout.h"
#include "../../../../../shared/ota_metadata.h"

#define OTA_STARTUP_HEALTH_WINDOW_MS 5000U
#define OTA_QSPI_DMA_TIMEOUT_MS 500U
#define OTA_QSPI_PROGRAM_TIMEOUT_MS 500U
#define OTA_QSPI_ERASE_TIMEOUT_MS 5000U
#define OTA_TERMINAL_CLEANUP_TIMEOUT_MS 10000U
#define OTA_CONFIRM_RETRY_DELAY_MS 1000U
#define OTA_CONFIRM_RETRY_LIMIT 3U

typedef enum
{
  CONFIRM_WAIT_HEALTH = 0,
  CONFIRM_READ_A_START,
  CONFIRM_READ_A_WAIT,
  CONFIRM_READ_B_START,
  CONFIRM_READ_B_WAIT,
  CONFIRM_SELECT,
  CONFIRM_ERASE_START,
  CONFIRM_ERASE_WAIT,
  CONFIRM_PROGRAM_START,
  CONFIRM_PROGRAM_DMA_WAIT,
  CONFIRM_PROGRAM_FLASH_WAIT,
  CONFIRM_VERIFY_READ_START,
  CONFIRM_VERIFY_READ_WAIT,
  CONFIRM_VERIFY,
  CONFIRM_TERMINAL_WAIT,
  CONFIRM_RETRY_WAIT,
  CONFIRM_NOT_REQUIRED,
  CONFIRM_DONE,
  CONFIRM_FAILED
} ConfirmationState;

static ConfirmationState state;
static OtaMetadata metadata_a;
static OtaMetadata metadata_b;
static OtaMetadata next_metadata;
static OtaMetadata verify_metadata;
static uint32_t healthy_since_ms;
static uint32_t deadline_ms;
static uint32_t target_address;
static bool health_window_started;
static ConfirmationState terminal_state;
static uint32_t terminal_deadline_ms;
static uint32_t retry_deadline_ms;
static uint32_t current_run_ms;
static uint8_t failure_count;
static bool critical_fault;

static bool DeadlineReached(uint32_t now_ms);
static void Fail(void);
static void FinishFailure(uint32_t now_ms);

void OtaConfirmation_Init(void)
{
  state = CONFIRM_WAIT_HEALTH;
  healthy_since_ms = 0U;
  deadline_ms = 0U;
  target_address = 0U;
  health_window_started = false;
  terminal_state = CONFIRM_WAIT_HEALTH;
  terminal_deadline_ms = 0U;
  retry_deadline_ms = 0U;
  current_run_ms = 0U;
  failure_count = 0U;
  critical_fault = false;
}

void OtaConfirmation_Run(uint32_t now_ms, bool startup_healthy,
                         bool qspi_available)
{
  FlashTransferStatus transfer_status;
  const OtaMetadata *current;
  OtaMetadataSelection selection;
  bool flash_busy;

  current_run_ms = now_ms;

  switch (state) {
    case CONFIRM_WAIT_HEALTH:
      if (!startup_healthy) {
        health_window_started = false;
      } else if (!health_window_started) {
        healthy_since_ms = now_ms;
        health_window_started = true;
      } else if (qspi_available &&
                 now_ms - healthy_since_ms >=
                     OTA_STARTUP_HEALTH_WINDOW_MS) {
        state = CONFIRM_READ_A_START;
      }
      break;
    case CONFIRM_READ_A_START:
      if (!qspi_available) {
        break;
      }
      if (flash_read_dma(QSPI_UPGRADE_METADATA_A_START,
                               (uint8_t *)&metadata_a,
                               sizeof(metadata_a))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = CONFIRM_READ_A_WAIT;
      } else {
        Fail();
      }
      break;
    case CONFIRM_READ_A_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        state = CONFIRM_READ_B_START;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail();
      }
      break;
    case CONFIRM_READ_B_START:
      if (flash_read_dma(QSPI_UPGRADE_METADATA_B_START,
                               (uint8_t *)&metadata_b,
                               sizeof(metadata_b))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = CONFIRM_READ_B_WAIT;
      } else {
        Fail();
      }
      break;
    case CONFIRM_READ_B_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        state = CONFIRM_SELECT;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail();
      }
      break;
    case CONFIRM_SELECT:
      selection = OtaMetadata_SelectLatest(&metadata_a, &metadata_b);
      current = selection.metadata;
      if (current == NULL || current->state != OTA_STATE_TRIAL) {
        state = CONFIRM_NOT_REQUIRED;
        break;
      }
      next_metadata = *current;
      next_metadata.sequence = current->sequence + 1U;
      next_metadata.state = OTA_STATE_CONFIRMED;
      next_metadata.confirmed_slot = current->candidate_slot;
      next_metadata.candidate_slot = OTA_SLOT_NONE;
      next_metadata.trial_boot_count = 0U;
      next_metadata.last_error = 0U;
      OtaMetadata_UpdateCrc(&next_metadata);
      target_address = selection.copy == OTA_METADATA_COPY_A
                           ? QSPI_UPGRADE_METADATA_B_START
                           : QSPI_UPGRADE_METADATA_A_START;
      if (OtaMetadata_Validate(&next_metadata)) {
        state = CONFIRM_ERASE_START;
      } else {
        Fail();
      }
      break;
    case CONFIRM_ERASE_START:
      if (flash_erase_sector(target_address)) {
        deadline_ms = now_ms + OTA_QSPI_ERASE_TIMEOUT_MS;
        state = CONFIRM_ERASE_WAIT;
      } else {
        Fail();
      }
      break;
    case CONFIRM_ERASE_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail();
      } else if (!flash_busy) {
        state = CONFIRM_PROGRAM_START;
      } else if (DeadlineReached(now_ms)) {
        Fail();
      }
      break;
    case CONFIRM_PROGRAM_START:
      if (flash_program_page_dma(
              target_address, (const uint8_t *)&next_metadata,
              sizeof(next_metadata))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = CONFIRM_PROGRAM_DMA_WAIT;
      } else {
        Fail();
      }
      break;
    case CONFIRM_PROGRAM_DMA_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        deadline_ms = now_ms + OTA_QSPI_PROGRAM_TIMEOUT_MS;
        state = CONFIRM_PROGRAM_FLASH_WAIT;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail();
      }
      break;
    case CONFIRM_PROGRAM_FLASH_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail();
      } else if (!flash_busy) {
        state = CONFIRM_VERIFY_READ_START;
      } else if (DeadlineReached(now_ms)) {
        Fail();
      }
      break;
    case CONFIRM_VERIFY_READ_START:
      if (flash_read_dma(target_address,
                               (uint8_t *)&verify_metadata,
                               sizeof(verify_metadata))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = CONFIRM_VERIFY_READ_WAIT;
      } else {
        Fail();
      }
      break;
    case CONFIRM_VERIFY_READ_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        state = CONFIRM_VERIFY;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail();
      }
      break;
    case CONFIRM_VERIFY:
      if (memcmp(&verify_metadata, &next_metadata,
                 sizeof(next_metadata)) == 0 &&
          OtaMetadata_Validate(&verify_metadata)) {
        state = CONFIRM_DONE;
      } else {
        Fail();
      }
      break;
    case CONFIRM_TERMINAL_WAIT:
      if (flash_get_transfer_status() == FLASH_TRANSFER_BUSY) {
        flash_abort();
      }
      if (flash_is_busy(&flash_busy) && !flash_busy) {
        FinishFailure(now_ms);
      } else if ((int32_t)(now_ms - terminal_deadline_ms) >= 0) {
        critical_fault = true;
      }
      break;
    case CONFIRM_RETRY_WAIT:
      if ((int32_t)(now_ms - retry_deadline_ms) >= 0) {
        state = CONFIRM_READ_A_START;
      }
      break;
    default:
      break;
  }
}

OtaConfirmationStatus OtaConfirmation_GetStatus(void)
{
  if (state == CONFIRM_NOT_REQUIRED) {
    return OTA_CONFIRMATION_NOT_REQUIRED;
  }
  if (state == CONFIRM_DONE) {
    return OTA_CONFIRMATION_CONFIRMED;
  }
  if (state == CONFIRM_FAILED) {
    return OTA_CONFIRMATION_FAILED;
  }
  if (state == CONFIRM_WAIT_HEALTH) {
    return OTA_CONFIRMATION_WAITING;
  }
  return OTA_CONFIRMATION_RUNNING;
}

bool OtaConfirmation_IsUsingQspi(void)
{
  return (state >= CONFIRM_READ_A_START && state <= CONFIRM_VERIFY) ||
         state == CONFIRM_TERMINAL_WAIT;
}

bool OtaConfirmation_HasCriticalFault(void)
{
  return critical_fault;
}

static bool DeadlineReached(uint32_t now_ms)
{
  return (int32_t)(now_ms - deadline_ms) >= 0;
}

static void Fail(void)
{
  bool flash_busy = false;

  if (flash_get_transfer_status() == FLASH_TRANSFER_BUSY) {
    flash_abort();
  }
  ++failure_count;
  terminal_state = failure_count < OTA_CONFIRM_RETRY_LIMIT
                       ? CONFIRM_RETRY_WAIT
                       : CONFIRM_FAILED;
  terminal_deadline_ms = current_run_ms +
                         OTA_TERMINAL_CLEANUP_TIMEOUT_MS;
  if (!flash_is_busy(&flash_busy) || flash_busy) {
    state = CONFIRM_TERMINAL_WAIT;
  } else {
    FinishFailure(current_run_ms);
  }
}

static void FinishFailure(uint32_t now_ms)
{
  state = terminal_state;
  if (state == CONFIRM_RETRY_WAIT) {
    retry_deadline_ms = now_ms + OTA_CONFIRM_RETRY_DELAY_MS;
  } else {
    critical_fault = true;
  }
}
