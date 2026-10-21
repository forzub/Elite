#pragma once

// Evidence-based, topology-independent ship mobility classification.
//
// This is NOT a damaged-engine count, an automatic law switch, or a route
// planner. Only authoritative subsystem proofs may populate the inputs.
// An unknown energy/thermal/control certificate cannot authorize a flight.
// The same four operational classes apply to Assisted I/Y/X, Newtonian
// I/Y/X, drones and later installations. Unverified is a separate
// diagnostic state, not a fifth degree of physical damage.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace game::ship::capability
{

enum class MobilityClass : std::uint8_t
{
    Unverified = 0,
    Factory,
    Limited,
    Emergency,
    Inoperable
};

enum MobilityBlocker : std::uint32_t
{
    None = 0u,
    MissingControlProof = 1u << 0,
    MissingPowerProof = 1u << 1,
    MissingThermalProof = 1u << 2,
    NoUsableThrust = 1u << 3,
    CannotArrestRotation = 1u << 4,
    CannotCompleteHalfTurn = 1u << 5,
    CannotHoldBurnAttitude = 1u << 6,
    CannotBrake = 1u << 7,
    PowerUnavailable = 1u << 8,
    ThermalRunawayBeforeManeuver = 1u << 9,
    TransientEnergyOnly = 1u << 10,
    TransientCoolingOnly = 1u << 11,
    ReducedAttitudeAuthority = 1u << 12,
    ReducedCourseAuthority = 1u << 13,
    OutsideFactoryEnvelope = 1u << 14
};

struct ControlMobilityProof
{
    bool verified = false;

    // These are results of an installed-motor, signed-wrench sequence proof,
    // not counts of engines or independent scalar-axis rating checks.
    bool canProvideControlledThrust = false;
    bool canArrestAngularVelocity = false;
    bool canCompleteHalfTurn = false; // >= pi in at least one direction
    bool canHoldRequiredBurnAttitude = false;
    bool canBrakeAfterReorientation = false;

    // Full course/attitude control is needed for ordinary operations, but
    // not for a proven restricted flip/coast/burn survival sequence.
    bool fullAttitudeAuthority = false;
    bool controlledCourseAuthority = false;
    bool factoryPerformanceEnvelope = false;

    // Conservative verified UPPER bound for a complete representative
    // survival maneuver, including powered attitude changes, angular
    // braking and propulsive velocity reduction. Must be >0 and finite.
    double safeManeuverDurationSeconds = 0.0;

    // Usable NET force, after unwanted angular moment has been trimmed.
    double controlledAccelerationMps2 = 0.0;
};

struct PowerMobilityProof
{
    bool verified = false;
    // Same electrical bus, real continuous capability, in MW. Include
    // engine electrical draw; current PowerBus alone does NOT yet do this.
    double continuouslyAvailableMW = 0.0;
    double essentialSystemsMW = 0.0;
    double maneuverActuatorsMW = 0.0;
    // Verified electrical storage usable for the chosen maneuver, in MJ.
    double usableStoredEnergyMJ = 0.0;
};

struct ThermalMobilityProof
{
    bool verified = false;
    // Same effective coolant/heat sink; exclude heat already radiated
    // directly by an engine nozzle. All figures are in MW = MJ/s.
    double maneuverHeatInputMW = 0.0;
    double sustainableHeatRejectionMW = 0.0;
    double remainingSafeHeatStorageMJ = 0.0;
};

struct ShipMobilityEvidence
{
    ControlMobilityProof control {};
    PowerMobilityProof power {};
    ThermalMobilityProof thermal {};
};

struct ShipMobilityAssessment
{
    MobilityClass mobility = MobilityClass::Unverified;
    std::uint32_t blockers = MobilityBlocker::None;

    // Unrestricted continuous thrust is never implied by survival ability.
    bool continuousPowerSustainable = false;
    bool continuousThermalSustainable = false;
    bool survivalManeuverCertified = false;
    bool candidateForSimpleReturnRoute = false;

    double safeManeuverDurationSeconds = 0.0;
    double certifiedAccelerationMps2 = 0.0;
    // Time until electrical/thermal stored reserves run out at the
    // specifically certified maneuver's sustained average load.
    double poweredSecondsAvailable =
        std::numeric_limits<double>::infinity();
    double thermalSecondsAvailable =
        std::numeric_limits<double>::infinity();
};

[[nodiscard]] inline ShipMobilityAssessment assessShipMobility(
    const ShipMobilityEvidence& evidence
) noexcept
{
    ShipMobilityAssessment out;
    const auto& c = evidence.control;
    const auto& p = evidence.power;
    const auto& t = evidence.thermal;
    const auto finiteNonNegative = [](double v) noexcept {
        return std::isfinite(v) && v >= 0.0;
    };

    if (!c.verified ||
        !std::isfinite(c.safeManeuverDurationSeconds) ||
        !finiteNonNegative(c.controlledAccelerationMps2))
    {
        out.blockers |= MissingControlProof;
    }
    if (!p.verified ||
        !finiteNonNegative(p.continuouslyAvailableMW) ||
        !finiteNonNegative(p.essentialSystemsMW) ||
        !finiteNonNegative(p.maneuverActuatorsMW) ||
        !finiteNonNegative(p.usableStoredEnergyMJ))
    {
        out.blockers |= MissingPowerProof;
    }
    if (!t.verified ||
        !finiteNonNegative(t.maneuverHeatInputMW) ||
        !finiteNonNegative(t.sustainableHeatRejectionMW) ||
        !finiteNonNegative(t.remainingSafeHeatStorageMJ))
    {
        out.blockers |= MissingThermalProof;
    }
    if (out.blockers != None)
        return out; // fail closed: no unverified route recommendation

    out.safeManeuverDurationSeconds = c.safeManeuverDurationSeconds;
    out.certifiedAccelerationMps2 = c.controlledAccelerationMps2;

    if (!c.canProvideControlledThrust ||
        c.controlledAccelerationMps2 <= 0.0)
        out.blockers |= NoUsableThrust;
    if (!c.canArrestAngularVelocity)
        out.blockers |= CannotArrestRotation;
    if (!c.canCompleteHalfTurn)
        out.blockers |= CannotCompleteHalfTurn;
    if (!c.canHoldRequiredBurnAttitude)
        out.blockers |= CannotHoldBurnAttitude;
    if (!c.canBrakeAfterReorientation)
        out.blockers |= CannotBrake;

    const double totalPowerMW =
        p.essentialSystemsMW + p.maneuverActuatorsMW;
    // The electric bus must at least keep the essential attitude, control
    // and safety systems alive continuously; propulsive load may use
    // explicit finite emergency storage if verified.
    if (p.continuouslyAvailableMW + 1.0e-9 <
        p.essentialSystemsMW)
        out.blockers |= PowerUnavailable;

    const double powerDeficitMW =
        std::max(0.0, totalPowerMW - p.continuouslyAvailableMW);
    const double thermalExcessMW =
        std::max(0.0, t.maneuverHeatInputMW -
                         t.sustainableHeatRejectionMW);

    out.continuousPowerSustainable = powerDeficitMW <= 1.0e-9;
    out.continuousThermalSustainable = thermalExcessMW <= 1.0e-9;

    if (!out.continuousPowerSustainable)
    {
        out.blockers |= TransientEnergyOnly;
        out.poweredSecondsAvailable =
            p.usableStoredEnergyMJ / powerDeficitMW;
    }
    if (!out.continuousThermalSustainable)
    {
        out.blockers |= TransientCoolingOnly;
        out.thermalSecondsAvailable =
            t.remainingSafeHeatStorageMJ / thermalExcessMW;
    }

    // A duration certificate of zero cannot establish any usable
    // complete-maneuver chain; self-spinning or mere motor ignition is not
    // enough to authorize navigation.
    const bool baseMotionImpossible =
        (out.blockers & (NoUsableThrust | CannotArrestRotation |
                         CannotCompleteHalfTurn | CannotHoldBurnAttitude |
                         CannotBrake | PowerUnavailable)) != 0 ||
        c.safeManeuverDurationSeconds <= 0.0;
    const double eps = 1.0e-9;
    if (baseMotionImpossible ||
        out.poweredSecondsAvailable + eps <
            c.safeManeuverDurationSeconds ||
        out.thermalSecondsAvailable + eps <
            c.safeManeuverDurationSeconds)
    {
        if (out.poweredSecondsAvailable + eps <
            c.safeManeuverDurationSeconds)
            out.blockers |= PowerUnavailable;
        if (out.thermalSecondsAvailable + eps <
            c.safeManeuverDurationSeconds)
            out.blockers |= ThermalRunawayBeforeManeuver;
        out.mobility = MobilityClass::Inoperable;
        return out;
    }

    out.survivalManeuverCertified = true;
    out.candidateForSimpleReturnRoute = true; // route itself still needs proof!

    if (!c.fullAttitudeAuthority)
        out.blockers |= ReducedAttitudeAuthority;
    if (!c.controlledCourseAuthority)
        out.blockers |= ReducedCourseAuthority;
    if (!c.factoryPerformanceEnvelope)
        out.blockers |= OutsideFactoryEnvelope;

    if (out.continuousPowerSustainable &&
        out.continuousThermalSustainable &&
        c.fullAttitudeAuthority && c.controlledCourseAuthority)
    {
        out.mobility = c.factoryPerformanceEnvelope
            ? MobilityClass::Factory
            : MobilityClass::Limited;
    }
    else
        out.mobility = MobilityClass::Emergency;

    return out;
}

} // namespace game::ship::capability
