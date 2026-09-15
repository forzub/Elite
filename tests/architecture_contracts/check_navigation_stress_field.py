#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[2]
scene = (root / "src/game/scene/GameSceneSetup.cpp").read_text(encoding="utf-8")
smoother = (root / "src/world/navigation/SmoothPathOptimizer.cpp").read_text(encoding="utf-8")

required_scene_tokens = [
    '"guidance_dock_cube_a"',
    '"guidance_dock_cylinder_b"',
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

print("NAVIGATION STRESS FIELD CONTRACT: PASS")
print(" - 2 authored guidance targets retained")
print(" - 16 deterministic stress obstacles retained (8 cubes + 8 cylinders)")
print(" - binary-search spline span lookup retained")
print(" - detailed navigation_perf.log phase profiling retained")
