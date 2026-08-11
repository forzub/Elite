#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")

prediction_h = SRC / "game/client/ClientHubTacticalPrediction.h"
client_world_cpp = SRC / "game/client/ClientWorldState.cpp"
dynamic_motion_cpp = SRC / "game/navigation/DynamicMotionSystem.cpp"

for path in (prediction_h, client_world_cpp, dynamic_motion_cpp):
    if not path.is_file():
        fail(path, "required HubTactical prediction contract file is missing")

if prediction_h.is_file():
    text = prediction_h.read_text(encoding="utf-8", errors="replace")
    for required in (
        "canPredictHubTacticalMotion(",
        "travelFrameForPrediction(",
        "DynamicMotionSystem::applyLocalFrameInput(",
        "DynamicMotionSystem::updateLocalFrameMotion(",
        "transform.motion.systemId == frame.systemId",
        "transform.motion.travelFrame.frameId",
    ):
        if required not in text:
            fail(prediction_h, f"client prediction does not reuse authoritative local-frame motion: {required}")

if client_world_cpp.is_file():
    text = client_world_cpp.read_text(encoding="utf-8", errors="replace")
    predict_start = text.find("void ClientWorldState::predict(")
    if predict_start < 0:
        fail(client_world_cpp, "ClientWorldState::predict is missing")
    else:
        predict_block = text[predict_start:]
        if "predictHubTacticalMotion(" not in predict_block:
            fail(client_world_cpp, "player prediction still advances attitude only")

    smoothing_start = text.find("ShipTransform smoothShipRenderTransform(")
    smoothing_end = text.find("static void applyGraphSnapshot", smoothing_start)
    if smoothing_start >= 0 and smoothing_end > smoothing_start:
        smoothing = text[smoothing_start:smoothing_end]
        for required in (
            "current.motion.localPositionMeters",
            "target.motion.localPositionMeters",
            "target.motion.localVelocityMps",
        ):
            if required not in smoothing:
                fail(client_world_cpp, f"render smoothing still discards predicted local motion: {required}")
        if "targetFrame.localPositionMeters - currentFrame.localPositionMeters" in smoothing:
            fail(client_world_cpp, "render smoothing returned to snapshot-only local translation")

if errors:
    print("HubTactical client-prediction architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("HubTactical client-prediction architecture check passed.")
