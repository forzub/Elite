param(
    [string]$GameExe = "D:\\__elite\\work\\build\\EliteGame.exe",
    [string]$Scenario = "ship-damage:Assisted-I@0,Assisted-Y@0,Assisted-X@0,Newtonian-I@0,Newtonian-Y@0,Newtonian-X@0"
)

$ErrorActionPreference = "Stop"

Write-Host "SHIP DAMAGE ORACLE SCENARIO"
Write-Host " scenario=$Scenario"
Write-Host ""
Write-Host "Run manually:"
Write-Host "  $GameExe --diagnostic-scenario '$Scenario'"
Write-Host ""
Write-Host "No marker file is created. The scenario is an explicit EliteGame argument."
