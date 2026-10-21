param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$PerFlightTimeoutSeconds = 240,
    [string]$ReportDirectory = "D:\\tmp\\elite_engine_out"
)
$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"
$exe = Join-Path $repoRoot "build\\EliteGame.exe"
if (-not (Test-Path $runner)) { throw "Missing navigation runner: $runner" }
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null

# The fixtures exercise the actual game diagnostic scenario, not synthetic test
# slots. Reactor/cooling idealization is a future fixture, NOT implied here.
$models = @("Assisted-I", "Assisted-Y", "Assisted-X")
$presets = @("aft-50", "aft-failed")
$old = @{}
$variables = @(
    "ELITE_SHIP_PHYSICAL_TEST_MODEL",
    "ELITE_SHIP_PHYSICAL_TEST_INDEX",
    "ELITE_SHIP_PHYSICAL_TEST_TOTAL",
    "ELITE_SHIP_DAMAGE_PRESET",
    "ELITE_NAV_STRESS_SIMPLE",
    "ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED",
    "ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY",
    "ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE",
    "ELITE_PHYSICAL_CORRIDOR_HOT_START",
    "ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS",
    "ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG"
)
foreach ($name in $variables) { $old[$name] = [Environment]::GetEnvironmentVariable($name, "Process") }

$results = @()
try {
    $env:ELITE_NAV_STRESS_SIMPLE = "1"
    $env:ELITE_NAV_STRESS_ASSISTED_HIGH_SPEED = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_GAMEPLAY_PARITY = "0"
    $env:ELITE_NAV_STRESS_ASSISTED_SAFE_COURSE_OBSTACLE = "0"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START = "1"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_SPEED_MPS = "300"
    $env:ELITE_PHYSICAL_CORRIDOR_HOT_START_NOSE_OFFSET_DEG = "0"
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = "6"
    $index = 0
    foreach ($model in $models) {
        foreach ($preset in $presets) {
            ++$index
            $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model
            $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = "$index"
            $env:ELITE_SHIP_DAMAGE_PRESET = $preset
            $filename = ("{0:D2}_{1}_{2}.log" -f $index, ($model -replace "-", "_"), ($preset -replace "-", "_"))
            $report = Join-Path $ReportDirectory $filename
            Write-Host "[AssistedDamage] $index/6 model=$model damage=$preset report=$report"
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $PerFlightTimeoutSeconds -ReportPath $report
            $code = $LASTEXITCODE
            $results += [PSCustomObject]@{ Index=$index; Model=$model; Damage=$preset; ExitCode=$code; Report=$report }
            if ($code -ne 0) { Write-Warning "$model $preset returned $code" }
        }
    }
}
finally {
    foreach ($name in $variables) {
        [Environment]::SetEnvironmentVariable($name, $old[$name], "Process")
    }
    if ($results.Count -gt 0) {
        $results | Export-Csv -Path (Join-Path $ReportDirectory "assisted_damage_flights_summary.csv") -NoTypeInformation -Encoding UTF8
    }
}
$failed = @($results | Where-Object { $_.ExitCode -ne 0 })
Write-Host "[AssistedDamage] completed=$($results.Count) failed=$($failed.Count) reports=$ReportDirectory"
if ($failed.Count -gt 0) { throw "$($failed.Count) damage-flight scenario(s) failed; review logs" }
