$port = [System.IO.Ports.SerialPort]::new("COM5", 115200)
$port.Open()
$port.WriteLine("ALERT: CIGARETTE")
Start-Sleep -Milliseconds 500
$resp = $port.ReadExisting()
$port.Close()
Write-Host "REPONSE STM32:" $resp
