#ifndef FIRMWARE_IMAGE_H
#define FIRMWARE_IMAGE_H

#include <stdint.h>

#define OTA_IMAGE_MAGIC 0x3141544FUL
#define OTA_IMAGE_FORMAT_VERSION 1U
#define OTA_IMAGE_FLAG_NONE 0UL

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

#if defined(__cplusplus)
static_assert(sizeof(OtaImageHeader) == 64U, "OTA image header ABI changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(OtaImageHeader) == 64U,
               "OTA image header ABI changed");
#endif

#endif
