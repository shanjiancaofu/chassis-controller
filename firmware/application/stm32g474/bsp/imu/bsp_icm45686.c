#include "bsp/imu/bsp_icm45686.h"

#include <stddef.h>
#include <string.h>

#include "board/board_config.h"
#include "main.h"

#define ICM45686_REG_ACCEL_DATA_X1 0x00U
#define ICM45686_REG_PWR_MGMT0 0x10U
#define ICM45686_REG_INT1_CONFIG0 0x16U
#define ICM45686_REG_INT1_CONFIG2 0x18U
#define ICM45686_REG_INT1_STATUS0 0x19U
#define ICM45686_REG_ACCEL_CONFIG0 0x1BU
#define ICM45686_REG_GYRO_CONFIG0 0x1CU
#define ICM45686_REG_WHO_AM_I 0x72U
#define ICM45686_REG_MISC2 0x7FU

#define ICM45686_WHO_AM_I_VALUE 0xE9U
#define ICM45686_SPI_READ 0x80U
#define ICM45686_SOFT_RESET 0x02U
#define ICM45686_INT_DRDY_ENABLE 0x04U
#define ICM45686_INT_ACTIVE_HIGH_PULSE 0x01U
#define ICM45686_ACCEL_4G_100HZ 0x39U
#define ICM45686_GYRO_500DPS_100HZ 0x39U
#define ICM45686_ACCEL_GYRO_LOW_NOISE 0x0FU
#define ICM45686_REGISTER_DATA_SIZE 14U
#define ICM45686_SPI_TIMEOUT_MS 2U
#define ICM45686_SAMPLE_PERIOD_MS 10U
#define ICM45686_STARTUP_TIME_MS 70U
#define ICM45686_RETRY_PERIOD_MS 1000U
#define ICM45686_MAX_CONSECUTIVE_ERRORS 5U

static BspIcm45686Snapshot imu_snapshot;
static volatile bool data_ready_interrupt;
static uint32_t ready_after_ms;
static uint32_t last_attempt_ms;
static uint32_t consecutive_errors;

static bool ReadRegisters(uint8_t reg, uint8_t *data, uint16_t length);
static bool WriteRegister(uint8_t reg, uint8_t value);
static bool ConfigureDevice(uint32_t now_ms);
static bool ReadSample(uint32_t now_ms);
static int16_t DecodeBigEndian(const uint8_t *data);

void BspIcm45686_Init(uint32_t now_ms)
{
  memset(&imu_snapshot, 0, sizeof(imu_snapshot));
  imu_snapshot.status = BSP_ICM45686_UNINITIALIZED;
  data_ready_interrupt = false;
  ready_after_ms = now_ms;
  last_attempt_ms = now_ms;
  consecutive_errors = 0U;
  (void)ConfigureDevice(now_ms);
}

void BspIcm45686_Run(uint32_t now_ms)
{
  if (imu_snapshot.status == BSP_ICM45686_NOT_FOUND ||
      imu_snapshot.status == BSP_ICM45686_DEGRADED) {
    if (now_ms - last_attempt_ms >= ICM45686_RETRY_PERIOD_MS) {
      last_attempt_ms = now_ms;
      (void)ConfigureDevice(now_ms);
    }
    return;
  }
  if (imu_snapshot.status != BSP_ICM45686_READY ||
      (int32_t)(now_ms - ready_after_ms) < 0) {
    return;
  }
  if (!__atomic_exchange_n(&data_ready_interrupt, false, __ATOMIC_RELAXED) &&
      imu_snapshot.sample_valid &&
      now_ms - imu_snapshot.last_sample_ms < ICM45686_SAMPLE_PERIOD_MS) {
    return;
  }
  (void)ReadSample(now_ms);
}

void BspIcm45686_OnDataReadyInterrupt(void)
{
  (void)__atomic_fetch_add(&imu_snapshot.interrupt_count, 1U,
                           __ATOMIC_RELAXED);
  __atomic_store_n(&data_ready_interrupt, true, __ATOMIC_RELAXED);
}

void BspIcm45686_GetSnapshot(BspIcm45686Snapshot *snapshot)
{
  if (snapshot != NULL) {
    *snapshot = imu_snapshot;
    snapshot->interrupt_count =
        __atomic_load_n(&imu_snapshot.interrupt_count, __ATOMIC_RELAXED);
  }
}

static bool ReadRegisters(uint8_t reg, uint8_t *data, uint16_t length)
{
  uint8_t tx[ICM45686_REGISTER_DATA_SIZE + 1U] = {0};
  uint8_t rx[ICM45686_REGISTER_DATA_SIZE + 1U] = {0};

  if (data == NULL || length == 0U ||
      length > ICM45686_REGISTER_DATA_SIZE) {
    return false;
  }
  tx[0] = reg | ICM45686_SPI_READ;
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
      &BOARD_IMU_SPI, tx, rx, length + 1U, ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
  if (status != HAL_OK) {
    return false;
  }
  memcpy(data, &rx[1], length);
  return true;
}

static bool WriteRegister(uint8_t reg, uint8_t value)
{
  uint8_t data[2] = {reg & ~ICM45686_SPI_READ, value};

  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_RESET);
  const HAL_StatusTypeDef status = HAL_SPI_Transmit(
      &BOARD_IMU_SPI, data, sizeof(data), ICM45686_SPI_TIMEOUT_MS);
  HAL_GPIO_WritePin(IMU_CS_GPIO_Port, IMU_CS_Pin, GPIO_PIN_SET);
  return status == HAL_OK;
}

static bool ConfigureDevice(uint32_t now_ms)
{
  uint8_t who_am_i = 0U;
  uint8_t interrupt_status = 0U;

  imu_snapshot.sample_valid = false;
  if (!ReadRegisters(ICM45686_REG_WHO_AM_I, &who_am_i, 1U)) {
    ++imu_snapshot.transfer_error_count;
    imu_snapshot.status = BSP_ICM45686_NOT_FOUND;
    return false;
  }
  imu_snapshot.who_am_i = who_am_i;
  if (who_am_i != ICM45686_WHO_AM_I_VALUE) {
    imu_snapshot.status = BSP_ICM45686_NOT_FOUND;
    return false;
  }
  if (!WriteRegister(ICM45686_REG_MISC2, ICM45686_SOFT_RESET)) {
    ++imu_snapshot.transfer_error_count;
    imu_snapshot.status = BSP_ICM45686_DEGRADED;
    return false;
  }
  HAL_Delay(2U);
  if (!ReadRegisters(ICM45686_REG_INT1_STATUS0, &interrupt_status, 1U) ||
      (interrupt_status & 0x80U) == 0U ||
      !WriteRegister(ICM45686_REG_ACCEL_CONFIG0,
                     ICM45686_ACCEL_4G_100HZ) ||
      !WriteRegister(ICM45686_REG_GYRO_CONFIG0,
                     ICM45686_GYRO_500DPS_100HZ) ||
      !WriteRegister(ICM45686_REG_INT1_CONFIG2,
                     ICM45686_INT_ACTIVE_HIGH_PULSE) ||
      !WriteRegister(ICM45686_REG_INT1_CONFIG0,
                     ICM45686_INT_DRDY_ENABLE) ||
      !WriteRegister(ICM45686_REG_PWR_MGMT0,
                     ICM45686_ACCEL_GYRO_LOW_NOISE)) {
    ++imu_snapshot.transfer_error_count;
    imu_snapshot.status = BSP_ICM45686_DEGRADED;
    return false;
  }

  consecutive_errors = 0U;
  ready_after_ms = now_ms + ICM45686_STARTUP_TIME_MS;
  imu_snapshot.status = BSP_ICM45686_READY;
  return true;
}

static bool ReadSample(uint32_t now_ms)
{
  uint8_t data[ICM45686_REGISTER_DATA_SIZE];

  if (!ReadRegisters(ICM45686_REG_ACCEL_DATA_X1, data, sizeof(data))) {
    ++imu_snapshot.transfer_error_count;
    if (++consecutive_errors >= ICM45686_MAX_CONSECUTIVE_ERRORS) {
      imu_snapshot.status = BSP_ICM45686_DEGRADED;
      imu_snapshot.sample_valid = false;
      last_attempt_ms = now_ms;
    }
    return false;
  }

  for (uint32_t axis = 0U; axis < 3U; ++axis) {
    imu_snapshot.accel[axis] = DecodeBigEndian(&data[axis * 2U]);
    imu_snapshot.gyro[axis] = DecodeBigEndian(&data[6U + axis * 2U]);
  }
  imu_snapshot.temperature = DecodeBigEndian(&data[12]);
  imu_snapshot.last_sample_ms = now_ms;
  ++imu_snapshot.sample_count;
  imu_snapshot.sample_valid = true;
  consecutive_errors = 0U;
  return true;
}

static int16_t DecodeBigEndian(const uint8_t *data)
{
  return (int16_t)(((uint16_t)data[0] << 8) | data[1]);
}
