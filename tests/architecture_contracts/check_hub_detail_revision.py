#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def fail(message: str) -> None:
    print("[FAIL] Hub/Detail revision regression check passed: " + message)
    raise SystemExit(1)

def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")

space = read("src/game/SpaceState.cpp")
client = read("src/game/client/GameClient.cpp")
service = read("src/game/client/ClientMapService.cpp")
for token in ("m_detailMapSnapshot = {};", "m_hasDetailMapSnapshot = false;", "m_authoritativeMapInterpolator = {};"):
    if token not in space:
        fail(f"revision fence no longer invalidates stale presentation: {token}")
for token in ("composeDetailMapSnapshot(target)", "m_loadedDetailTarget", "beginSystemMapDetailTransition(target)"):
    if token not in space:
        fail(f"Hub -> Detail local recomposition seam missing: {token}")
if "m_lastSimulationMetadata" not in client:
    fail("accepted simulation metadata is not retained")
if "simulationHasReached" in service or "m_deferredDetailResponse" in service:
    fail("old response-epoch waiting survived local composition")

print("[PASS] Hub/Detail revision regression check passed")
