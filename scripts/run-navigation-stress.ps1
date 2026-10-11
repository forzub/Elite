param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$AbsoluteTimeoutSeconds = 120,
    [string]$ReportPath = "D:\tmp\elite_navigation_stress.log"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $PSScriptRoot "..\build\EliteGame.exe"
$exe = [System.IO.Path]::GetFullPath($exe)

if (-not (Test-Path $exe)) {
    throw "EliteGame.exe not found: $exe"
}

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}

if (Test-Path $ReportPath) {
    Remove-Item -Force $ReportPath
}

$oldStress = $env:ELITE_NAV_STRESS
$oldReport = $env:ELITE_NAV_STRESS_REPORT

$env:ELITE_NAV_STRESS = "1"
$env:ELITE_NAV_STRESS_REPORT = $ReportPath

Write-Host "[NavStressRunner] starting $exe"
Write-Host "[NavStressRunner] report $ReportPath"
Write-Host "[NavStressRunner] no-progress timeout $($NoProgressTimeoutSeconds)s; absolute timeout $($AbsoluteTimeoutSeconds)s"

$started = Get-Date
$lastProgressAt = $started
$lastSemanticEvent = ""
$completed = $false
$timedOut = $false
$timeoutReason = ""

try {
    $process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru

    while (-not $process.HasExited) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()

        if (Test-Path $ReportPath) {
            # SAMPLE lines are heartbeats, not proof of planner progress.
            # Watch semantic state transitions so a worker stuck forever in
            # RoutePlanner is detected even while the render/update loop keeps
            # appending telemetry.
            $tail = Get-Content -Path $ReportPath -Tail 96 -ErrorAction SilentlyContinue
            $semanticEvent =
                $tail |
                Where-Object { $_ -match '^(EVENT|SUMMARY|WATCHDOG)' } |
                Select-Object -Last 1

            if ($semanticEvent -and $semanticEvent -ne $lastSemanticEvent) {
                $lastSemanticEvent = $semanticEvent
                $lastProgressAt = Get-Date
            }

            if ($tail -match "status=COMPLETE") {
                $completed = $true
                break
            }
        }

        $now = Get-Date
        if (($now - $lastProgressAt).TotalSeconds -ge $NoProgressTimeoutSeconds) {
            $timedOut = $true
            $timeoutReason = "no semantic navigation progress for $NoProgressTimeoutSeconds seconds"
            break
        }

        if (($now - $started).TotalSeconds -ge $AbsoluteTimeoutSeconds) {
            $timedOut = $true
            $timeoutReason = "absolute timeout of $AbsoluteTimeoutSeconds seconds"
            break
        }
    }

    if ($completed) {
        Write-Host "[NavStressRunner] COMPLETE"
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
        exit 0
    }

    if ($timedOut) {
        $stamp = Get-Date -Format "o"
        $line = 'WATCHDOG timestamp=' + $stamp + ' status=TIMEOUT reason="' + $timeoutReason + '"'
        Add-Content -Path $ReportPath -Value $line
        Write-Host "[NavStressRunner] TIMEOUT: $timeoutReason"

        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
        exit 2
    }

    Write-Host "[NavStressRunner] game exited before COMPLETE, code=$($process.ExitCode)"
    Add-Content -Path $ReportPath -Value "WATCHDOG status=GAME_EXIT exit_code=$($process.ExitCode)"
    exit 3
}
finally {
    $env:ELITE_NAV_STRESS = $oldStress
    $env:ELITE_NAV_STRESS_REPORT = $oldReport
}
