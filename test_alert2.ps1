$port = [System.IO.Ports.SerialPort]::new("COM5", 115200)
$port.ReadTimeout = 1000
$port.NewLine = "`r`n"
$port.Open()

# Vider le buffer de réception existant
$port.DiscardInBuffer()

Write-Host "Envoi de l'alerte..."
$port.WriteLine("ALERT: CIGARETTE")

$end = (Get-Date).AddSeconds(3)
while ((Get-Date) -lt $end) {
    try {
        $line = $port.ReadLine()
        Write-Host "STM32: $line"
    } catch [TimeoutException] {
        # timeout normal
    }
}

$port.Close()
