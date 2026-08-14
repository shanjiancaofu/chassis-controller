#ifdef ICM45686_HOST_TEST

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "components/icm45686/icm45686.h"

#define REG_ACCEL_DATA_X1 0x00U
#define REG_FIFO_COUNT_0 0x12U
#define REG_FIFO_CONFIG0 0x1DU
#define REG_FIFO_CONFIG1_0 0x1EU
#define REG_FIFO_CONFIG1_1 0x1FU
#define REG_FIFO_CONFIG2 0x20U
#define REG_FIFO_CONFIG3 0x21U
#define REG_FIFO_CONFIG4 0x22U
#define REG_PWR_MGMT0 0x10U
#define REG_INT1_CONFIG0 0x16U
#define REG_INT1_CONFIG1 0x17U
#define REG_INT1_CONFIG2 0x18U
#define REG_INT1_STATUS0 0x19U
#define REG_ACCEL_CONFIG0 0x1BU
#define REG_GYRO_CONFIG0 0x1CU
#define REG_INTF_CONFIG1_OVRD 0x2DU
#define REG_DRIVE_CONFIG0 0x32U
#define REG_IREG_ADDR_15_8 0x7CU
#define REG_IREG_DATA 0x7EU
#define REG_WHO_AM_I 0x72U
#define REG_MISC2 0x7FU

typedef struct {
  uint8_t registers[256];
  uint32_t delay_total_ms;
  uint32_t delay_total_us;
  uint16_t mreg_address;
  uint8_t smc_control_0;
  uint8_t sreg_control;
} FakeTransport;

static bool FakeRead(void *context, uint8_t reg, uint8_t *data,
                     size_t length)
{
  FakeTransport *fake = context;

  if (fake == NULL || data == NULL || (size_t)reg + length > 256U) {
    return false;
  }
  if (reg == REG_IREG_DATA && length == 1U) {
    data[0] = fake->mreg_address == 0xA258U
                  ? fake->smc_control_0
                  : fake->mreg_address == 0xA267U ? fake->sreg_control : 0U;
    ++fake->mreg_address;
    return true;
  }
  memcpy(data, &fake->registers[reg], length);
  return true;
}

static bool FakeWrite(void *context, uint8_t reg, const uint8_t *data,
                      size_t length)
{
  FakeTransport *fake = context;

  if (fake == NULL || data == NULL || (size_t)reg + length > 256U) {
    return false;
  }
  if (reg == REG_IREG_ADDR_15_8 && (length == 2U || length == 3U)) {
    fake->mreg_address = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    if (length == 3U && fake->mreg_address == 0xA258U) {
      fake->smc_control_0 = data[2];
      ++fake->mreg_address;
    } else if (length == 3U && fake->mreg_address == 0xA267U) {
      fake->sreg_control = data[2];
      ++fake->mreg_address;
    }
  } else if (reg == REG_IREG_DATA && length == 1U) {
    if (fake->mreg_address == 0xA258U) {
      fake->smc_control_0 = data[0];
    } else if (fake->mreg_address == 0xA267U) {
      fake->sreg_control = data[0];
    }
    ++fake->mreg_address;
  }
  memcpy(&fake->registers[reg], data, length);
  if (reg == REG_MISC2 && length == 1U && (data[0] & 0x02U) != 0U) {
    const uint8_t who_am_i = fake->registers[REG_WHO_AM_I];

    memset(fake->registers, 0, sizeof(fake->registers));
    fake->smc_control_0 = 0U;
    fake->sreg_control = 0U;
    fake->mreg_address = 0U;
    fake->registers[REG_WHO_AM_I] = who_am_i;
    fake->registers[REG_INT1_STATUS0] = 0x80U;
  }
  return true;
}

static void FakeDelay(void *context, uint32_t delay_ms)
{
  FakeTransport *fake = context;

  fake->delay_total_ms += delay_ms;
}

static void FakeDelayUs(void *context, uint32_t delay_us)
{
  FakeTransport *fake = context;

  fake->delay_total_us += delay_us;
}

int main(void)
{
  FakeTransport fake = {0};
  Icm45686Device device;
  Icm45686RawSample raw;
  Icm45686FifoSample fifo_sample;
  Icm45686Sample sample;
  uint16_t fifo_count;
  const Icm45686Transport transport = {
      .context = &fake,
      .read = FakeRead,
      .write = FakeWrite,
      .delay_ms = FakeDelay,
      .delay_us = FakeDelayUs,
  };
  const Icm45686Config config = {
      .accel_full_scale = ICM45686_ACCEL_FS_4_G,
      .gyro_full_scale = ICM45686_GYRO_FS_500_DPS,
      .output_data_rate = ICM45686_ODR_100_HZ,
      .data_ready_interrupt_enabled = true,
      .fifo_enabled = true,
      .fifo_watermark_frames = 4U,
  };

  fake.registers[REG_WHO_AM_I] = ICM45686_WHO_AM_I_VALUE;
  fake.registers[REG_INTF_CONFIG1_OVRD] = 0x05U;
  assert(Icm45686_Init(&device, &transport, &config) ==
         ICM45686_RESULT_OK);
  assert(device.initialized);
  assert(device.who_am_i == ICM45686_WHO_AM_I_VALUE);
  assert((fake.registers[REG_DRIVE_CONFIG0] & 0x0EU) == 0x04U);
  assert(fake.registers[REG_INTF_CONFIG1_OVRD] == 0x05U);
  assert(fake.registers[REG_ACCEL_CONFIG0] == 0x39U);
  assert(fake.registers[REG_GYRO_CONFIG0] == 0x39U);
  assert(fake.registers[REG_INT1_CONFIG0] == 0x02U);
  assert(fake.registers[REG_INT1_CONFIG1] == 0U);
  assert((fake.registers[REG_INT1_CONFIG2] & 0x07U) == 0x01U);
  assert((fake.registers[REG_PWR_MGMT0] & 0x0FU) == 0x0FU);
  assert(fake.registers[REG_FIFO_CONFIG0] == 0x5EU);
  assert(fake.registers[REG_FIFO_CONFIG1_0] == 4U);
  assert(fake.registers[REG_FIFO_CONFIG1_1] == 0U);
  assert((fake.registers[REG_FIFO_CONFIG2] & 0x08U) == 0x08U);
  assert((fake.registers[REG_FIFO_CONFIG3] & 0x0FU) == 0x07U);
  assert((fake.registers[REG_FIFO_CONFIG4] & 0x02U) == 0x02U);
  assert((fake.smc_control_0 & 0x01U) == 0x01U);
  assert((fake.sreg_control & 0x01U) == 0x01U);
  assert(fake.delay_total_ms == 4U);
  assert(fake.delay_total_us == 32U);

  fake.registers[REG_FIFO_COUNT_0] = 0U;
  fake.registers[REG_FIFO_COUNT_0 + 1U] = 3U;
  assert(Icm45686_ReadFifoCount(&device, &fifo_count) == ICM45686_RESULT_OK);
  assert(fifo_count == 3U);

  fake.registers[REG_ACCEL_DATA_X1 + 0U] = 0x40U;
  fake.registers[REG_ACCEL_DATA_X1 + 1U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 2U] = 0xC0U;
  fake.registers[REG_ACCEL_DATA_X1 + 3U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 4U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 5U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 6U] = 0x40U;
  fake.registers[REG_ACCEL_DATA_X1 + 7U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 8U] = 0xC0U;
  fake.registers[REG_ACCEL_DATA_X1 + 9U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 10U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 11U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 12U] = 0x00U;
  fake.registers[REG_ACCEL_DATA_X1 + 13U] = 0x80U;

  assert(Icm45686_ReadRaw(&device, &raw) == ICM45686_RESULT_OK);
  assert(raw.accel[0] == 16384);
  assert(raw.accel[1] == -16384);
  assert(raw.gyro[0] == 16384);
  assert(raw.gyro[1] == -16384);
  assert(raw.temperature == 128);

  Icm45686_ConvertSample(&device, &raw, &sample);
  assert(fabsf(sample.accel_mps2[0] - 19.6133f) < 0.0002f);
  assert(fabsf(sample.accel_mps2[1] + 19.6133f) < 0.0002f);
  assert(fabsf(sample.gyro_rad_s[0] - 4.363323f) < 0.000002f);
  assert(fabsf(sample.gyro_rad_s[1] + 4.363323f) < 0.000002f);
  assert(fabsf(sample.temperature_c - 26.0f) < 0.0001f);

  {
    const uint8_t frame[ICM45686_FIFO_FRAME_SIZE] = {
        0x68U, 0x40U, 0x00U, 0xC0U, 0x00U, 0x00U, 0x00U, 0x40U,
        0x00U, 0xC0U, 0x00U, 0x00U, 0x00U, 0x01U, 0x12U, 0x34U};

    assert(Icm45686_ParseFifoFrame(frame, sizeof(frame), &fifo_sample) ==
           ICM45686_RESULT_OK);
    assert(fifo_sample.raw.accel[0] == 16384);
    assert(fifo_sample.raw.accel[1] == -16384);
    assert(fifo_sample.raw.gyro[0] == 16384);
    assert(fifo_sample.raw.gyro[1] == -16384);
    assert(fifo_sample.raw.temperature == 256);
    assert(fifo_sample.timestamp == 0x1234U);
  }
  return 0;
}

#endif
