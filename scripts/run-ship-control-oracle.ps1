param(
    [string]$GameExe = "D:\\__elite\\work\\build\\EliteGame.exe",
    [string]$ReportPath = "D:\\tmp\\elite\\ship_control_oracle_matrix.log"
)

$ErrorActionPreference = "Stop"

if (Test-Path -LiteralPath $ReportPath) {
    Remove-Item -Force -LiteralPath $ReportPath
}

$env:ELITE_SHIP_CONTROL_ORACLE_REPORT = $ReportPath

Write-Host "SHIP CONTROL ORACLE SCENARIO"
Write-Host " report=$ReportPath"
Write-Host ""
Write-Host "Run manually:"
Write-Host "  $GameExe --diagnostic-scenario 'ship-control'"
Write-Host ""
Write-Host "No marker file is created. Scenario selection is explicit."
