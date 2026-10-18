#!/usr/bin/env python3
"""Stop six-axis motor/telemetry authority from regressing to HUD guesses."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def contains(path: str, *tokens: str) -> str:
    text = read(path)
    for token in tokens:
        if token not in text:
            raise SystemExit(f"[ENGINE MATRIX][FAIL] {path}: missing {token}")
    return text

contains(
    "src/game/ship/plant/ShipEngineControlMatrix.h",
    "executeEngineControlMatrix(",
    "normalizedPositionBody",
    "nominalForceDirectionBody",
    "projectThrustCone(",
    "availableGimbalRangeRad",
    "out.torqueBodyNm+=glm::cross(",
    "output.execute(EngineCommand",
)
contains(
    "src/game/ship/control/ShipControlExecutionGateway.cpp",
    "executeEngineControlMatrix(",
    "motion.assistedStabilizationAccelerationMps2 = {};",
    "motion.mainEngineAccelerationMps2 =",
    "motion.mainPropulsionAngularAccelerationBodyRadPerSec2 =",
    "motion.engineTelemetry.reset(",
    "motion.mainEngineDeliveredThrust01[i] = e.deliveredThrust01;",
)
contains(
    "src/game/ship/core/ShipCore.cpp",
    "requestedMainAngularAccelerationBodyRadPerSec2 =",
    "angularAfter-angularBefore",
    "m_transform.pitchRate = static_cast<float>(angularBefore.x)",
    "CobrettyCR1VectorMainDrive()",
    "physicalEngines=&installedEngines;",
)
hud = contains(
    "src/game/presentation/EngineThrustHudPresentation.h",
    "readEngineMotorLiveView(",
    "bar.thrust01 = static_cast<float>(live.thrust01);",
    "buildShipBiteState(",
)
for bad in (
    "normalizedDemand(",
    "engineAccelerationMps2",
    "lateralDemand01",
    "std::hypot(demand",
):
    if bad in hud:
        raise SystemExit(f"[ENGINE MATRIX][FAIL] HUD invented thrust using {bad}")
contains(
    "src/game/presentation/EngineLiveTelemetryPresentation.h",
    "motion.engineTelemetry",
    "motion.mainEngineDeliveredThrust01",
)
contains(
    "src/game/ship/telemetry/EngineLiveTelemetry.h",
    "nozzleYawRad", "nozzlePitchRad", "forceBodyNewtons",
    "electricalMW", "wasteHeatMW",
)
contains(
    "src/game/network/WireDataSchema.h",
    "game::ship::telemetry::EngineLiveTelemetry",
    "v.engineTelemetry",
    "v.mainEngineDeliveredThrust01",
)
contains(
    "src/game/network/WireBinaryCodec.h",
    "struct IsArray", "IsArray<U>::value",
)
contains(
    "src/game/network/WireDataCodec.h",
    "SimulationSnapshotWireSchemaVersion = 16u",
)
# User-approved fictional TVC doctrine: no mechanical nozzle lag,
# Assisted at its previous gameplay envelope, separate Newtonian law.
preset=contains(
    "src/game/equipment/engine/presets/CobrettyCR1VectorMainDrive.h",
    "e.nominalGimbalRangeRad = 1.3089969389957472;",
)
matrix=contains(
    "src/game/ship/plant/ShipEngineControlMatrix.h",
    "for (int pass=0;pass<96;++pass)",
    "const bool mainAxisOnly =",
    "equalDeliveredN / instantaneousN",
    "out.torqueBodyNm +=",
    "const std::array<double,6> weights {1.0,1.0,1.0,80.0,80.0,80.0};",
    "PhysicalControlFamily::Newtonian",
    "ship.mainGimbalYawLimitRad",
    "ship.mainGimbalPitchLimitRad",
    "t.nozzleYawRad = 0.0;",
    "t.nozzlePitchRad = 0.0;",
    "std::tan(clampFinite(maxAngleRad, 0.0, 1.45))",
)
for forbidden in (
    "demand.previousYawRad", "demand.previousPitchRad",
    "demand.elapsedSeconds", "m[i].rate = ",
):
    if forbidden in matrix:
        raise SystemExit(
            f"[ENGINE MATRIX][FAIL] old servo-lag path returned: {forbidden}"
        )
simulation=contains(
    "src/game/simulation/GameSimulation.cpp",
    "const auto& angularMotion = ship.core().transform().motion;",
    "shipPtr->updateTranslationControl(motionControlDt);",
    "shipPtr->propagateMotionOrientation(fdt);",
)
after_force = simulation.split(
    "shipPtr->updateTranslationControl(motionControlDt);", 1
)[1]
if not after_force.lstrip().startswith(
    "// Rotation now uses the ACTUAL physical engine"
):
    raise SystemExit("[ENGINE MATRIX][FAIL] motor/attitude stage not contiguous")
if not after_force.split("}", 1)[0].__contains__(
    "shipPtr->propagateMotionOrientation(fdt);"
):
    raise SystemExit("[ENGINE MATRIX][FAIL] torque not applied before orientation")
contains(
    "tests/architecture_contracts/InstalledEngineActuatorTests.cpp",
    "assistedKeypadAxialReleaseHasNoParasiticPitch();",
    "KP8/KP2 axial motor bank generated unwanted angular rate",
    "KP8/KP2 longitudinal braking",
)
# Gameplay authority is now generated ONCE per layout/health revision, and
# each Assisted physics step mixes six physically executed prepared maneuvers.
program=contains(
    "src/game/ship/plant/ShipPresetEngineManeuvers.h",
    "struct AssistedManeuverPrograms final",
    "void build(",
    "buildInverse()",
    "void build(",
    "programs[2*axis+direction]",
    "matrix_detail::projectThrustCone(",
    "program.achievable=requested*fraction",
    "const double mix=std::min(",
    "globalScale=std::min(globalScale",
    "out.torqueBodyNm+=glm::cross(",
)
contains(
    "src/game/ship/core/ShipCore.h",
    "m_assistedManeuvers;",
)
contains(
    "src/game/ship/core/ShipCore.cpp",
    "&m_assistedManeuvers",
)
gateway=contains(
    "src/game/ship/control/ShipControlExecutionGateway.cpp",
    "assisted && maneuverPrograms",
    "maneuverPrograms->execute(",
)
contains(
    "tests/architecture_contracts/InstalledEngineActuatorTests.cpp",
    "preparedManeuversSurviveSingleMainEngineFailure();",
    "one failed engine prevented slower balanced straight flight",
    "repair did not restore nominal maneuver program",
)
if program.count("for(int iteration=0;iteration<144;++iteration)")!=1:
    raise SystemExit("[ENGINE PRESETS][FAIL] unexpected program-generation iteration count")
if "execute(" not in program or "solve(axis," not in program:
    raise SystemExit("[ENGINE PRESETS][FAIL] missing damage-only precompute")

print("ENGINE SIX-AXIS MATRIX + LIVE TELEMETRY + GAMEPLAY PRESET MOTOR CONTRACT: PASS")
