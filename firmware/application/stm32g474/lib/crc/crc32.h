#ifndef CRC32_H
#define CRC32_H

#include <stddef.h>
#include <stdint.h>

typedef struct
{
  uint32_t value;
} Crc32Context;

void Crc32_Init(Crc32Context *context);
void Crc32_Update(Crc32Context *context, const void *data, size_t length);
uint32_t Crc32_Finalize(const Crc32Context *context);
uint32_t Crc32_Calculate(const void *data, size_t length);

#endif
