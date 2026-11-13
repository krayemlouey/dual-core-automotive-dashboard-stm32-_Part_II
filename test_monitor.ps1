$port = [System.IO.Ports.SerialPort]::new("COM5", 115200)
$port.ReadTimeout = 500
$port.NewLine = "`r`n"
$port.Open()
$port.DiscardInBuffer()

Write-Host "=== Surveillance port COM5 (STM32H747) ===" -ForegroundColor Cyan
Write-Host "Attente 2s puis envoi de l'alerte CIGARETTE..." -ForegroundColor Yellow
Start-Sleep -Seconds 2

Write-Host "`n>>> ENVOI ALERT: CIGARETTE <<<" -ForegroundColor Red
$port.WriteLine("ALERT: CIGARETTE")

$end = (Get-Date).AddSeconds(6)
while ((Get-Date) -lt $end) {
    try {
        $line = $port.ReadLine()
        $now = (Get-Date).ToString("HH:mm:ss.fff")
        if ($line -match "ALERTE|CIGARETTE|STM32 RX") {
            Write-Host "[$now] STM32: $line" -ForegroundColor Red
        } else {
            Write-Host "[$now] STM32: $line"
        }
    } catch [System.TimeoutException] { }
}

$port.Close()
Write-Host "=== Test termine ===" -ForegroundColor Cyan
