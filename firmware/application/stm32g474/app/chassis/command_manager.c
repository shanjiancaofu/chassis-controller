#include "app/chassis/command_manager.h"

#include <stddef.h>

#include "config/control_config.h"
#include "config/protocol_config.h"

static CommandManagerCommand active_command;
static bool command_valid;
static CommandSource control_owner;

static bool IsOwnerSource(CommandSource source)
{
  switch (source) {
    case COMMAND_SOURCE_CAN_REMOTE:
    case COMMAND_SOURCE_CONSOLE:
    case COMMAND_SOURCE_SELF_TEST:
    case COMMAND_SOURCE_OTA:
      return true;
    case COMMAND_SOURCE_NONE:
    default:
      return false;
  }
}

static bool IsMotionSource(CommandSource source)
{
  return source == COMMAND_SOURCE_CAN_REMOTE ||
         source == COMMAND_SOURCE_CONSOLE ||
         source == COMMAND_SOURCE_SELF_TEST;
}

void CommandManager_Init(void)
{
  active_command = (CommandManagerCommand){0};
  command_valid = false;
  control_owner = COMMAND_SOURCE_NONE;
}

bool CommandManager_Acquire(CommandSource source)
{
  if (!IsOwnerSource(source) ||
      (control_owner != COMMAND_SOURCE_NONE && control_owner != source)) {
    return false;
  }

  control_owner = source;
  return true;
}

void CommandManager_Release(CommandSource source)
{
  if (source == COMMAND_SOURCE_NONE || control_owner != source) {
    return;
  }

  active_command = (CommandManagerCommand){0};
  command_valid = false;
  control_owner = COMMAND_SOURCE_NONE;
}

CommandSource CommandManager_GetOwner(void)
{
  return control_owner;
}

CommandManagerSubmitResult CommandManager_Submit(
    const CommandManagerCommand *command)
{
  if (command == NULL || !IsMotionSource(command->source) ||
      (command->source == COMMAND_SOURCE_CAN_REMOTE) !=
          command->has_sequence ||
      command->left_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      command->left_target > MOTOR_CONTROL_TARGET_LIMIT ||
      command->right_target < -MOTOR_CONTROL_TARGET_LIMIT ||
      command->right_target > MOTOR_CONTROL_TARGET_LIMIT) {
    return COMMAND_SUBMIT_INVALID_ARGUMENT;
  }
  if (!CommandManager_Acquire(command->source)) {
    return COMMAND_SUBMIT_NOT_OWNER;
  }

  active_command = *command;
  command_valid = true;
  return COMMAND_SUBMIT_ACCEPTED;
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

void CommandManager_ClearCommand(void)
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
  if (!command_valid) {
    return true;
  }

  switch (active_command.source) {
    case COMMAND_SOURCE_CAN_REMOTE:
    case COMMAND_SOURCE_SELF_TEST:
      return now_ms - active_command.received_ms >=
             MOTOR_COMMAND_TIMEOUT_MS;
    case COMMAND_SOURCE_CONSOLE:
      return false;
    case COMMAND_SOURCE_NONE:
    case COMMAND_SOURCE_OTA:
    default:
      return true;
  }
}
