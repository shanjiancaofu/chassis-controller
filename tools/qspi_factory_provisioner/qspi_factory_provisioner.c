#include <stdbool.h>
#include <stdint.h>

#include "boot/boot_trace.h"
#include "boot/image_validator.h"
#include "bsp/qspi/boot_qspi_flash.h"
#include "components/crc/crc32.h"
#include "gpio.h"
#include "iwdg.h"
#include "main.h"
#include "quadspi.h"
#include "../../firmware/shared/flash_layout.h"
#include "../../firmware/shared/ota_metadata.h"
#include "../../firmware/shared/qspi_flash_identity.h"

#define STAGED_PACKAGE_ADDRESS 0x08008000UL
#define STAGED_PACKAGE_SIZE 183660UL
#define STAGED_METADATA_ADDRESS 0x0807F000UL
#define VERIFY_CHUNK_SIZE 256U

static bool ValidateInputs(const uint8_t *package,
                           const OtaMetadata *metadata);
static bool VerifyErased(uint32_t address);
static bool VerifyQspiMatches(uint32_t address, const uint8_t *expected,
                              uint32_t size);
static void Provision(void);
static void Stop(void) __attribute__((noreturn));
static void SystemClock_Config(void);

int main(void)
{
  uint8_t jedec_id[3];

  SCB->VTOR = FLASH_BASE;
  __DSB();
  __ISB();
  BootWatchdog_Refresh();

  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_QUADSPI1_Init();
  MX_IWDG_Init();

  BootTrace_Init();
  BootTrace_Write("PROVISION: START\r\n");

  if (!BootQspiFlash_ReadJedecId(jedec_id)) {
    BootTrace_Write("PROVISION: QSPI ID READ FAILED\r\n");
    Stop();
  }
  if (!QspiFlashIdentity_IsSupported(jedec_id)) {
    BootTrace_Write("PROVISION: QSPI ID UNSUPPORTED\r\n");
    Stop();
  }
  BootTrace_Write("PROVISION: QSPI EF4017\r\n");
  Provision();
}

static void Provision(void)
{
  const uint8_t *package = (const uint8_t *)STAGED_PACKAGE_ADDRESS;
  const OtaMetadata *metadata =
      (const OtaMetadata *)STAGED_METADATA_ADDRESS;
  uint32_t offset;

  BootTrace_Write("PROVISION: VALIDATE INPUTS\r\n");
  if (!ValidateInputs(package, metadata)) {
    BootTrace_Write("PROVISION: INPUTS INVALID\r\n");
    Stop();
  }
  BootTrace_Write("PROVISION: INPUTS VALID\r\n");

  if (!BootQspiFlash_EraseSector(OTA_QSPI_METADATA_A_START) ||
      !BootQspiFlash_EraseSector(OTA_QSPI_METADATA_B_START) ||
      !VerifyErased(OTA_QSPI_METADATA_A_START) ||
      !VerifyErased(OTA_QSPI_METADATA_B_START)) {
    BootTrace_Write("PROVISION: METADATA ERASE FAILED\r\n");
    Stop();
  }
  BootTrace_Write("PROVISION: METADATA ERASED\r\n");

  for (offset = 0U; offset < STAGED_PACKAGE_SIZE;
       offset += OTA_QSPI_SECTOR_SIZE) {
    if (!BootQspiFlash_EraseSector(OTA_QSPI_SLOT_A_START + offset)) {
      BootTrace_WriteValue("PROVISION: SLOT ERASE FAILED=", offset);
      Stop();
    }
    BootWatchdog_Refresh();
  }
  BootTrace_Write("PROVISION: SLOT A ERASED\r\n");

  for (offset = 0U; offset < STAGED_PACKAGE_SIZE;
       offset += VERIFY_CHUNK_SIZE) {
    const uint32_t remaining = STAGED_PACKAGE_SIZE - offset;
    const uint32_t chunk = remaining < VERIFY_CHUNK_SIZE
                               ? remaining
                               : VERIFY_CHUNK_SIZE;

    if (!BootQspiFlash_Program(OTA_QSPI_SLOT_A_START + offset,
                               &package[offset], chunk)) {
      BootTrace_WriteValue("PROVISION: SLOT WRITE FAILED=", offset);
      Stop();
    }
    BootWatchdog_Refresh();
  }
  BootTrace_Write("PROVISION: SLOT A WRITTEN\r\n");

  if (!VerifyQspiMatches(OTA_QSPI_SLOT_A_START, package,
                         STAGED_PACKAGE_SIZE)) {
    BootTrace_Write("PROVISION: SLOT VERIFY FAILED\r\n");
    Stop();
  }
  BootTrace_Write("PROVISION: SLOT A VERIFIED\r\n");

  if (!BootQspiFlash_Program(OTA_QSPI_METADATA_A_START, metadata,
                             sizeof(*metadata)) ||
      !VerifyQspiMatches(OTA_QSPI_METADATA_A_START,
                         (const uint8_t *)metadata,
                         sizeof(*metadata)) ||
      !VerifyErased(OTA_QSPI_METADATA_B_START)) {
    BootTrace_Write("PROVISION: METADATA COMMIT FAILED\r\n");
    Stop();
  }

  BootTrace_Write("PROVISION: METADATA A VERIFIED\r\n");
  BootTrace_Write("PROVISION: METADATA B ERASED\r\n");
  BootTrace_Write("PROVISION: PASS\r\n");
  Stop();
}

static bool ValidateInputs(const uint8_t *package,
                           const OtaMetadata *metadata)
{
  const OtaImageHeader *header = (const OtaImageHeader *)package;
  const uint32_t metadata_crc =
      BootCrc32_Calculate(metadata, sizeof(*metadata) - sizeof(uint32_t));

  return BootImageValidator_ValidatePackage(package, STAGED_PACKAGE_SIZE) ==
             BOOT_IMAGE_OK &&
         metadata->magic == OTA_METADATA_MAGIC &&
         metadata->format_version == OTA_METADATA_FORMAT_VERSION &&
         metadata->record_size == sizeof(*metadata) &&
         metadata->state == OTA_STATE_CONFIRMED &&
         metadata->confirmed_slot == OTA_SLOT_A &&
         metadata->candidate_slot == OTA_SLOT_NONE &&
         metadata->image_size == header->payload_size &&
         metadata->image_crc32 == header->payload_crc32 &&
         metadata->record_crc32 == metadata_crc;
}

static bool VerifyErased(uint32_t address)
{
  uint8_t buffer[VERIFY_CHUNK_SIZE];
  uint32_t offset;
  uint32_t index;

  for (offset = 0U; offset < OTA_QSPI_SECTOR_SIZE;
       offset += sizeof(buffer)) {
    if (!BootQspiFlash_Read(address + offset, buffer, sizeof(buffer))) {
      return false;
    }
    for (index = 0U; index < sizeof(buffer); ++index) {
      if (buffer[index] != 0xFFU) {
        return false;
      }
    }
    BootWatchdog_Refresh();
  }
  return true;
}

static bool VerifyQspiMatches(uint32_t address, const uint8_t *expected,
                              uint32_t size)
{
  uint8_t buffer[VERIFY_CHUNK_SIZE];
  uint32_t offset;
  uint32_t index;

  for (offset = 0U; offset < size; offset += sizeof(buffer)) {
    const uint32_t remaining = size - offset;
    const uint32_t chunk =
        remaining < sizeof(buffer) ? remaining : sizeof(buffer);

    if (!BootQspiFlash_Read(address + offset, buffer, chunk)) {
      return false;
    }
    for (index = 0U; index < chunk; ++index) {
      if (buffer[index] != expected[offset + index]) {
        return false;
      }
    }
    BootWatchdog_Refresh();
  }
  return true;
}

static void Stop(void)
{
  for (;;) {
    __WFI();
  }
}

static void SystemClock_Config(void)
{
  RCC_OscInitTypeDef oscillator = {0};
  RCC_ClkInitTypeDef clocks = {0};

  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
  oscillator.OscillatorType = RCC_OSCILLATORTYPE_HSI |
                              RCC_OSCILLATORTYPE_LSI;
  oscillator.HSIState = RCC_HSI_ON;
  oscillator.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  oscillator.LSIState = RCC_LSI_ON;
  oscillator.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&oscillator) != HAL_OK) {
    Error_Handler();
  }

  clocks.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                     RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  clocks.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clocks.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clocks.APB1CLKDivider = RCC_HCLK_DIV1;
  clocks.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clocks, FLASH_LATENCY_0) != HAL_OK) {
    Error_Handler();
  }
}

void Error_Handler(void)
{
  __disable_irq();
  Stop();
}
