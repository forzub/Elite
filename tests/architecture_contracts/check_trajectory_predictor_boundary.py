#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
header = (ROOT / "src/game/navigation/TrajectoryPredictor.h").read_text(encoding="utf-8")
impl = (ROOT / "src/game/navigation/TrajectoryPredictor.cpp").read_text(encoding="utf-8")
adapter = (ROOT / "src/game/system_map/TrajectoryMapAdapter.h").read_text(encoding="utf-8")
combined = header + "\n" + impl


def fail(message: str) -> None:
    print(f"[FAIL] {message}", file=sys.stderr)
    raise SystemExit(1)


for forbidden in (
    "system_map/",
    "render/",
    "GameServer",
    "SpaceState",
    "RoutePlan",
    "Autopilot",
):
    if forbidden in combined:
        fail(f"shared TrajectoryPredictor depends on higher navigation/presentation layer: {forbidden}")

for required in (
    "WorldKinematicState initialState",
    "initialProperAccelerationMps2",
    "std::vector<GravityBody> gravityBodies",
    "std::vector<TrajectoryAccelerationKey> accelerationProgram",
    "TrajectoryMotionEnvelope motionEnvelope",
    "glm::dvec3 properAccelerationMps2",
    "glm::dvec3 gravityAccelerationMps2",
    "double properLoadGs",
    "double cumulativeProperDeltaVMps",
):
    if required not in header:
        fail(f"trajectory result/input contract missing: {required}")

if "GravityFieldSystem::sample" not in impl:
    fail("trajectory predictor is not using the shared gravity field")

if "StandardGravityMps2" not in header or "properLoadGs" not in impl:
    fail("proper acceleration is not exposed as a crew-load diagnostic")

if "maxProperJerkMps3" not in combined or "jerkClamped" not in combined:
    fail("trajectory predictor has no jerk envelope/diagnostic")

if "TrajectoryPredictor.h" not in adapter or "MapObjectOverlay.h" not in adapter:
    fail("map conversion is not isolated in the presentation-side adapter")

if "sample.state.positionMeters" not in adapter:
    fail("map adapter does not consume shared prediction positions")

print("[PASS] shared trajectory predictor boundary + map adapter contract")
