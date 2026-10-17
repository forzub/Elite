param(
    [string]$MarkerPath = "D:\\tmp\\elite\\ship_damage_oracle.enable",
    [string]$ReportPath = "D:\\tmp\\elite\\ship_damage_oracle.log"
)

$ErrorActionPreference = "Stop"

$healthyMarker = "D:\tmp\elite\ship_control_oracle.enable"
if (Test-Path -LiteralPath $healthyMarker) {
    Remove-Item -Force -LiteralPath $healthyMarker
}

$markerDir = Split-Path -Parent $MarkerPath
if ($markerDir -and -not (Test-Path -LiteralPath $markerDir)) {
    New-Item -ItemType Directory -Force -Path $markerDir | Out-Null
}

if (Test-Path -LiteralPath $ReportPath) {
    Remove-Item -Force -LiteralPath $ReportPath
}

Set-Content -LiteralPath $MarkerPath -Value "armed" -Encoding ascii

Write-Host "SHIP DAMAGE ORACLE: ARMED"
Write-Host " marker=$MarkerPath"
Write-Host " report=$ReportPath"
Write-Host " Start EliteGame.exe manually as usual."
Write-Host " The game will test Assisted-Y then Newtonian-Y."
Write-Host " Slot 0 main is failed authoritatively on the server."
Write-Host " The probe is condition-driven; elapsed time does not create physics FAIL."
