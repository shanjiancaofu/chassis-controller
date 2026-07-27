#include "components/crc/crc32.h"

uint32_t BootCrc32_Calculate(const void *data, size_t length)
{
  const uint8_t *bytes = (const uint8_t *)data;
  uint32_t crc = 0xFFFFFFFFUL;
  size_t index;
  uint8_t bit;

  if (data == 0 && length != 0U) {
    return 0UL;
  }

  for (index = 0U; index < length; ++index) {
    crc ^= bytes[index];
    for (bit = 0U; bit < 8U; ++bit) {
      if ((crc & 1UL) != 0UL) {
        crc = (crc >> 1) ^ 0xEDB88320UL;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc ^ 0xFFFFFFFFUL;
}
