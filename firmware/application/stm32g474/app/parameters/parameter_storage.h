#ifndef PARAMETER_STORAGE_H
#define PARAMETER_STORAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "app/parameters/parameter_manager.h"

typedef enum {
  PARAMETER_STORAGE_DEFAULTS = 0,
  PARAMETER_STORAGE_LOADED,
  PARAMETER_STORAGE_SAVING,
  PARAMETER_STORAGE_STORED,
  PARAMETER_STORAGE_ERROR
} ParameterStorageStatus;

typedef struct {
  ParameterStorageStatus status;
  uint32_t sequence;
  uint32_t error_count;
  bool dirty;
} ParameterStorageSnapshot;

bool ParameterStorage_Init(ParameterSnapshot *restored_parameters);
bool ParameterStorage_RequestSave(const ParameterSnapshot *parameters);
void ParameterStorage_Run(uint32_t now_ms, bool qspi_available);
bool ParameterStorage_IsUsingQspi(void);
bool ParameterStorage_TakeCompletion(bool *success);
void ParameterStorage_GetSnapshot(ParameterStorageSnapshot *snapshot);

#endif
