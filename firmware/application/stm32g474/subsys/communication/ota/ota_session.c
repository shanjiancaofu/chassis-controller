#include "subsys/communication/ota/ota_session.h"

#include <stddef.h>
#include <string.h>

#include "drivers/flash.h"
#include "subsys/communication/ota/ota_metadata.h"
#include "lib/crc/crc32.h"
#include "config/storage_layout.h"
#include "../../../../../shared/firmware_image.h"

#define OTA_QSPI_DMA_TIMEOUT_MS 500U
#define OTA_QSPI_PROGRAM_TIMEOUT_MS 500U
#define OTA_QSPI_ERASE_TIMEOUT_MS 5000U
#define OTA_PREPARING_TIMEOUT_MS 120000U
#define OTA_PREPARING_PROGRESS_TIMEOUT_MS 6000U
#define OTA_RESET_DELAY_MS 500U
#define OTA_TERMINAL_CLEANUP_TIMEOUT_MS 10000U
#define OTA_VECTOR_SIZE 8U
#define OTA_SRAM_START 0x20000000UL
#define OTA_SRAM_END 0x20020000UL

typedef enum
{
  SESSION_IDLE = 0,
  SESSION_METADATA_A_START,
  SESSION_METADATA_A_WAIT,
  SESSION_METADATA_B_START,
  SESSION_METADATA_B_WAIT,
  SESSION_PREPARE,
  SESSION_ERASE_START,
  SESSION_ERASE_WAIT,
  SESSION_RECEIVING,
  SESSION_WRITE_START,
  SESSION_WRITE_DMA_WAIT,
  SESSION_WRITE_FLASH_WAIT,
  SESSION_FINALIZE,
  SESSION_METADATA_ERASE_START,
  SESSION_METADATA_ERASE_WAIT,
  SESSION_METADATA_PROGRAM_START,
  SESSION_METADATA_PROGRAM_DMA_WAIT,
  SESSION_METADATA_PROGRAM_FLASH_WAIT,
  SESSION_METADATA_VERIFY_START,
  SESSION_METADATA_VERIFY_WAIT,
  SESSION_STAGED,
  SESSION_TERMINAL_WAIT,
  SESSION_ABORTED,
  SESSION_FAILED
} SessionState;

static SessionState state;
static OtaSource source;
static uint8_t session_id;
static uint32_t package_size;
static uint32_t expected_package_crc;
static uint32_t next_offset;
static uint32_t last_activity_ms;
static uint32_t preparing_started_ms;
static uint32_t last_progress_ms;
static uint32_t deadline_ms;
static uint32_t slot_start;
static uint32_t slot_size;
static uint32_t erase_address;
static uint32_t erase_end;
static uint32_t metadata_target_address;
static uint32_t staged_ms;
static OtaSlotId candidate_slot;
static OtaMetadata metadata_a;
static OtaMetadata metadata_b;
static OtaMetadata staged_metadata;
static OtaMetadata verify_metadata;
static OtaImageHeader image_header;
static uint8_t header_bytes[sizeof(OtaImageHeader)];
static uint8_t vector_bytes[OTA_VECTOR_SIZE];
static uint8_t write_buffer[OTA_MESSAGE_DATA_MAX_SIZE];
static uint16_t write_length;
static uint16_t write_position;
static uint16_t program_length;
static Crc32Context package_crc;
static Crc32Context payload_crc;
static OtaResult last_result;
static OtaResponse pending_response;
static bool response_pending;
static bool staged_response_submitted;
static SessionState terminal_state;
static uint32_t terminal_deadline_ms;
static uint32_t current_run_ms;
static bool critical_fault;

static void QueueResponse(OtaSource destination, uint8_t response_session_id,
                          OtaResult result);
static void Fail(OtaResult result);
static void Abort(uint32_t now_ms);
static void EnterTerminalState(SessionState requested_state, OtaResult result,
                               uint32_t now_ms);
static bool DeadlineReached(uint32_t now_ms);
static uint32_t ReadU32(const uint8_t *data);
static void CaptureStoredData(uint32_t offset, const uint8_t *data,
                              uint16_t length);
static OtaResult ValidateCompletedPackage(void);
static bool IsMetadataStateReplaceable(const OtaMetadataSelection *selection);

void OtaSession_Init(void)
{
  state = SESSION_IDLE;
  source = OTA_SOURCE_NONE;
  session_id = 0U;
  package_size = 0U;
  expected_package_crc = 0U;
  next_offset = 0U;
  last_activity_ms = 0U;
  preparing_started_ms = 0U;
  last_progress_ms = 0U;
  response_pending = false;
  staged_response_submitted = false;
  terminal_state = SESSION_IDLE;
  terminal_deadline_ms = 0U;
  current_run_ms = 0U;
  critical_fault = false;
  last_result = OTA_RESULT_OK;
}

bool OtaSession_Submit(const OtaMessage *message, uint32_t now_ms,
                       bool begin_allowed)
{
  current_run_ms = now_ms;
  if (message == NULL || response_pending) {
    return false;
  }

  if (message->type == OTA_MESSAGE_STATUS) {
    if (message->data_length != 0U || message->argument != 0U) {
      QueueResponse(message->source, message->session_id,
                    OTA_RESULT_INVALID_HEADER);
    } else {
      if (OtaSession_IsActive() && message->source == source &&
          message->session_id == session_id) {
        last_activity_ms = now_ms;
      }
      QueueResponse(message->source, message->session_id, last_result);
    }
    return true;
  }
  if (message->type == OTA_MESSAGE_BEGIN) {
    if (OtaSession_IsActive()) {
      QueueResponse(message->source, message->session_id, OTA_RESULT_BUSY);
      return true;
    }
    if (!begin_allowed || message->data_length != sizeof(uint32_t) ||
        message->argument <= sizeof(OtaImageHeader) ||
        message->argument >
            sizeof(OtaImageHeader) + OTA_APPLICATION_SIZE) {
      QueueResponse(message->source, message->session_id,
                    begin_allowed ? OTA_RESULT_INVALID_HEADER
                                  : OTA_RESULT_INVALID_STATE);
      return true;
    }

    source = message->source;
    session_id = message->session_id;
    package_size = message->argument;
    expected_package_crc = ReadU32(message->data);
    next_offset = 0U;
    last_activity_ms = now_ms;
    preparing_started_ms = now_ms;
    last_progress_ms = now_ms;
    last_result = OTA_RESULT_OK;
    memset(header_bytes, 0, sizeof(header_bytes));
    memset(vector_bytes, 0, sizeof(vector_bytes));
    Crc32_Init(&package_crc);
    Crc32_Init(&payload_crc);
    state = SESSION_METADATA_A_START;
    QueueResponse(source, session_id, OTA_RESULT_OK);
    return true;
  }

  if (!OtaSession_IsActive() || message->source != source ||
      message->session_id != session_id) {
    QueueResponse(message->source, message->session_id,
                  OTA_RESULT_INVALID_STATE);
    return true;
  }
  last_activity_ms = now_ms;

  if (message->type == OTA_MESSAGE_ABORT) {
    if (message->data_length != 0U || message->argument != 0U) {
      QueueResponse(source, session_id, OTA_RESULT_INVALID_HEADER);
    } else {
      Abort(now_ms);
      QueueResponse(message->source, message->session_id, OTA_RESULT_OK);
    }
    return true;
  }
  if (message->type == OTA_MESSAGE_DATA) {
    if (state != SESSION_RECEIVING) {
      QueueResponse(source, session_id, OTA_RESULT_BUSY);
    } else if (message->data_length == 0U ||
               message->argument != next_offset ||
               next_offset > package_size ||
               message->data_length > package_size - next_offset) {
      QueueResponse(source, session_id, OTA_RESULT_INVALID_SEQUENCE);
    } else {
      memcpy(write_buffer, message->data, message->data_length);
      write_length = message->data_length;
      write_position = 0U;
      state = SESSION_WRITE_START;
    }
    return true;
  }
  if (message->type == OTA_MESSAGE_END) {
    if (state != SESSION_RECEIVING) {
      QueueResponse(source, session_id, OTA_RESULT_BUSY);
    } else if (message->data_length != sizeof(uint32_t) ||
               message->argument != package_size ||
               ReadU32(message->data) != expected_package_crc ||
               next_offset != package_size) {
      Fail(OTA_RESULT_INVALID_SEQUENCE);
    } else {
      state = SESSION_FINALIZE;
    }
    return true;
  }

  QueueResponse(message->source, message->session_id,
                OTA_RESULT_INVALID_HEADER);
  return true;
}

void OtaSession_Run(uint32_t now_ms)
{
  FlashTransferStatus transfer_status;
  OtaMetadataSelection selection;
  bool flash_busy;
  uint32_t page_remaining;
  uint32_t remaining;

  current_run_ms = now_ms;

  if (state >= SESSION_RECEIVING &&
      state <= SESSION_WRITE_FLASH_WAIT &&
      now_ms - last_activity_ms > OTA_SESSION_TIMEOUT_MS) {
    Fail(OTA_RESULT_TIMEOUT);
  }
  if (state >= SESSION_METADATA_A_START &&
      state <= SESSION_ERASE_WAIT &&
      (now_ms - preparing_started_ms > OTA_PREPARING_TIMEOUT_MS ||
       now_ms - last_progress_ms >
           OTA_PREPARING_PROGRESS_TIMEOUT_MS)) {
    Fail(OTA_RESULT_TIMEOUT);
  }

  switch (state) {
    case SESSION_METADATA_A_START:
      if (flash_read_dma(QSPI_UPGRADE_METADATA_A_START,
                               (uint8_t *)&metadata_a,
                               sizeof(metadata_a))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = SESSION_METADATA_A_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_A_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        last_progress_ms = now_ms;
        state = SESSION_METADATA_B_START;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_B_START:
      if (flash_read_dma(QSPI_UPGRADE_METADATA_B_START,
                               (uint8_t *)&metadata_b,
                               sizeof(metadata_b))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = SESSION_METADATA_B_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_B_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        last_progress_ms = now_ms;
        state = SESSION_PREPARE;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_PREPARE:
      selection = OtaMetadata_SelectLatest(&metadata_a, &metadata_b);
      if (!IsMetadataStateReplaceable(&selection)) {
        Fail(OTA_RESULT_INVALID_STATE);
        break;
      }
      if (selection.valid &&
          selection.metadata->confirmed_slot == OTA_SLOT_A) {
        candidate_slot = OTA_SLOT_B;
        slot_start = QSPI_UPGRADE_SLOT_B_START;
        slot_size = QSPI_UPGRADE_SLOT_B_SIZE;
      } else {
        candidate_slot = OTA_SLOT_A;
        slot_start = QSPI_UPGRADE_SLOT_A_START;
        slot_size = QSPI_UPGRADE_SLOT_A_SIZE;
      }
      if (package_size > slot_size) {
        Fail(OTA_RESULT_NO_SPACE);
        break;
      }
      erase_address = slot_start;
      erase_end = slot_start +
                  ((package_size + QSPI_FLASH_SECTOR_SIZE - 1U) /
                   QSPI_FLASH_SECTOR_SIZE) * QSPI_FLASH_SECTOR_SIZE;
      state = SESSION_ERASE_START;
      break;
    case SESSION_ERASE_START:
      if (erase_address >= erase_end) {
        state = SESSION_RECEIVING;
      } else if (flash_erase_sector(erase_address)) {
        deadline_ms = now_ms + OTA_QSPI_ERASE_TIMEOUT_MS;
        state = SESSION_ERASE_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_ERASE_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail(OTA_RESULT_IO_ERROR);
      } else if (!flash_busy) {
        erase_address += QSPI_FLASH_SECTOR_SIZE;
        last_progress_ms = now_ms;
        state = SESSION_ERASE_START;
      } else if (DeadlineReached(now_ms)) {
        Fail(OTA_RESULT_TIMEOUT);
      }
      break;
    case SESSION_WRITE_START:
      remaining = write_length - write_position;
      page_remaining = QSPI_FLASH_PAGE_SIZE -
                       ((slot_start + next_offset + write_position) %
                        QSPI_FLASH_PAGE_SIZE);
      program_length = (uint16_t)(remaining < page_remaining
                                      ? remaining
                                      : page_remaining);
      if (flash_program_page_dma(
              slot_start + next_offset + write_position,
              &write_buffer[write_position], program_length)) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = SESSION_WRITE_DMA_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_WRITE_DMA_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        deadline_ms = now_ms + OTA_QSPI_PROGRAM_TIMEOUT_MS;
        state = SESSION_WRITE_FLASH_WAIT;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_WRITE_FLASH_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail(OTA_RESULT_IO_ERROR);
      } else if (!flash_busy) {
        write_position += program_length;
        if (write_position < write_length) {
          state = SESSION_WRITE_START;
        } else {
          CaptureStoredData(next_offset, write_buffer, write_length);
          next_offset += write_length;
          state = SESSION_RECEIVING;
          QueueResponse(source, session_id, OTA_RESULT_OK);
        }
      } else if (DeadlineReached(now_ms)) {
        Fail(OTA_RESULT_TIMEOUT);
      }
      break;
    case SESSION_FINALIZE:
      last_result = ValidateCompletedPackage();
      if (last_result != OTA_RESULT_OK) {
        Fail(last_result);
        break;
      }
      selection = OtaMetadata_SelectLatest(&metadata_a, &metadata_b);
      if (selection.valid) {
        staged_metadata = *selection.metadata;
        staged_metadata.sequence = selection.metadata->sequence + 1U;
        metadata_target_address =
            selection.copy == OTA_METADATA_COPY_A
                ? QSPI_UPGRADE_METADATA_B_START
                : QSPI_UPGRADE_METADATA_A_START;
      } else {
        memset(&staged_metadata, 0, sizeof(staged_metadata));
        staged_metadata.magic = OTA_METADATA_MAGIC;
        staged_metadata.format_version = OTA_METADATA_FORMAT_VERSION;
        staged_metadata.record_size = sizeof(OtaMetadata);
        staged_metadata.sequence = 1U;
        staged_metadata.confirmed_slot = OTA_SLOT_NONE;
        metadata_target_address = QSPI_UPGRADE_METADATA_A_START;
      }
      staged_metadata.state = OTA_STATE_STAGED;
      staged_metadata.candidate_slot = candidate_slot;
      staged_metadata.image_size = image_header.payload_size;
      staged_metadata.image_crc32 = image_header.payload_crc32;
      staged_metadata.install_attempts = 0U;
      staged_metadata.trial_boot_count = 0U;
      staged_metadata.last_error = 0U;
      OtaMetadata_UpdateCrc(&staged_metadata);
      if (!OtaMetadata_Validate(&staged_metadata)) {
        Fail(OTA_RESULT_INVALID_STATE);
      } else {
        state = SESSION_METADATA_ERASE_START;
      }
      break;
    case SESSION_METADATA_ERASE_START:
      if (flash_erase_sector(metadata_target_address)) {
        deadline_ms = now_ms + OTA_QSPI_ERASE_TIMEOUT_MS;
        state = SESSION_METADATA_ERASE_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_ERASE_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail(OTA_RESULT_IO_ERROR);
      } else if (!flash_busy) {
        state = SESSION_METADATA_PROGRAM_START;
      } else if (DeadlineReached(now_ms)) {
        Fail(OTA_RESULT_TIMEOUT);
      }
      break;
    case SESSION_METADATA_PROGRAM_START:
      if (flash_program_page_dma(
              metadata_target_address,
              (const uint8_t *)&staged_metadata,
              sizeof(staged_metadata))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = SESSION_METADATA_PROGRAM_DMA_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_PROGRAM_DMA_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        deadline_ms = now_ms + OTA_QSPI_PROGRAM_TIMEOUT_MS;
        state = SESSION_METADATA_PROGRAM_FLASH_WAIT;
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_PROGRAM_FLASH_WAIT:
      if (!flash_is_busy(&flash_busy)) {
        Fail(OTA_RESULT_IO_ERROR);
      } else if (!flash_busy) {
        state = SESSION_METADATA_VERIFY_START;
      } else if (DeadlineReached(now_ms)) {
        Fail(OTA_RESULT_TIMEOUT);
      }
      break;
    case SESSION_METADATA_VERIFY_START:
      if (flash_read_dma(metadata_target_address,
                               (uint8_t *)&verify_metadata,
                               sizeof(verify_metadata))) {
        deadline_ms = now_ms + OTA_QSPI_DMA_TIMEOUT_MS;
        state = SESSION_METADATA_VERIFY_WAIT;
      } else {
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_METADATA_VERIFY_WAIT:
      transfer_status = flash_get_transfer_status();
      if (transfer_status == FLASH_TRANSFER_COMPLETE) {
        if (memcmp(&verify_metadata, &staged_metadata,
                   sizeof(staged_metadata)) == 0 &&
            OtaMetadata_Validate(&verify_metadata)) {
          state = SESSION_STAGED;
          staged_ms = now_ms;
          staged_response_submitted = false;
          QueueResponse(source, session_id, OTA_RESULT_OK);
        } else {
          Fail(OTA_RESULT_IO_ERROR);
        }
      } else if (transfer_status == FLASH_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        flash_abort();
        Fail(OTA_RESULT_IO_ERROR);
      }
      break;
    case SESSION_TERMINAL_WAIT:
      if (flash_get_transfer_status() == FLASH_TRANSFER_BUSY) {
        flash_abort();
      }
      if (flash_is_busy(&flash_busy) && !flash_busy) {
        state = terminal_state;
      } else if (DeadlineReached(now_ms)) {
        critical_fault = true;
      }
      break;
    default:
      break;
  }
}

bool OtaSession_TakeResponse(OtaResponse *response)
{
  if (response == NULL || !response_pending) {
    return false;
  }
  *response = pending_response;
  response_pending = false;
  return true;
}

void OtaSession_ResponseSubmitted(void)
{
  if (state == SESSION_STAGED) {
    staged_response_submitted = true;
  }
}

void OtaSession_AbortSource(OtaSource aborted_source, uint32_t now_ms)
{
  if (OtaSession_IsActive() && source == aborted_source) {
    Abort(now_ms);
  }
}

OtaTransferState OtaSession_GetState(void)
{
  if (state == SESSION_IDLE) {
    return OTA_TRANSFER_IDLE;
  }
  if (state >= SESSION_METADATA_A_START && state <= SESSION_ERASE_WAIT) {
    return OTA_TRANSFER_PREPARING;
  }
  if (state >= SESSION_RECEIVING && state <= SESSION_WRITE_FLASH_WAIT) {
    return OTA_TRANSFER_RECEIVING;
  }
  if (state >= SESSION_FINALIZE && state <= SESSION_METADATA_VERIFY_WAIT) {
    return OTA_TRANSFER_FINALIZING;
  }
  if (state == SESSION_STAGED) {
    return OTA_TRANSFER_STAGED;
  }
  if (state == SESSION_ABORTED ||
      (state == SESSION_TERMINAL_WAIT &&
       terminal_state == SESSION_ABORTED)) {
    return OTA_TRANSFER_ABORTED;
  }
  return OTA_TRANSFER_FAILED;
}

OtaSource OtaSession_GetSource(void)
{
  return source;
}

uint32_t OtaSession_GetNextOffset(void)
{
  return next_offset;
}

bool OtaSession_IsActive(void)
{
  return state != SESSION_IDLE && state != SESSION_ABORTED &&
         state != SESSION_FAILED;
}

bool OtaSession_IsUsingQspi(void)
{
  return state >= SESSION_METADATA_A_START &&
         state <= SESSION_TERMINAL_WAIT;
}

bool OtaSession_HasCriticalFault(void)
{
  return critical_fault;
}

bool OtaSession_IsResetRequested(uint32_t now_ms)
{
  return state == SESSION_STAGED &&
         now_ms - staged_ms >= OTA_RESET_DELAY_MS &&
         staged_response_submitted && !response_pending;
}

static void QueueResponse(OtaSource destination, uint8_t response_session_id,
                          OtaResult result)
{
  pending_response.source = destination;
  pending_response.result = result;
  pending_response.state = OtaSession_GetState();
  pending_response.next_offset = next_offset;
  pending_response.session_id = response_session_id;
  response_pending = true;
  last_result = result;
}

static void Fail(OtaResult result)
{
  EnterTerminalState(SESSION_FAILED, result, current_run_ms);
  QueueResponse(source, session_id, result);
}

static void Abort(uint32_t now_ms)
{
  EnterTerminalState(SESSION_ABORTED, OTA_RESULT_OK, now_ms);
}

static void EnterTerminalState(SessionState requested_state, OtaResult result,
                               uint32_t now_ms)
{
  bool flash_busy = false;

  (void)now_ms;
  if (flash_get_transfer_status() == FLASH_TRANSFER_BUSY) {
    flash_abort();
  }
  terminal_state = requested_state;
  terminal_deadline_ms = now_ms + OTA_TERMINAL_CLEANUP_TIMEOUT_MS;
  last_result = result;
  if (!flash_is_busy(&flash_busy) || flash_busy) {
    state = SESSION_TERMINAL_WAIT;
  } else {
    state = requested_state;
  }
}

static bool DeadlineReached(uint32_t now_ms)
{
  const uint32_t target = state == SESSION_TERMINAL_WAIT
                              ? terminal_deadline_ms
                              : deadline_ms;

  return (int32_t)(now_ms - target) >= 0;
}

static uint32_t ReadU32(const uint8_t *data)
{
  return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
         ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

static void CaptureStoredData(uint32_t offset, const uint8_t *data,
                              uint16_t length)
{
  uint32_t copy_start;
  uint32_t copy_end;

  Crc32_Update(&package_crc, data, length);
  copy_start = offset < sizeof(header_bytes) ? offset
                                             : sizeof(header_bytes);
  copy_end = offset + length < sizeof(header_bytes)
                 ? offset + length
                 : sizeof(header_bytes);
  if (copy_end > copy_start) {
    memcpy(&header_bytes[copy_start], &data[copy_start - offset],
           copy_end - copy_start);
  }

  copy_start = offset > sizeof(header_bytes) ? offset
                                             : sizeof(header_bytes);
  copy_end = offset + length;
  if (copy_end > copy_start) {
    Crc32_Update(&payload_crc, &data[copy_start - offset],
                 copy_end - copy_start);
  }

  copy_start = offset > sizeof(header_bytes) ? offset
                                             : sizeof(header_bytes);
  copy_end = offset + length < sizeof(header_bytes) + sizeof(vector_bytes)
                 ? offset + length
                 : sizeof(header_bytes) + sizeof(vector_bytes);
  if (copy_end > copy_start) {
    memcpy(&vector_bytes[copy_start - sizeof(header_bytes)],
           &data[copy_start - offset], copy_end - copy_start);
  }
}

static OtaResult ValidateCompletedPackage(void)
{
  const uint32_t initial_sp = ReadU32(&vector_bytes[0]);
  const uint32_t reset_handler = ReadU32(&vector_bytes[4]);
  const uint32_t reset_address = reset_handler & ~1UL;

  memcpy(&image_header, header_bytes, sizeof(image_header));
  if (Crc32_Finalize(&package_crc) != expected_package_crc ||
      Crc32_Finalize(&payload_crc) != image_header.payload_crc32) {
    return OTA_RESULT_INVALID_CRC;
  }
  if (image_header.magic != OTA_IMAGE_MAGIC ||
      image_header.format_version != OTA_IMAGE_FORMAT_VERSION ||
      image_header.header_size != sizeof(OtaImageHeader) ||
      Crc32_Calculate(&image_header,
                      sizeof(image_header) -
                          sizeof(image_header.header_crc32)) !=
          image_header.header_crc32 ||
      image_header.flags != OTA_IMAGE_FLAG_NONE ||
      image_header.load_address != OTA_APPLICATION_START ||
      image_header.vector_address != OTA_APPLICATION_START ||
      image_header.payload_size == 0U ||
      image_header.payload_size > OTA_APPLICATION_SIZE ||
      package_size != sizeof(OtaImageHeader) + image_header.payload_size ||
      initial_sp < OTA_SRAM_START || initial_sp > OTA_SRAM_END ||
      initial_sp % 8UL != 0UL || (reset_handler & 1UL) == 0UL ||
      reset_address < OTA_APPLICATION_START ||
      reset_address >= OTA_APPLICATION_START + OTA_APPLICATION_SIZE) {
    return OTA_RESULT_INVALID_HEADER;
  }
  return OTA_RESULT_OK;
}

static bool IsMetadataStateReplaceable(const OtaMetadataSelection *selection)
{
  if (selection == NULL || !selection->valid) {
    return true;
  }
  return OtaMetadata_IsReplaceableState(
      (OtaState)selection->metadata->state);
}
