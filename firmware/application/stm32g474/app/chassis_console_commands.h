#ifndef CHASSIS_CONSOLE_COMMANDS_H
#define CHASSIS_CONSOLE_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "subsys/console/console.h"
#include "app/modules/chassis/command_manager.h"
#include "tests/target/motor_target_test.h"

typedef struct {
  CommandManagerSubmitResult (*submit_motion_command)(
      int32_t left_target, int32_t right_target, CommandSource source,
      uint32_t now_ms, bool has_sequence, uint8_t sequence);
  bool (*start_control)(void);
  void (*stop_control)(void);
  bool (*reset_wheel_odometry)(void);
  bool (*arm_uart_ota)(uint32_t now_ms);
  bool (*acquire_target_test_lock)(void);
  bool (*start_motor_target_test)(MotorTargetTestAction action,
                                  uint32_t now_ms);
} ChassisConsoleCommandPort;

bool ChassisConsoleCommands_Init(const ChassisConsoleCommandPort *port);
void ChassisConsoleCommands_Process(const ConsoleCommand *command,
                                    uint32_t now_ms);

#endif
