#ifdef PARAMETER_RECORD_HOST_TEST

#include <assert.h>
#include <string.h>

#include "config/control_config.h"
#include "app/parameters/parameter_record.h"

static ParameterSnapshot TestParameters(void)
{
  return (ParameterSnapshot){
      .left_pid = {.kp = 200U, .ki = 300U, .kd = 4U},
      .right_pid = {.kp = 250U, .ki = 400U, .kd = 5U},
  };
}

int main(void)
{
  ParameterSnapshot parameters = TestParameters();
  ParameterRecord record_a;
  ParameterRecord record_b;
  ParameterRecordSelection selection;

  ParameterRecord_Prepare(&record_a, 7U, &parameters);
  assert(ParameterRecord_Validate(&record_a));

  record_a.parameters.left_pid.kp ^= 1U;
  assert(!ParameterRecord_Validate(&record_a));

  parameters.left_pid.kp = MOTOR_PID_KP_MAX + 1U;
  assert(!ParameterRecord_ParametersValid(&parameters));
  parameters = TestParameters();

  ParameterRecord_Prepare(&record_a, 7U, &parameters);
  ParameterRecord_Prepare(&record_b, 8U, &parameters);
  selection = ParameterRecord_SelectLatest(&record_a, &record_b);
  assert(selection.valid);
  assert(selection.copy == PARAMETER_RECORD_COPY_B);
  assert(selection.record == &record_b);

  record_b.record_crc32 ^= 1U;
  selection = ParameterRecord_SelectLatest(&record_a, &record_b);
  assert(selection.valid);
  assert(selection.copy == PARAMETER_RECORD_COPY_A);

  memset(&record_a, 0, sizeof(record_a));
  selection = ParameterRecord_SelectLatest(&record_a, &record_b);
  assert(!selection.valid);
  return 0;
}

#endif
