#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# ShipCore is split into ShipCore*.cpp units (one class): read them as one.
def _ship_core_sources(root):
    d = root / "src/game/ship/core"
    return chr(10).join(p.read_text(encoding="utf-8") for p in sorted(d.glob("ShipCore*.cpp")))



def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


SNAPSHOT = read("src/game/ship/capability/ShipCapabilitySnapshot.h")
BUILDER = read("src/game/ship/capability/ShipCapabilityBuilder.h")
CORE_H = read("src/game/ship/core/ShipCore.h")
CORE_CPP = _ship_core_sources(ROOT)
PROPULSION = read("src/game/ship/ShipPropulsionState.h")
CLIENT = read("src/game/client/ClientWorldState.cpp")
GATEWAY_H = read("src/game/ship/control/ShipControlExecutionGateway.h")
GATEWAY_CPP = read("src/game/ship/control/ShipControlExecutionGateway.cpp")
CONTRACT = read("src/game/ship/capability/SHIP_CAPABILITY_CONTRACT.md")

for token in (
    "ShipCapabilitySnapshot",
    "ControlHealthState",
    "AxisCapability",
    "longitudinalAccelerationMps2",
    "strafeAccelerationMps2",
    "pitchAccelerationRadPerSec2",
    "effectivePhysics",
):
    require(token in SNAPSHOT, f"capability public surface lost: {token}")

for forbidden in (
    "PhysicalControlModelId",
    "PropulsionLayout",
    "EngineSlotDefinition",
    "EngineSlotState",
    "thrustUtilization",
    "poweredMainVectoring",
):
    require(
        forbidden not in SNAPSHOT,
        f"physical topology leaked into ShipCapabilitySnapshot: {forbidden}",
    )

for token in (
    "buildShipCapability",
    "physicalControlSlots",
    "evaluatePhysicalControlModel",
    "healthFraction",
    "slotFromModule",
    "slotFromSnapshot",
):
    require(token in BUILDER, f"capability builder lost: {token}")

require(
    "ShipCapabilitySnapshot capabilities() const" in CORE_H,
    "ShipCore does not expose current capability",
)
require(
    "ShipCore::capabilities() const" in CORE_CPP
    and "buildShipCapability(" in CORE_CPP,
    "ShipCore capability does not derive from physical plant",
)
require(
    "buildShipCapability(" in PROPULSION,
    "legacy effectiveShipPhysics bypasses capability layer",
)
require(
    "effectiveShipPhysics(" in CLIENT
    and "ship.modules" in CLIENT,
    "client prediction does not consume replicated capability state",
)

for body, name in (
    (GATEWAY_H, "gateway header"),
    (GATEWAY_CPP, "gateway implementation"),
):
    for forbidden in (
        "PhysicalControlModelId",
        "PropulsionLayout",
        "EngineSlotDefinition",
    ):
        require(
            forbidden not in body,
            f"{name} knows physical topology: {forbidden}",
        )

for phrase in (
    "what the ship can do now",
    "A failed or impossible control direction is a legal degraded state",
    "jump/cruise drive remains a separate mechanic",
):
    require(phrase in CONTRACT, f"capability contract lost: {phrase}")

print("SHIP CAPABILITY LAYER CONTRACT: PASS")
print(" - damage/topology stay below ShipCapabilitySnapshot")
print(" - server and client derive motion authority through one capability path")
print(" - gateway remains topology-blind")
print(" - failed control authority is degradation, never process-fatal")
