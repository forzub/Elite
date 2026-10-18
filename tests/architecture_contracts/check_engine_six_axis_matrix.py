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

matrix = contains(
    "src/game/ship/plant/ShipEngineControlMatrix.h",
    "executeEngineControlMatrix(",
    "normalizedPositionBody",
    "nominalForceDirectionBody",
    "projectThrustCone(",
    "availableGimbalRangeRad",
    "availableGimbalRateRadPerSec",
    "out.torqueBodyNm+=glm::cross(",
    "output.execute(EngineCommand",
) if False else contains(
    "src/game/ship/plant/ShipEngineControlMatrix.h",
    "executeEngineControlMatrix(",
    "normalizedPositionBody",
    "nominalForceDirectionBody",
    "projectThrustCone(",
    "availableGimbalRangeRad",
    "availableGimbalRateRadPerSec",
    "out.torqueBodyNm+=glm::cross(",
    "output.execute(EngineCommand" if False else "output.execute(EngineCommand",
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
print("ENGINE SIX-AXIS MATRIX + LIVE TELEMETRY CONTRACT: PASS")
