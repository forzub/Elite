param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$PerFlightTimeoutSeconds = 240,
    [string]$ReportDirectory = "D:\\tmp\\elite_engine_out",
    [switch]$AssistedIOnly,
    [ValidateSet("aft-50", "aft-failed")]
    [string]$OnlyPreset
)
$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$runner = Join-Path $PSScriptRoot "run-navigation-stress.ps1"
$exe = Join-Path $repoRoot "build\\EliteGame.exe"
if (-not (Test-Path $runner)) { throw "Missing navigation runner: $runner" }
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null

# The fixtures exercise the actual game diagnostic scenario, not synthetic test
# slots. Reactor/cooling idealization is a future fixture, NOT implied here.
$models = if ($AssistedIOnly) { @("Assisted-I") } else { @("Assisted-I", "Assisted-Y", "Assisted-X") }
# Focus a known failure without needlessly repeating the accepted 50% run.
# Omitted OnlyPreset preserves the original full matrix behavior.
$presets = if ($OnlyPreset) { @($OnlyPreset) } else { @("aft-50", "aft-failed") }
$total = $models.Count * $presets.Count
$receiptLog = "D:\tmp\elite_engine_damage_preset.log"
$attitudeTrace = "D:\tmp\elite_attitude_jitter.csv"
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
    $env:ELITE_SHIP_PHYSICAL_TEST_TOTAL = "$total"
    $index = 0
    foreach ($model in $models) {
        foreach ($preset in $presets) {
            ++$index
            $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model
            $env:ELITE_SHIP_PHYSICAL_TEST_INDEX = "$index"
            $env:ELITE_SHIP_DAMAGE_PRESET = $preset
            $filename = ("{0:D2}_{1}_{2}.log" -f $index, ($model -replace "-", "_"), ($preset -replace "-", "_"))
            $report = Join-Path $ReportDirectory $filename
            # The startup preset log is append-only; take only records from
            # this run so stale receipts cannot masquerade as successful damage.
            $receiptsBefore = if (Test-Path -LiteralPath $receiptLog) {
                @(Get-Content -LiteralPath $receiptLog).Count
            } else { 0 }
            $startedAt = (Get-Date).ToUniversalTime()
            Write-Host "[AssistedDamage] $index/$total model=$model damage=$preset report=$report"
            & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $runner -NoProgressTimeoutSeconds $NoProgressTimeoutSeconds -AbsoluteTimeoutSeconds $PerFlightTimeoutSeconds -ReportPath $report
            $code = $LASTEXITCODE

            $newReceipts = @()
            if (Test-Path -LiteralPath $receiptLog) {
                $newReceipts = @(Get-Content -LiteralPath $receiptLog |
                    Select-Object -Skip $receiptsBefore)
            }
            $receiptPath = Join-Path $ReportDirectory (
                "{0:D2}_{1}_{2}_receipt.log" -f $index, ($model -replace "-", "_"), ($preset -replace "-", "_"))
            if ($newReceipts.Count -gt 0) {
                Set-Content -LiteralPath $receiptPath -Value $newReceipts -Encoding UTF8
            }
            $damageApplied = @($newReceipts | Where-Object {
                $_.Contains("preset=$preset status=APPLIED") -and
                $_.Contains("module=ship_engine_L")
            }).Count -gt 0
            if (-not $damageApplied) {
                Write-Warning "$model ${preset}: no fresh APPLIED receipt for the installed aft-left CR-1"
                if ($code -eq 0) { $code = 5 }
            }

            $tracePath = Join-Path $ReportDirectory (
                "{0:D2}_{1}_{2}_attitude.csv" -f $index, ($model -replace "-", "_"), ($preset -replace "-", "_"))
            $traceInfo = Get-Item -LiteralPath $attitudeTrace -ErrorAction SilentlyContinue
            $hasFreshTrace = $null -ne $traceInfo -and
                $traceInfo.LastWriteTimeUtc -ge $startedAt.AddSeconds(-2)
            if ($hasFreshTrace) {
                Copy-Item -LiteralPath $attitudeTrace -Destination $tracePath -Force
            } else {
                Write-Warning "$model ${preset}: no fresh attitude trace at $attitudeTrace"
            }
            # Results are only accepted with both the authoritative damage
            # receipt and a new flight trace; no stale file passes the batch.
            if (-not $hasFreshTrace -and $code -eq 0) { $code = 6 }
            $receiptResult = if ($newReceipts.Count -gt 0) { $receiptPath } else { "" }
            $traceResult = if ($hasFreshTrace) { $tracePath } else { "" }
            $results += [PSCustomObject]@{
                Index=$index; Model=$model; Damage=$preset
                ExitCode=$code; DamageApplied=$damageApplied
                Report=$report; Receipt=$receiptResult
                AttitudeTrace=$traceResult
            }
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
