#pragma once

// MotherComputer's synchronous flight-strategy selector.
//
// IMPORTANT: this is a policy over physically attainable references, not a
// motor allocation, not a model/family switch and NOT a flight certificate.
// The deterministic physics/capability provider may prove a safe axis and
// selected motor; the computer decides how to present/use it. The server
// remains free to reject any physically infeasible command.

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/telemetry/ShipControlAuthorityTelemetry.h"
#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::computer
{
enum class PropulsionDoctrine
{
    AwaitingPhysicalEvidence,
    NominalNoseAxis,
    DamageOffsetAxis,
    BallisticFlipBurnCandidate,
    NoSafeAxialThrust
};

struct FlightControlProfile
{
    bool valid = false;
    PropulsionDoctrine doctrine = PropulsionDoctrine::AwaitingPhysicalEvidence;
    glm::dvec3 workingForwardBody {0.0,0.0,-1.0};
    int preferredDriveMotorIndex = -1;
    double forwardAccelerationMps2 = 0.0;
    double reverseAccelerationMps2 = 0.0;
    // Candidate status never proves completion of a 180-degree maneuver,
    // stopping spin, thermal endurance or power-budget sustainment.
    bool mobilityCertified = false;
};

[[nodiscard]] inline FlightControlProfile selectFlightDoctrine(
    const ShipParams& physics,
    game::ship::plant::PhysicalControlModelId model,
    const game::ship::telemetry::ShipControlAuthorityTelemetry& authority,
    bool geometryAvailable
) noexcept
{
    FlightControlProfile out;
    if(!geometryAvailable) return out;
    const bool assisted = game::ship::plant::familyOf(model) ==
        game::ship::plant::PhysicalControlFamily::Assisted;

    // Fail closed for Assisted if the real installed actuator matrix is
    // missing. Newtonian does not yet have a matching telemetry provider:
    // policy can observe its plant envelope, but cannot certify mobility.
    if(assisted && (!authority.valid || authority.model!=model))
        return out;

    const ShipParams p = assisted
        ? game::ship::telemetry::applyAuthorityToControlParams(
              physics,authority)
        : physics;

    out.valid=true;
    out.forwardAccelerationMps2 =
        p.forwardMainEngineAvailable
            ? std::max(0.0,
                static_cast<double>(
                    p.forwardMainEngineAccelerationMps2))
            : 0.0;
    out.reverseAccelerationMps2 =
        p.reverseMainEngineAvailable
            ? std::max(0.0,
                static_cast<double>(
                    p.reverseMainEngineAccelerationMps2))
            : 0.0;
    if(out.forwardAccelerationMps2<=1.0e-6)
    {
        out.doctrine=PropulsionDoctrine::NoSafeAxialThrust;
        return out;
    }

    if(p.assistedWorkingThrustAxisEmergency)
    {
        if(p.assistedWorkingThrustMotorIndex<0)
        {
            // Misconfigured candidate: fail rather than use wrong motor.
            out.doctrine=PropulsionDoctrine::NoSafeAxialThrust;
            out.forwardAccelerationMps2=0.0;
            return out;
        }
        out.doctrine=PropulsionDoctrine::DamageOffsetAxis;
        out.workingForwardBody=p.assistedWorkingThrustAxisBody;
        out.preferredDriveMotorIndex=p.assistedWorkingThrustMotorIndex;
    }
    else if(assisted && out.reverseAccelerationMps2<=1.0e-6)
        out.doctrine=PropulsionDoctrine::BallisticFlipBurnCandidate;
    else
        out.doctrine=PropulsionDoctrine::NominalNoseAxis;

    return out;
}
}
