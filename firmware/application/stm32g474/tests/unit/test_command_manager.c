#ifdef COMMAND_MANAGER_HOST_TEST

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "modules/chassis/command_manager.h"

static CommandManagerCommand MakeCommand(CommandSource source,
                                         uint32_t received_ms,
                                         bool has_sequence)
{
  const CommandManagerCommand command = {
      .left_target = 10,
      .right_target = -10,
      .received_ms = received_ms,
      .source = source,
      .sequence = 1U,
      .has_sequence = has_sequence,
  };

  return command;
}

int main(void)
{
  CommandManagerCommand command;

  CommandManager_Init();
  assert(CommandManager_Acquire(COMMAND_SOURCE_OTA));
  CommandManager_ClearCommand();
  assert(CommandManager_GetOwner() == COMMAND_SOURCE_OTA);
  command = MakeCommand(COMMAND_SOURCE_CONSOLE, 0U, false);
  assert(CommandManager_Submit(&command) == COMMAND_SUBMIT_NOT_OWNER);
  CommandManager_Release(COMMAND_SOURCE_CONSOLE);
  assert(CommandManager_GetOwner() == COMMAND_SOURCE_OTA);
  CommandManager_Release(COMMAND_SOURCE_OTA);
  assert(CommandManager_GetOwner() == COMMAND_SOURCE_NONE);

  command = MakeCommand(COMMAND_SOURCE_CONSOLE, 10U, false);
  assert(CommandManager_Submit(&command) == COMMAND_SUBMIT_ACCEPTED);
  assert(!CommandManager_IsTimedOut(UINT32_MAX));
  CommandManager_ClearCommand();
  assert(CommandManager_GetOwner() == COMMAND_SOURCE_CONSOLE);
  CommandManager_Release(COMMAND_SOURCE_CONSOLE);

  command = MakeCommand(COMMAND_SOURCE_CAN_REMOTE, 1000U, true);
  assert(CommandManager_Submit(&command) == COMMAND_SUBMIT_ACCEPTED);
  assert(!CommandManager_IsTimedOut(1199U));
  assert(CommandManager_IsTimedOut(1200U));
  CommandManager_Release(COMMAND_SOURCE_CAN_REMOTE);

  command = MakeCommand(COMMAND_SOURCE_OTA, 0U, false);
  assert(CommandManager_Submit(&command) == COMMAND_SUBMIT_INVALID_ARGUMENT);
  assert(!CommandManager_Acquire((CommandSource)99));
  return 0;
}

#endif
