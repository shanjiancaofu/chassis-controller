#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint32_t value;
} BootCrc32Context;

void BootCrc32_Init(BootCrc32Context *context);
void BootCrc32_Update(BootCrc32Context *context, const void *data,
                      size_t length);
uint32_t BootCrc32_Finalize(const BootCrc32Context *context);
uint32_t BootCrc32_Calculate(const void *data, size_t length);

#endif
