#include "bsp/imu/bsp_icm45686.h"

#include <stddef.h>
#include <string.h>

#include "board/board_config.h"
#include "components/icm45686/icm45686.h"

#define ICM45686_SPI_READ 0x80U
#define ICM45686_MAX_REGISTER_TRANSFER_SIZE 16U
#define ICM45686_SPI_TIMEOUT_MS 2U
#define ICM45686_FIFO_WATERMARK_FRAMES 4U
#define ICM45686_FIFO_BATCH_FRAMES 4U
#define ICM45686_FIFO_TRANSFER_SIZE                                          \
  (1U + ICM45686_FIFO_BATCH_FRAMES * ICM45686_FIFO_FRAME_SIZE)
#define ICM45686_FIFO_POLL_PERIOD_MS 40U
#define ICM45686_DMA_TIMEOUT_MS 10U
#define ICM45686_STARTUP_TIME_MS 70U
#define ICM45686_RETRY_PERIOD_MS 1000U
#define ICM45686_MAX_CONSECUTIVE_ERRORS 5U
#define ICM45686_DEFAULT_SAMPLE_PERIOD_SECONDS 0.01f
#define ICM45686_MIN_SAMPLE_PERIOD_SECONDS 0.002f
#define ICM45686_MAX_SAMPLE_PERIOD_SECONDS 0.05f

typedef enum {
  DMA_IDLE = 0,
  DMA_BUSY,
  DMA_COMPLETE,
  DMA_ERROR
} DmaState;

static const Icm45686Config imu_config = {
    .accel_full_scale = ICM45686_ACCEL_FS_4_G,
    .gyro_full_scale = ICM45686_GYRO_FS_500_DPS,
    .output_data_rate = ICM45686_ODR_100_HZ,
    .low_noise_bandwidth = ICM45686_LN_BW_ODR_DIV_4,
    .data_ready_interrupt_enabled = false,
    .fifo_enabled = true,
    .fifo_watermark_frames = ICM45686_FIFO_WATERMARK_FRAMES,
};

static Icm45686Device imu_device;
static BspIcm45686Snapshot imu_snapshot;
static BspIcm45686SampleSink sample_sink;
static volatile bool fifo_interrupt;
static volatile uint32_t interrupt_count;
static volatile DmaState dma_state;
static uint8_t dma_tx[ICM45686_FIFO_TRANSFER_SIZE];
static uint8_t dma_rx[ICM45686_FIFO_TRANSFER_SIZE];
static uint16_t dma_frame_count;
static uint16_t dma_remaining_frame_count;
static bool drain_pending;
static uint32_t dma_started_ms;
static uint32_t ready_after_ms;
static uint32_t last_attempt_ms;
static uint32_t last_fifo_poll_ms;
static uint32_t consecutive_errors;
static uint16_t last_fifo_timestamp;
static bool fifo_timestamp_valid;

static bool SpiRead(void *context, uint8_t reg, uint8_t *data, size_t length);
static bool SpiWrite(void *context, uint8_t reg, const uint8_t *data,
                     size_t length);
static void DelayMs(void *context, uint32_t delay_ms);
static void DelayUs(void *context, uint32_t delay_us);
static bool ConfigureDevice(uint32_t now_ms);
static bool StartFifoDma(uint32_t now_ms);
static void ProcessDmaFrames(uint32_t now_ms);
static void RecordTransferError(uint32_t now_ms);
static bool FlushFifo(void);
static float GetSamplePeriod(uint16_t timestamp);

bool BspIcm45686_SetSampleSink(const BspIcm45686SampleSink *sink)
{
  if (sink == NULL || sink->reset == NULL || sink->process_sample == NULL) {
    return false;
  }
  sample_sink = *sink;
  return true;
}

void BspIcm45686_Init(uint32_t now_ms)
{
  memset(&imu_snapshot, 0, sizeof(imu_snapshot));
  memset(&imu_device, 0, sizeof(imu_device));
  imu_snapshot.status = BSP_ICM45686_UNINITIALIZED;
  imu_snapshot.fifo_enabled = true;
  fifo_interrupt = false;
  interrupt_count = 0U;
  dma_state = DMA_IDLE;
  dma_frame_count = 0U;
  dma_remaining_frame_count = 0U;
  drain_pending = false;
  ready_after_ms = now_ms;
  last_attempt_ms = now_ms;
  last_fifo_poll_ms = now_ms;
  consecutive_errors = 0U;
  last_fifo_timestamp = 0U;
  fifo_timestamp_valid = false;
  imu_snapshot.sample_period_s = ICM45686_DEFAULT_SAMPLE_PERIOD_SECONDS;
  (void)ConfigureDevice(now_ms);
}

void BspIcm45686_Run(uint32_t now_ms)
{
  const DmaState state = dma_state;

  if (state == DMA_COMPLETE) {
    dma_state = DMA_IDLE;
    ProcessDmaFrames(now_ms);
  } else if (state == DMA_ERROR) {
    dma_state = DMA_IDLE;
    (void)FlushFifo();
    RecordTransferError(now_ms);
  } else if (state == DMA_BUSY &&
             now_ms - dma_started_ms >= ICM45686_DMA_TIMEOUT_MS) {
    if (dma_state == DMA_BUSY) {
      (void)HAL_SPI_Abort(&BOARD_IMU_SPI);
      HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
      dma_state = DMA_IDLE;
      ++imu_snapshot.dma_timeout_count;
      (void)FlushFifo();
      RecordTransferError(now_ms);
    }
  }

  if (imu_snapshot.status == BSP_ICM45686_NOT_FOUND ||
      imu_snapshot.status == BSP_ICM45686_DEGRADED) {
    if (dma_state == DMA_IDLE &&
        now_ms - last_attempt_ms >= ICM45686_RETRY_PERIOD_MS) {
      last_attempt_ms = now_ms;
      (void)ConfigureDevice(now_ms);
    }
    return;
  }
  if (imu_snapshot.status != BSP_ICM45686_READY ||
      dma_state != DMA_IDLE || (int32_t)(now_ms - ready_after_ms) < 0) {
    return;
  }

  const bool interrupted =
      __atomic_exchange_n(&fifo_interrupt, false, __ATOMIC_RELAXED);
  if (!interrupted && !drain_pending &&
      now_ms - last_fifo_poll_ms < ICM45686_FIFO_POLL_PERIOD_MS) {
    return;
  }
  last_fifo_poll_ms = now_ms;
  if (!StartFifoDma(now_ms)) {
    RecordTransferError(now_ms);
  }
}

void BspIcm45686_OnDataReadyInterrupt(void)
{
  (void)__atomic_fetch_add(&interrupt_count, 1U, __ATOMIC_RELAXED);
  __atomic_store_n(&fifo_interrupt, true, __ATOMIC_RELAXED);
}

void BspIcm45686_OnSpiTransferComplete(void)
{
  if (dma_state == DMA_BUSY) {
    HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
    dma_state = DMA_COMPLETE;
  }
}

void BspIcm45686_OnSpiTransferError(void)
{
  if (dma_state == DMA_BUSY) {
    HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
    dma_state = DMA_ERROR;
  }
}

void BspIcm45686_GetSnapshot(BspIcm45686Snapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = imu_snapshot;
    snapshot->interrupt_count =
        __atomic_load_n(&interrupt_count, __ATOMIC_RELAXED);
    snapshot->dma_busy = dma_state == DMA_BUSY;
  }
}

static bool SpiRead(void *context, uint8_t reg, uint8_t *data, size_t length)
{
  uint8_t tx[ICM45686_MAX_REGISTER_TRANSFER_SIZE + 1U] = {0};
  uint8_t rx[ICM45686_MAX_REGISTER_TRANSFER_SIZE + 1U] = {0};
  SPI_HandleTypeDef *spi = context;

  if (spi == NULL || data == NULL || length == 0U ||
      length > ICM45686_MAX_REGISTER_TRANSFER_SIZE) {
    return false;
  }
  tx[0] = reg | ICM45686_SPI_READ;
  HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
      spi, tx, rx, (uint16_t)(length + 1U), ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
  if (status != HAL_OK) {
    return false;
  }
  memcpy(data, &rx[1], length);
  return true;
}

static bool SpiWrite(void *context, uint8_t reg, const uint8_t *data,
                     size_t length)
{
  uint8_t tx[ICM45686_MAX_REGISTER_TRANSFER_SIZE + 1U];
  SPI_HandleTypeDef *spi = context;

  if (spi == NULL || data == NULL || length == 0U ||
      length > ICM45686_MAX_REGISTER_TRANSFER_SIZE) {
    return false;
  }
  tx[0] = reg & (uint8_t)~ICM45686_SPI_READ;
  memcpy(&tx[1], data, length);
  HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_Transmit(
      spi, tx, (uint16_t)(length + 1U), ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
  return status == HAL_OK;
}

static void DelayMs(void *context, uint32_t delay_ms)
{
  (void)context;
  HAL_Delay(delay_ms);
}

static void DelayUs(void *context, uint32_t delay_us)
{
  uint32_t cycles_per_us;
  uint32_t start;

  (void)context;
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U) {
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
  }
  cycles_per_us = SystemCoreClock / 1000000U;
  start = DWT->CYCCNT;
  while ((uint32_t)(DWT->CYCCNT - start) < cycles_per_us * delay_us) {
    __NOP();
  }
}

static bool ConfigureDevice(uint32_t now_ms)
{
  const Icm45686Transport transport = {
      .context = &BOARD_IMU_SPI,
      .read = SpiRead,
      .write = SpiWrite,
      .delay_ms = DelayMs,
      .delay_us = DelayUs,
  };
  Icm45686Result result;

  result = Icm45686_Init(&imu_device, &transport, &imu_config);

  imu_snapshot.sample_valid = false;
  imu_snapshot.who_am_i = imu_device.who_am_i;
  if (result != ICM45686_RESULT_OK) {
    ++imu_snapshot.transfer_error_count;
    imu_snapshot.status =
        result == ICM45686_RESULT_DEVICE_ID_MISMATCH ||
                imu_device.who_am_i != ICM45686_WHO_AM_I_VALUE
            ? BSP_ICM45686_NOT_FOUND
            : BSP_ICM45686_DEGRADED;
    return false;
  }

  if (sample_sink.reset != NULL) {
    sample_sink.reset();
  }
  consecutive_errors = 0U;
  drain_pending = false;
  fifo_interrupt = false;
  fifo_timestamp_valid = false;
  imu_snapshot.sample_period_s = ICM45686_DEFAULT_SAMPLE_PERIOD_SECONDS;
  ready_after_ms = now_ms + ICM45686_STARTUP_TIME_MS;
  last_fifo_poll_ms = now_ms;
  imu_snapshot.status = BSP_ICM45686_READY;
  return true;
}

static bool StartFifoDma(uint32_t now_ms)
{
  Icm45686FifoStatus fifo_status;
  uint16_t available_frames;
  uint16_t transfer_length;

  if (Icm45686_ReadFifoStatus(&imu_device, &fifo_status) !=
      ICM45686_RESULT_OK) {
    return false;
  }
  if (fifo_status.full) {
    ++imu_snapshot.fifo_full_count;
    return FlushFifo();
  }
  if (Icm45686_ReadFifoCount(&imu_device, &available_frames) !=
      ICM45686_RESULT_OK) {
    return false;
  }
  if (available_frames == 0U) {
    drain_pending = false;
    return true;
  }
  dma_frame_count = available_frames > ICM45686_FIFO_BATCH_FRAMES
                        ? ICM45686_FIFO_BATCH_FRAMES
                        : available_frames;
  dma_remaining_frame_count = available_frames - dma_frame_count;
  drain_pending = available_frames > dma_frame_count;
  transfer_length =
      (uint16_t)(1U + dma_frame_count * ICM45686_FIFO_FRAME_SIZE);
  memset(dma_tx, 0, transfer_length);
  memset(dma_rx, 0, transfer_length);
  dma_tx[0] = ICM45686_FIFO_DATA_REGISTER | ICM45686_SPI_READ;
  dma_started_ms = now_ms;
  dma_state = DMA_BUSY;
  HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_RESET);
  if (HAL_SPI_TransmitReceive_DMA(&BOARD_IMU_SPI, dma_tx, dma_rx,
                                 transfer_length) != HAL_OK) {
    HAL_GPIO_WritePin(BOARD_IMU_CS_GPIO_PORT, BOARD_IMU_CS_GPIO_PIN, GPIO_PIN_SET);
    dma_state = DMA_IDLE;
    return false;
  }
  return true;
}

static void ProcessDmaFrames(uint32_t now_ms)
{
  bool parse_error = false;
  bool recovery_failed = false;
  uint16_t valid_frames = 0U;
  uint32_t sample_delay_ms;

  for (uint16_t index = 0U; index < dma_frame_count; ++index) {
    Icm45686FifoSample fifo_sample;
    Icm45686Sample sample;
    BspIcm45686Sample published_sample;
    const uint8_t *frame =
        &dma_rx[1U + index * ICM45686_FIFO_FRAME_SIZE];

    if (Icm45686_ParseFifoFrame(frame, ICM45686_FIFO_FRAME_SIZE,
                               &fifo_sample) != ICM45686_RESULT_OK) {
      ++imu_snapshot.fifo_parse_error_count;
      parse_error = true;
      continue;
    }
    Icm45686_ConvertSample(&imu_device, &fifo_sample.raw, &sample);
    imu_snapshot.sample_period_s = GetSamplePeriod(fifo_sample.timestamp);
    imu_snapshot.fifo_timestamp = fifo_sample.timestamp;
    memcpy(published_sample.accel_mps2, sample.accel_mps2,
           sizeof(published_sample.accel_mps2));
    memcpy(published_sample.gyro_rad_s, sample.gyro_rad_s,
           sizeof(published_sample.gyro_rad_s));
    published_sample.sample_period_s = imu_snapshot.sample_period_s;
    if (sample_sink.process_sample != NULL) {
      sample_sink.process_sample(&published_sample);
    }
    memcpy(imu_snapshot.accel, fifo_sample.raw.accel,
           sizeof(fifo_sample.raw.accel));
    memcpy(imu_snapshot.gyro, fifo_sample.raw.gyro,
           sizeof(fifo_sample.raw.gyro));
    imu_snapshot.temperature = fifo_sample.raw.temperature;
    memcpy(imu_snapshot.accel_mps2, sample.accel_mps2,
           sizeof(sample.accel_mps2));
    memcpy(imu_snapshot.gyro_rad_s, sample.gyro_rad_s,
           sizeof(sample.gyro_rad_s));
    imu_snapshot.temperature_c = sample.temperature_c;
    ++imu_snapshot.sample_count;
    ++valid_frames;
  }
  imu_snapshot.fifo_frame_count += dma_frame_count;
  if (parse_error) {
    recovery_failed = !FlushFifo();
  }
  if (valid_frames > 0U) {
    sample_delay_ms =
        (uint32_t)((float)dma_remaining_frame_count *
                       imu_snapshot.sample_period_s * 1000.0f +
                   0.5f);
    imu_snapshot.last_sample_ms =
        sample_delay_ms <= dma_started_ms ? dma_started_ms - sample_delay_ms
                                          : 0U;
    imu_snapshot.sample_valid = true;
    if (!parse_error) {
      consecutive_errors = 0U;
    }
    if (recovery_failed) {
      RecordTransferError(now_ms);
    }
  } else if (dma_frame_count > 0U) {
    RecordTransferError(now_ms);
  }
}

static bool FlushFifo(void)
{
  ++imu_snapshot.fifo_flush_count;
  drain_pending = false;
  fifo_interrupt = false;
  fifo_timestamp_valid = false;
  if (Icm45686_FlushFifo(&imu_device) == ICM45686_RESULT_OK) {
    return true;
  }
  ++imu_snapshot.fifo_flush_error_count;
  return false;
}

static float GetSamplePeriod(uint16_t timestamp)
{
  float period = ICM45686_DEFAULT_SAMPLE_PERIOD_SECONDS;

  if (fifo_timestamp_valid) {
    const float measured_period = Icm45686_TimestampDeltaSeconds(
        last_fifo_timestamp, timestamp);

    if (measured_period >= ICM45686_MIN_SAMPLE_PERIOD_SECONDS &&
        measured_period <= ICM45686_MAX_SAMPLE_PERIOD_SECONDS) {
      period = measured_period;
    } else {
      ++imu_snapshot.timestamp_error_count;
    }
  }
  last_fifo_timestamp = timestamp;
  fifo_timestamp_valid = true;
  return period;
}

static void RecordTransferError(uint32_t now_ms)
{
  ++imu_snapshot.transfer_error_count;
  if (++consecutive_errors >= ICM45686_MAX_CONSECUTIVE_ERRORS) {
    imu_snapshot.status = BSP_ICM45686_DEGRADED;
    imu_snapshot.sample_valid = false;
    drain_pending = false;
    last_attempt_ms = now_ms;
  }
}
