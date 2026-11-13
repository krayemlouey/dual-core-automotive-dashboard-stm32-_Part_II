$port = [System.IO.Ports.SerialPort]::new("COM5", 115200)
$port.ReadTimeout = 2000
$port.Open()
try {
    $line = $port.ReadLine()
    Write-Host "Ligne reçue de la STM32: $line"
} catch {
    Write-Host "Timeout attente ligne: $_"
}
$port.WriteLine("ALERT: CIGARETTE")
Start-Sleep -Milliseconds 300
try {
    $line2 = $port.ReadLine()
    Write-Host "Réponse alerte: $line2"
} catch {
    Write-Host "Pas de 2eme ligne: $_"
}
$port.Close()
