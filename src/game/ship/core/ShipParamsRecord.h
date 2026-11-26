#pragma once

// ShipParams by NAME for anything persisted outside the process (autopilot replay cases): the record does not depend
// on the C++ memory layout - a field added / moved / removed in ShipParams never shifts an old case (a raw-byte dump
// once broke the whole Newtonian replay matrix when fields were inserted). Every ShipParams field must be listed here
// (tests/architecture_contracts/check_ship_params_record.py); a removed field stays readable as an unknown name.

#include "src/game/ship/core/ShipParams.h"

template <class P, class F>
inline void forEachShipParamsField(P& p, F&& f)
{
    f("maxPitchRate", p.maxPitchRate);
    f("maxYawRate", p.maxYawRate);
    f("maxRollRate", p.maxRollRate);
    f("angularAccel", p.angularAccel);
    f("angularDamping", p.angularDamping);
    f("maxCombatSpeed", p.maxCombatSpeed);
    f("maxCruiseSpeed", p.maxCruiseSpeed);
    f("throttleAccel", p.throttleAccel);
    f("stopSpeedEpsilonMps", p.stopSpeedEpsilonMps);
    f("brakeAlignmentCosine", p.brakeAlignmentCosine);
    f("assistedMinimumTargetSpeedChangeRateMps2", p.assistedMinimumTargetSpeedChangeRateMps2);
    f("assistedTargetSpeedChangeRateFractionPerSecond", p.assistedTargetSpeedChangeRateFractionPerSecond);
    f("fallbackThrottleResponsePerSecond", p.fallbackThrottleResponsePerSecond);
    f("forwardMainEngineAvailable", p.forwardMainEngineAvailable);
    f("reverseMainEngineAvailable", p.reverseMainEngineAvailable);
    f("forwardMainEngineAccelerationMps2", p.forwardMainEngineAccelerationMps2);
    f("reverseMainEngineAccelerationMps2", p.reverseMainEngineAccelerationMps2);
    f("mainGimbalPitchLimitRad", p.mainGimbalPitchLimitRad);
    f("mainGimbalYawLimitRad", p.mainGimbalYawLimitRad);
    f("mainGimbalRateRadPerSec", p.mainGimbalRateRadPerSec);
    f("autoLevelStrength", p.autoLevelStrength);
    f("strafeAccel", p.strafeAccel);
    f("strafeDamping", p.strafeDamping);
    f("maxStrafeSpeed", p.maxStrafeSpeed);
    f("manoeuvreThrusterAccel", p.manoeuvreThrusterAccel);
    f("manoeuvreGasUsePerSecond", p.manoeuvreGasUsePerSecond);
    f("manoeuvreGasRechargePerSecond", p.manoeuvreGasRechargePerSecond);
    f("manoeuvreGasRestartFraction", p.manoeuvreGasRestartFraction);
    f("maxGs", p.maxGs);
    f("maxLinearGs", p.maxLinearGs);
    f("turnRadius", p.turnRadius);
    f("massKg", p.massKg);
    f("centerOfMassLocalMeters", p.centerOfMassLocalMeters);
    f("pitchInertiaKgM2", p.pitchInertiaKgM2);
    f("yawInertiaKgM2", p.yawInertiaKgM2);
    f("rollInertiaKgM2", p.rollInertiaKgM2);
    f("manoeuvreThrusterAuthorityExplicit", p.manoeuvreThrusterAuthorityExplicit);
    f("physicalActuatorHalfExtentsMeters", p.physicalActuatorHalfExtentsMeters);
    f("assistedWorkingThrustAxisEmergency", p.assistedWorkingThrustAxisEmergency);
    f("assistedWorkingThrustAxisBody", p.assistedWorkingThrustAxisBody);
    f("assistedWorkingThrustAxisAngleRad", p.assistedWorkingThrustAxisAngleRad);
    f("assistedWorkingThrustMotorIndex", p.assistedWorkingThrustMotorIndex);
    f("newtonianRcsPositiveMps2", p.newtonianRcsPositiveMps2);
    f("newtonianRcsNegativeMps2", p.newtonianRcsNegativeMps2);
    f("newtonianAngularPositiveRadPerSec2", p.newtonianAngularPositiveRadPerSec2);
    f("newtonianAngularNegativeRadPerSec2", p.newtonianAngularNegativeRadPerSec2);
    f("newtonianRcsCombinedShare01", p.newtonianRcsCombinedShare01);
    f("actuatorTopologyKey", p.actuatorTopologyKey);
    f("jumpCoreRatingT", p.jumpCoreRatingT);
    f("jumpChargeCapacityT", p.jumpChargeCapacityT);
}
