param([string]$ReportDirectory = "D:\tmp\elite_engine_out")
$ErrorActionPreference = "Stop"
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$testBuild = Join-Path $repoRoot "build\tests\navigation_runtime"
$gameBuild = Join-Path $repoRoot "build"
New-Item -ItemType Directory -Force -Path $ReportDirectory | Out-Null
$transcript = Join-Path $ReportDirectory "assisted_engine_out_gate.log"
Start-Transcript -Path $transcript -Force | Out-Null
try {
    Write-Host "[EngineOut] Configure navigation runtime tests"
    & cmake -S (Join-Path $repoRoot "tests\navigation_runtime") -B $testBuild -G Ninja
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed: $LASTEXITCODE" }
    $targets = @("working_thrust_axis_analysis_tests", "engine_thrust_hud_presentation_tests", "mother_computer_tests", "ship_mobility_assessment_tests", "assisted_authority_contract_tests", "ship_physical_control_models_tests", "ship_control_execution_gateway_tests", "client_route_autopilot_tests")
    & cmake --build $testBuild --target $targets -j 8
    if ($LASTEXITCODE -ne 0) { throw "Navigation test build failed: $LASTEXITCODE" }
    & ctest --test-dir $testBuild -R "^(working_thrust_axis_analysis|engine_thrust_hud_presentation|mother_computer|ship_mobility_assessment|assisted_authority_contract|ship_physical_control_models|ship_control_execution_gateway|client_route_autopilot)$" --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "Navigation tests failed: $LASTEXITCODE" }
    Write-Host "[EngineOut] Building EliteGame (HUD, wire schema and control)"
    & cmake --build $gameBuild --target EliteGame -j 8
    if ($LASTEXITCODE -ne 0) { throw "EliteGame build failed: $LASTEXITCODE" }
    Write-Host "[EngineOut] BUILD AND HEADLESS TESTS PASS."
    Write-Host "[EngineOut] Real-game acceptance still required: healthy, aft-50, aft-failed."
    Write-Host "[EngineOut] Use ELITE_SHIP_DAMAGE_PRESET unset, aft-50, or aft-failed per flight."
}
finally { Stop-Transcript | Out-Null }
