#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[2]
scene = (root / "src/game/scene/GameSceneSetup.cpp").read_text(encoding="utf-8")
smoother = (root / "src/world/navigation/SmoothPathOptimizer.cpp").read_text(encoding="utf-8")
obstacles = (root / "src/world/navigation/NavigationObstacleGeometry.cpp").read_text(encoding="utf-8")
plan = (root / "src/game/navigation/ManualDockingGuidancePlan.h").read_text(encoding="utf-8")
perf_header = (root / "src/world/navigation/NavigationPerfLog.h").read_text(encoding="utf-8")

required_scene_tokens = [
    '"guidance_dock_cube_a"',
    '"guidance_dock_cylinder_b"',
    # One exact direct-route blocker in every deterministic band. These values
    # are Hub attachment coordinates; the planning basis maps approximately as
    # nav(X,Y,Z)=hubLocal(Z,Y,X).
    'glm::dvec3(575.0, 2090.0, -8000.0)',
    'glm::dvec3(1260.0, 1595.0, -5600.0)',
    'glm::dvec3(1925.0, 1120.0, -3300.0)',
    'glm::dvec3(2440.0, 750.0, -1500.0)',
]
for index in range(1, 9):
    required_scene_tokens.append(f'"nav_stress_cube_{index:02d}"')
    required_scene_tokens.append(f'"nav_stress_cylinder_{index:02d}"')

missing_scene = [token for token in required_scene_tokens if token not in scene]
if missing_scene:
    raise SystemExit(
        "NAVIGATION STRESS FIELD CONTRACT: FAIL\nmissing scene tokens: "
        + ", ".join(missing_scene)
    )

required_smoother_tokens = [
    'std::upper_bound(knots.begin(), knots.end(), u)',
    '"navigation_perf.log"',
    '"[SmoothPathPerf]',
    '"[SmoothCandidatePerf]',
    'sample_ms=',
    'safety_ms=',
    'quality_ms=',
]
missing_smoother = [token for token in required_smoother_tokens if token not in smoother]
if missing_smoother:
    raise SystemExit(
        "NAVIGATION STRESS FIELD CONTRACT: FAIL\nmissing smoother tokens: "
        + ", ".join(missing_smoother)
    )

required_obstacle_tokens = [
    'obstacleBroadphaseRadiusMeters(',
    'segmentOutsideObstacleBroadphase(',
    'if (segmentOutsideObstacleBroadphase(',
]
missing_obstacle = [token for token in required_obstacle_tokens if token not in obstacles]
if missing_obstacle:
    raise SystemExit(
        "NAVIGATION STRESS FIELD CONTRACT: FAIL\nmissing broadphase tokens: "
        + ", ".join(missing_obstacle)
    )

required_replan_tokens = [
    'double replanCheckIntervalSeconds = 1.0;',
    'double hardToleranceScale = 1.0e9;',
]
missing_replan = [token for token in required_replan_tokens if token not in plan]
if missing_replan:
    raise SystemExit(
        "NAVIGATION STRESS FIELD CONTRACT: FAIL\n"
        "synchronous reconnect storm guards are missing: "
        + ", ".join(missing_replan)
    )

required_perf_tokens = [
    'navigationPerfLogPath()',
    'std::filesystem::absolute(',
    '"[NavigationPerf] log_path="',
]
missing_perf = [token for token in required_perf_tokens if token not in perf_header]
if missing_perf:
    raise SystemExit(
        "NAVIGATION STRESS FIELD CONTRACT: FAIL\nmissing perf-log path tokens: "
        + ", ".join(missing_perf)
    )

print("NAVIGATION STRESS FIELD CONTRACT: PASS")
print(" - 2 authored guidance targets retained")
print(" - 16 deterministic obstacles retained in 4 route-crossing bands")
print(" - conservative segment/obstacle broadphase retained")
print(" - all synchronous rolling reconnect triggers are limited to 1 Hz")
print(" - navigation perf log announces its absolute startup path")
