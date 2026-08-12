#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"[FAIL] replication static-definition boundary: {message}")
    sys.exit(1)


snapshot_h = (ROOT / "src/game/simulation/ObjectModuleSnapshot.h").read_text(encoding="utf-8")
simulation_cpp = (ROOT / "src/game/simulation/GameSimulation.cpp").read_text(encoding="utf-8")
client_builder_h = (ROOT / "src/game/client/ClientModuleViewBuilder.h").read_text(encoding="utf-8")
space_state_cpp = (ROOT / "src/game/SpaceState.cpp").read_text(encoding="utf-8")
ship_snapshot_h = (ROOT / "src/game/simulation/ShipSnapshot.h").read_text(encoding="utf-8")
object_snapshot_h = (ROOT / "src/game/simulation/ObjectSnapshot.h").read_text(encoding="utf-8")

# Runtime replication is allowed to identify a module and report mutable state.
required_runtime_fields = (
    "std::string moduleId",
    "std::uint8_t state",
    "float health",
    "int aliveSupportCount",
)
for token in required_runtime_fields:
    if token not in snapshot_h:
        fail(f"ObjectModuleSnapshot lost runtime field: {token}")

# These values are deterministic catalog/descriptor data and must not drift back
# into the ordinary per-instance network DTO.
forbidden_static_fields = (
    "parentModuleId",
    "subsystemId",
    "maxHealth",
    "destructible",
    "detachable",
    "hangable",
    "destroyPolicy",
    "detachPolicy",
    "attachmentType",
    "meshPartIds",
    "supportModuleIds",
    "minSupportsForAttached",
    "minSupportsForStable",
)
for token in forbidden_static_fields:
    if token in snapshot_h:
        fail(f"static descriptor field returned to ObjectModuleSnapshot: {token}")

# The server snapshot builder must not manually repack those static fields under
# another name or through stale ModuleViewData assignments.
for token in (
    "ms.parentModuleId",
    "ms.subsystemId",
    "ms.maxHealth",
    "ms.destructible",
    "ms.detachable",
    "ms.hangable",
    "ms.destroyPolicy",
    "ms.detachPolicy",
    "ms.attachmentType",
    "ms.meshPartIds",
    "ms.supportModuleIds",
    "ms.minSupportsForAttached",
    "ms.minSupportsForStable",
):
    if token in simulation_cpp:
        fail(f"GameSimulation still serializes static module descriptor data: {token}")

# A replicated entity still needs a compact catalog key. The client then joins
# that key to its own local descriptor library.
if "ObjectType" not in ship_snapshot_h or "typeId" not in ship_snapshot_h:
    fail("ShipSnapshot must keep typeId as the local catalog key")
if "ObjectType" not in object_snapshot_h or "type" not in object_snapshot_h:
    fail("ObjectSnapshot must keep type as the local catalog key")

for token in (
    "ModuleDescriptor",
    "ObjectModuleSnapshot",
    "ModuleViewData",
    "descriptor.parentModuleId",
    "descriptor.meshPartIds",
    "runtime.state",
    "runtime.health",
):
    if token not in client_builder_h:
        fail(f"client-side descriptor/runtime join is incomplete: {token}")

if "buildModuleViews(" not in space_state_cpp:
    fail("Structure Debug must rehydrate static module metadata on the client")
if "ObjectDescriptorRegistry::get(ship.typeId)" not in space_state_cpp:
    fail("Structure Debug is not using the local object descriptor catalog")

print("[PASS] static definitions stay local; replication carries module runtime only")
