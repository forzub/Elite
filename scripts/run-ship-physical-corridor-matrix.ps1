param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$PerModelTimeoutSeconds = 240,
    [string]$ReportDirectory = "D:\\tmp\\elite_ship_physical_matrix"
)

$ErrorActionPreference = "Stop"

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

$results = @()

try {
    $env:ELITE_NAV_STRESS_SIMPLE = "1"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = [string]$models.Count

    for ($i = 0; $i -lt $models.Count; ++$i) {
        $model = $models[$i]
        $index = $i + 1

        $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model
        $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = [string]$index

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
    Write-Host "=========================================================="
}
finally {
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $oldModel
    $env:ELITE_NAV_STRESS_SIMPLE = $oldSimple
    $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = $oldIndex
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = $oldTotal
}
