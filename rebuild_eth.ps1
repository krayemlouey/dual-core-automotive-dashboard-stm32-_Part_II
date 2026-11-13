#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Recompile les fichiers modifiés et re-linke target.elf / intflash.hex pour STM32H747I-DISCO (CM7).
#>

$base    = "C:\Users\ironpc\Downloads\Dual-core_automotiveDashbord_onSTM32-main\Dual-core_automotiveDashbord_onSTM32-main"
$gcc_dir = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
$gcc     = "$gcc_dir\arm-none-eabi-gcc.exe"
$gpp     = "$gcc_dir\arm-none-eabi-g++.exe"
$objcopy = "$gcc_dir\arm-none-eabi-objcopy.exe"
$size_tool = "$gcc_dir\arm-none-eabi-size.exe"

$build_obj = "$base\CM7\TouchGFX\build\STM32H747I_DISCO"
$bin       = "$base\CM7\TouchGFX\build\bin"
$target_elf= "$bin\target.elf"
$target_hex= "$bin\target.hex"
$elf_out   = "$bin\intflash.elf"
$hex_out   = "$bin\intflash.hex"

$CPU_FLAGS = @(
    "-mcpu=cortex-m7",
    "-mthumb",
    "-mfpu=fpv5-sp-d16",
    "-mfloat-abi=hard"
)

$C_FLAGS = @(
    "-std=gnu11",
    "-Os",
    "-g3",
    "-Wall",
    "-ffunction-sections",
    "-fdata-sections",
    "-fstack-usage"
)

$DEFINES = @(
    "-DST",
    "-DSTM32H747xx",
    "-DUSE_HAL_DRIVER",
    "-DUSE_STM32H747I_DISCO",
    "-DUSE_PWR_DIRECT_SMPS_SUPPLY",
    "-DCORE_CM7"
)

$INCLUDES = @(
    "-I$base\Drivers\BSP\STM32H747I-Discovery",
    "-I$base\Drivers\CMSIS\Include",
    "-I$base\Drivers\CMSIS\Device\ST\STM32H7xx\Include",
    "-I$base\Drivers\STM32H7xx_HAL_Driver\Inc",
    "-I$base\Drivers\STM32H7xx_HAL_Driver\Inc\Legacy",
    "-I$base\CM7\Core\Inc",
    "-I$base\CM7\FATFS\Target",
    "-I$base\CM7\FATFS\App",
    "-I$base\CM7\LIBJPEG\App",
    "-I$base\CM7\LIBJPEG\Target",
    "-I$base\Middlewares\Third_Party\LibJPEG\include",
    "-I$base\Middlewares\Third_Party\FatFs\src",
    "-I$base\Middlewares\Third_Party\LwIP\src\include",
    "-I$base\Middlewares\Third_Party\LwIP\src\include\lwip",
    "-I$base\Middlewares\Third_Party\LwIP\src\include\netif",
    "-I$base\Middlewares\Third_Party\LwIP\system",
    "-I$base\Drivers\BSP\Components\lan8742",
    "-I$base\Middlewares\Third_Party\FreeRTOS\Source\include",
    "-I$base\Middlewares\Third_Party\FreeRTOS\Source\CMSIS_RTOS_V2",
    "-I$base\Middlewares\Third_Party\FreeRTOS\Source\portable\GCC\ARM_CM4F",
    "-I$base\Drivers\CMSIS\RTOS2\Include",
    "-I$base\CM7\Middlewares\ST\touchgfx\framework\include",
    "-I$base\CM7\TouchGFX\App",
    "-I$base\CM7\TouchGFX\target",
    "-I$base\CM7\TouchGFX\gui\include",
    "-I$base\CM7\TouchGFX\generated\gui_generated\include",
    "-I$base\CM7\TouchGFX\generated\fonts\include",
    "-I$base\CM7\TouchGFX\generated\images\include",
    "-I$base\CM7\TouchGFX\generated\texts\include",
    "-I$base\CM7\TouchGFX\generated\videos\include",
    "-I$base\Common\Inc",
    "-I$base\Utilities\CPU",
    "-I$base\Utilities\JPEG",
    "-I$base\Utilities\Fonts"
)

$CXX_FLAGS = @(
    "-std=c++14",
    "-fno-rtti",
    "-fno-exceptions",
    "-Os",
    "-g3",
    "-Wall",
    "-ffunction-sections",
    "-fdata-sections"
)

$ALL_C_FLAGS   = $CPU_FLAGS + $C_FLAGS + $DEFINES + $INCLUDES
$ALL_CXX_FLAGS = $CPU_FLAGS + $CXX_FLAGS + $DEFINES + $INCLUDES

Write-Host "=== RECOMPILATION FICHIERS MODIFIES ===" -ForegroundColor Cyan

$files_to_rebuild = @(
    @{ src = "CM7\Core\Src\main.c";                                              obj = "$build_obj\CM7\Core\Src\main.o"; is_cpp = $false },
    @{ src = "CM7\Core\Src\ethernetif.c";                                        obj = "$build_obj\CM7\Core\Src\ethernetif.o"; is_cpp = $false },
    @{ src = "CM7\Core\Src\udp_sender.c";                                        obj = "$build_obj\CM7\Core\Src\udp_sender.o"; is_cpp = $false },
    @{ src = "CM7\Core\Src\stream_receiver.c";                                   obj = "$build_obj\CM7\Core\Src\stream_receiver.o"; is_cpp = $false },
    @{ src = "CM7\Core\Src\freertos.c";                                          obj = "$build_obj\CM7\Core\Src\freertos.o"; is_cpp = $false },
    @{ src = "gcc\sysmem.c";                                                     obj = "$build_obj\gcc\sysmem.o"; is_cpp = $false },
    @{ src = "CM7\TouchGFX\gui\src\dashboardmedium_screen\DashboardMediumView.cpp"; obj = "$build_obj\CM7\TouchGFX\gui\src\dashboardmedium_screen\DashboardMediumView.o"; is_cpp = $true }
)

$failed = $false
foreach ($f in $files_to_rebuild) {
    $src_full = "$base\$($f.src)"
    $obj      = $f.obj
    $compiler = if ($f.is_cpp) { $gpp } else { $gcc }
    $flags    = if ($f.is_cpp) { $ALL_CXX_FLAGS } else { $ALL_C_FLAGS }

    Write-Host "`n[$($if_cpp = if($f.is_cpp){'CXX'}else{'CC'})] $($f.src)..." -ForegroundColor Yellow
    
    $args_list = $flags + @("-c", $src_full, "-o", $obj)
    & $compiler @args_list 2>&1 | ForEach-Object { Write-Host "  $_" }
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  ERREUR compilation $($f.src)" -ForegroundColor Red
        $failed = $true
        break
    } else {
        Write-Host "  OK -> $obj" -ForegroundColor Green
    }
}

if ($failed) {
    Write-Host "`nBUILD ECHOUE" -ForegroundColor Red
    exit 1
}

Write-Host "`n=== LINKING de target.elf (g++) ===" -ForegroundColor Cyan

$all_objs = Get-ChildItem -Path $build_obj -Recurse -Filter "*.o" | 
            ForEach-Object { $_.FullName } |
            Where-Object { $_ -notmatch "simulator" }

Write-Host "Nombre d'objets: $($all_objs.Count)"

# Ecrire la liste d'objets dans un fichier de réponse avec forward slashes
$obj_rsp = "$bin\objects.rsp"
($all_objs | ForEach-Object { $_.Replace('\', '/') }) | Out-File -FilePath $obj_rsp -Encoding ASCII

$LD_FLAGS = @(
    "-mcpu=cortex-m7",
    "-mthumb",
    "-mfpu=fpv5-sp-d16",
    "-mfloat-abi=hard",
    "--specs=nosys.specs",
    "-Os",
    "-Wl,--gc-sections",
    "-static",
    "-Wl,--print-memory-usage",
    "-fno-exceptions",
    "-fno-rtti",
    "-g3",
    "-T$base\CM7\stm32h747xx_flash_CM7.ld",
    "-Wl,-Map=$bin\application.map",
    "-L$base\CM7\Middlewares\ST\touchgfx\lib\core\cortex_m7\gcc",
    "@$obj_rsp",
    "-o", $target_elf,
    "-Wl,--start-group",
    "-ltouchgfx-float-abi-hard",
    "-lc", "-lm", "-lstdc++", "-lsupc++",
    "-Wl,--end-group"
)

Write-Host "[LD] Linkage en cours..."
& $gpp @LD_FLAGS 2>&1 | Select-Object -Last 30

if ($LASTEXITCODE -ne 0) {
    Write-Host "LINK ECHOUE" -ForegroundColor Red
    exit 1
}
Write-Host "[LD] OK -> $target_elf" -ForegroundColor Green

Write-Host "[HEX] Génération target.hex..."
& $objcopy -O ihex $target_elf $target_hex

Write-Host "[HEX] Génération intflash.elf (sans sections externes)..."
& $objcopy --remove-section=ExtFlashSection --remove-section=TextFlashSection $target_elf $elf_out

Write-Host "[HEX] Génération intflash.hex (sans sections externes)..."
& $objcopy -O ihex --remove-section=ExtFlashSection --remove-section=TextFlashSection $target_elf $hex_out

Write-Host "`n=== BUILD TERMINE AVEC SUCCES ===" -ForegroundColor Green
& $size_tool $elf_out
