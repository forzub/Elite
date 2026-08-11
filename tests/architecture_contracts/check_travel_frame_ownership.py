#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []


def text(path: Path) -> str:
    if not path.is_file():
        errors.append(f"{path.relative_to(ROOT)}: required file is missing")
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def require(path: Path, body: str, token: str, message: str) -> None:
    if token not in body:
        errors.append(f"{path.relative_to(ROOT)}: {message}: {token}")


motion_h = SRC / "game/navigation/DynamicMotionState.h"
travel_h = SRC / "game/navigation/TravelFrameSystem.h"
dynamic_cpp = SRC / "game/navigation/DynamicMotionSystem.cpp"
sim_cpp = SRC / "game/simulation/GameSimulation.cpp"
snapshot_h = SRC / "game/simulation/ShipReferenceFrameSnapshot.h"
prediction_h = SRC / "game/client/ClientHubTacticalPrediction.h"

motion = text(motion_h)
travel = text(travel_h)
dynamic = text(dynamic_cpp)
sim = text(sim_cpp)
snapshot = text(snapshot_h)
prediction = text(prediction_h)

for token in (
    "KinematicFrame travelFrame",
    "matchedToReferenceFrame",
    "matchedReferenceFrameId",
):
    require(motion_h, motion, token, "ship-owned travel-frame state disappeared")

for token in (
    "TravelFrameSystem",
    "matchToReference(",
    "refreshMatchedReference(",
    "detach(",
    "const std::string ownedFrameId = motion.travelFrame.frameId",
):
    require(travel_h, travel, token, "travel-frame ownership operation disappeared")

for token in (
    "applyLocalFrameInput(",
    "updateLocalFrameMotion(",
    "const KinematicFrame& frame",
):
    require(dynamic_cpp, dynamic, token, "local motion is no longer generic-frame based")

for token in (
    "TravelFrameSystem::matchToReference(",
    "TravelFrameSystem::refreshMatchedReference(",
    "tr.motion.travelFrame",
    "DynamicMotionSystem::applyLocalFrameInput(",
    "DynamicMotionSystem::updateLocalFrameMotion(",
    "seedFromLocalTravelFrame",
    "const auto& frame = tr.motion.travelFrame",
    "if (tr.motion.matchedToReferenceFrame)",
):
    require(sim_cpp, sim, token, "server stopped using ship-owned travel-frame authority")

for token in (
    "std::string frameId",
    "matchedToReferenceFrame",
    "frame.frameId = frameId.empty() ? hubId : frameId",
):
    require(snapshot_h, snapshot, token, "snapshot lost owned travel-frame identity")

for token in (
    "travelFrameForPrediction(",
    "frameSnapshot.matchedToReferenceFrame",
    "transform.motion.travelFrame = frame",
):
    require(prediction_h, prediction, token, "client prediction stopped consuming authoritative travel-frame state")

# Hub kinematics may still be the source while matched, but local integration
# itself must not be called directly with HubNavigationFrame anymore.
for forbidden in (
    "DynamicMotionSystem::applyHubTacticalInput(",
    "DynamicMotionSystem::updateHubTactical(",
):
    if forbidden in sim or forbidden in prediction:
        errors.append(f"local integration still depends directly on hub-specific API: {forbidden}")

if errors:
    print("Travel-frame ownership architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("Travel-frame ownership architecture check passed.")
