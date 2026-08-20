#ifdef UART_PROTOCOL_HOST_TEST

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "drivers/uart.h"
#include "communication/uart_protocol/uart_protocol.h"

bool uart_write(const void *data, size_t length)
{
  (void)data;
  (void)length;
  return false;
}

uint8_t uart_get_tx_slots_available(void)
{
  return 0U;
}

int main(void)
{
  char text[24];

  assert(UartProtocol_FormatSigned64(text, sizeof(text), 0));
  assert(strcmp(text, "0") == 0);
  assert(UartProtocol_FormatSigned64(text, sizeof(text), INT64_MAX));
  assert(strcmp(text, "9223372036854775807") == 0);
  assert(UartProtocol_FormatSigned64(text, sizeof(text), INT64_MIN));
  assert(strcmp(text, "-9223372036854775808") == 0);
  assert(!UartProtocol_FormatSigned64(text, 2U, 10));
  return 0;
}

#endif
