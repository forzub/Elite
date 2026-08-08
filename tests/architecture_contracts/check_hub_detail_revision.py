#!/usr/bin/env python3

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
SPACE = ROOT / "src/game/SpaceState.cpp"


def function_body(source: str, signature: str):
    start = source.find(signature)
    if start < 0:
        return None
    brace = source.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return None


def fail(message: str) -> None:
    print(f"Hub/Detail revision regression check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


source = SPACE.read_text(encoding="utf-8", errors="replace")
prepare = function_body(source, "void SpaceState::prepareFrame(float dt)")
loaded_detail = function_body(source, "void SpaceState::setSystemMapLoadedDetailMode()")

if prepare is None or loaded_detail is None:
    fail("required SpaceState functions are missing")

# Revision boundaries must throw old map bytes away while preserving the
# semantic Detail target used to return from Hub.
for required in (
    "m_detailMapSnapshot = {};",
    "m_hasDetailMapSnapshot = false;",
    "m_authoritativeMapInterpolator = {};",
):
    if required not in prepare:
        fail(f"revision fence no longer invalidates stale map state: {required}")

if "m_loadedDetailTarget =" in prepare or "m_loadedDetailTarget = {};" in prepare:
    fail("revision fence destroys the semantic Detail target")

if "if (!m_hasDetailMapSnapshot)\n        return;" in loaded_detail:
    fail("Hub -> Detail is disabled when the branch-local Detail cache was invalidated")

for required in (
    "m_loadedDetailTarget",
    "target.valid()",
    "requestDetailMapSnapshot(target, true)",
    "MapTransitionController::simulationHasReached(",
    "m_mapTransitions.beginDetail(target)",
    "beginSystemMapDetailTransition(target)",
):
    if required not in loaded_detail:
        fail(f"Hub -> Detail cannot reacquire its target after a revision fence: {required}")

print("Hub/Detail revision regression check passed.")
