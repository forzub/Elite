param(
    [double]$StartSpeedMps = 400.0,
    [double]$MinimumRequestSpeedMps = 390.0,
    [double]$MinimumRouteReadySpeedMps = 350.0,
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$AbsoluteTimeoutSeconds = 90,
    [string]$ReportPath = "D:\\tmp\\elite\\assisted_high_speed\\assisted_i_400.log"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"

if (-not (Test-Path $runner)) {
    throw "Navigation stress runner not found: $runner"
}

Write-Host "[AssistedHighSpeed] building EliteGame"
& cmake --build $buildDir --target EliteGame -j 8
if ($LASTEXITCODE -ne 0) {
    throw "EliteGame build failed (exit code $LASTEXITCODE)"
}

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}

$oldModel = $env:ELITE_SHIP_PHYSICAL_TEST_MODEL
$oldIndex = $env:ELITE_SHIP_PHYSICAL_TEST_INDEX
$oldTotal = $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL
$oldSimple = $env:ELITE_NAV_STRESS_SIMPLE
$oldHotStart = $env:ELITE_PHYSICAL_CORRIDOR_HOT_START
$oldHotStartSpeed = $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS
$oldHotStartNoseOffset = $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG
$oldHighSpeed = $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED
$oldMinRequest = $env:ELITE_ASSISTED_HIGH_SPEED_MIN_REQUEST_MPS
$oldMinReady = $env:ELITE_ASSISTED_HIGH_SPEED_MIN_ROUTE_READY_MPS

$runExitCode = 0

try {
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = "Assisted-I"
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = "1"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = "1"
    $env:ELITE_NAV_STRESS_SIMPLE = "1"

    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = "1"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0}", $StartSpeedMps)
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG = "0"

    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = "1"
    $env:ELITE_ASSISTED_HIGH_SPEED_MIN_REQUEST_MPS = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0}", $MinimumRequestSpeedMps)
    $env:ELITE_ASSISTED_HIGH_SPEED_MIN_ROUTE_READY_MPS = [string]::Format([System.Globalization.CultureInfo]::InvariantCulture, "{0}", $MinimumRouteReadySpeedMps)

    Write-Host ""
    Write-Host "============================================================"
    Write-Host "[AssistedHighSpeed] START Assisted-I"
    Write-Host "[AssistedHighSpeed] injected speed: $StartSpeedMps m/s"
    Write-Host "[AssistedHighSpeed] nose/course offset: 0 deg (isolate speed/planner timing)"
    Write-Host "[AssistedHighSpeed] request threshold: $MinimumRequestSpeedMps m/s"
    Write-Host "[AssistedHighSpeed] route-ready threshold: $MinimumRouteReadySpeedMps m/s"
    Write-Host "[AssistedHighSpeed] report: $ReportPath"
    Write-Host "============================================================"

    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $AbsoluteTimeoutSeconds -ReportPath $ReportPath
    $runExitCode = $LASTEXITCODE
}
finally {
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $oldModel
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = $oldIndex
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = $oldTotal
    $env:ELITE_NAV_STRESS_SIMPLE = $oldSimple
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = $oldHotStart
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS = $oldHotStartSpeed
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG = $oldHotStartNoseOffset
    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = $oldHighSpeed
    $env:ELITE_ASSISTED_HIGH_SPEED_MIN_REQUEST_MPS = $oldMinRequest
    $env:ELITE_ASSISTED_HIGH_SPEED_MIN_ROUTE_READY_MPS = $oldMinReady
}

if ($runExitCode -ne 0) {
    throw "Assisted high-speed maneuver probe failed (exit code $runExitCode)"
}

Write-Host ""
Write-Host "[AssistedHighSpeed] PASS"
Write-Host "[AssistedHighSpeed] report: $ReportPath"
