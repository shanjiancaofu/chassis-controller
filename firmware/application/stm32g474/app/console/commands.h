#ifndef CONSOLE_COMMANDS_H
#define CONSOLE_COMMANDS_H

#include <stdbool.h>
#include <stdint.h>

#include "app/console/console.h"
#include "app/chassis/command_manager.h"
#include "app/maintenance/self_test/motor_self_test.h"

typedef struct {
  CommandManagerSubmitResult (*submit_motion_command)(
      int32_t left_target, int32_t right_target, CommandSource source,
      uint32_t now_ms, bool has_sequence, uint8_t sequence);
  bool (*start_control)(void);
  void (*stop_control)(void);
  bool (*reset_wheel_odometry)(void);
  bool (*arm_uart_ota)(uint32_t now_ms);
  bool (*acquire_self_test_lock)(void);
  bool (*start_motor_self_test)(MotorSelfTestAction action,
                                  uint32_t now_ms);
} ConsoleCommandPort;

bool ConsoleCommands_Init(const ConsoleCommandPort *port);
void ConsoleCommands_Process(const ConsoleCommand *command,
                                    uint32_t now_ms);

#endif
