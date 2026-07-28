#include "components/crc/crc32.h"

void BootCrc32_Init(BootCrc32Context *context)
{
  if (context != 0) {
    context->value = 0xFFFFFFFFUL;
  }
}

void BootCrc32_Update(BootCrc32Context *context, const void *data,
                      size_t length)
{
  const uint8_t *bytes = (const uint8_t *)data;
  size_t index;
  uint8_t bit;

  if (context == 0 || (data == 0 && length != 0U)) {
    return;
  }

  for (index = 0U; index < length; ++index) {
    context->value ^= bytes[index];
    for (bit = 0U; bit < 8U; ++bit) {
      if ((context->value & 1UL) != 0UL) {
        context->value = (context->value >> 1) ^ 0xEDB88320UL;
      } else {
        context->value >>= 1;
      }
    }
  }
}

uint32_t BootCrc32_Finalize(const BootCrc32Context *context)
{
  return context == 0 ? 0UL : context->value ^ 0xFFFFFFFFUL;
}

uint32_t BootCrc32_Calculate(const void *data, size_t length)
{
  BootCrc32Context context;

  if (data == 0 && length != 0U) {
    return 0UL;
  }

  BootCrc32_Init(&context);
  BootCrc32_Update(&context, data, length);
  return BootCrc32_Finalize(&context);
}
