param(
    [int]$NoProgressTimeoutSeconds = 12,
    [int]$AbsoluteTimeoutSeconds = 60,
    [string]$OutputDirectory = "D:\tmp\elite\ship_control_oracle"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$exe = [System.IO.Path]::GetFullPath((Join-Path $repoRoot "build\EliteGame.exe"))

if (-not (Test-Path -LiteralPath $exe)) { throw "EliteGame.exe not found: $exe" }

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$models = @("Assisted-I","Assisted-Y","Assisted-X","Newtonian-I","Newtonian-Y","Newtonian-X")

$oldOracle = $env:ELITE_SHIP_CONTROL_ORACLE
$oldReport = $env:ELITE_SHIP_CONTROL_ORACLE_REPORT
$oldModel = $env:ELITE_SHIP_PHYSICAL_TEST_MODEL

$failedModels = New-Object System.Collections.Generic.List[string]

try {
    foreach ($model in $models) {
        $report = Join-Path $OutputDirectory ($model + ".log")
        if (Test-Path -LiteralPath $report) { Remove-Item -Force -LiteralPath $report }

        $env:ELITE_SHIP_CONTROL_ORACLE = "1"
        $env:ELITE_SHIP_CONTROL_ORACLE_REPORT = $report
        $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $model

        Write-Host ""
        Write-Host "=== SHIP CONTROL ORACLE: $model ==="
        Write-Host "report=$report"

        $process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru
        $started = Get-Date
        $lastProgress = $started
        $lastLength = -1L
        $status = $null

        try {
            while (-not $process.HasExited) {
                Start-Sleep -Milliseconds 250
                $process.Refresh()

                if (Test-Path -LiteralPath $report) {
                    $info = Get-Item -LiteralPath $report
                    if ($info.Length -ne $lastLength) {
                        $lastLength = $info.Length
                        $lastProgress = Get-Date
                    }

                    $tail = Get-Content -LiteralPath $report -Tail 16
                    if ($tail -match "SUMMARY .* status=FAILED") { $status = "FAILED"; break }
                    if ($tail -match "SUMMARY .* status=COMPLETE") { $status = "COMPLETE"; break }
                }

                $now = Get-Date
                if (($now - $lastProgress).TotalSeconds -ge $NoProgressTimeoutSeconds) {
                    $status = "TIMEOUT_NO_PROGRESS"
                    break
                }
                if (($now - $started).TotalSeconds -ge $AbsoluteTimeoutSeconds) {
                    $status = "TIMEOUT_ABSOLUTE"
                    break
                }
            }

            if (-not $status -and $process.HasExited) {
                $status = "GAME_EXIT_$($process.ExitCode)"
            }
        }
        finally {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force
                $process.WaitForExit()
            }
        }

        if (Test-Path -LiteralPath $report) {
            Get-Content -LiteralPath $report |
                Where-Object { $_ -match "^(COMMAND|EXPECTED|ACTUAL|RESULT|SUMMARY)" } |
                ForEach-Object { Write-Host $_ }
        }

        if ($status -ne "COMPLETE") { $failedModels.Add("$model`:$status") }
        Write-Host "=== $model -> $status ==="
    }
}
finally {
    $env:ELITE_SHIP_CONTROL_ORACLE = $oldOracle
    $env:ELITE_SHIP_CONTROL_ORACLE_REPORT = $oldReport
    $env:ELITE_SHIP_PHYSICAL_TEST_MODEL = $oldModel
}

if ($failedModels.Count -gt 0) {
    Write-Host ""
    Write-Host "SHIP CONTROL ORACLE MATRIX: FAIL"
    foreach ($failure in $failedModels) { Write-Host " - $failure" }
    exit 1
}

Write-Host ""
Write-Host "SHIP CONTROL ORACLE MATRIX: PASS"
Write-Host " - all six physical constructions ran inside EliteGame.exe"
Write-Host " - each run used the ordinary pilot ShipControlState path"
Write-Host " - reports: $OutputDirectory"
