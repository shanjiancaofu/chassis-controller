#ifndef FIRMWARE_IMAGE_H
#define FIRMWARE_IMAGE_H

#include <stddef.h>
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
_Static_assert(offsetof(OtaImageHeader, magic) == 0U,
               "image magic offset");
_Static_assert(offsetof(OtaImageHeader, format_version) == 4U,
               "image format offset");
_Static_assert(offsetof(OtaImageHeader, header_size) == 6U,
               "image header size offset");
_Static_assert(offsetof(OtaImageHeader, payload_size) == 8U,
               "image payload size offset");
_Static_assert(offsetof(OtaImageHeader, payload_crc32) == 12U,
               "image payload crc offset");
_Static_assert(offsetof(OtaImageHeader, load_address) == 16U,
               "image load offset");
_Static_assert(offsetof(OtaImageHeader, vector_address) == 20U,
               "image vector offset");
_Static_assert(offsetof(OtaImageHeader, firmware_version) == 24U,
               "image version offset");
_Static_assert(offsetof(OtaImageHeader, build_number) == 28U,
               "image build offset");
_Static_assert(offsetof(OtaImageHeader, rollback_counter) == 32U,
               "image rollback offset");
_Static_assert(offsetof(OtaImageHeader, flags) == 36U,
               "image flags offset");
_Static_assert(offsetof(OtaImageHeader, reserved) == 40U,
               "image reserved offset");
_Static_assert(offsetof(OtaImageHeader, header_crc32) == 60U,
               "image crc offset");
#endif

#endif
