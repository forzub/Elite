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
    "src/game/client/ClientPresentationClock.h",
    "class ClientPresentationClock",
    "minimumSnapshotLeadSeconds",
    "recoverySnapshotLeadSeconds",
    "hardRebaseThresholdSeconds",
    "starvationCount()",
)

require(
    "src/game/client/SnapshotPresentationWindow.h",
    "struct SnapshotPresentationWindow",
    "resolveSnapshotPresentationWindow(",
    "hasInterpolationBracket",
    "interpolationAlpha",
)

require(
    "src/game/client/ClientWorldState.cpp",
    "resolveSnapshotPresentationWindow(",
    "presentationWindow.interpolationAlpha",
    "sampleRenderReferenceFrame",
    "sampleLocalPredictedPresentationTarget",
)

require(
    "src/game/client/GameClient.cpp",
    "m_presentationClock.update(",
    "renderServerTimeSeconds()",
    "m_world.prepareLocalPredictedPresentation(",
    "m_accumulator,",
)

require(
    "src/game/client/PRESENTATION_PIPELINE_CONTRACT.md",
    "Single render epoch",
    "Remote dynamic entities",
    "Local controlled player",
    "Analytic deterministic entities",
    "Snapshot DTO safety",
)

# The object renderer must not recreate a second clamp/bracket algorithm after
# the shared presentation-window resolver was introduced.
world_state = ROOT / "src/game/client/ClientWorldState.cpp"
if world_state.is_file():
    text = world_state.read_text(encoding="utf-8", errors="replace")
    update_start = text.find("void ClientWorldState::update(")
    update_end = text.find("void ClientWorldState::prepareLocalPredictedPresentation(")
    if update_start >= 0 and update_end > update_start:
        update = text[update_start:update_end]
        forbidden = [
            "if (renderTime > newest)",
            "if (renderTime < oldest)",
            "(renderTime - older->metadata.serverTimeSeconds) / span",
        ]
        for needle in forbidden:
            if needle in update:
                errors.append(
                    "ClientWorldState::update: duplicated snapshot presentation "
                    f"timeline logic found: {needle!r}"
                )

if errors:
    print("[FAIL] presentation pipeline architecture guard", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("[PASS] presentation pipeline architecture guard")
