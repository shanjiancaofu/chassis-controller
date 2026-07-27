#ifndef BOOT_CRC32_H
#define BOOT_CRC32_H

#include <stddef.h>
#include <stdint.h>

uint32_t BootCrc32_Calculate(const void *data, size_t length);

#endif
