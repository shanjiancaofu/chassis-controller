#include "modules/chassis/command_manager.h"

#include <stddef.h>

#include "config/control_config.h"
#include "config/protocol_config.h"

static CommandManagerCommand active_command;
static bool command_valid;

void CommandManager_Init(void)
{
  active_command = (CommandManagerCommand){0};
  command_valid = false;
}

bool CommandManager_Submit(const CommandManagerCommand *command)
{
  if (command == NULL || command->source == COMMAND_SOURCE_NONE ||
      command->source > COMMAND_SOURCE_DEMO ||
      (command->source == COMMAND_SOURCE_CAN) != command->has_sequence ||
      command->left_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      command->left_target > MOTOR_CONTROL_TARGET_LIMIT ||
      command->right_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      command->right_target > MOTOR_CONTROL_TARGET_LIMIT) {
    return false;
  }

  active_command = *command;
  command_valid = true;
  return true;
}

bool CommandManager_Refresh(CommandSource source, uint32_t now_ms)
{
  if (!command_valid || source == COMMAND_SOURCE_NONE ||
      active_command.source != source) {
    return false;
  }

  active_command.received_ms = now_ms;
  return true;
}

void CommandManager_Clear(void)
{
  active_command = (CommandManagerCommand){0};
  command_valid = false;
}

bool CommandManager_Get(CommandManagerCommand *command)
{
  if (command == NULL || !command_valid) {
    return false;
  }

  *command = active_command;
  return true;
}

bool CommandManager_IsTimedOut(uint32_t now_ms)
{
  return !command_valid ||
         now_ms - active_command.received_ms >= MOTOR_COMMAND_TIMEOUT_MS;
}
