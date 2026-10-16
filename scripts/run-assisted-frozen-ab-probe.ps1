param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$AbsoluteTimeoutSeconds = 240
)

$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"

Write-Host "[AssistedFrozenAB] building EliteGame"
& cmake --build $buildDir --target EliteGame -j 8
if ($LASTEXITCODE -ne 0) { throw "EliteGame build failed (exit code $LASTEXITCODE)" }

$saved = @{}
$names = @(
  "ELITE_NAV_STRESS_SIMPLE",
  "ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED",
  "ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY",
  "ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE",
  "ELITE_SHIP_PHYSICAL_TEST_MODEL",
  "ELITE_SHIP_PHYSICAL_TEST_INDEX",
  "ELITE_SHIP_PHYSICAL_TEST_TOTAL",
  "ELITE_PHYSICAL_CORRIDOR_HOT_START",
  "ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS",
  "ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG"
)
foreach ($name in $names) { $saved[$name] = [Environment]::GetEnvironmentVariable($name, "Process") }

try {
    $env:ELITE_NAV_STRESS_SIMPLE = "1"
    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE = "0"
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = "Assisted-I"
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = "1"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = "1"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = "1"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS = "300"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG = "60"

    $report = "D:\tmp\assisted_frozen_ab_300mps_60deg.log"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $AbsoluteTimeoutSeconds -ReportPath $report
    exit $LASTEXITCODE
}
finally {
    foreach ($name in $names) {
        [Environment]::SetEnvironmentVariable($name, $saved[$name], "Process")
    }
}
