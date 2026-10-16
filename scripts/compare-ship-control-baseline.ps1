param(
    [Parameter(Mandatory = $true)]
    [string]$BaselineCsv,

    [Parameter(Mandatory = $true)]
    [string]$CandidateCsv,

    [double]$Tolerance = 1.0e-6
)

$ErrorActionPreference = "Stop"

function Load-HealthyRows([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { throw "CSV not found: $Path" }
    $rows = Import-Csv -LiteralPath $Path | Where-Object { $_.damage_case -eq "D0_healthy" }
    if (-not $rows) { throw "No D0_healthy rows found in: $Path" }
    return @($rows)
}

function Row-Key($Row) {
    return "{0}|{1}|{2}|{3}" -f $Row.model, $Row.family, $Row.scenario, $Row.time_s
}

$baselineRows = Load-HealthyRows $BaselineCsv
$candidateRows = Load-HealthyRows $CandidateCsv
$baseline = @{}
foreach ($row in $baselineRows) { $baseline[(Row-Key $row)] = $row }
$candidate = @{}
foreach ($row in $candidateRows) { $candidate[(Row-Key $row)] = $row }

$missing = @($baseline.Keys | Where-Object { -not $candidate.ContainsKey($_) })
$extra = @($candidate.Keys | Where-Object { -not $baseline.ContainsKey($_) })
if ($missing.Count -gt 0 -or $extra.Count -gt 0) {
    Write-Host "SHIP CONTROL BASELINE: FAIL"
    foreach ($key in $missing) { Write-Host " missing: $key" }
    foreach ($key in $extra) { Write-Host " extra:   $key" }
    exit 1
}

$ignoredColumns = @("slot_state")
$columns = @($baselineRows[0].PSObject.Properties.Name)
$failures = New-Object System.Collections.Generic.List[string]
$maxNumericDelta = 0.0
$comparedNumeric = 0

foreach ($key in ($baseline.Keys | Sort-Object)) {
    $a = $baseline[$key]
    $b = $candidate[$key]
    foreach ($column in $columns) {
        if ($ignoredColumns -contains $column) { continue }
        $av = [string]$a.$column
        $bv = [string]$b.$column
        $an = 0.0
        $bn = 0.0
        $aNumeric = [double]::TryParse($av, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$an)
        $bNumeric = [double]::TryParse($bv, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$bn)
        if ($aNumeric -and $bNumeric) {
            $delta = [math]::Abs($an - $bn)
            if ($delta -gt $maxNumericDelta) { $maxNumericDelta = $delta }
            $comparedNumeric++
            if ($delta -gt $Tolerance) {
                $failures.Add("$key column=$column baseline=$av candidate=$bv delta=$delta")
            }
        } elseif ($av -ne $bv) {
            $failures.Add("$key column=$column baseline='$av' candidate='$bv'")
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host "SHIP CONTROL BASELINE: FAIL"
    Write-Host " tolerance=$Tolerance"
    Write-Host " differences=$($failures.Count)"
    foreach ($failure in $failures | Select-Object -First 80) { Write-Host " $failure" }
    if ($failures.Count -gt 80) { Write-Host " ... $($failures.Count - 80) more differences" }
    exit 1
}

Write-Host "SHIP CONTROL BASELINE: PASS"
Write-Host " healthy_rows=$($baseline.Count)"
Write-Host " numeric_values_compared=$comparedNumeric"
Write-Host " max_numeric_delta=$maxNumericDelta"
Write-Host " tolerance=$Tolerance"
