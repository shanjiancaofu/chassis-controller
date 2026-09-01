#include "lib/icm45686/icm45686.h"

#include <string.h>

#define REG_ACCEL_DATA_X1 0x00U
#define REG_FIFO_COUNT_0 0x12U
#define REG_FIFO_DATA 0x14U
#define REG_PWR_MGMT0 0x10U
#define REG_INT1_CONFIG0 0x16U
#define REG_INT1_CONFIG1 0x17U
#define REG_INT1_CONFIG2 0x18U
#define REG_INT1_STATUS0 0x19U
#define REG_ACCEL_CONFIG0 0x1BU
#define REG_GYRO_CONFIG0 0x1CU
#define REG_FIFO_CONFIG0 0x1DU
#define REG_FIFO_CONFIG1_0 0x1EU
#define REG_FIFO_CONFIG2 0x20U
#define REG_FIFO_CONFIG3 0x21U
#define REG_FIFO_CONFIG4 0x22U
#define REG_TMST_WOM_CONFIG 0x23U
#define REG_INTF_CONFIG1_OVRD 0x2DU
#define REG_DRIVE_CONFIG0 0x32U
#define REG_IREG_ADDR_15_8 0x7CU
#define REG_IREG_DATA 0x7EU
#define REG_WHO_AM_I 0x72U
#define REG_MISC2 0x7FU
#define MREG_SMC_CONTROL_0 0xA258U
#define MREG_SREG_CTRL 0xA267U
#define MREG_GYRO_UI_LPFBW 0xA4ACU
#define MREG_ACCEL_UI_LPFBW 0xA583U

#define SOFT_RESET 0x02U
#define RESET_DONE 0x80U
#define INT1_DRDY_ENABLE 0x04U
#define INT1_FIFO_THRESHOLD_ENABLE 0x02U
#define INT1_FIFO_FULL_ENABLE 0x01U
#define INT1_ACTIVE_HIGH_PULSE_PUSH_PULL 0x01U
#define SPI_SLEW_MASK 0x0EU
#define SPI_SLEW_TYPICAL_10_NS 0x04U
#define ACCEL_GYRO_LOW_NOISE 0x0FU
#define SENSOR_DATA_LENGTH 14U
#define FIFO_MODE_MASK 0x03U
#define FIFO_MODE_STREAM 0x01U
#define FIFO_DEPTH_MASK 0xFCU
#define FIFO_DEPTH_MAX (0x1EU << 2)
#define FIFO_WATERMARK_GE 0x08U
#define FIFO_FLUSH 0x80U
#define FIFO_ACCEL_GYRO 0x06U
#define FIFO_TIMESTAMP_ENABLE 0x02U
#define SMC_TIMESTAMP_ENABLE 0x01U
#define SREG_DATA_ENDIAN_MASK 0x02U
#define SREG_DATA_BIG_ENDIAN 0x02U
#define LOW_NOISE_BANDWIDTH_MASK 0x07U
#define TIMESTAMP_CONFIG_MASK 0x60U
#define TIMESTAMP_RESOLUTION_16_US 0x20U
#define FIFO_STATUS_FULL 0x01U
#define FIFO_STATUS_THRESHOLD 0x02U
#define FIFO_FLUSH_POLL_US 100U
#define FIFO_FLUSH_TIMEOUT_US 10000U
#define TIMESTAMP_TICK_SECONDS 0.000016f
#define FIFO_HEADER_ACCEL_GYRO 0x60U
#define FIFO_HEADER_TIMESTAMP 0x08U
#define FIFO_HEADER_HIGH_RESOLUTION 0x10U
#define FIFO_HEADER_EXTENDED 0x80U
#define FIFO_MAX_FRAME_COUNT 512U
#define STANDARD_GRAVITY_MPS2 9.80665f
#define DEGREES_TO_RADIANS 0.01745329251994329577f
#define SENSOR_COUNTS 32768.0f

static bool IsConfigValid(const Icm45686Config *config);
static Icm45686Result Read(const Icm45686Device *device, uint8_t reg,
                           uint8_t *data, size_t length);
static Icm45686Result Write(const Icm45686Device *device, uint8_t reg,
                            uint8_t value);
static Icm45686Result WriteBuffer(const Icm45686Device *device, uint8_t reg,
                                  const uint8_t *data, size_t length);
static Icm45686Result Update(const Icm45686Device *device, uint8_t reg,
                             uint8_t mask, uint8_t value);
static Icm45686Result ReadMreg(const Icm45686Device *device, uint16_t reg,
                              uint8_t *data, size_t length);
static Icm45686Result WriteMreg(const Icm45686Device *device, uint16_t reg,
                               const uint8_t *data, size_t length);
static Icm45686Result UpdateMreg(const Icm45686Device *device, uint16_t reg,
                                uint8_t mask, uint8_t value);
static Icm45686Result SoftReset(Icm45686Device *device);
static Icm45686Result ConfigureFifo(Icm45686Device *device);
static int16_t DecodeBigEndian(const uint8_t *data);
static float AccelRangeG(Icm45686AccelFullScale full_scale);
static float GyroRangeDps(Icm45686GyroFullScale full_scale);

Icm45686Result Icm45686_Init(Icm45686Device *device,
                             const Icm45686Transport *transport,
                             const Icm45686Config *config)
{
  Icm45686Result result;
  uint8_t who_am_i = 0U;

  if (device == NULL || transport == NULL || transport->read == NULL ||
      transport->write == NULL || transport->delay_ms == NULL ||
      transport->delay_us == NULL ||
      !IsConfigValid(config)) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }

  memset(device, 0, sizeof(*device));
  device->transport = *transport;
  device->config = *config;

  result = Update(device, REG_DRIVE_CONFIG0, SPI_SLEW_MASK,
                  SPI_SLEW_TYPICAL_10_NS);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->transport.delay_ms(device->transport.context, 1U);

  result = Read(device, REG_WHO_AM_I, &who_am_i, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->who_am_i = who_am_i;
  if (who_am_i != ICM45686_WHO_AM_I_VALUE) {
    return ICM45686_RESULT_DEVICE_ID_MISMATCH;
  }

  result = SoftReset(device);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  {
    uint8_t sreg_control;

    result = ReadMreg(device, MREG_SREG_CTRL, &sreg_control, 1U);
    if (result != ICM45686_RESULT_OK) {
      return result;
    }
    sreg_control =
        (sreg_control & (uint8_t)~SREG_DATA_ENDIAN_MASK) |
        SREG_DATA_BIG_ENDIAN;
    result = WriteMreg(device, MREG_SREG_CTRL, &sreg_control, 1U);
    if (result != ICM45686_RESULT_OK) {
      return result;
    }
    result = ReadMreg(device, MREG_SREG_CTRL, &sreg_control, 1U);
    if (result != ICM45686_RESULT_OK) {
      return result;
    }
    if ((sreg_control & SREG_DATA_ENDIAN_MASK) != SREG_DATA_BIG_ENDIAN) {
      return ICM45686_RESULT_CONFIGURATION_FAILED;
    }
  }
  result = Update(device, REG_ACCEL_CONFIG0, 0x7FU,
                  ((uint8_t)config->accel_full_scale << 4) |
                      (uint8_t)config->output_data_rate);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_GYRO_CONFIG0,
                 ((uint8_t)config->gyro_full_scale << 4) |
                     (uint8_t)config->output_data_rate);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = UpdateMreg(device, MREG_ACCEL_UI_LPFBW,
                      LOW_NOISE_BANDWIDTH_MASK,
                      (uint8_t)config->low_noise_bandwidth);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = UpdateMreg(device, MREG_GYRO_UI_LPFBW,
                      LOW_NOISE_BANDWIDTH_MASK,
                      (uint8_t)config->low_noise_bandwidth);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_INT1_CONFIG2, 0x07U,
                  INT1_ACTIVE_HIGH_PULSE_PUSH_PULL);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_INT1_CONFIG1, 0U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(
      device, REG_INT1_CONFIG0,
      config->fifo_enabled
          ? INT1_FIFO_THRESHOLD_ENABLE | INT1_FIFO_FULL_ENABLE
          : (config->data_ready_interrupt_enabled ? INT1_DRDY_ENABLE : 0U));
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_PWR_MGMT0, 0x0FU, ACCEL_GYRO_LOW_NOISE);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->transport.delay_ms(device->transport.context, 1U);
  result = ConfigureFifo(device);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->initialized = true;
  return ICM45686_RESULT_OK;
}

Icm45686Result Icm45686_ReadFifoCount(Icm45686Device *device,
                                     uint16_t *frame_count)
{
  uint8_t data[2];
  Icm45686Result result;

  if (device == NULL || frame_count == NULL || !device->initialized ||
      !device->config.fifo_enabled) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  result = Read(device, REG_FIFO_COUNT_0, data, sizeof(data));
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Read(device, REG_FIFO_COUNT_0, data, sizeof(data));
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  *frame_count = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
  return ICM45686_RESULT_OK;
}

Icm45686Result Icm45686_ReadFifoStatus(Icm45686Device *device,
                                      Icm45686FifoStatus *status)
{
  uint8_t interrupt_status;
  Icm45686Result result;

  if (device == NULL || status == NULL || !device->initialized ||
      !device->config.fifo_enabled) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  result = Read(device, REG_INT1_STATUS0, &interrupt_status, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  status->full = (interrupt_status & FIFO_STATUS_FULL) != 0U;
  status->threshold = (interrupt_status & FIFO_STATUS_THRESHOLD) != 0U;
  return ICM45686_RESULT_OK;
}

Icm45686Result Icm45686_FlushFifo(Icm45686Device *device)
{
  uint8_t fifo_config;
  Icm45686Result result;

  if (device == NULL || !device->initialized ||
      !device->config.fifo_enabled) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  result = Update(device, REG_FIFO_CONFIG2, FIFO_FLUSH, FIFO_FLUSH);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->transport.delay_us(device->transport.context, 10U);
  for (uint32_t waited_us = 0U; waited_us < FIFO_FLUSH_TIMEOUT_US;
       waited_us += FIFO_FLUSH_POLL_US) {
    result = Read(device, REG_FIFO_CONFIG2, &fifo_config, 1U);
    if (result != ICM45686_RESULT_OK) {
      return result;
    }
    if ((fifo_config & FIFO_FLUSH) == 0U) {
      return ICM45686_RESULT_OK;
    }
    device->transport.delay_us(device->transport.context,
                               FIFO_FLUSH_POLL_US);
  }
  return ICM45686_RESULT_TIMEOUT;
}

Icm45686Result Icm45686_ParseFifoFrame(const uint8_t *frame, size_t length,
                                      Icm45686FifoSample *sample)
{
  if (frame == NULL || sample == NULL || length != ICM45686_FIFO_FRAME_SIZE ||
      (frame[0] & FIFO_HEADER_ACCEL_GYRO) != FIFO_HEADER_ACCEL_GYRO ||
      (frame[0] & FIFO_HEADER_TIMESTAMP) == 0U ||
      (frame[0] & (FIFO_HEADER_HIGH_RESOLUTION | FIFO_HEADER_EXTENDED)) != 0U) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    sample->raw.accel[axis] = DecodeBigEndian(&frame[1U + axis * 2U]);
    sample->raw.gyro[axis] = DecodeBigEndian(&frame[7U + axis * 2U]);
  }
  sample->raw.temperature = (int16_t)((int16_t)(int8_t)frame[13] * 256);
  sample->timestamp =
      (uint16_t)(((uint16_t)frame[14] << 8) | (uint16_t)frame[15]);
  return ICM45686_RESULT_OK;
}

float Icm45686_TimestampDeltaSeconds(uint16_t previous, uint16_t current)
{
  return (float)(uint16_t)(current - previous) * TIMESTAMP_TICK_SECONDS;
}

Icm45686Result Icm45686_ReadRaw(Icm45686Device *device,
                                Icm45686RawSample *sample)
{
  uint8_t data[SENSOR_DATA_LENGTH];
  Icm45686Result result;

  if (device == NULL || sample == NULL || !device->initialized) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  result = Read(device, REG_ACCEL_DATA_X1, data, sizeof(data));
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    sample->accel[axis] = DecodeBigEndian(&data[axis * 2U]);
    sample->gyro[axis] = DecodeBigEndian(&data[6U + axis * 2U]);
  }
  sample->temperature = DecodeBigEndian(&data[12]);
  return ICM45686_RESULT_OK;
}

void Icm45686_ConvertSample(const Icm45686Device *device,
                            const Icm45686RawSample *raw,
                            Icm45686Sample *sample)
{
  float accel_scale;
  float gyro_scale;

  if (device == NULL || raw == NULL || sample == NULL) {
    return;
  }
  accel_scale =
      AccelRangeG(device->config.accel_full_scale) * STANDARD_GRAVITY_MPS2 /
      SENSOR_COUNTS;
  gyro_scale = GyroRangeDps(device->config.gyro_full_scale) *
               DEGREES_TO_RADIANS / SENSOR_COUNTS;
  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    sample->accel_mps2[axis] = (float)raw->accel[axis] * accel_scale;
    sample->gyro_rad_s[axis] = (float)raw->gyro[axis] * gyro_scale;
  }
  sample->temperature_c = 25.0f + (float)raw->temperature / 128.0f;
}

static bool IsConfigValid(const Icm45686Config *config)
{
  return config != NULL &&
         config->accel_full_scale <= ICM45686_ACCEL_FS_2_G &&
         config->gyro_full_scale <= ICM45686_GYRO_FS_15_625_DPS &&
         config->output_data_rate >= ICM45686_ODR_6400_HZ &&
         config->output_data_rate <= ICM45686_ODR_1_5625_HZ &&
         config->low_noise_bandwidth <= ICM45686_LN_BW_ODR_DIV_128 &&
         (!config->fifo_enabled ||
          (config->fifo_watermark_frames > 0U &&
           config->fifo_watermark_frames <= FIFO_MAX_FRAME_COUNT));
}

static Icm45686Result Read(const Icm45686Device *device, uint8_t reg,
                           uint8_t *data, size_t length)
{
  return device->transport.read(device->transport.context, reg, data, length)
             ? ICM45686_RESULT_OK
             : ICM45686_RESULT_TRANSPORT_ERROR;
}

static Icm45686Result Write(const Icm45686Device *device, uint8_t reg,
                            uint8_t value)
{
  return WriteBuffer(device, reg, &value, 1U);
}

static Icm45686Result WriteBuffer(const Icm45686Device *device, uint8_t reg,
                                  const uint8_t *data, size_t length)
{
  return device->transport.write(device->transport.context, reg, data, length)
             ? ICM45686_RESULT_OK
             : ICM45686_RESULT_TRANSPORT_ERROR;
}

static Icm45686Result Update(const Icm45686Device *device, uint8_t reg,
                             uint8_t mask, uint8_t value)
{
  Icm45686Result result;
  uint8_t current;

  result = Read(device, reg, &current, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  current = (current & (uint8_t)~mask) | (value & mask);
  return Write(device, reg, current);
}

static Icm45686Result ReadMreg(const Icm45686Device *device, uint16_t reg,
                              uint8_t *data, size_t length)
{
  const uint8_t address[2] = {(uint8_t)(reg >> 8), (uint8_t)reg};
  Icm45686Result result;

  device->transport.delay_us(device->transport.context, 4U);
  result = WriteBuffer(device, REG_IREG_ADDR_15_8, address, sizeof(address));
  for (size_t index = 0U;
       result == ICM45686_RESULT_OK && index < length; ++index) {
    device->transport.delay_us(device->transport.context, 4U);
    result = Read(device, REG_IREG_DATA, &data[index], 1U);
  }
  return result;
}

static Icm45686Result WriteMreg(const Icm45686Device *device, uint16_t reg,
                               const uint8_t *data, size_t length)
{
  uint8_t first_write[3];
  Icm45686Result result;

  if (data == NULL || length == 0U) {
    return ICM45686_RESULT_INVALID_ARGUMENT;
  }
  first_write[0] = (uint8_t)(reg >> 8);
  first_write[1] = (uint8_t)reg;
  first_write[2] = data[0];
  device->transport.delay_us(device->transport.context, 4U);
  result = WriteBuffer(device, REG_IREG_ADDR_15_8, first_write,
                       sizeof(first_write));
  for (size_t index = 1U;
       result == ICM45686_RESULT_OK && index < length; ++index) {
    device->transport.delay_us(device->transport.context, 4U);
    result = WriteBuffer(device, REG_IREG_DATA, &data[index], 1U);
  }
  device->transport.delay_us(device->transport.context, 4U);
  return result;
}

static Icm45686Result UpdateMreg(const Icm45686Device *device, uint16_t reg,
                                uint8_t mask, uint8_t value)
{
  uint8_t current;
  Icm45686Result result = ReadMreg(device, reg, &current, 1U);

  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  current = (current & (uint8_t)~mask) | (value & mask);
  return WriteMreg(device, reg, &current, 1U);
}

static Icm45686Result ConfigureFifo(Icm45686Device *device)
{
  Icm45686Result result;
  uint8_t smc_control;

  result = Update(device, REG_FIFO_CONFIG3, 0x01U, 0U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG0, FIFO_MODE_MASK, 0U);
  if (result != ICM45686_RESULT_OK || !device->config.fifo_enabled) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG0, FIFO_DEPTH_MASK, FIFO_DEPTH_MAX);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_FIFO_CONFIG1_0,
                 (uint8_t)device->config.fifo_watermark_frames);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_FIFO_CONFIG1_0 + 1U,
                 (uint8_t)(device->config.fifo_watermark_frames >> 8));
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG2, FIFO_WATERMARK_GE,
                  FIFO_WATERMARK_GE);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG3, 0x0FU,
                  FIFO_ACCEL_GYRO);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG4, FIFO_TIMESTAMP_ENABLE,
                  FIFO_TIMESTAMP_ENABLE);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_TMST_WOM_CONFIG, TIMESTAMP_CONFIG_MASK,
                  TIMESTAMP_RESOLUTION_16_US);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = ReadMreg(device, MREG_SMC_CONTROL_0, &smc_control, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  smc_control |= SMC_TIMESTAMP_ENABLE;
  result = WriteMreg(device, MREG_SMC_CONTROL_0, &smc_control, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Update(device, REG_FIFO_CONFIG0, FIFO_MODE_MASK,
                  FIFO_MODE_STREAM);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  return Update(device, REG_FIFO_CONFIG3, 0x01U, 0x01U);
}

static Icm45686Result SoftReset(Icm45686Device *device)
{
  Icm45686Result result;
  uint8_t interface_override;
  uint8_t drive_config;
  uint8_t interrupt_status;

  result = Read(device, REG_INTF_CONFIG1_OVRD, &interface_override, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Read(device, REG_DRIVE_CONFIG0, &drive_config, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_MISC2, SOFT_RESET);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  device->transport.delay_ms(device->transport.context, 2U);
  result = Write(device, REG_DRIVE_CONFIG0, drive_config);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Write(device, REG_INTF_CONFIG1_OVRD, interface_override);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  result = Read(device, REG_INT1_STATUS0, &interrupt_status, 1U);
  if (result != ICM45686_RESULT_OK) {
    return result;
  }
  return (interrupt_status & RESET_DONE) != 0U
             ? ICM45686_RESULT_OK
             : ICM45686_RESULT_RESET_FAILED;
}

static int16_t DecodeBigEndian(const uint8_t *data)
{
  return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static float AccelRangeG(Icm45686AccelFullScale full_scale)
{
  static const float ranges[] = {32.0f, 16.0f, 8.0f, 4.0f, 2.0f};

  return ranges[(uint32_t)full_scale];
}

static float GyroRangeDps(Icm45686GyroFullScale full_scale)
{
  static const float ranges[] = {4000.0f, 2000.0f, 1000.0f,
                                 500.0f,  250.0f,  125.0f,
                                 62.5f,   31.25f,  15.625f};

  return ranges[(uint32_t)full_scale];
}
