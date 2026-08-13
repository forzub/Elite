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


def require(path: str, *needles: str) -> None:
    text = read(path)
    for needle in needles:
        if needle not in text:
            errors.append(f"{path}: missing {needle!r}")


require(
    "src/game/client/ClientLocalAuthority.h",
    "isLocalControlledEntity(",
    "controlledEntityId.value != 0",
    "candidate == controlledEntityId",
)

require(
    "src/game/client/GameClient.cpp",
    "m_playerId = welcome.controlledEntityId;",
    "m_world.setLocalControlledEntity(m_playerId);",
)

require(
    "src/game/client/ClientWorldState.h",
    "setLocalControlledEntity(EntityId id)",
    "localControlledEntityId() const",
    "isLocalControlledEntity(EntityId id) const",
    "m_localControlledEntityId",
)

require(
    "src/game/client/ClientWorldState.cpp",
    "isLocalControlledEntity(ship.id)",
    "m_ships.find(m_localControlledEntityId.value)",
    "if (!isLocalControlledEntity(id) || !ship.descriptor)",
)

require(
    "src/game/client/ClientMapService.cpp",
    "m_world.localControlledEntityId()",
)

for path in (
    "src/game/client/ClientWorldState.cpp",
    "src/game/client/ClientSystemMapShipBridge.h",
    "src/game/client/ClientDetailMapBridge.h",
    "src/game/client/ClientHubMapBridge.h",
):
    text = read(path)
    for forbidden in (
        "role == ShipRole::Player",
        "role != ShipRole::Player",
        "ship.role == ShipRole::Player",
        "source.role == ShipRole::Player",
    ):
        if forbidden in text:
            errors.append(
                f"{path}: local authority/presentation still inferred from {forbidden!r}"
            )

system_bridge = read("src/game/client/ClientSystemMapShipBridge.h")
for needle in (
    "EntityId localControlledEntityId",
    "isLocalPlayer",
    'object.stableId = isLocalPlayer',
):
    if needle not in system_bridge:
        errors.append(f"ClientSystemMapShipBridge.h: missing {needle!r}")

hub_bridge = read("src/game/client/ClientHubMapBridge.h")
for needle in (
    "EntityId localControlledEntityId",
    "isLocalPlayer",
    "if (!usesThisHubFrame && !isLocalPlayer)",
    "ship.player = isLocalPlayer;",
):
    if needle not in hub_bridge:
        errors.append(f"ClientHubMapBridge.h: missing {needle!r}")

if errors:
    print("[FAIL] multiplayer client local-vs-remote identity", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    sys.exit(1)

print("[PASS] multiplayer client uses server-assigned local controlled entity")
