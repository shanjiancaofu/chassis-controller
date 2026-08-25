#ifndef CHASSIS_HOST_STM32G4XX_HAL_H
#define CHASSIS_HOST_STM32G4XX_HAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR = 1 } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET = 0, GPIO_PIN_SET = 1 } GPIO_PinState;

typedef struct {
  uint32_t unused;
} GPIO_TypeDef;

GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *port, uint16_t pin);
void HAL_GPIO_WritePin(GPIO_TypeDef *port, uint16_t pin,
                       GPIO_PinState state);
void HAL_GPIO_TogglePin(GPIO_TypeDef *port, uint16_t pin);

typedef struct {
  uint32_t unused;
} DMA_HandleTypeDef;

typedef struct {
  void *Instance;
  uint32_t gState;
  uint32_t RxState;
  DMA_HandleTypeDef *hdmarx;
} UART_HandleTypeDef;

#define HAL_UART_STATE_READY 0U
#define HAL_UART_STATE_BUSY 1U
#define DMA_IT_HT 1U
#define __HAL_DMA_DISABLE_IT(handle, interrupt) \
  do { (void)(handle); (void)(interrupt); } while (0)

static inline uint32_t __get_PRIMASK(void) { return 0U; }
static inline void __disable_irq(void) {}
static inline void __set_PRIMASK(uint32_t value) { (void)value; }

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *handle,
                                               uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *handle,
                                        uint8_t *data, uint16_t size);

typedef struct {
  void *Instance;
  uint32_t State;
} QSPI_HandleTypeDef;

typedef struct {
  uint32_t Instruction;
  uint32_t InstructionMode;
  uint32_t Address;
  uint32_t AddressMode;
  uint32_t AddressSize;
  uint32_t AlternateByteMode;
  uint32_t DataMode;
  uint32_t DummyCycles;
  uint32_t NbData;
  uint32_t DdrMode;
  uint32_t DdrHoldHalfCycle;
  uint32_t SIOOMode;
} QSPI_CommandTypeDef;

#define QUADSPI ((void *)0x1000U)
#define HAL_QSPI_STATE_READY 0U
#define QSPI_INSTRUCTION_1_LINE 1U
#define QSPI_ADDRESS_NONE 0U
#define QSPI_ADDRESS_1_LINE 1U
#define QSPI_ADDRESS_24_BITS 24U
#define QSPI_ALTERNATE_BYTES_NONE 0U
#define QSPI_DATA_NONE 0U
#define QSPI_DATA_1_LINE 1U
#define QSPI_DDR_MODE_DISABLE 0U
#define QSPI_DDR_HHC_ANALOG_DELAY 0U
#define QSPI_SIOO_INST_EVERY_CMD 0U

HAL_StatusTypeDef HAL_QSPI_Command(QSPI_HandleTypeDef *handle,
                                   QSPI_CommandTypeDef *command,
                                   uint32_t timeout);
HAL_StatusTypeDef HAL_QSPI_Receive(QSPI_HandleTypeDef *handle, uint8_t *data,
                                   uint32_t timeout);
HAL_StatusTypeDef HAL_QSPI_Receive_DMA(QSPI_HandleTypeDef *handle,
                                       uint8_t *data);
HAL_StatusTypeDef HAL_QSPI_Transmit_DMA(QSPI_HandleTypeDef *handle,
                                        uint8_t *data);
HAL_StatusTypeDef HAL_QSPI_Abort(QSPI_HandleTypeDef *handle);

#endif
