#ifndef OTA_IMAGE_H
#define OTA_IMAGE_H

#include <stdint.h>

#define OTA_IMAGE_MAGIC 0x3141544FUL
#define OTA_METADATA_MAGIC 0x314D544FUL
#define OTA_FORMAT_VERSION 1U

#define OTA_BOOTLOADER_START 0x08000000UL
#define OTA_BOOTLOADER_SIZE 0x00008000UL
#define OTA_APPLICATION_START 0x08008000UL
#define OTA_APPLICATION_SIZE 0x00078000UL

#define OTA_QSPI_METADATA_A_START 0x00400000UL
#define OTA_QSPI_METADATA_B_START 0x00401000UL
#define OTA_QSPI_IMAGE_START 0x00402000UL
#define OTA_QSPI_IMAGE_SIZE 0x003FD000UL

#define OTA_IMAGE_FLAG_NONE 0UL

typedef enum
{
  OTA_STATE_EMPTY = 0,
  OTA_STATE_STAGED,
  OTA_STATE_INSTALLING,
  OTA_STATE_INSTALLED,
  OTA_STATE_FAILED
} OtaState;

typedef struct
{
  uint32_t magic;
  uint16_t format_version;
  uint16_t header_size;
  uint32_t payload_size;
  uint32_t payload_crc32;
  uint32_t load_address;
  uint32_t vector_address;
  uint32_t firmware_version;
  uint32_t build_number;
  uint32_t rollback_counter;
  uint32_t flags;
  uint8_t reserved[20];
  uint32_t header_crc32;
} OtaImageHeader;

typedef struct
{
  uint32_t magic;
  uint16_t format_version;
  uint16_t record_size;
  uint32_t sequence;
  uint32_t state;
  uint32_t image_address;
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t install_attempts;
  uint32_t last_error;
  uint8_t reserved[24];
  uint32_t record_crc32;
} OtaMetadata;

#if defined(__cplusplus)
static_assert(sizeof(OtaImageHeader) == 64U, "OTA image header ABI changed");
static_assert(sizeof(OtaMetadata) == 64U, "OTA metadata ABI changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(OtaImageHeader) == 64U, "OTA image header ABI changed");
_Static_assert(sizeof(OtaMetadata) == 64U, "OTA metadata ABI changed");
#endif

#endif
