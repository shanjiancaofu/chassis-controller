#include "bsp/imu/bsp_icm45686_dma.h"

#include "board/board_config.h"
#include "main.h"

static DMA_HandleTypeDef imu_dma_rx;
static DMA_HandleTypeDef imu_dma_tx;
static bool dma_initialized;

bool BspIcm45686Dma_Init(void)
{
  if (dma_initialized) {
    return true;
  }

  __HAL_RCC_DMAMUX1_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  imu_dma_rx.Instance = DMA1_Channel5;
  imu_dma_rx.Init.Request = DMA_REQUEST_SPI3_RX;
  imu_dma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;
  imu_dma_rx.Init.PeriphInc = DMA_PINC_DISABLE;
  imu_dma_rx.Init.MemInc = DMA_MINC_ENABLE;
  imu_dma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  imu_dma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  imu_dma_rx.Init.Mode = DMA_NORMAL;
  imu_dma_rx.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&imu_dma_rx) != HAL_OK) {
    return false;
  }

  imu_dma_tx.Instance = DMA1_Channel6;
  imu_dma_tx.Init.Request = DMA_REQUEST_SPI3_TX;
  imu_dma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
  imu_dma_tx.Init.PeriphInc = DMA_PINC_DISABLE;
  imu_dma_tx.Init.MemInc = DMA_MINC_ENABLE;
  imu_dma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  imu_dma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  imu_dma_tx.Init.Mode = DMA_NORMAL;
  imu_dma_tx.Init.Priority = DMA_PRIORITY_HIGH;
  if (HAL_DMA_Init(&imu_dma_tx) != HAL_OK) {
    HAL_DMA_DeInit(&imu_dma_rx);
    return false;
  }

  __HAL_LINKDMA(&BOARD_IMU_SPI, hdmarx, imu_dma_rx);
  __HAL_LINKDMA(&BOARD_IMU_SPI, hdmatx, imu_dma_tx);
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5U, 0U);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  dma_initialized = true;
  return true;
}

void DMA1_Channel5_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&imu_dma_rx);
}

void DMA1_Channel6_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&imu_dma_tx);
}
