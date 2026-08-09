#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
ACTIVATION = ROOT / "src/game/simulation/activation"

forbidden_tokens = (
    "Radar",
    "Sensor",
    "PresentationPolicy",
    "ClientWorldState",
    "GameClient",
)

violations = []
for path in sorted(ACTIVATION.glob("*.h")):
    text = path.read_text(encoding="utf-8")
    include_lines = "\n".join(
        line for line in text.splitlines() if line.lstrip().startswith("#include")
    )
    for token in forbidden_tokens:
        if token in include_lines:
            violations.append(f"{path.relative_to(ROOT)}: forbidden dependency {token}")

interaction = (ACTIVATION / "InteractionHorizon.h").read_text(encoding="utf-8")
required = (
    "lookAheadSeconds",
    "safetyMarginMeters",
    "gameplayRangeMeters",
    "timeToClosestSeconds",
    "closestSurfaceDistanceMeters",
    "entersEnvelopeWithinHorizon",
)
for token in required:
    if token not in interaction:
        violations.append(f"InteractionHorizon contract missing {token}")

spatial = (ACTIVATION / "SpatialBounds.h").read_text(encoding="utf-8")
if "LogicalDimensions" not in spatial:
    violations.append("SpatialBounds must derive conservative bounds from existing LogicalDimensions")

if violations:
    print("Interaction activation architecture check failed:")
    for violation in violations:
        print(f"- {violation}")
    sys.exit(1)

print("[PASS] interaction activation architecture guard")
