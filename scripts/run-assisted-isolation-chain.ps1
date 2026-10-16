param(
    [string]$BuildDir = "build/tests/navigation_runtime"
)

$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildPath = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))

$stages = @(
    @{ Name = "A1 ship_state"; Target = "ship_state_tests"; CTest = "^ship_state$" },
    @{ Name = "A2 ship_physical_control_models"; Target = "ship_physical_control_models_tests"; CTest = "^ship_physical_control_models$" },

    @{ Name = "B preplanning_safety_course"; Target = "preplanning_safety_course_tests"; CTest = "^preplanning_safety_course$" },

    @{ Name = "C route_planner_api"; Target = "route_planner_api_tests"; CTest = "^route_planner_api$" },
    @{ Name = "D route_entry_binder"; Target = "route_entry_binder_tests"; CTest = "^route_entry_binder$" },

    @{ Name = "E hull_pose_guidance"; Target = "hull_pose_guidance_tests"; CTest = "^hull_pose_guidance$" },
    @{ Name = "F hull_attitude_control"; Target = "hull_attitude_control_tests"; CTest = "^hull_attitude_control$" },

    @{ Name = "G assisted_safe_course_executor_isolation"; Target = "assisted_safe_course_executor_isolation_tests"; CTest = "^assisted_safe_course_executor_isolation$" },

    @{ Name = "H client_route_autopilot"; Target = "client_route_autopilot_tests"; CTest = "^client_route_autopilot$" }
)

Write-Host "[AssistedIsolation] repo=$repoRoot"
Write-Host "[AssistedIsolation] build=$buildPath"

foreach ($stage in $stages) {
    Write-Host ""
    Write-Host "============================================================"
    Write-Host ("[AssistedIsolation] BUILD {0}" -f $stage.Name)
    Write-Host "============================================================"

    & cmake --build $buildPath --target $stage.Target -j 8
    if ($LASTEXITCODE -ne 0) {
        throw ("BUILD FAILED at {0} (exit {1})" -f $stage.Name, $LASTEXITCODE)
    }

    Write-Host ""
    Write-Host ("[AssistedIsolation] TEST {0}" -f $stage.Name)

    & ctest --test-dir $buildPath -R $stage.CTest --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        Write-Host ""
        Write-Host ("[AssistedIsolation] FIRST RED BLOCK: {0}" -f $stage.Name)
        exit $LASTEXITCODE
    }

    Write-Host ("[AssistedIsolation] PASS {0}" -f $stage.Name)
}

Write-Host ""
Write-Host "[AssistedIsolation] ALL ISOLATED BLOCKS PASS"
exit 0
