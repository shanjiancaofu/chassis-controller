#include "tests/target/qspi_target_test.h"

#include <string.h>

#include "bsp/qspi/bsp_qspi_flash.h"
#include "config/storage_layout.h"
#include "modules/diagnostics/board_health.h"

#define QSPI_ERASE_TIMEOUT_MS 5000U
#define QSPI_PROGRAM_TIMEOUT_MS 500U
#define QSPI_TERMINAL_CLEANUP_TIMEOUT_MS 10000U
#define QSPI_DMA_TIMEOUT_MS 500U
#define QSPI_TEST_DATA_SIZE 1024U

typedef enum {
  TEST_IDLE = 0,
  TEST_ERASE_START,
  TEST_ERASE_WAIT,
  TEST_ERASE_READ_START,
  TEST_ERASE_READ_WAIT,
  TEST_ERASE_VERIFY,
  TEST_PROGRAM_START,
  TEST_PROGRAM_DMA_WAIT,
  TEST_PROGRAM_FLASH_WAIT,
  TEST_READ_START,
  TEST_READ_WAIT,
  TEST_VERIFY,
  TEST_TERMINAL_WAIT,
  TEST_PASSED,
  TEST_FAILED
} TestState;

static TestState test_state;
static uint32_t deadline_ms;
static uint32_t test_offset;
static uint8_t read_buffer[QSPI_TEST_DATA_SIZE];
static uint8_t test_pattern[QSPI_TEST_DATA_SIZE];
static bool completion_pending;
static TestState terminal_state;
static uint32_t terminal_deadline_ms;
static uint32_t current_run_ms;
static bool critical_fault;

static bool DeadlineReached(uint32_t now_ms);
static void Complete(TestState final_state);
static void Fail(void);

void QspiTargetTest_Init(void)
{
  size_t index;

  test_state = TEST_IDLE;
  completion_pending = false;
  terminal_state = TEST_IDLE;
  terminal_deadline_ms = 0U;
  current_run_ms = 0U;
  critical_fault = false;
  for (index = 0U; index < sizeof(test_pattern); ++index) {
    test_pattern[index] = (uint8_t)((index * 37U + 0x5AU) & 0xFFU);
  }
}

bool QspiTargetTest_Start(void)
{
  BoardHealthSnapshot health;

  BoardHealth_GetSnapshot(&health);
  if (QspiTargetTest_GetStatus() == QSPI_TARGET_TEST_RUNNING ||
      !health.qspi_id_valid ||
      health.qspi_capacity_bytes != QSPI_FLASH_CAPACITY_BYTES) {
    return false;
  }

  test_offset = 0U;
  completion_pending = false;
  test_state = TEST_ERASE_START;
  return true;
}

void QspiTargetTest_Run(uint32_t now_ms)
{
  BspQspiTransferStatus transfer_status;
  bool flash_busy;
  size_t index;

  current_run_ms = now_ms;

  switch (test_state) {
    case TEST_ERASE_START:
      if (BspQspiFlash_EraseSector(QSPI_TEST_START)) {
        deadline_ms = now_ms + QSPI_ERASE_TIMEOUT_MS;
        test_state = TEST_ERASE_WAIT;
      } else {
        Fail();
      }
      break;
    case TEST_ERASE_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        Fail();
      } else if (!flash_busy) {
        test_state = TEST_ERASE_READ_START;
      } else if (DeadlineReached(now_ms)) {
        Fail();
      }
      break;
    case TEST_ERASE_READ_START:
      if (BspQspiFlash_ReadDma(QSPI_TEST_START, read_buffer,
                               sizeof(read_buffer))) {
        deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        test_state = TEST_ERASE_READ_WAIT;
      } else {
        Fail();
      }
      break;
    case TEST_ERASE_READ_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        test_state = TEST_ERASE_VERIFY;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        BspQspiFlash_Abort();
        Fail();
      }
      break;
    case TEST_ERASE_VERIFY:
      for (index = 0U; index < sizeof(read_buffer); ++index) {
        if (read_buffer[index] != 0xFFU) {
          Fail();
          return;
        }
      }
      test_offset = 0U;
      test_state = TEST_PROGRAM_START;
      break;
    case TEST_PROGRAM_START:
      if (BspQspiFlash_ProgramPageDma(
              QSPI_TEST_START + test_offset, &test_pattern[test_offset],
              QSPI_FLASH_PAGE_SIZE)) {
        deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        test_state = TEST_PROGRAM_DMA_WAIT;
      } else {
        Fail();
      }
      break;
    case TEST_PROGRAM_DMA_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        deadline_ms = now_ms + QSPI_PROGRAM_TIMEOUT_MS;
        test_state = TEST_PROGRAM_FLASH_WAIT;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        BspQspiFlash_Abort();
        Fail();
      }
      break;
    case TEST_PROGRAM_FLASH_WAIT:
      if (!BspQspiFlash_IsBusy(&flash_busy)) {
        Fail();
      } else if (!flash_busy) {
        test_offset += QSPI_FLASH_PAGE_SIZE;
        test_state = test_offset < QSPI_TEST_DATA_SIZE
                         ? TEST_PROGRAM_START
                         : TEST_READ_START;
      } else if (DeadlineReached(now_ms)) {
        Fail();
      }
      break;
    case TEST_READ_START:
      if (BspQspiFlash_ReadDma(QSPI_TEST_START, read_buffer,
                               sizeof(read_buffer))) {
        deadline_ms = now_ms + QSPI_DMA_TIMEOUT_MS;
        test_state = TEST_READ_WAIT;
      } else {
        Fail();
      }
      break;
    case TEST_READ_WAIT:
      transfer_status = BspQspiFlash_GetTransferStatus();
      if (transfer_status == BSP_QSPI_TRANSFER_COMPLETE) {
        test_state = TEST_VERIFY;
      } else if (transfer_status == BSP_QSPI_TRANSFER_FAILED ||
                 DeadlineReached(now_ms)) {
        BspQspiFlash_Abort();
        Fail();
      }
      break;
    case TEST_VERIFY:
      Complete(memcmp(read_buffer, test_pattern, sizeof(test_pattern)) == 0
                   ? TEST_PASSED
                   : TEST_FAILED);
      break;
    case TEST_TERMINAL_WAIT:
      if (BspQspiFlash_GetTransferStatus() == BSP_QSPI_TRANSFER_BUSY) {
        BspQspiFlash_Abort();
      }
      if (BspQspiFlash_IsBusy(&flash_busy) && !flash_busy) {
        Complete(terminal_state);
      } else if (DeadlineReached(now_ms)) {
        critical_fault = true;
      }
      break;
    default:
      break;
  }
}

QspiTargetTestStatus QspiTargetTest_GetStatus(void)
{
  if ((test_state >= TEST_ERASE_START && test_state <= TEST_VERIFY) ||
      test_state == TEST_TERMINAL_WAIT) {
    return QSPI_TARGET_TEST_RUNNING;
  }
  if (test_state == TEST_PASSED) {
    return QSPI_TARGET_TEST_PASSED;
  }
  if (test_state == TEST_FAILED) {
    return QSPI_TARGET_TEST_FAILED;
  }
  return QSPI_TARGET_TEST_IDLE;
}

bool QspiTargetTest_TakeCompletion(void)
{
  const bool completed = completion_pending;

  completion_pending = false;
  return completed;
}

bool QspiTargetTest_HasCriticalFault(void)
{
  return critical_fault;
}

static bool DeadlineReached(uint32_t now_ms)
{
  const uint32_t target = test_state == TEST_TERMINAL_WAIT
                              ? terminal_deadline_ms
                              : deadline_ms;

  return (int32_t)(now_ms - target) >= 0;
}

static void Complete(TestState final_state)
{
  test_state = final_state;
  completion_pending = true;
}

static void Fail(void)
{
  bool flash_busy = false;

  if (BspQspiFlash_GetTransferStatus() == BSP_QSPI_TRANSFER_BUSY) {
    BspQspiFlash_Abort();
  }
  terminal_state = TEST_FAILED;
  terminal_deadline_ms = current_run_ms +
                         QSPI_TERMINAL_CLEANUP_TIMEOUT_MS;
  if (!BspQspiFlash_IsBusy(&flash_busy) || flash_busy) {
    test_state = TEST_TERMINAL_WAIT;
  } else {
    Complete(TEST_FAILED);
  }
}
