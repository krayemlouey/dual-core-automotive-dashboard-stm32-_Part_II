$port = [System.IO.Ports.SerialPort]::new("COM5", 115200)
$port.ReadTimeout = 800
$port.NewLine = "`r`n"
$port.Open()
$port.DiscardInBuffer()

Write-Host "=== Test alerte STM32 (FIFO enabled, polling 1ms) ===" -ForegroundColor Cyan
Write-Host "Attente 3s pour laisser le STM32 demarrer..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

Write-Host "`n>>> ENVOI '1' (commande courte, 1 octet) <<<" -ForegroundColor Red
# Envoyer juste "1\n" - 2 octets - fiable avec FIFO
$port.WriteLine("1")

Write-Host "Attente reponse STM32 (6 secondes)..." -ForegroundColor Yellow
$end = (Get-Date).AddSeconds(6)
$alertReceived = $false
while ((Get-Date) -lt $end) {
    try {
        $line = $port.ReadLine()
        $now  = (Get-Date).ToString("HH:mm:ss.fff")
        if ($line -match "ALERTE|CIGARETTE|STM32 RX") {
            Write-Host "[$now] *** $line ***" -ForegroundColor Red
            $alertReceived = $true
        } else {
            Write-Host "[$now] $line" -ForegroundColor Gray
        }
    } catch [System.TimeoutException] { }
}

if ($alertReceived) {
    Write-Host "`n=== SUCCES : L'alerte a ete confirmee par le STM32 ! ===" -ForegroundColor Green
} else {
    Write-Host "`n=== (Pas de reponse texte - verifier l'ecran LCD pour la couleur rouge) ===" -ForegroundColor Yellow
}

$port.Close()
