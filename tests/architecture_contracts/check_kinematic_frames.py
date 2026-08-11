#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(path: Path, message: str) -> None:
    print(f"[FAIL] kinematic-frame architecture: {path}: {message}", file=sys.stderr)
    raise SystemExit(1)


def read(rel: str) -> tuple[Path, str]:
    path = ROOT / rel
    if not path.is_file():
        fail(path, "required file is missing")
    return path, path.read_text(encoding="utf-8", errors="replace")


frame_path, frame = read("src/game/navigation/KinematicFrame.h")
hub_path, hub = read("src/game/navigation/HubNavigationFrame.h")
snapshot_path, snapshot = read("src/game/simulation/ShipReferenceFrameSnapshot.h")
sim_path, simulation = read("src/game/simulation/GameSimulation.cpp")
prediction_path, prediction = read("src/game/client/ClientHubTacticalPrediction.h")
presentation_path, presentation = read("src/game/client/ReferenceFramePresentation.h")

for required in (
    "struct KinematicFrame",
    "linearVelocityMps",
    "linearAccelerationMps2",
    "localToWorldBasis",
    "angularVelocityWorldRadPerSecond",
    "angularAccelerationWorldRadPerSecond2",
    "localToWorldAcceleration",
    "worldToLocalAcceleration",
    "rebaseLocalKinematics",
    "from.systemId != to.systemId",
):
    if required not in frame:
        fail(frame_path, f"generic moving-frame contract disappeared: {required}")

for required in (
    "accelerationMetersPerSecond2",
    "angularAccelerationWorldRadPerSecond2",
    "KinematicFrame kinematicFrame() const",
):
    if required not in hub:
        fail(hub_path, f"hub frame no longer publishes complete kinematics: {required}")

for required in (
    "accelerationMetersPerSecond2",
    "angularAccelerationWorldRadPerSecond2",
    "game::navigation::KinematicFrame kinematicFrame() const",
):
    if required not in snapshot:
        fail(snapshot_path, f"snapshot lost moving-frame kinematics: {required}")

for required in (
    "const auto previousFrames = m_hubNavigationFrames",
    "rebuildHubNavigationFrames(trajectoryDeltaSeconds)",
    "std::abs(frameDeltaSeconds)",
    "frame.accelerationMetersPerSecond2",
    "frame.angularAccelerationWorldRadPerSecond2",
    "s.referenceFrame.accelerationMetersPerSecond2",
    "s.referenceFrame.angularAccelerationWorldRadPerSecond2",
):
    if required not in simulation:
        fail(sim_path, f"server no longer publishes frame acceleration: {required}")

for required in (
    "source.accelerationMetersPerSecond2",
    "source.angularAccelerationWorldRadPerSecond2",
):
    if required not in prediction:
        fail(prediction_path, f"prediction adapter dropped frame acceleration: {required}")

for required in (
    "out.accelerationMetersPerSecond2",
    "out.angularAccelerationWorldRadPerSecond2",
):
    if required not in presentation:
        fail(presentation_path, f"presentation interpolation dropped frame acceleration: {required}")

print("[PASS] generic kinematic-frame shadow model and acceleration propagation")
