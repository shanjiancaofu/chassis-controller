#ifndef COMMAND_MANAGER_H
#define COMMAND_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  COMMAND_SOURCE_NONE = 0,
  COMMAND_SOURCE_CAN_REMOTE,
  COMMAND_SOURCE_CONSOLE,
  COMMAND_SOURCE_SELF_TEST,
  COMMAND_SOURCE_OTA
} CommandSource;

typedef struct {
  int32_t left_target;
  int32_t right_target;
  uint32_t received_ms;
  CommandSource source;
  uint8_t sequence;
  bool has_sequence;
} CommandManagerCommand;

typedef enum {
  COMMAND_SUBMIT_ACCEPTED = 0,
  COMMAND_SUBMIT_INVALID_ARGUMENT,
  COMMAND_SUBMIT_NOT_OWNER
} CommandManagerSubmitResult;

void CommandManager_Init(void);
bool CommandManager_Acquire(CommandSource source);
void CommandManager_Release(CommandSource source);
CommandSource CommandManager_GetOwner(void);
CommandManagerSubmitResult CommandManager_Submit(
    const CommandManagerCommand *command);
bool CommandManager_Refresh(CommandSource source, uint32_t now_ms);
void CommandManager_ClearCommand(void);
bool CommandManager_Get(CommandManagerCommand *command);
bool CommandManager_IsTimedOut(uint32_t now_ms);

#endif
