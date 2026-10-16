param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$PerModelTimeoutSeconds = 240,
    [string]$ReportDirectory = "D:\\tmp"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildDir = Join-Path $repoRoot "build"
$runtimeTestBuildDir = Join-Path $buildDir "tests\navigation_runtime"

Write-Host "[PhysicalMatrix] configuring navigation runtime tests"
& cmake -S (Join-Path $repoRoot "tests\navigation_runtime") `
    -B $runtimeTestBuildDir -G Ninja
if ($LASTEXITCODE -ne 0) {
    throw "Navigation runtime configure failed (exit code $LASTEXITCODE)"
}

Write-Host "[PhysicalMatrix] building headless corridor gate"
& cmake --build $runtimeTestBuildDir `
    --target ship_physical_corridor_matrix_tests -j 8
if ($LASTEXITCODE -ne 0) {
    throw "Physical corridor test build failed (exit code $LASTEXITCODE)"
}

Write-Host "[PhysicalMatrix] building EliteGame"
& cmake --build $buildDir --target EliteGame -j 8
if ($LASTEXITCODE -ne 0) {
    throw "EliteGame build failed (exit code $LASTEXITCODE)"
}

Write-Host "[PhysicalMatrix] running six-model headless physical gate"
& ctest --test-dir $runtimeTestBuildDir `
    -R "^ship_physical_corridor_matrix$" `
    --output-on-failure
if ($LASTEXITCODE -ne 0) {
    throw "Headless physical corridor matrix failed (exit code $LASTEXITCODE)"
}

$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"
if (-not (Test-Path $runner)) {
    throw "Navigation stress runner not found: $runner"
}

if (-not (Test-Path $ReportDirectory)) {
    New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
}

$models = @(
    "Assisted-I",
    "Assisted-Y",
    "Assisted-X",
    "Newtonian-I",
    "Newtonian-Y",
    "Newtonian-X"
)

$oldModel = $env:ELITE_SHIP_PHYSICAL_TEST_MODEL
$oldSimple = $env:ELITE_NAV_STRESS_SIMPLE
$oldIndex = $env:ELITE_SHIP_PHYSICAL_TEST_INDEX
$oldTotal = $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL
$oldHotStart = $env:ELITE_PHYSICAL_CORRIDOR_HOT_START

$results = @()

try {
    $env:ELITE_NAV_STRESS_SIMPLE = "1"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = [string]$models.Count

    for ($i = 0; $i -lt $models.Count; ++$i) {
        $model = $models[$i]
        $index = $i + 1

        $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model
        $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = [string]$index
        $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = if ($model -like "Assisted-*") { "1" } else { "0" }

        $safeModel = $model -replace '[^A-Za-z0-9]+', '_'
        $report = Join-Path $ReportDirectory ("{0:D2}_{1}.log" -f $index, $safeModel)

        Write-Host ""
        Write-Host "============================================================"
        Write-Host "[PhysicalMatrix] RUN $index/$($models.Count): $model"
        Write-Host "[PhysicalMatrix] report: $report"
        Write-Host "============================================================"

        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $PerModelTimeoutSeconds -ReportPath $report

        $exitCode = $LASTEXITCODE
        $results += [PSCustomObject]@{
            Index = $index
            Model = $model
            ExitCode = $exitCode
            Report = $report
        }

        if ($exitCode -ne 0) {
            throw "Physical corridor run failed: $model (exit code $exitCode)"
        }
    }

    Write-Host ""
    Write-Host "================ PHYSICAL MATRIX COMPLETE ================"
    foreach ($result in $results) {
        Write-Host ("[PhysicalMatrix] PASS {0}/6 {1} -> {2}" -f $result.Index, $result.Model, $result.Report)
    }
    Write-Host "[PhysicalMatrix] headless summary: D:\tmp\ship_physical_corridor_summary.csv"
    Write-Host "=========================================================="
}
finally {
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $oldModel
    $env:ELITE_NAV_STRESS_SIMPLE = $oldSimple
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = $oldIndex
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = $oldTotal
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = $oldHotStart
}
