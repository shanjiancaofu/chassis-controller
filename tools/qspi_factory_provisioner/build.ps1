$ErrorActionPreference = "Stop"

$repo = (Resolve-Path "$PSScriptRoot\..\..").Path
$project = Join-Path $repo "firmware\bootloader\stm32g474"
$release = Join-Path $project "Release"
$output = Join-Path $repo "_output\archive\qspi-factory-provisioner"
$plugin = Get-ChildItem "E:\STM32CubeIDE\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins" `
  -Directory -Filter "com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*" |
  Sort-Object Name -Descending | Select-Object -First 1
$bin = Join-Path $plugin.FullName "tools\bin"
$gcc = Join-Path $bin "arm-none-eabi-gcc.exe"
$objcopy = Join-Path $bin "arm-none-eabi-objcopy.exe"
$size = Join-Path $bin "arm-none-eabi-size.exe"

New-Item -ItemType Directory -Force $output | Out-Null
$common = @(
  "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard",
  "-std=gnu11", "-DUSE_HAL_DRIVER", "-DSTM32G474xx", "-Os",
  "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra", "-Werror",
  "-I$project", "-I$project\Core\Inc",
  "-I$project\Drivers\STM32G4xx_HAL_Driver\Inc",
  "-I$project\Drivers\STM32G4xx_HAL_Driver\Inc\Legacy",
  "-I$project\Drivers\CMSIS\Device\ST\STM32G4xx\Include",
  "-I$project\Drivers\CMSIS\Include"
)

$toolObject = Join-Path $output "qspi_factory_provisioner.o"
& $gcc @common -c (Join-Path $PSScriptRoot "qspi_factory_provisioner.c") -o $toolObject
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$objects = @(
  "$release\Core\Src\gpio.o", "$release\Core\Src\iwdg.o",
  "$release\Core\Src\quadspi.o", "$release\Core\Src\stm32g4xx_hal_msp.o",
  "$release\Core\Src\stm32g4xx_it.o", "$release\Core\Src\syscalls.o",
  "$release\Core\Src\sysmem.o", "$release\Core\Src\system_stm32g4xx.o",
  "$release\Core\Startup\startup_stm32g474vetx.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_cortex.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_dma.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_dma_ex.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_gpio.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_iwdg.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_pwr.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_pwr_ex.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_qspi.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_rcc.o",
  "$release\Drivers\STM32G4xx_HAL_Driver\Src\stm32g4xx_hal_rcc_ex.o",
  "$release\boot\boot_trace.o", "$release\boot\image_validator.o",
  "$release\bsp\qspi\boot_qspi_flash.o", "$release\components\crc\crc32.o",
  $toolObject
)

$elf = Join-Path $output "qspi-factory-provisioner.elf"
$image = Join-Path $output "qspi-factory-provisioner.bin"
& $gcc @objects "-T$project\STM32G474VETX_FLASH.ld" `
  "-Wl,--gc-sections" "-Wl,-Map=$output\qspi-factory-provisioner.map" `
  "--specs=nosys.specs" "--specs=nano.specs" -static `
  -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard `
  "-Wl,--start-group" -lc -lm "-Wl,--end-group" -o $elf
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $objcopy -O binary $elf $image
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $size $elf
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Get-FileHash -Algorithm SHA256 $image | Format-List
