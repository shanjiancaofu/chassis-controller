#ifndef PARAMETER_RECORD_H
#define PARAMETER_RECORD_H

#include <stdbool.h>
#include <stdint.h>

#include "app/modules/parameters/parameter_manager.h"

#define PARAMETER_RECORD_MAGIC 0x50494450UL
#define PARAMETER_RECORD_FORMAT_VERSION 1U

typedef enum {
  PARAMETER_RECORD_COPY_A = 0,
  PARAMETER_RECORD_COPY_B
} ParameterRecordCopy;

typedef struct {
  uint32_t magic;
  uint16_t format_version;
  uint16_t record_size;
  uint32_t sequence;
  ParameterSnapshot parameters;
  uint32_t record_crc32;
} ParameterRecord;

typedef struct {
  const ParameterRecord *record;
  ParameterRecordCopy copy;
  bool valid;
} ParameterRecordSelection;

bool ParameterRecord_ParametersValid(const ParameterSnapshot *parameters);
bool ParameterRecord_Validate(const ParameterRecord *record);
void ParameterRecord_Prepare(ParameterRecord *record, uint32_t sequence,
                             const ParameterSnapshot *parameters);
ParameterRecordSelection ParameterRecord_SelectLatest(
    const ParameterRecord *copy_a, const ParameterRecord *copy_b);

#endif
