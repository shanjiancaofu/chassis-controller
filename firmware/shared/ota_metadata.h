#ifndef OTA_METADATA_H
#define OTA_METADATA_H

#include <stddef.h>
#include <stdint.h>

#define OTA_METADATA_MAGIC 0x314D544FUL
#define OTA_METADATA_FORMAT_VERSION 1U

typedef enum
{
  OTA_SLOT_NONE = 0,
  OTA_SLOT_A,
  OTA_SLOT_B
} OtaSlotId;

typedef enum
{
  OTA_STATE_EMPTY = 0,
  OTA_STATE_RECEIVING,
  OTA_STATE_STAGED,
  OTA_STATE_INSTALLING,
  OTA_STATE_TRIAL,
  OTA_STATE_CONFIRMED,
  OTA_STATE_ROLLBACK_PENDING,
  OTA_STATE_FAILED
} OtaState;

typedef struct
{
  uint32_t magic;
  uint16_t format_version;
  uint16_t record_size;
  uint32_t sequence;
  uint32_t state;
  uint32_t confirmed_slot;
  uint32_t candidate_slot;
  uint32_t image_size;
  uint32_t image_crc32;
  uint32_t install_attempts;
  uint32_t trial_boot_count;
  uint32_t last_error;
  uint8_t reserved[16];
  uint32_t record_crc32;
} OtaMetadata;

#if defined(__cplusplus)
static_assert(sizeof(OtaMetadata) == 64U, "OTA metadata ABI changed");
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(sizeof(OtaMetadata) == 64U, "OTA metadata ABI changed");
_Static_assert(offsetof(OtaMetadata, magic) == 0U, "metadata magic offset");
_Static_assert(offsetof(OtaMetadata, format_version) == 4U,
               "metadata format offset");
_Static_assert(offsetof(OtaMetadata, record_size) == 6U,
               "metadata size offset");
_Static_assert(offsetof(OtaMetadata, sequence) == 8U,
               "metadata sequence offset");
_Static_assert(offsetof(OtaMetadata, state) == 12U,
               "metadata state offset");
_Static_assert(offsetof(OtaMetadata, confirmed_slot) == 16U,
               "metadata confirmed offset");
_Static_assert(offsetof(OtaMetadata, candidate_slot) == 20U,
               "metadata candidate offset");
_Static_assert(offsetof(OtaMetadata, image_size) == 24U,
               "metadata image size offset");
_Static_assert(offsetof(OtaMetadata, image_crc32) == 28U,
               "metadata image crc offset");
_Static_assert(offsetof(OtaMetadata, install_attempts) == 32U,
               "metadata attempts offset");
_Static_assert(offsetof(OtaMetadata, trial_boot_count) == 36U,
               "metadata trial offset");
_Static_assert(offsetof(OtaMetadata, last_error) == 40U,
               "metadata error offset");
_Static_assert(offsetof(OtaMetadata, reserved) == 44U,
               "metadata reserved offset");
_Static_assert(offsetof(OtaMetadata, record_crc32) == 60U,
               "metadata crc offset");
#endif

#endif
