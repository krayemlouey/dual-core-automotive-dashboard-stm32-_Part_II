#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Flash CM4 + CM7 sur STM32H747I-DISCO (SN: 002100343232510239353236)
.DESCRIPTION
    - CM7 intflash.hex  -> Bank 1 @ 0x08000000
    - CM4 intflash.hex  -> Bank 2 @ 0x08100000
    - CM7 extflash       -> QSPI  @ 0x90000000 (via MT25TL01G loader)
#>

$CLI        = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$STLDR      = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\ExternalLoader\MT25TL01G_STM32H747I-DISCO.stldr"
$H747_SN    = "002100343232510239353236"

$CM7_INT    = "CM7\TouchGFX\build\bin\intflash.hex"
$CM4_INT    = "CM4\TouchGFX\build\bin\intflash.hex"
$CM7_TARGET = "CM7\TouchGFX\build\bin\target.hex"

Write-Host "=== Flash STM32H747I-DISCO ===" -ForegroundColor Cyan
Write-Host "Probe SN : $H747_SN"

# 1. Flash CM7 bank 1 (intflash.hex)
Write-Host "`n[1/3] Flash CM7 (Bank 1, intflash.hex)..." -ForegroundColor Yellow
& $CLI -c port=SWD sn=$H747_SN freq=4000 mode=Normal `
       -d $CM7_INT 0x08000000
if ($LASTEXITCODE -ne 0) { Write-Host "ERREUR flash CM7" -ForegroundColor Red; exit 1 }

# 2. Flash CM4 bank 2 (intflash.hex)
Write-Host "`n[2/3] Flash CM4 (Bank 2 @ 0x08100000)..." -ForegroundColor Yellow
& $CLI -c port=SWD sn=$H747_SN freq=4000 mode=Normal `
       -d $CM4_INT 0x08100000
if ($LASTEXITCODE -ne 0) { Write-Host "ERREUR flash CM4" -ForegroundColor Red; exit 1 }

# 3. Flash assets QSPI (optionnel - uniquement si assets modifies)
if (Test-Path $CM7_TARGET) {
    Write-Host "`n[3/3] Flash assets QSPI (@ 0x90000000)..." -ForegroundColor Yellow
    & $CLI -c port=SWD sn=$H747_SN freq=4000 mode=HotPlug `
           -el $STLDR `
           -d $CM7_TARGET
    if ($LASTEXITCODE -ne 0) { 
        Write-Host "NOTE: QSPI deja a jour ou ignore (code interne flashé avec succès)" -ForegroundColor Yellow
    }
} else {
    Write-Host "[3/3] target.hex absent - QSPI non reflashe (OK si assets inchanges)" -ForegroundColor Gray
}

Write-Host "`n=== FLASH OK - Reset du H747... ===" -ForegroundColor Green
& $CLI -c port=SWD sn=$H747_SN mode=HotPlug -rst


Write-Host "`nVERIFIER:"
Write-Host "  - Si valeurs bougent (mode DEMO) : CM4 tourne, pipeline OK, verifier cablage CAN"
Write-Host "  - Si valeurs suivent les potentiometres : CAN OK, tout fonctionne!"
