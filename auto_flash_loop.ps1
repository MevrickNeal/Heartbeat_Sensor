Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " PROJECT NEAL - RAPID RESET FLASH LOOP " -ForegroundColor Yellow
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Tap your red Reset button rapidly now (press and release)." -ForegroundColor White
Write-Host "The loop will catch your reset automatically!" -ForegroundColor Green
Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray

$success = $false
for ($i = 1; $i -le 20; $i++) {
    Write-Host "`n>>> [Attempt $i/20] Listening for Reset..." -ForegroundColor Cyan
    $output = & ".\tools\arduino-cli.exe" upload -p COM5 --fqbn arduino:avr:uno "$PSScriptRoot" 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n==========================================================" -ForegroundColor Green
        Write-Host " SUCCESS: FIRMWARE FLASHED 100%! " -ForegroundColor Green
        Write-Host "==========================================================" -ForegroundColor Green
        $success = $true
        break
    } else {
        Write-Host "    (Sync waiting... keep tapping reset every 1-2 seconds)" -ForegroundColor Gray
    }
    Start-Sleep -Milliseconds 200
}

if (-not $success) {
    Write-Host "`nTimed out after 20 attempts." -ForegroundColor Red
}
