#include "drivers/sensor/icm45686_stm32_private.h"

#include <stddef.h>
#include <string.h>

#include "devicetree.h"
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

#define DMA_IDLE ICM45686_DMA_IDLE
#define DMA_BUSY ICM45686_DMA_BUSY
#define DMA_COMPLETE ICM45686_DMA_COMPLETE
#define DMA_ERROR ICM45686_DMA_ERROR

static const Icm45686Config imu_config = {
    .accel_full_scale = ICM45686_ACCEL_FS_4_G,
    .gyro_full_scale = ICM45686_GYRO_FS_500_DPS,
    .output_data_rate = ICM45686_ODR_100_HZ,
    .low_noise_bandwidth = ICM45686_LN_BW_ODR_DIV_4,
    .data_ready_interrupt_enabled = false,
    .fifo_enabled = true,
    .fifo_watermark_frames = ICM45686_FIFO_WATERMARK_FRAMES,
};

static const struct device *DefaultDevice(void)
{
  return DEVICE_DT_GET(DT_NODELABEL(imu0));
}

static Icm45686Stm32Data *DriverData(void)
{
  return (Icm45686Stm32Data *)DefaultDevice()->data;
}

static const Icm45686Stm32Config *DriverConfig(void)
{
  return DefaultDevice()->config;
}

#define imu_device (DriverData()->chip)
#define imu_snapshot (DriverData()->snapshot)
#define sample_sink (DriverData()->sample_sink)
#define fifo_interrupt (DriverData()->fifo_interrupt)
#define driver_interrupt_count (DriverData()->interrupt_count)
#define dma_state (DriverData()->dma_state)
#define dma_tx (DriverData()->dma_tx)
#define dma_rx (DriverData()->dma_rx)
#define dma_frame_count (DriverData()->dma_frame_count)
#define dma_remaining_frame_count (DriverData()->dma_remaining_frame_count)
#define drain_pending (DriverData()->drain_pending)
#define dma_started_ms (DriverData()->dma_started_ms)
#define ready_after_ms (DriverData()->ready_after_ms)
#define last_attempt_ms (DriverData()->last_attempt_ms)
#define last_fifo_poll_ms (DriverData()->last_fifo_poll_ms)
#define consecutive_errors (DriverData()->consecutive_errors)
#define last_fifo_timestamp (DriverData()->last_fifo_timestamp)
#define fifo_timestamp_valid (DriverData()->fifo_timestamp_valid)

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

static bool Icm45686Stm32_SetSampleSink(const Icm45686Stm32SampleSink *sink)
{
  if (sink == NULL || sink->reset == NULL || sink->process_sample == NULL) {
    return false;
  }
  sample_sink = *sink;
  return true;
}

static void Icm45686Stm32_Init(uint32_t now_ms)
{
  memset(DriverData(), 0, sizeof(*DriverData()));
  imu_snapshot.status = ICM45686_UNINITIALIZED;
  imu_snapshot.fifo_enabled = true;
  fifo_interrupt = false;
  driver_interrupt_count = 0U;
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

static void Icm45686Stm32_Run(uint32_t now_ms)
{
  const Icm45686DmaState state = dma_state;

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
      (void)HAL_SPI_Abort(DriverConfig()->spi);
      HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
      dma_state = DMA_IDLE;
      ++imu_snapshot.dma_timeout_count;
      (void)FlushFifo();
      RecordTransferError(now_ms);
    }
  }

  if (imu_snapshot.status == ICM45686_NOT_FOUND ||
      imu_snapshot.status == ICM45686_DEGRADED) {
    if (dma_state == DMA_IDLE &&
        now_ms - last_attempt_ms >= ICM45686_RETRY_PERIOD_MS) {
      last_attempt_ms = now_ms;
      (void)ConfigureDevice(now_ms);
    }
    return;
  }
  if (imu_snapshot.status != ICM45686_READY ||
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

static void Icm45686Stm32_OnDataReadyInterrupt(void)
{
  (void)__atomic_fetch_add(&driver_interrupt_count, 1U, __ATOMIC_RELAXED);
  __atomic_store_n(&fifo_interrupt, true, __ATOMIC_RELAXED);
}

static void Icm45686Stm32_OnSpiTransferComplete(void)
{
  if (dma_state == DMA_BUSY) {
    HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
    dma_state = DMA_COMPLETE;
  }
}

static void Icm45686Stm32_OnSpiTransferError(void)
{
  if (dma_state == DMA_BUSY) {
    HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
    dma_state = DMA_ERROR;
  }
}

static void Icm45686Stm32_GetSnapshot(Icm45686Stm32Snapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = imu_snapshot;
    snapshot->interrupt_count =
        __atomic_load_n(&driver_interrupt_count, __ATOMIC_RELAXED);
    snapshot->dma_busy = dma_state == DMA_BUSY;
  }
}

static bool SetSink(const struct device *device,
                    const Icm45686Stm32SampleSink *sink)
{
  (void)device;
  return Icm45686Stm32_SetSampleSink(sink);
}

static void RunDevice(const struct device *device, uint32_t now_ms)
{
  (void)device;
  Icm45686Stm32_Run(now_ms);
}

static void GetDeviceSnapshot(const struct device *device,
                              Icm45686Stm32Snapshot *snapshot)
{
  (void)device;
  Icm45686Stm32_GetSnapshot(snapshot);
}
static void DataReady(const struct device*d){(void)d;Icm45686Stm32_OnDataReadyInterrupt();}
static void TransferComplete(const struct device*d){(void)d;Icm45686Stm32_OnSpiTransferComplete();}
static void TransferError(const struct device*d){(void)d;Icm45686Stm32_OnSpiTransferError();}

const SensorDriverApi icm45686_stm32_api = {
    .set_sample_sink = SetSink,
    .run = RunDevice,
    .get_snapshot = GetDeviceSnapshot,
    .on_data_ready = DataReady,
    .on_transfer_complete = TransferComplete,
    .on_transfer_error = TransferError,
};

int Icm45686Device_Init(const struct device *device)
{
  if (device == NULL || device != DefaultDevice() || device->data == NULL ||
      device->config == NULL) {
    return -1;
  }
  Icm45686Stm32_Init(0U);
  return 0;
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
  HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
      spi, tx, rx, (uint16_t)(length + 1U), ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
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
  HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_Transmit(
      spi, tx, (uint16_t)(length + 1U), ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
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
      .context = DriverConfig()->spi,
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
            ? ICM45686_NOT_FOUND
            : ICM45686_DEGRADED;
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
  imu_snapshot.status = ICM45686_READY;
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
  HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_RESET);
  if (HAL_SPI_TransmitReceive_DMA(DriverConfig()->spi, dma_tx, dma_rx,
                                 transfer_length) != HAL_OK) {
    HAL_GPIO_WritePin(DriverConfig()->cs_port, DriverConfig()->cs_pin, GPIO_PIN_SET);
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
    Icm45686Stm32Sample published_sample;
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
    imu_snapshot.status = ICM45686_DEGRADED;
    imu_snapshot.sample_valid = false;
    drain_pending = false;
    last_attempt_ms = now_ms;
  }
}
