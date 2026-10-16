param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$PerLayoutTimeoutSeconds = 240
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"

if (-not (Test-Path $runner)) {
    throw "Navigation stress runner not found: $runner"
}

Write-Host "[AssistedLayoutGame] building EliteGame"
& cmake --build $buildDir --target EliteGame -j 8
if ($LASTEXITCODE -ne 0) {
    throw "EliteGame build failed (exit code $LASTEXITCODE)"
}

$layouts = @(
    "Assisted-I",
    "Assisted-Y",
    "Assisted-X"
)

$oldModel = $env:ELITE_SHIP_PHYSICAL_TEST_MODEL
$oldSimple = $env:ELITE_NAV_STRESS_SIMPLE
$oldIndex = $env:ELITE_SHIP_PHYSICAL_TEST_INDEX
$oldTotal = $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL
$oldHotStart = $env:ELITE_PHYSICAL_CORRIDOR_HOT_START
$oldHighSpeed = $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED
$oldGameplayParity = $env:ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY
$oldSafeCourseObstacle = $env:ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE

$results = @()

try {
    # This runner owns the scenario completely. Never inherit another stress
    # probe from the caller shell, otherwise the matrix silently stops being a
    # comparable I/Y/X corridor test.
    $env:ELITE_NAV_STRESS_SIMPLE = "1"
    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE = "0"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = [string]$layouts.Count
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = "1"

    for ($i = 0; $i -lt $layouts.Count; ++$i) {
        $model = $layouts[$i]
        $index = $i + 1

        $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model
        $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = [string]$index

        $safeModel = $model -replace '[^A-Za-z0-9]+', '_'
        $report = "D:\tmp\assisted_layout_game_{0:D2}_{1}.log" -f $index, $safeModel

        Write-Host ""
        Write-Host "============================================================"
        Write-Host "[AssistedLayoutGame] GAME RUN $index/$($layouts.Count): $model"
        Write-Host "[AssistedLayoutGame] report: $report"
        Write-Host "============================================================"

        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $PerLayoutTimeoutSeconds -ReportPath $report

        $exitCode = $LASTEXITCODE
        $results += [PSCustomObject]@{
            Index = $index
            Model = $model
            ExitCode = $exitCode
            Report = $report
        }

        if ($exitCode -ne 0) {
            throw "Assisted in-game layout run failed: $model (exit code $exitCode)"
        }
    }

    $summaryPath = "D:\tmp\assisted_layout_game_summary.txt"
    $summary = @(
        "ASSISTED IN-GAME LAYOUT MATRIX: PASS"
        "build_dir=$buildDir"
    )
    foreach ($result in $results) {
        $summary += "PASS $($result.Index)/3 $($result.Model) report=$($result.Report)"
    }
    Set-Content -Path $summaryPath -Value $summary -Encoding UTF8

    Write-Host ""
    Write-Host "================ ASSISTED GAME MATRIX COMPLETE ================"
    foreach ($result in $results) {
        Write-Host ("[AssistedLayoutGame] PASS {0}/3 {1} -> {2}" -f $result.Index, $result.Model, $result.Report)
    }
    Write-Host "[AssistedLayoutGame] summary: $summaryPath"
    Write-Host "================================================================"
}
finally {
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $oldModel
    $env:ELITE_NAV_STRESS_SIMPLE = $oldSimple
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = $oldIndex
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = $oldTotal
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = $oldHotStart
    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = $oldHighSpeed
    $env:ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY = $oldGameplayParity
    $env:ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE = $oldSafeCourseObstacle
}
