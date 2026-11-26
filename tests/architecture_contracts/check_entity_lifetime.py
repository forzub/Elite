#!/usr/bin/env python3
"""Lifetime contract (architecture audit 2026-11-19).

1. Every EntityId-keyed member table of GameSimulation is erased by forgetShip / forgetShipDetail
   (GameSimulation.cpp), or is listed below with the reason it is not a per-ship table.
2. No cache validates against the size of m_staticObjects: an erase + insert keeps the size and leaves dangling
   pointers - caches validate by m_staticObjectsGeneration.
3. Every structural mutator of m_staticObjects bumps m_staticObjectsGeneration.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SIM = ROOT / "src" / "game" / "simulation"

NOT_PER_SHIP = {
    "m_ships": "the owner table itself (erased by its removal path)",
    "m_shipShells": "the owner table itself (erased by removeTrafficShell)",
    "m_staticObjects": "static objects, not ships",
    "m_staticTracks": "static objects, not ships",
    # re-verified 2026-11-26: erased by setPlayerControlled(false) (ship switch GameServer.cpp ~2596, last session
    # disconnect ~2768) and markShipDerelict; a player ship never leaves m_ships (only dematerializeShip erases it and
    # player ships are pinned full)
    "m_playerControlledShipIds": "control pin with its own lifecycle (setPlayerControlled / markShipDerelict)",
    "m_hubMotionLabShips": "DEV lab registry with its own teardown",
    "m_dockObserverPlaced": "DEV observer bookkeeping",
    "m_dockPilotTraceSeconds": "DEV trace throttle",
}

# Per-ship tables NESTED in other state (keyed by the raw id): forgetShip must erase them too
NESTED_PER_SHIP = {
    "violators": "m_hubSecurity[hub].violators (ship id -> security level)",
}

errors = []
header = (SIM / "GameSimulation.h").read_text(encoding="utf-8", errors="replace")
impl = "".join(p.read_text(encoding="utf-8", errors="replace") for p in sorted(SIM.glob("GameSimulation*.cpp")))

# 1. EntityId-keyed members (single-line or wrapped declarations)
flat = re.sub(r"\s+", " ", header)
members = set(re.findall(r"std::(?:unordered_map|unordered_set|map|set)<\s*EntityId\b[^;]*?>\s*(m_[A-Za-z0-9_]+)\s*[;{=]", flat))


def body(name):
    i = impl.find(f"void GameSimulation::{name}(EntityId id)")
    if i < 0:
        errors.append(f"GameSimulation::{name}(EntityId) is missing")
        return ""
    depth, start = 0, impl.find("{", i)
    for k in range(start, len(impl)):
        depth += impl[k] == "{"
        depth -= impl[k] == "}"
        if depth == 0:
            return impl[start:k]
    return ""


forgotten = body("forgetShip") + body("forgetShipDetail")
for m in sorted(members):
    if m in NOT_PER_SHIP:
        continue
    if not re.search(rf"\b{m}\b", forgotten):
        errors.append(f"{m}: an EntityId-keyed table neither forgetShip nor forgetShipDetail erases "
                      f"(add it there, or to NOT_PER_SHIP with the reason)")

# a stale exception (the table is gone) must be removed, not carried
for m in sorted(NOT_PER_SHIP):
    if not re.search(rf"\b{m}\b", header):
        errors.append(f"NOT_PER_SHIP lists {m}, which GameSimulation.h no longer declares - drop the exception")
for nested, what in NESTED_PER_SHIP.items():
    if re.search(rf"\b{nested}\b", header) and not re.search(rf"\b{nested}\.erase\(", forgotten):
        errors.append(f"{what}: a nested per-ship table forgetShip does not erase")

# 2. no size-validated static caches
for p in sorted(SIM.glob("*.cpp")):
    for n, line in enumerate(p.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        if re.search(r"(==|!=)\s*m_staticObjects\.size\(\)|m_staticObjects\.size\(\)\s*(==|!=)|Size\s*=\s*m_staticObjects\.size\(\)", line):
            errors.append(f"{p.relative_to(ROOT)}:{n}: a cache validated by m_staticObjects.size() - use m_staticObjectsGeneration")

# 3. mutators bump the generation
for fn in ("spawnStation", "setStaticObjectIdentity", "setStaticObjectMapInfo", "setStaticObjectPackageHitVolumes",
           "attachStaticObjectToHub"):
    i = impl.find(f"GameSimulation::{fn}(")
    if i < 0:
        errors.append(f"{fn}: not found")
        continue
    j = impl.find("\n}\n", i)
    if "++m_staticObjectsGeneration" not in impl[i:j]:
        errors.append(f"{fn}: changes m_staticObjects without ++m_staticObjectsGeneration")
for m in re.finditer(r"m_staticObjects\.(erase|clear|extract)\(", impl):
    k = impl.rfind("\n}\n", 0, m.start())
    if "++m_staticObjectsGeneration" not in impl[k:impl.find("\n}\n", m.start())]:
        errors.append("an m_staticObjects erase / clear without ++m_staticObjectsGeneration in the same function")

print(f"EntityId-keyed tables found: {len(members)}")
if errors:
    for e in errors:
        print("FAIL", e)
    sys.exit(1)
print("Entity lifetime contract check passed.")
