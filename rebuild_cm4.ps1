$base    = "C:\Users\ironpc\Downloads\Dual-core_automotiveDashbord_onSTM32-main\Dual-core_automotiveDashbord_onSTM32-main"
$gcc_dir = "C:\ST\STM32CubeIDE_2.2.0\STM32CubeIDE\plugins\com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.win32_1.0.100.202602081740\tools\bin"
$gcc     = "$gcc_dir\arm-none-eabi-gcc.exe"
$gpp     = "$gcc_dir\arm-none-eabi-g++.exe"
$objcopy = "$gcc_dir\arm-none-eabi-objcopy.exe"
$size_tool = "$gcc_dir\arm-none-eabi-size.exe"

$build_obj = "$base\CM4\TouchGFX\build\STM32H747I_DISCO"
$bin       = "$base\CM4\TouchGFX\build\bin"
$target_elf= "$bin\target.elf"
$target_hex= "$bin\target.hex"
$elf_out   = "$bin\intflash.elf"
$hex_out   = "$bin\intflash.hex"

$CPU_FLAGS = @("-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard")
$C_FLAGS   = @("-std=gnu11", "-Os", "-g", "-Wall", "-ffunction-sections", "-fdata-sections", "-fstack-usage")
$DEFINES   = @("-DST", "-DSTM32H747xx", "-DUSE_HAL_DRIVER", "-DUSE_STM32H747I_DISCO", "-DUSE_PWR_DIRECT_SMPS_SUPPLY", "-DCORE_CM4")
$INCLUDES  = @(
    "-I$base\Drivers\BSP\STM32H747I-Discovery",
    "-I$base\Drivers\CMSIS\Include",
    "-I$base\Drivers\CMSIS\Device\ST\STM32H7xx\Include",
    "-I$base\Drivers\STM32H7xx_HAL_Driver\Inc",
    "-I$base\CM4\Core\Inc"
)

Write-Host "=== RECOMPILATION CM4 (Cortex-M4) ===" -ForegroundColor Cyan
$main_c = "$base\CM4\Core\Src\main.c"
$main_o = "$build_obj\CM4\Core\Src\main.o"
Write-Host "[CC] CM4\Core\Src\main.c..."
& $gcc $CPU_FLAGS $C_FLAGS $DEFINES $INCLUDES -c $main_c -o $main_o
if ($LASTEXITCODE -ne 0) { throw "Erreur compilation main.c CM4" }
Write-Host "  OK -> $main_o" -ForegroundColor Green

Write-Host "=== LINKING CM4 target.elf ===" -ForegroundColor Cyan
$all_objs = Get-ChildItem -Path $build_obj -Recurse -Filter "*.o" | ForEach-Object { $_.FullName.Replace("\", "/") }
$rsp_file = "$build_obj\cm4_objects.rsp"
$all_objs -join " " | Set-Content -Path $rsp_file -NoNewline -Encoding ASCII

$ld_script = "$base\CM4\stm32h747xx_flash_CM4.ld".Replace("\", "/")
$map_file  = "$bin\application.map".Replace("\", "/")

$ld_flags = @(
    "-mcpu=cortex-m4", "-mthumb", "-mfpu=fpv5-sp-d16", "-mfloat-abi=hard",
    "--specs=nosys.specs", "-Os", "-Wl,--gc-sections", "-static",
    "-T", $ld_script,
    "-Wl,-Map=$map_file",
    "-Wl,--start-group", "-lc", "-lm", "-lstdc++", "-lsupc++", "-Wl,--end-group",
    "-fno-exceptions", "-fno-rtti", "-g"
)

& $gpp $ld_flags -o $target_elf ("@" + $rsp_file)
if ($LASTEXITCODE -ne 0) { throw "Erreur linking CM4" }
Write-Host "[LD] OK -> $target_elf" -ForegroundColor Green

& $objcopy -O ihex $target_elf $target_hex
Copy-Item $target_elf $elf_out -Force
Copy-Item $target_hex $hex_out -Force

Write-Host "=== BUILD CM4 TERMINE AVEC SUCCES ===" -ForegroundColor Green
& $size_tool $elf_out
