param(
    [string]$MarkerPath = "D:\tmp\elite\ship_control_oracle.enable",
    [string]$ReportPath = "D:\tmp\elite\ship_control_oracle_matrix.log"
)

$ErrorActionPreference = "Stop"

$markerDir = Split-Path -Parent $MarkerPath
if ($markerDir -and -not (Test-Path -LiteralPath $markerDir)) {
    New-Item -ItemType Directory -Force -Path $markerDir | Out-Null
}

if (Test-Path -LiteralPath $ReportPath) {
    Remove-Item -Force -LiteralPath $ReportPath
}

Set-Content -LiteralPath $MarkerPath -Value "armed" -Encoding ascii

Write-Host "SHIP CONTROL ORACLE: ARMED"
Write-Host " marker=$MarkerPath"
Write-Host " report=$ReportPath"
Write-Host " Start EliteGame.exe manually as usual."
Write-Host " The game will cycle Assisted-I/Y/X and Newtonian-I/Y/X itself."
Write-Host " Commands are condition-driven; elapsed time does not produce physics FAIL."
Write-Host " The marker is removed automatically after all six models complete."
