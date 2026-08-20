#include "subsys/settings/parameter_record.h"

#include <stddef.h>

#include "components/crc/crc32.h"
#include "config/control_config.h"

bool ParameterRecord_ParametersValid(const ParameterSnapshot *parameters)
{
  return parameters != NULL && parameters->left_pid.kp <= MOTOR_PID_KP_MAX &&
         parameters->left_pid.ki <= MOTOR_PID_KI_MAX &&
         parameters->left_pid.kd <= MOTOR_PID_KD_MAX &&
         parameters->right_pid.kp <= MOTOR_PID_KP_MAX &&
         parameters->right_pid.ki <= MOTOR_PID_KI_MAX &&
         parameters->right_pid.kd <= MOTOR_PID_KD_MAX;
}

bool ParameterRecord_Validate(const ParameterRecord *record)
{
  if (record == NULL || record->magic != PARAMETER_RECORD_MAGIC ||
      record->format_version != PARAMETER_RECORD_FORMAT_VERSION ||
      record->record_size != sizeof(*record) ||
      !ParameterRecord_ParametersValid(&record->parameters)) {
    return false;
  }

  return Crc32_Calculate(record,
                         sizeof(*record) - sizeof(record->record_crc32)) ==
         record->record_crc32;
}

void ParameterRecord_Prepare(ParameterRecord *record, uint32_t sequence,
                             const ParameterSnapshot *parameters)
{
  if (record == NULL || !ParameterRecord_ParametersValid(parameters)) {
    return;
  }

  *record = (ParameterRecord){
      .magic = PARAMETER_RECORD_MAGIC,
      .format_version = PARAMETER_RECORD_FORMAT_VERSION,
      .record_size = sizeof(*record),
      .sequence = sequence,
      .parameters = *parameters,
  };
  record->record_crc32 =
      Crc32_Calculate(record, sizeof(*record) - sizeof(record->record_crc32));
}

ParameterRecordSelection ParameterRecord_SelectLatest(
    const ParameterRecord *copy_a, const ParameterRecord *copy_b)
{
  const bool valid_a = ParameterRecord_Validate(copy_a);
  const bool valid_b = ParameterRecord_Validate(copy_b);
  ParameterRecordSelection selection = {0};

  if (valid_a && valid_b) {
    if ((int32_t)(copy_a->sequence - copy_b->sequence) >= 0) {
      selection.record = copy_a;
      selection.copy = PARAMETER_RECORD_COPY_A;
    } else {
      selection.record = copy_b;
      selection.copy = PARAMETER_RECORD_COPY_B;
    }
    selection.valid = true;
  } else if (valid_a) {
    selection.record = copy_a;
    selection.copy = PARAMETER_RECORD_COPY_A;
    selection.valid = true;
  } else if (valid_b) {
    selection.record = copy_b;
    selection.copy = PARAMETER_RECORD_COPY_B;
    selection.valid = true;
  }

  return selection;
}
