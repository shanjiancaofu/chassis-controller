#ifndef CONTROL_RUNTIME_H
#define CONTROL_RUNTIME_H

#include <stdbool.h>
#include <stdint.h>

#include "app/chassis/command_manager.h"
#include "app/maintenance/self_test/motor_self_test.h"
#include "device.h"

bool ControlRuntime_BindDevices(const struct device *drive,
                                const struct device *left_encoder,
                                const struct device *right_encoder);
bool ControlRuntime_Init(void);
void ControlRuntime_Run(uint32_t notification_count);
bool ControlRuntime_Start(void);
void ControlRuntime_Stop(void);
CommandManagerSubmitResult ControlRuntime_SubmitMotionCommand(
    int32_t left_target, int32_t right_target, CommandSource source,
    uint32_t now_ms, bool has_sequence, uint8_t sequence);
bool ControlRuntime_ResetOdometry(void);
bool ControlRuntime_StartMotorSelfTest(MotorSelfTestAction action,
                                       uint32_t now_ms);
bool ControlRuntime_AcquireSelfTestLock(void);
bool ControlRuntime_AcquireOtaLock(void);
void ControlRuntime_ReleaseOtaLock(void);
void ControlRuntime_ReleaseFinishedSelfTestLock(void);
void ControlRuntime_LatchInternalFault(uint32_t fault);
void ControlRuntime_EmergencyStopOutputs(void);
void ControlRuntime_CoastOutputs(void);
void ControlRuntime_FatalError(void);
void ControlRuntime_PanicStopFromException(void);
bool ControlRuntime_ClearEmergencyStop(void);

#endif
