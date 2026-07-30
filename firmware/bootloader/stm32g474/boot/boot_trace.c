#include "boot/boot_trace.h"

#include <stdbool.h>
#include <stdint.h>

#include "main.h"

#define BOOT_TRACE_BAUD_RATE 115200UL
#define BOOT_TRACE_FLAG_WAIT_LIMIT 1000000UL

static bool WaitForFlag(uint32_t flag)
{
  uint32_t remaining = BOOT_TRACE_FLAG_WAIT_LIMIT;

  while ((USART1->ISR & flag) == 0U) {
    if (--remaining == 0U) {
      return false;
    }
  }
  return true;
}

static bool WriteByte(uint8_t value)
{
  if (!WaitForFlag(USART_ISR_TXE_TXFNF)) {
    return false;
  }
  USART1->TDR = value;
  if (!WaitForFlag(USART_ISR_TC)) {
    return false;
  }
  return true;
}

void BootTrace_Init(void)
{
  uint32_t divider;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART1_FORCE_RESET();
  __HAL_RCC_USART1_RELEASE_RESET();

  MODIFY_REG(GPIOA->MODER,
             GPIO_MODER_MODE9 | GPIO_MODER_MODE10,
             GPIO_MODER_MODE9_1 | GPIO_MODER_MODE10_1);
  CLEAR_BIT(GPIOA->OTYPER, GPIO_OTYPER_OT9 | GPIO_OTYPER_OT10);
  CLEAR_BIT(GPIOA->PUPDR, GPIO_PUPDR_PUPD9 | GPIO_PUPDR_PUPD10);
  MODIFY_REG(GPIOA->AFR[1], GPIO_AFRH_AFSEL9 | GPIO_AFRH_AFSEL10,
             (7UL << GPIO_AFRH_AFSEL9_Pos) |
                 (7UL << GPIO_AFRH_AFSEL10_Pos));

  USART1->CR1 = 0U;
  divider = (HAL_RCC_GetPCLK2Freq() + BOOT_TRACE_BAUD_RATE / 2U) /
            BOOT_TRACE_BAUD_RATE;
  USART1->BRR = divider;
  USART1->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
  if (!WaitForFlag(USART_ISR_TEACK) ||
      !WaitForFlag(USART_ISR_REACK)) {
    return;
  }
  BootTrace_Write("BOOT: START\r\n");
}

void BootTrace_Write(const char *text)
{
  if (text == 0) {
    return;
  }
  while (*text != '\0') {
    if (!WriteByte((uint8_t)*text++)) {
      return;
    }
  }
}

void BootTrace_WriteValue(const char *label, uint32_t value)
{
  static const char digits[] = "0123456789ABCDEF";
  int32_t shift;

  BootTrace_Write(label);
  BootTrace_Write("0x");
  for (shift = 28; shift >= 0; shift -= 4) {
    if (!WriteByte(
            (uint8_t)digits[(value >> (uint32_t)shift) & 0x0FU])) {
      return;
    }
  }
  BootTrace_Write("\r\n");
}
