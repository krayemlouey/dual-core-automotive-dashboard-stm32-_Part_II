# Script de compilation et téléversement automatique pour NUCLEO-F446RE
$ErrorActionPreference = "Stop"

$toolchain = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
$gcc = "$toolchain\arm-none-eabi-gcc.exe"
$objcopy = "$toolchain\arm-none-eabi-objcopy.exe"
$size = "$toolchain\arm-none-eabi-size.exe"
$programmer = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

$baseDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = "$baseDir\build"
if (!(Test-Path $buildDir)) { New-Item -ItemType Directory -Path $buildDir -Force | Out-Null }

Write-Host ">>> 1. Compilation des sources..." -ForegroundColor Cyan

$inc = @(
  "-I$baseDir/Core/Inc",
  "-I$baseDir/Drivers/STM32F4xx_HAL_Driver/Inc",
  "-I$baseDir/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy",
  "-I$baseDir/Drivers/CMSIS/Device/ST/STM32F4xx/Include",
  "-I$baseDir/Drivers/CMSIS/Include",
  "-I$baseDir/Drivers/BSP/STM32F4xx-Nucleo"
)
$cflags = @("-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-DSTM32F446xx", "-DUSE_HAL_DRIVER", "-O2", "-Wall", "-fdata-sections", "-ffunction-sections")

$sources = @(
  "$baseDir/Core/Src/main.c",
  "$baseDir/Core/Src/stm32f4xx_hal_msp.c",
  "$baseDir/Core/Src/stm32f4xx_it.c",
  "$baseDir/Core/Src/system_stm32f4xx.c",
  "$baseDir/Core/Src/sysmem.c",
  "$baseDir/Core/Src/syscalls.c",
  "$baseDir/Core/Startup/startup_stm32f446retx.s",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc_ex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_dma_ex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_cortex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_pwr_ex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_flash_ex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_exti.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_adc_ex.c",
  "$baseDir/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_can.c"
)


$objs = @()
foreach ($src in $sources) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj = "$buildDir/$name.o"
    & $gcc $cflags $inc -c $src -o $obj
    $objs += $obj
}

$ld = "$baseDir/STM32F446RETX_FLASH.ld"
$elf = "$buildDir/NUCLEO_F446RE_CAN_TX.elf"
$hex = "$buildDir/NUCLEO_F446RE_CAN_TX.hex"
$bin = "$buildDir/NUCLEO_F446RE_CAN_TX.bin"

$ldflags = @("-mcpu=cortex-m4", "-mfpu=fpv4-sp-d16", "-mfloat-abi=hard", "-mthumb", "-T$ld", "-Wl,--gc-sections", "-Wl,-Map=$buildDir/NUCLEO_F446RE_CAN_TX.map", "--specs=nano.specs", "--specs=nosys.specs")
& $gcc $ldflags -o $elf $objs -lm -lc
& $objcopy -O ihex $elf $hex
& $objcopy -O binary $elf $bin
& $size $elf

Write-Host ">>> 2. Téléversement vers NUCLEO-F446RE..." -ForegroundColor Cyan
& $programmer -c port=SWD sn=0668FF303435554157102434 -w $bin 0x08000000 -v -rst

Write-Host ">>> TERMINE AVEC SUCCES !" -ForegroundColor Green
