#include "lib/crc/crc32.h"

void Crc32_Init(Crc32Context *context)
{
  if (context != NULL) {
    context->value = 0xFFFFFFFFUL;
  }
}

void Crc32_Update(Crc32Context *context, const void *data, size_t length)
{
  const uint8_t *bytes = (const uint8_t *)data;
  size_t index;
  uint8_t bit;

  if (context == NULL || (data == NULL && length != 0U)) {
    return;
  }

  for (index = 0U; index < length; ++index) {
    context->value ^= bytes[index];
    for (bit = 0U; bit < 8U; ++bit) {
      context->value = (context->value & 1UL) != 0UL
                           ? (context->value >> 1) ^ 0xEDB88320UL
                           : context->value >> 1;
    }
  }
}

uint32_t Crc32_Finalize(const Crc32Context *context)
{
  return context == NULL ? 0UL : context->value ^ 0xFFFFFFFFUL;
}

uint32_t Crc32_Calculate(const void *data, size_t length)
{
  Crc32Context context;

  if (data == NULL && length != 0U) {
    return 0UL;
  }
  Crc32_Init(&context);
  Crc32_Update(&context, data, length);
  return Crc32_Finalize(&context);
}
