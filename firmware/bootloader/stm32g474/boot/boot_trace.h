#ifndef BOOT_TRACE_H
#define BOOT_TRACE_H

#include <stdint.h>

void BootTrace_Init(void);
void BootTrace_Write(const char *text);
void BootTrace_WriteValue(const char *label, uint32_t value);

#endif
