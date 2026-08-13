#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
errors = []


def read(path: str) -> str:
    p = ROOT / path
    if not p.is_file():
        errors.append(f"{path}: missing file")
        return ""
    return p.read_text(encoding="utf-8", errors="replace")


def require(path: str, *needles: str) -> str:
    text = read(path)
    for needle in needles:
        if needle not in text:
            errors.append(f"{path}: missing {needle!r}")
    return text


server_h = require(
    "src/game/server/GameServer.h",
    "navigationStateForSession(",
    "navigationStateForEntity(",
    "resolveSingleActiveSimulationSystemId() const",
    "applyCelestialOrbitParentParameters(int systemId)",
    "m_simulation.activeCelestialSystemId()",
)

server_cpp = require(
    "src/game/server/GameServer.cpp",
    "bool GameServer::navigationStateForSession(",
    "m_sessions.controlledEntity(sessionId)",
    "outNavigation = navigationStateForEntity(controlledEntityId);",
    "snapshot.session.playerNavigation = {};",
    "navigationStateForSession(sessionId, sessionNavigation)",
    "outSnapshot.session.playerNavigation = sessionNavigation;",
    "int GameServer::resolveSingleActiveSimulationSystemId() const",
    "m_simulation.playerControlledShipIds()",
    "simulationContextSystemId",
    "applyCelestialOrbitParentParameters(simulationContextSystemId)",
)

for path, text in (
    ("src/game/server/GameServer.h", server_h),
    ("src/game/server/GameServer.cpp", server_cpp),
):
    if "m_playerNavigation" in text:
        errors.append(
            f"{path}: singleton m_playerNavigation returned after per-session navigation migration"
        )

context_start = server_cpp.find("int GameServer::resolveSingleActiveSimulationSystemId() const")
context_end = server_cpp.find("GameServer::navigationStateForEntity", context_start)
if context_start >= 0 and context_end > context_start:
    context_body = server_cpp[context_start:context_end]
    if "m_simulation.playerId()" in context_body:
        errors.append(
            "GameServer.cpp: active simulation context still selects a primary player"
        )
    for needle in (
        "ship->core().transform().motion.systemId",
        "resolvedSystemId != shipSystemId",
        "m_simulation.activeCelestialSystemId()",
    ):
        if needle not in context_body:
            errors.append(
                f"GameServer.cpp: single-active context resolver missing {needle!r}"
            )

update_start = server_cpp.find("void GameServer::update(double dt)")
update_end = server_cpp.find("bool GameServer::enqueueMapRequest", update_start)
if update_start >= 0 and update_end > update_start:
    update_body = server_cpp[update_start:update_end]
    for forbidden in (
        "m_playerNavigation",
        "synchronizePlayerSystemMembership",
        "navigationStateForEntity(m_simulation.playerId())",
    ):
        if forbidden in update_body:
            errors.append(
                f"GameServer.cpp: fixed-step world context still depends on {forbidden!r}"
            )

architecture = require(
    "src/game/ARCHITECTURE_STATUS.md",
    "M4 removes the server-side singleton `m_playerNavigation`",
    "session-derived",
    "single active celestial-system runtime",
)

if "Windows" in architecture and "Linux" not in architecture:
    errors.append("ARCHITECTURE_STATUS.md: platform-neutral server boundary note regressed")

if errors:
    print("[FAIL] multiplayer per-session navigation boundary", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("[PASS] multiplayer navigation is session-derived; world runtime no longer follows a primary player")
