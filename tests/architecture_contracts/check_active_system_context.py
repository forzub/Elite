#!/usr/bin/env python3

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "src"
errors = []


def fail(path: Path, message: str) -> None:
    errors.append(f"{path.relative_to(ROOT)}: {message}")


def read(path: Path) -> str:
    if not path.is_file():
        fail(path, "required active-system contract file is missing")
        return ""
    return path.read_text(encoding="utf-8", errors="replace")


def function_body(source: str, signature: str):
    start = source.find(signature)
    if start < 0:
        return None
    brace = source.find("{", start)
    if brace < 0:
        return None
    depth = 0
    for index in range(brace, len(source)):
        ch = source[index]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    return None


simulation_h = SRC / "game/simulation/GameSimulation.h"
runtime_policy_h = SRC / "game/simulation/RuntimeSystemPolicy.h"
simulation_cpp = SRC / "game/simulation/GameSimulation.cpp"
server_cpp = SRC / "game/server/GameServer.cpp"
static_h = SRC / "world/objects/StaticObject.h"
signal_h = SRC / "world/WorldSignal.h"
planet_h = SRC / "world/Planet.h"
interference_h = SRC / "world/InterferenceSource.h"
ship_cpp = SRC / "game/ship/Ship.cpp"
scene_cpp = SRC / "game/scene/GameSceneSetup.cpp"

simulation_h_text = read(simulation_h)
runtime_policy_text = read(runtime_policy_h)
simulation_text = read(simulation_cpp)
server_text = read(server_cpp)
static_text = read(static_h)
signal_text = read(signal_h)
planet_text = read(planet_h)
interference_text = read(interference_h)
ship_text = read(ship_cpp)
scene_text = read(scene_cpp)

for required in (
    "validRuntimeSystemId",
    "sameRuntimeSystem",
    "canCreateInActiveRuntimeSystem",
):
    if required not in runtime_policy_text:
        fail(runtime_policy_h, f"runtime system policy helper is incomplete: {required}")

# Current runtime deliberately simulates one dynamic star-system context at a
# time. That limitation must be explicit; otherwise same-valued local
# coordinates from different systems can silently share gravity/reference data.
if "int m_activeCelestialSystemId = -1;" not in simulation_h_text:
    fail(simulation_h, "single active celestial runtime context is not explicit")

for signature in (
    "void setCelestialBodyKinematicStateAu(\n        int systemId,",
    "void setCelestialBodyGravityParameters(\n        int systemId,",
    "bool resolveCelestialBodyMeters(\n        int systemId,",
    "bool resolveCelestialBodyVelocityMetersPerSecond(\n        int systemId,",
):
    if signature not in simulation_h_text:
        fail(simulation_h, f"celestial runtime API is not system-scoped: {signature.split('(')[0]}")

set_kinematics = function_body(
    simulation_text,
    "void GameSimulation::setCelestialBodyKinematicStateAu("
)
if set_kinematics is None:
    fail(simulation_cpp, "could not locate active-system celestial injection")
else:
    for token in (
        "m_activeCelestialSystemId != systemId",
        "m_celestialBodyGravityParameters.clear()",
        "m_hubNavigationFrames.clear()",
        "m_gravityBodies.clear()",
        "m_orbitalCorridors.clear()",
    ):
        if token not in set_kinematics:
            fail(simulation_cpp, f"system switch does not invalidate dependent runtime cache: {token}")

spawn_ship = function_body(simulation_text, "EntityId GameSimulation::spawnShip(")
spawn_station = function_body(simulation_text, "EntityId GameSimulation::spawnStation(")
register_hub = function_body(simulation_text, "bool GameSimulation::registerOrbitalHub(")
for label, body in (("ship", spawn_ship), ("station", spawn_station), ("hub", register_hub)):
    if body is None:
        fail(simulation_cpp, f"could not locate {label} runtime creation")
    elif "m_activeCelestialSystemId" not in body:
        fail(simulation_cpp, f"{label} creation can enter a foreign unsupported dynamic system")

rebuild_gravity = function_body(
    simulation_text,
    "void GameSimulation::rebuildNavigationGravityContext("
)
if rebuild_gravity is None:
    fail(simulation_cpp, "could not locate navigation gravity rebuild")
else:
    for forbidden in (
        '"system_0.Sol.Земля"',
        '"earth_orbital_hub"',
        '3.986004418e14',
    ):
        if forbidden in rebuild_gravity:
            fail(simulation_cpp, f"navigation runtime still hard-codes Earth policy: {forbidden}")
    for required in (
        "m_celestialBodyGravityParameters",
        "m_celestialBodyPositionsAu",
        "hub.systemId != m_activeCelestialSystemId",
        'corridor.id = hub.id + "_corridor"',
    ):
        if required not in rebuild_gravity:
            fail(simulation_cpp, f"navigation context is not data/system driven: {required}")

update_navigation = function_body(
    simulation_text,
    "void GameSimulation::updateDynamicNavigationContext("
)
if update_navigation is None or "tr.motion.systemId != m_activeCelestialSystemId" not in update_navigation:
    fail(simulation_cpp, "foreign-system ships can consume the active system gravity/corridor cache")

begin_diag = function_body(
    simulation_text,
    "bool GameSimulation::beginUniverseTrajectoryDiagnostic("
)
if begin_diag is None or "reason=inactive_system_context" not in begin_diag:
    fail(simulation_cpp, "diagnostic branch can mix ships from unsupported system contexts")

# Map/index metadata may describe a parent, but it must not mutate spatial
# membership or supply the physical orbital parent.
if "std::string orbitalParentBodyId;" not in static_text:
    fail(static_h, "static orbital parent is still conflated with map metadata")

map_info = function_body(simulation_text, "bool GameSimulation::setStaticObjectMapInfo(")
if map_info is None:
    fail(simulation_cpp, "could not locate static map metadata setter")
else:
    if "systemId" in map_info:
        fail(simulation_cpp, "map metadata setter can mutate/read spatial system authority")
    if "orbitalParentBodyId" in map_info:
        fail(simulation_cpp, "map metadata setter can mutate orbital parent authority")

orbital_motion = function_body(simulation_text, "bool GameSimulation::setStaticObjectOrbitalMotion(")
if orbital_motion is None:
    fail(simulation_cpp, "could not locate static orbital-motion authority setter")
else:
    for required in (
        "orbitalParentBodyId = parentBodyId",
        "it->second.systemId != m_activeCelestialSystemId",
        "m_celestialBodyPositionsAu.find(parentBodyId)",
    ):
        if required not in orbital_motion:
            fail(simulation_cpp, f"static orbital motion can bind outside active spatial authority: {required}")

# Physics code may assign mapParentBodyId in setStaticObjectMapInfo, but outside
# that function it must not use presentation parent metadata to move objects.
if map_info is not None:
    stripped = simulation_text.replace(map_info, "")
    if "mapParentBodyId" in stripped:
        fail(simulation_cpp, "simulation physics still consumes mapParentBodyId")

# Sensor domains must follow system membership too. Same numeric local position
# in two star systems is not proximity.
for path, text in ((signal_h, signal_text), (planet_h, planet_text), (interference_h, interference_text)):
    if not re.search(r"\bint\s+systemId\s*=\s*-1\s*;", text):
        fail(path, "sensor-space source lacks system membership")

emit_signal = function_body(ship_text, "std::optional<WorldSignal> Ship::emitSignal() const")
if emit_signal is None or "signal->systemId = m_core.transform().motion.systemId" not in emit_signal:
    fail(ship_cpp, "ship signal does not inherit ship system membership")

update = function_body(simulation_text, "void GameSimulation::update(")
if update is None:
    fail(simulation_cpp, "could not locate simulation update")
else:
    for required in (
        "signalsBySystem",
        "planetsBySystem",
        "interferenceBySystem",
        "sameRuntimeSystem(",
    ):
        if required not in update:
            fail(simulation_cpp, f"sensor/radar update can cross system membership: {required}")

# A single-active-system runtime must freeze foreign ships completely rather
# than merely withholding gravity. AI/physics/repair state from a different
# local coordinate space cannot advance under the active system context.
update = function_body(simulation_text, "void GameSimulation::update(")
if update is not None:
    if update.count("sameRuntimeSystem(") < 5:
        fail(simulation_cpp, "inactive-system ships are not fenced from production gameplay loops")

repair = function_body(
    simulation_text,
    "bool GameSimulation::startBestRepairJobForMissingSlot("
)
if repair is None:
    fail(simulation_cpp, "could not locate repair-source selection contract")
else:
    for required in (
        "targetSystemId",
        "m_activeCelestialSystemId",
        "sourceShip->core().transform().motion.systemId",
        "sameRuntimeSystem(",
    ):
        if required not in repair:
            fail(simulation_cpp, f"repair source selection can cross star systems: {required}")

if "hub.systemId != 0" in scene_text:
    fail(scene_cpp, "initial-world loader still hard-codes system 0 instead of the active runtime context")
if "sim.activeCelestialSystemId()" not in scene_text:
    fail(scene_cpp, "initial-world loader does not derive its system from runtime authority")

server_update = function_body(server_text, "void GameServer::update(double dt)")
if server_update is None:
    fail(server_cpp, "could not locate GameServer update")
else:
    if "m_simulation.setCelestialBodyKinematicStateAu(\n    m_playerNavigation.currentSystemId," not in server_update:
        fail(server_cpp, "server injects celestial kinematics without explicit active system id")
    if "m_appliedSimulationContextSystemId" not in server_update:
        fail(server_cpp, "server does not rebuild gravity/orbit-parent parameters after a system context change")

if errors:
    print("Active-system runtime architecture check failed:", file=sys.stderr)
    for error in errors:
        print(f"  - {error}", file=sys.stderr)
    raise SystemExit(1)

print("Active-system runtime architecture check passed.")
