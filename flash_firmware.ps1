param(
    [string]$Port = "COM5"
)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " PROJECT NEAL DEFENCE WING - FIRMWARE UPLOADER " -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Trigger the dedicated "UPLOADING FIRMWARE" screen on the TFT
Write-Host "[1/3] Triggering 'UPLOADING FIRMWARE' screen on LCD..." -ForegroundColor White
try {
    $sp = New-Object System.IO.Ports.SerialPort $Port, 256000, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One
    $sp.ReadTimeout = 500
    $sp.WriteTimeout = 500
    $sp.Open()
    $sp.WriteLine("FLASH")
    Start-Sleep -Milliseconds 400
    $sp.Close()
    Write-Host "      Screen command sent successfully! LCD will show Upload Screen." -ForegroundColor Green
} catch {
    Write-Host "      Note: Port busy or sensor transmitting (standard). Proceeding to flash." -ForegroundColor Gray
}

# 2. Flash advice regarding Pin 0 RX sharing
Write-Host "[2/3] Hardware Note:" -ForegroundColor Yellow
Write-Host "      If the radar module is powered, temporarily unplug its 5V wire" -ForegroundColor Yellow
Write-Host "      for 5 seconds so avrdude gets clean access to Pin 0 (RX)." -ForegroundColor Yellow
Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray

# 3. Compile & Upload via arduino-cli
Write-Host "[3/3] Compiling and Uploading via arduino-cli..." -ForegroundColor Cyan
& ".\tools\arduino-cli.exe" compile --fqbn arduino:avr:uno "$PSScriptRoot"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed! Check compiler errors above." -ForegroundColor Red
    exit $LASTEXITCODE
}

& ".\tools\arduino-cli.exe" upload -p $Port --fqbn arduino:avr:uno -v "$PSScriptRoot"
if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "==========================================================" -ForegroundColor Green
    Write-Host " SUCCESS: FIRMWARE FLASHED! RECONNECT SENSOR 5V NOW. " -ForegroundColor Green
    Write-Host "==========================================================" -ForegroundColor Green
} else {
    Write-Host ""
    Write-Host "Upload failed. Please unplug the radar 5V pin and re-run this script." -ForegroundColor Red
}
