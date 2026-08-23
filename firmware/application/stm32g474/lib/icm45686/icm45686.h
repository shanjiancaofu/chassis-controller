#ifndef ICM45686_H
#define ICM45686_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ICM45686_WHO_AM_I_VALUE 0xE9U
#define ICM45686_FIFO_DATA_REGISTER 0x14U
#define ICM45686_FIFO_FRAME_SIZE 16U

typedef bool (*Icm45686ReadRegisters)(void *context, uint8_t reg,
                                     uint8_t *data, size_t length);
typedef bool (*Icm45686WriteRegisters)(void *context, uint8_t reg,
                                      const uint8_t *data, size_t length);
typedef void (*Icm45686DelayMs)(void *context, uint32_t delay_ms);
typedef void (*Icm45686DelayUs)(void *context, uint32_t delay_us);

typedef struct {
  void *context;
  Icm45686ReadRegisters read;
  Icm45686WriteRegisters write;
  Icm45686DelayMs delay_ms;
  Icm45686DelayUs delay_us;
} Icm45686Transport;

typedef enum {
  ICM45686_ACCEL_FS_32_G = 0x0,
  ICM45686_ACCEL_FS_16_G = 0x1,
  ICM45686_ACCEL_FS_8_G = 0x2,
  ICM45686_ACCEL_FS_4_G = 0x3,
  ICM45686_ACCEL_FS_2_G = 0x4
} Icm45686AccelFullScale;

typedef enum {
  ICM45686_GYRO_FS_4000_DPS = 0x0,
  ICM45686_GYRO_FS_2000_DPS = 0x1,
  ICM45686_GYRO_FS_1000_DPS = 0x2,
  ICM45686_GYRO_FS_500_DPS = 0x3,
  ICM45686_GYRO_FS_250_DPS = 0x4,
  ICM45686_GYRO_FS_125_DPS = 0x5,
  ICM45686_GYRO_FS_62_5_DPS = 0x6,
  ICM45686_GYRO_FS_31_25_DPS = 0x7,
  ICM45686_GYRO_FS_15_625_DPS = 0x8
} Icm45686GyroFullScale;

typedef enum {
  ICM45686_ODR_6400_HZ = 0x3,
  ICM45686_ODR_3200_HZ = 0x4,
  ICM45686_ODR_1600_HZ = 0x5,
  ICM45686_ODR_800_HZ = 0x6,
  ICM45686_ODR_400_HZ = 0x7,
  ICM45686_ODR_200_HZ = 0x8,
  ICM45686_ODR_100_HZ = 0x9,
  ICM45686_ODR_50_HZ = 0xA,
  ICM45686_ODR_25_HZ = 0xB,
  ICM45686_ODR_12_5_HZ = 0xC,
  ICM45686_ODR_6_25_HZ = 0xD,
  ICM45686_ODR_3_125_HZ = 0xE,
  ICM45686_ODR_1_5625_HZ = 0xF
} Icm45686OutputDataRate;

typedef enum {
  ICM45686_LN_BW_NO_FILTER = 0x0,
  ICM45686_LN_BW_ODR_DIV_4 = 0x1,
  ICM45686_LN_BW_ODR_DIV_8 = 0x2,
  ICM45686_LN_BW_ODR_DIV_16 = 0x3,
  ICM45686_LN_BW_ODR_DIV_32 = 0x4,
  ICM45686_LN_BW_ODR_DIV_64 = 0x5,
  ICM45686_LN_BW_ODR_DIV_128 = 0x6
} Icm45686LowNoiseBandwidth;

typedef struct {
  Icm45686AccelFullScale accel_full_scale;
  Icm45686GyroFullScale gyro_full_scale;
  Icm45686OutputDataRate output_data_rate;
  Icm45686LowNoiseBandwidth low_noise_bandwidth;
  bool data_ready_interrupt_enabled;
  bool fifo_enabled;
  uint16_t fifo_watermark_frames;
} Icm45686Config;

typedef struct {
  int16_t accel[3];
  int16_t gyro[3];
  int16_t temperature;
} Icm45686RawSample;

typedef struct {
  Icm45686RawSample raw;
  uint16_t timestamp;
} Icm45686FifoSample;

typedef struct {
  bool full;
  bool threshold;
} Icm45686FifoStatus;

typedef struct {
  float accel_mps2[3];
  float gyro_rad_s[3];
  float temperature_c;
} Icm45686Sample;

typedef enum {
  ICM45686_RESULT_OK = 0,
  ICM45686_RESULT_INVALID_ARGUMENT,
  ICM45686_RESULT_TRANSPORT_ERROR,
  ICM45686_RESULT_DEVICE_ID_MISMATCH,
  ICM45686_RESULT_RESET_FAILED,
  ICM45686_RESULT_CONFIGURATION_FAILED,
  ICM45686_RESULT_TIMEOUT
} Icm45686Result;

typedef struct {
  Icm45686Transport transport;
  Icm45686Config config;
  uint8_t who_am_i;
  bool initialized;
} Icm45686Device;

Icm45686Result Icm45686_Init(Icm45686Device *device,
                             const Icm45686Transport *transport,
                             const Icm45686Config *config);
Icm45686Result Icm45686_ReadRaw(Icm45686Device *device,
                                Icm45686RawSample *sample);
Icm45686Result Icm45686_ReadFifoCount(Icm45686Device *device,
                                     uint16_t *frame_count);
Icm45686Result Icm45686_ReadFifoStatus(Icm45686Device *device,
                                      Icm45686FifoStatus *status);
Icm45686Result Icm45686_FlushFifo(Icm45686Device *device);
Icm45686Result Icm45686_ParseFifoFrame(const uint8_t *frame, size_t length,
                                      Icm45686FifoSample *sample);
float Icm45686_TimestampDeltaSeconds(uint16_t previous, uint16_t current);
void Icm45686_ConvertSample(const Icm45686Device *device,
                            const Icm45686RawSample *raw,
                            Icm45686Sample *sample);

#endif
