#include "infrastructure/parameter_storage/parameter_storage.h"

#include <stddef.h>
#include <string.h>

#include "bsp/qspi/bsp_qspi_flash.h"
#include "config/storage_layout.h"
#include "infrastructure/parameter_storage/parameter_record.h"

#define PARAMETER_COPY_A_START QSPI_PARAMETERS_START
#define PARAMETER_COPY_B_START (QSPI_PARAMETERS_START + QSPI_FLASH_SECTOR_SIZE)
#define PARAMETER_ERASE_TIMEOUT_MS 5000U
#define PARAMETER_PROGRAM_TIMEOUT_MS 500U
#define PARAMETER_DMA_TIMEOUT_MS 500U
#define PARAMETER_TERMINAL_TIMEOUT_MS 10000U
#define PARAMETER_RETRY_DELAY_MS 1000U
#define PARAMETER_RETRY_LIMIT 3U

#if QSPI_PARAMETERS_SIZE < (2U * QSPI_FLASH_SECTOR_SIZE)
#error "QSPI parameter area must contain two sectors"
#endif

_Static_assert(sizeof(ParameterRecord) <= QSPI_FLASH_PAGE_SIZE,
               "Parameter record exceeds one QSPI page");

typedef enum {
  STORAGE_IDLE = 0,
  STORAGE_ERASE_START,
  STORAGE_ERASE_WAIT,
  STORAGE_PROGRAM_START,
  STORAGE_PROGRAM_DMA_WAIT,
  STORAGE_PROGRAM_FLASH_WAIT,
  STORAGE_VERIFY_READ_START,
  STORAGE_VERIFY_READ_WAIT,
  STORAGE_VERIFY,
  STORAGE_TERMINAL_WAIT,
  STORAGE_RETRY_WAIT
} StorageState;

static StorageState state;
static ParameterStorageSnapshot storage_snapshot;
static ParameterSnapshot requested_parameters;
static ParameterRecord save_record;
static ParameterRecord verify_record;
static ParameterRecordCopy active_copy;
static uint32_t requested_generation;
static uint32_t saving_generation;
static uint32_t target_address;
static uint32_t deadline_ms;
static uint32_t terminal_deadline_ms;
static uint32_t retry_deadline_ms;
static uint8_t retry_count;
static bool has_active_copy;
static bool save_requested;
static bool completion_pending;
static bool completion_success;

static bool DeadlineReached(uint32_t now_ms, uint32_t deadline);
static void StartSave(void);
static void CompleteSave(void);
static void FailSave(uint32_t now_ms);
static void FinishFailure(uint32_t now_ms);

bool ParameterStorage_Init(ParameterSnapshot *restored_parameters)
{
  ParameterRecord record_a;
  ParameterRecord record_b;
  ParameterRecordSelection selection;
  const bool read_a = BspQspiFlash_Read(PARAMETER_COPY_A_START,
                                        (uint8_t *)&record_a, sizeof(record_a));
  const bool read_b = BspQspiFlash_Read(PARAMETER_COPY_B_START,
                                        (uint8_t *)&record_b, sizeof(record_b));

  state = STORAGE_IDLE;
  storage_snapshot = (ParameterStorageSnapshot){
      .status = PARAMETER_STORAGE_DEFAULTS,
  };
  requested_generation = 0U;
  saving_generation = 0U;
  target_address = 0U;
  deadline_ms = 0U;
  terminal_deadline_ms = 0U;
  retry_deadline_ms = 0U;
  retry_count = 0U;
  has_active_copy = false;
  save_requested = false;
  completion_pending = false;
  completion_success = false;

  if (!read_a) {
    memset(&record_a, 0, sizeof(record_a));
  }
  if (!read_b) {
    memset(&record_b, 0, sizeof(record_b));
  }
  selection = ParameterRecord_SelectLatest(&record_a, &record_b);
  if (!selection.valid) {
    if (!read_a && !read_b) {
      storage_snapshot.status = PARAMETER_STORAGE_ERROR;
      storage_snapshot.error_count = 1U;
    }
    return false;
  }

  has_active_copy = true;
  active_copy = selection.copy;
  storage_snapshot.status = PARAMETER_STORAGE_LOADED;
  storage_snapshot.sequence = selection.record->sequence;
  if (restored_parameters != NULL) {
    *restored_parameters = selection.record->parameters;
  }
  return true;
}

bool ParameterStorage_RequestSave(const ParameterSnapshot *parameters)
{
  if (!ParameterRecord_ParametersValid(parameters)) {
    return false;
  }

  requested_parameters = *parameters;
  ++requested_generation;
  save_requested = true;
  storage_snapshot.dirty = true;
  if (storage_snapshot.status == PARAMETER_STORAGE_ERROR) {
    retry_count = 0U;
  }
  return true;
}

void ParameterStorage_Run(uint32_t now_ms, bool qspi_available)
{
  BspQspiTransferStatus transfer_status;
  bool flash_busy;

  switch (state) {
    case STORAGE_IDLE:
      if (save_requested && qspi_available) {
        StartSave();
      }
      break;
    case STORAGE_ERASE_START:
      if (BspQspiFlash_EraseSector(target_address)) {
        deadline_ms = now_ms + PARAMETER_ERASE_TIMEOUT_MS;
        state = STORAGE_ERASE_WAIT;
      } else {
        FailSave(now_ms);
      }
      break;
    case STORAGE_ERASE_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        FailSave(now_ms);
      } else if (!flash_busy) {
        state = STORAGE_PROGRAM_START;
      } else if (DeadlineReached(now_ms, deadline_ms)) {
        FailSave(now_ms);
      }
      break;
    case STORAGE_PROGRAM_START:
      if (BspQspiFlash_ProgramPageDma(target_address,
                                      (const uint8_t *)&save_record,
                                      sizeof(save_record))) {
        deadline_ms = now_ms + PARAMETER_DMA_TIMEOUT_MS;
        state = STORAGE_PROGRAM_DMA_WAIT;
      } else {
        FailSave(now_ms);
      }
      break;
    case STORAGE_PROGRAM_DMA_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        deadline_ms = now_ms + PARAMETER_PROGRAM_TIMEOUT_MS;
        state = STORAGE_PROGRAM_FLASH_WAIT;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 DeadlineReached(now_ms, deadline_ms)) {
        BspQspiFlash_Abort();
        FailSave(now_ms);
      }
      break;
    case STORAGE_PROGRAM_FLASH_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        FailSave(now_ms);
      } else if (!flash_busy) {
        state = STORAGE_VERIFY_READ_START;
      } else if (DeadlineReached(now_ms, deadline_ms)) {
        FailSave(now_ms);
      }
      break;
    case STORAGE_VERIFY_READ_START:
      if (BspQspiFlash_ReadDma(target_address, (uint8_t *)&verify_record,
                               sizeof(verify_record))) {
        deadline_ms = now_ms + PARAMETER_DMA_TIMEOUT_MS;
        state = STORAGE_VERIFY_READ_WAIT;
      } else {
        FailSave(now_ms);
      }
      break;
    case STORAGE_VERIFY_READ_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        state = STORAGE_VERIFY;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 DeadlineReached(now_ms, deadline_ms)) {
        BspQspiFlash_Abort();
        FailSave(now_ms);
      }
      break;
    case STORAGE_VERIFY:
      if (memcmp(&verify_record, &save_record, sizeof(save_record)) == 0 &&
          ParameterRecord_Validate(&verify_record)) {
        CompleteSave();
      } else {
        FailSave(now_ms);
      }
      break;
    case STORAGE_TERMINAL_WAIT:
      if (BspQspiFlash_GetTransferStatus() == BSP_QSPI_TRANSFER_BUSY) {
        BspQspiFlash_Abort();
      }
      if (BspQspiFlash_IsBusy(&flash_busy) && !flash_busy) {
        FinishFailure(now_ms);
      } else if (DeadlineReached(now_ms, terminal_deadline_ms)) {
        if (storage_snapshot.status != PARAMETER_STORAGE_ERROR) {
          storage_snapshot.status = PARAMETER_STORAGE_ERROR;
          save_requested = false;
          storage_snapshot.dirty = false;
          completion_pending = true;
          completion_success = false;
          retry_count = PARAMETER_RETRY_LIMIT;
        }
      }
      break;
    case STORAGE_RETRY_WAIT:
      if (qspi_available && DeadlineReached(now_ms, retry_deadline_ms)) {
        StartSave();
      }
      break;
    default:
      state = STORAGE_IDLE;
      break;
  }
}

bool ParameterStorage_IsUsingQspi(void)
{
  return state >= STORAGE_ERASE_START && state <= STORAGE_TERMINAL_WAIT;
}

bool ParameterStorage_TakeCompletion(bool *success)
{
  const bool pending = completion_pending;

  if (pending) {
    completion_pending = false;
    if (success != NULL) {
      *success = completion_success;
    }
  }
  return pending;
}

void ParameterStorage_GetSnapshot(ParameterStorageSnapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = storage_snapshot;
  }
}

static bool DeadlineReached(uint32_t now_ms, uint32_t deadline)
{
  return (int32_t)(now_ms - deadline) >= 0;
}

static void StartSave(void)
{
  const uint32_t next_sequence = storage_snapshot.sequence + 1U;

  saving_generation = requested_generation;
  ParameterRecord_Prepare(&save_record, next_sequence, &requested_parameters);
  target_address = !has_active_copy || active_copy == PARAMETER_RECORD_COPY_B
                       ? PARAMETER_COPY_A_START
                       : PARAMETER_COPY_B_START;
  storage_snapshot.status = PARAMETER_STORAGE_SAVING;
  storage_snapshot.dirty = true;
  state = STORAGE_ERASE_START;
}

static void CompleteSave(void)
{
  has_active_copy = true;
  active_copy = target_address == PARAMETER_COPY_A_START
                    ? PARAMETER_RECORD_COPY_A
                    : PARAMETER_RECORD_COPY_B;
  storage_snapshot.sequence = save_record.sequence;
  storage_snapshot.status = PARAMETER_STORAGE_STORED;
  retry_count = 0U;
  if (saving_generation == requested_generation) {
    save_requested = false;
  }
  storage_snapshot.dirty = save_requested;
  completion_pending = true;
  completion_success = true;
  state = STORAGE_IDLE;
}

static void FailSave(uint32_t now_ms)
{
  bool flash_busy = false;

  if (BspQspiFlash_GetTransferStatus() == BSP_QSPI_TRANSFER_BUSY) {
    BspQspiFlash_Abort();
  }
  ++storage_snapshot.error_count;
  ++retry_count;
  terminal_deadline_ms = now_ms + PARAMETER_TERMINAL_TIMEOUT_MS;
  if (!BspQspiFlash_IsBusy(&flash_busy) || flash_busy) {
    state = STORAGE_TERMINAL_WAIT;
  } else {
    FinishFailure(now_ms);
  }
}

static void FinishFailure(uint32_t now_ms)
{
  if (retry_count < PARAMETER_RETRY_LIMIT) {
    retry_deadline_ms = now_ms + PARAMETER_RETRY_DELAY_MS;
    state = STORAGE_RETRY_WAIT;
  } else {
    storage_snapshot.status = PARAMETER_STORAGE_ERROR;
    save_requested = false;
    storage_snapshot.dirty = false;
    completion_pending = true;
    completion_success = false;
    state = STORAGE_IDLE;
  }
}
