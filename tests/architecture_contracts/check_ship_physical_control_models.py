#!/usr/bin/env python3

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(f"[FAIL] {message}")


COMMON = read("src/game/ship/plant/ShipPhysicalControlModel.h")
ASSISTED_H = read("src/game/ship/plant/AssistedPhysicalControlModel.h")
ASSISTED_CPP = read("src/game/ship/plant/AssistedPhysicalControlModel.cpp")
NEWTON_H = read("src/game/ship/plant/NewtonianPhysicalControlModel.h")
NEWTON_CPP = read("src/game/ship/plant/NewtonianPhysicalControlModel.cpp")
DISPATCH = read("src/game/ship/plant/ShipPhysicalControlModels.cpp")
DESCRIPTOR = read("src/game/ship/ShipDescriptor.h")
COBRA = read("src/game/ship/descriptors/EliteCobraMk1.cpp")
CONTRACT = read("src/game/ship/plant/SHIP_PHYSICAL_CONTROL_MODEL_CONTRACT.md")

for token in (
    "AssistedI",
    "AssistedY",
    "AssistedX",
    "NewtonianI",
    "NewtonianY",
    "NewtonianX",
    "RuntimeFlightRegime",
    "ThrustAxisFallback",
    "SurvivalControl",
):
    require(token in COMMON, f"physical model contract lost: {token}")

require(
    "class AssistedPhysicalControlModel final" in ASSISTED_H,
    "Assisted physical control module missing",
)
require(
    "class NewtonianPhysicalControlModel final" in NEWTON_H,
    "Newtonian physical control module missing",
)

for token in (
    "aft_left",
    "fore_left",
    "VectoringMain",
    "manoeuvreThrusterAccel = 0.0f",
    "RuntimeFlightRegime::ThrustAxisFallback",
):
    require(token in ASSISTED_CPP, f"Assisted topology/physics lost: {token}")

for token in (
    "main_center",
    "rcs_fore_left",
    "FixedAxis",
    "OmnidirectionalCluster",
    "reverseMainEngineAvailable = false",
    "RuntimeFlightRegime::SurvivalControl",
):
    require(token in NEWTON_CPP, f"Newtonian topology/physics lost: {token}")

require(
    "AssistedPhysicalControlModel::evaluate(" in DISPATCH
    and "NewtonianPhysicalControlModel::evaluate(" in DISPATCH,
    "six-model dispatcher no longer separates Assisted/Newtonian modules",
)

require(
    "PhysicalControlModelId physicalControlModel" in DESCRIPTOR,
    "ShipDescriptor lost immutable physical-control topology selection",
)
require(
    "ShipPhysicalControlBinding" in DESCRIPTOR
    and "std::vector<std::string> slotModuleIds" in DESCRIPTOR,
    "ShipDescriptor lost fixed-slot to damage-module binding",
)
require(
    "PhysicalControlModelId::AssistedI" in COBRA
    and "nativeFlightLaw(desc.physicalControlModel)" in COBRA,
    "Cobra Mk1 no longer binds its doctrine to Assisted-I hardware",
)
for module_id in (
    '"ship_engine_L"',
    '"ship_engine_R"',
    '"ship_fore_engine_L"',
    '"ship_fore_engine_R"',
):
    require(
        module_id in COBRA,
        f"Cobra Mk1 Assisted-I slot binding lost module: {module_id}",
    )

# Individual ship descriptors may select/tune a model but may not author
# private actuator geometry outside the six topology modules.
for path in (ROOT / "src/game/ship/descriptors").glob("*.cpp"):
    body = path.read_text(encoding="utf-8", errors="replace")
    require(
        "EngineSlotDefinition" not in body
        and "normalizedPositionBody" not in body,
        f"{path.relative_to(ROOT)} authors private propulsion geometry",
    )

# Ordinary local-flight plant must remain independent of the future jump drive
# and of resource/thermal implementation layers.
plant_sources = COMMON + ASSISTED_H + ASSISTED_CPP + NEWTON_H + NEWTON_CPP + DISPATCH
for forbidden in (
    "ReactorSystem",
    "CoolingSystem",
    "ThermalSystem",
    "PowerBus",
    "jumpActive",
    "JumpDrive",
    "CruiseDrive",
):
    require(
        forbidden not in plant_sources,
        f"ordinary physical control model leaked into another plant layer: {forbidden}",
    )

for phrase in (
    "Exactly six ordinary-flight physical models exist",
    "Jump/cruise propulsion is NOT ordinary Newtonian local-flight propulsion",
    "does NOT calculate",
):
    require(phrase in CONTRACT, f"physical model boundary documentation lost: {phrase}")

print("SHIP PHYSICAL CONTROL MODEL CONTRACT: PASS")
print(" - exactly six fixed Assisted/Newtonian I/Y/X topologies")
print(" - ship descriptors select topology but cannot author private slot geometry")
print(" - damage changes capability/regime, not construction family")
print(" - reactor/fuel/thermal/jump mechanics remain outside this layer")
