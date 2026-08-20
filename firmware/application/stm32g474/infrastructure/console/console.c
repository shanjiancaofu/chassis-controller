#include "infrastructure/console/console.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "bsp/uart/uart_bsp.h"
#include "config/control_config.h"
#include "config/target_test_config.h"

#define CONSOLE_LINE_SIZE 32U
#define CONSOLE_COMMAND_QUEUE_DEPTH 8U
#define CONSOLE_READ_CHUNK_SIZE 32U

static char line_buffer[CONSOLE_LINE_SIZE];
static uint8_t line_length;
static bool line_discarded;
static ConsoleCommand command_queue[CONSOLE_COMMAND_QUEUE_DEPTH];
static uint8_t command_head;
static uint8_t command_tail;
static uint8_t command_count;
static uint32_t dropped_command_count;

static ConsoleCommand ParseCommand(const char *line);
static void QueueCommand(ConsoleCommand command);

void Console_Init(void)
{
  line_length = 0U;
  line_discarded = false;
  command_head = 0U;
  command_tail = 0U;
  command_count = 0U;
  dropped_command_count = 0U;
}

void Console_Run(void)
{
  uint8_t bytes[CONSOLE_READ_CHUNK_SIZE];
  size_t count;
  size_t index;

  do {
    count = BspUart_Read(bytes, sizeof(bytes));
    for (index = 0U; index < count; ++index) {
      const uint8_t value = bytes[index];

      if (value == '\n') {
        line_buffer[line_length] = '\0';
        if (line_discarded) {
          QueueCommand((ConsoleCommand){.type = CONSOLE_COMMAND_INVALID});
        } else if (line_length != 0U) {
          QueueCommand(ParseCommand(line_buffer));
        }
        line_length = 0U;
        line_discarded = false;
      } else if (value != '\r' && !line_discarded) {
        if (value >= 32U && value <= 126U &&
            line_length < sizeof(line_buffer) - 1U) {
          line_buffer[line_length++] = (char)value;
        } else {
          line_length = 0U;
          line_discarded = true;
        }
      }
    }
  } while (count == sizeof(bytes));
}

bool Console_TakeCommand(ConsoleCommand *command)
{
  if (command == NULL || command_count == 0U) {
    return false;
  }

  *command = command_queue[command_head];
  command_head =
      (uint8_t)((command_head + 1U) % CONSOLE_COMMAND_QUEUE_DEPTH);
  --command_count;
  return true;
}

uint32_t Console_GetDroppedCommandCount(void)
{
  return dropped_command_count;
}

static ConsoleCommand ParseCommand(const char *line)
{
  ConsoleCommand command = {.type = CONSOLE_COMMAND_INVALID};

  if (strcmp(line, "help") == 0) {
    command.type = CONSOLE_COMMAND_HELP;
  } else if (strcmp(line, "ping") == 0) {
    command.type = CONSOLE_COMMAND_PING;
  } else if (strcmp(line, "status") == 0) {
    command.type = CONSOLE_COMMAND_STATUS;
  } else if (strcmp(line, "telemetry on") == 0 ||
             strcmp(line, "telemetry text") == 0) {
    command.type = CONSOLE_COMMAND_TELEMETRY_TEXT;
  } else if (strcmp(line, "telemetry vofa") == 0) {
    command.type = CONSOLE_COMMAND_TELEMETRY_VOFA;
  } else if (strcmp(line, "telemetry off") == 0) {
    command.type = CONSOLE_COMMAND_TELEMETRY_OFF;
  } else if (strcmp(line, "can status") == 0) {
    command.type = CONSOLE_COMMAND_CAN_STATUS;
  } else if (strcmp(line, "can tx confirm") == 0) {
    command.type = CONSOLE_COMMAND_CAN_TRANSMIT;
  } else if (strcmp(line, "pid show") == 0) {
    command.type = CONSOLE_COMMAND_PID_SHOW;
  } else if (strncmp(line, "pid left ", 9U) == 0 ||
             strncmp(line, "pid right ", 10U) == 0) {
    const bool left = line[4] == 'l';
    const size_t offset = left ? 9U : 10U;
    unsigned int kp;
    unsigned int ki;
    unsigned int kd;
    int consumed = 0;

    if (sscanf(&line[offset], "%u %u %u%n", &kp, &ki, &kd, &consumed) ==
            3 &&
        line[offset + (size_t)consumed] == '\0' && kp <= UINT16_MAX &&
        ki <= UINT16_MAX && kd <= UINT16_MAX) {
      command.type = left ? CONSOLE_COMMAND_PID_SET_LEFT
                          : CONSOLE_COMMAND_PID_SET_RIGHT;
      command.arguments.pid.kp = (uint16_t)kp;
      command.arguments.pid.ki = (uint16_t)ki;
      command.arguments.pid.kd = (uint16_t)kd;
    }
  } else if (strncmp(line, "pid target ", 11U) == 0) {
    int left;
    int right;
    int consumed = 0;

    if (sscanf(&line[11], "%d %d%n", &left, &right, &consumed) == 2 &&
        line[11U + (size_t)consumed] == '\0' &&
        left >= -MOTOR_CONTROL_TARGET_LIMIT &&
        left <= MOTOR_CONTROL_TARGET_LIMIT &&
        right >= -MOTOR_CONTROL_TARGET_LIMIT &&
        right <= MOTOR_CONTROL_TARGET_LIMIT) {
      command.type = CONSOLE_COMMAND_PID_TARGET;
      command.arguments.target.left = (int16_t)left;
      command.arguments.target.right = (int16_t)right;
    }
  } else if (strcmp(line, "pid stop") == 0) {
    command.type = CONSOLE_COMMAND_PID_STOP;
  } else if (strcmp(line, "encoder zero") == 0) {
    command.type = CONSOLE_COMMAND_ENCODER_ZERO;
  } else if (strcmp(line, "encoder result") == 0) {
    command.type = CONSOLE_COMMAND_ENCODER_RESULT;
  } else if (strcmp(line, "odometry reset") == 0) {
    command.type = CONSOLE_COMMAND_ODOMETRY_RESET;
  } else if (strcmp(line, "ota uart confirm") == 0) {
    command.type = CONSOLE_COMMAND_OTA_UART;
  } else if (strcmp(line, "qspi test confirm") == 0) {
    command.type = CONSOLE_COMMAND_QSPI_TEST;
  } else if (strcmp(line, "iwdg reset confirm") == 0) {
    command.type = CONSOLE_COMMAND_IWDG_RESET;
  } else if (strncmp(line, "motor duty ", 11U) == 0) {
    unsigned int duty;
    int consumed = 0;

    if (sscanf(&line[11], "%u%n", &duty, &consumed) == 1 &&
        line[11U + (size_t)consumed] == '\0' &&
        duty <= MOTOR_OPEN_LOOP_TEST_DUTY_MAX) {
      command.type = CONSOLE_COMMAND_MOTOR_DUTY;
      command.arguments.motor_duty = (uint16_t)duty;
    }
  } else if (strcmp(line, "motor stop") == 0) {
    command.type = CONSOLE_COMMAND_MOTOR_STOP;
  } else if (strcmp(line, "motor left forward confirm") == 0) {
    command.type = CONSOLE_COMMAND_MOTOR_LEFT_FORWARD;
  } else if (strcmp(line, "motor left reverse confirm") == 0) {
    command.type = CONSOLE_COMMAND_MOTOR_LEFT_REVERSE;
  } else if (strcmp(line, "motor right forward confirm") == 0) {
    command.type = CONSOLE_COMMAND_MOTOR_RIGHT_FORWARD;
  } else if (strcmp(line, "motor right reverse confirm") == 0) {
    command.type = CONSOLE_COMMAND_MOTOR_RIGHT_REVERSE;
  }

  return command;
}

static void QueueCommand(ConsoleCommand command)
{
  if (command_count >= CONSOLE_COMMAND_QUEUE_DEPTH) {
    ++dropped_command_count;
    return;
  }

  command_queue[command_tail] = command;
  command_tail =
      (uint8_t)((command_tail + 1U) % CONSOLE_COMMAND_QUEUE_DEPTH);
  ++command_count;
}
