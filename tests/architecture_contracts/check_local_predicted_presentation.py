#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
errors = []


def require(path: str, *needles: str) -> None:
    p = ROOT / path
    if not p.is_file():
        errors.append(f"{path}: missing file")
        return
    text = p.read_text(encoding="utf-8", errors="replace")
    for needle in needles:
        if needle not in text:
            errors.append(f"{path}: missing {needle!r}")


require(
    "src/game/client/presentation/LocalPredictedPresentation.h",
    "sampleLocalPredictedPresentationTarget(",
    "ShipTransform target = fixedPredictedTransform;",
    "SharedShipPhysics::integrate(",
    "predictHubTacticalMotion(",
    "fractionalStepSeconds",
    "fixedStepSeconds",
)

require(
    "src/game/client/GameClient.cpp",
    "m_world.prepareLocalPredictedPresentation(",
    "m_accumulator,",
    "fixedDt",
    "m_world.clearLocalPredictedPresentation();",
)

require(
    "src/game/client/ClientWorldState.cpp",
    "m_localPredictedPresentationTarget",
    "presentationTarget =",
    "&m_localPredictedPresentationTarget",
    "playerPredictionRemainderMilliseconds",
    "playerRenderToFractionalTargetMeters",
)

# The fractional presentation path must never overwrite the fixed predicted
# transform used by reconciliation. The implementation works from a copy.
presentation = ROOT / "src/game/client/presentation/LocalPredictedPresentation.h"
if presentation.is_file():
    text = presentation.read_text(encoding="utf-8", errors="replace")
    if "fixedPredictedTransform =" in text:
        errors.append(
            "LocalPredictedPresentation.h: presentation mutates fixed predicted state"
        )

if errors:
    print("[FAIL] local predicted presentation architecture guard", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("[PASS] local predicted presentation architecture guard")
