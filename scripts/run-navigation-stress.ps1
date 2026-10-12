param(
    [int]$NoProgressTimeoutSeconds = 15,
    [int]$AbsoluteTimeoutSeconds = 300,
    [string]$ReportPath = "D:\tmp\elite_navigation_stress.log"
)

$ErrorActionPreference = "Stop"

$exe = Join-Path $PSScriptRoot "..\build\EliteGame.exe"
$exe = [System.IO.Path]::GetFullPath($exe)

if (-not (Test-Path $exe)) {
    throw "EliteGame.exe not found: $exe"
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$gitCommit = "unknown"
$gitBranch = "unknown"
$gitDirty = "unknown"

try {
    $commitOutput = & git -C $repoRoot rev-parse --short=12 HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $commitOutput) {
        $gitCommit = ($commitOutput | Select-Object -First 1).Trim()
    }

    $branchOutput = & git -C $repoRoot branch --show-current 2>$null
    if ($LASTEXITCODE -eq 0 -and $branchOutput) {
        $gitBranch = ($branchOutput | Select-Object -First 1).Trim()
    }

    $dirtyOutput = & git -C $repoRoot status --porcelain --untracked-files=no 2>$null
    if ($LASTEXITCODE -eq 0) {
        $gitDirty = if ($dirtyOutput) { "dirty" } else { "clean" }
    }
}
catch {
    # Stress execution must not depend on git being installed.
}

$exeInfo = Get-Item $exe
$exeHash = (Get-FileHash -Algorithm SHA256 -Path $exe).Hash.ToLowerInvariant()
$exeHashShort = $exeHash.Substring(0, 16)
$exeWriteUtc = $exeInfo.LastWriteTimeUtc.ToString("o")
$buildId = "$gitCommit@$gitBranch/$gitDirty exe=$exeHashShort built=$exeWriteUtc"

$reportDir = Split-Path -Parent $ReportPath
if ($reportDir -and -not (Test-Path $reportDir)) {
    New-Item -ItemType Directory -Force -Path $reportDir | Out-Null
}

if (Test-Path $ReportPath) {
    Remove-Item -Force $ReportPath
}

$oldStress = $env:ELITE_NAV_STRESS
$oldReport = $env:ELITE_NAV_STRESS_REPORT
$oldBuildId = $env:ELITE_NAV_STRESS_BUILD_ID

$env:ELITE_NAV_STRESS = "1"
$env:ELITE_NAV_STRESS_REPORT = $ReportPath
$env:ELITE_NAV_STRESS_BUILD_ID = $buildId

Write-Host "[NavStressRunner] starting $exe"
Write-Host "[NavStressRunner] build $buildId"
Write-Host "[NavStressRunner] report $ReportPath"
Write-Host "[NavStressRunner] no-progress timeout $($NoProgressTimeoutSeconds)s; absolute timeout $($AbsoluteTimeoutSeconds)s"

$started = Get-Date
$lastHeartbeatAt = $started
$lastLength = -1L
$lastWriteUtc = [DateTime]::MinValue
$reportSeen = $false
$completed = $false
$scenarioFailed = $false
$timedOut = $false
$timeoutReason = ""

try {
    $process = Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe) -PassThru

    while (-not $process.HasExited) {
        Start-Sleep -Milliseconds 500
        $process.Refresh()

        if (Test-Path $ReportPath) {
            # SAMPLE output is the game-thread heartbeat. Planner geometry runs
            # asynchronously and may legitimately spend many seconds inside one
            # optimizer stage with no EVENT transition at all. Killing on
            # "semantic" inactivity therefore kills a healthy game while the
            # planner is still working.
            $info = Get-Item $ReportPath -ErrorAction SilentlyContinue
            if ($null -ne $info) {
                $reportSeen = $true
                if ($info.Length -ne $lastLength -or
                    $info.LastWriteTimeUtc -ne $lastWriteUtc) {
                    $lastLength = $info.Length
                    $lastWriteUtc = $info.LastWriteTimeUtc
                    $lastHeartbeatAt = Get-Date
                }
            }

            $tail = Get-Content -Path $ReportPath -Tail 96 -ErrorAction SilentlyContinue
            if ($tail -match "status=FAILED") {
                $scenarioFailed = $true
                break
            }

            if ($tail -match "status=COMPLETE") {
                $completed = $true
                break
            }
        }

        $now = Get-Date

        # No-progress protects against a frozen/crashed game loop only after
        # telemetry has actually started. A slow startup or a long asynchronous
        # optimizer is not a no-progress failure. The absolute timeout below is
        # the guard against a planner worker that never returns.
        if ($reportSeen -and
            ($now - $lastHeartbeatAt).TotalSeconds -ge $NoProgressTimeoutSeconds) {
            $timedOut = $true
            $timeoutReason = "navigation report heartbeat stopped for $NoProgressTimeoutSeconds seconds"
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

    if ($scenarioFailed) {
        Write-Host "[NavStressRunner] FAILED: navigation scenario reported failure"
        if (-not $process.HasExited) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
        exit 4
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
    $env:ELITE_NAV_STRESS_BUILD_ID = $oldBuildId
}
