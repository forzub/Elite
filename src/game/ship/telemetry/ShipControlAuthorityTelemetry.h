#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/plant/ShipPresetEngineManeuvers.h"

namespace game::ship::telemetry
{
// Read-only PHYSICAL control-authority telemetry, separate from BITE failure
// diagnostics and from actual-red-thrust HUD samples. The autopilot sees the
// same precomputed six-axis limits that the Assisted motor executor uses.
// Basis entries are sign-specific, and include active cooling/impact damage.
struct ShipControlAuthorityTelemetry
{
    bool valid = false;
    game::ship::plant::PhysicalControlModelId model =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    std::uint64_t sourceActuatorRevision = 0;
    std::array<double,6> positive {};
    std::array<double,6> negative {};
    // Usable physical acceleration along the selected CoM-compatible
    // working-forward thrust ray: body -Z on a healthy symmetric ship,
    // or a geometrically selected off-nose ray when damaged. This is what
    // flight planning consumes. negative[2] separately measures pure
    // hull -Z motor-program authority and can correctly be zero.
    double workingForwardAccelerationMps2 = 0.0;
    // Indices: body X (strafe), body Y (lift), body Z (reverse positive),
    // angular pitch, yaw, roll. Magnitudes always nonnegative.
};

[[nodiscard]] inline ShipControlAuthorityTelemetry
readAssistedControlAuthority(
    const game::ship::plant::AssistedManeuverPrograms& motorPrograms,
    std::uint64_t actuatorRevision
) noexcept
{
    ShipControlAuthorityTelemetry out;
    if(!motorPrograms.valid) return out;
    out.valid=true;
    out.model=motorPrograms.model;
    out.sourceActuatorRevision=actuatorRevision;
    for(std::size_t axis=0;axis<6;++axis)
    {
        out.positive[axis]=std::max(
            0.0,motorPrograms.programs[2*axis].achievable
        );
        out.negative[axis]=std::max(
            0.0,motorPrograms.programs[2*axis+1].achievable
        );
    }
    // A centered healthy bank's physical working ray is hull -Z.
    // For damage-offset operation the surviving CoM-aligned motor below
    // supplies the physical working acceleration instead. In BOTH cases
    // callers consume this one value, never select pure -Z by a mode flag.
    if(!motorPrograms.shape.assistedWorkingThrustAxisEmergency)
        out.workingForwardAccelerationMps2=out.negative[2];
    else
    {
        const auto& slots=game::ship::plant::physicalControlSlots(
            motorPrograms.model
        );
        for(std::size_t i=0;i<motorPrograms.count;++i)
            if(slots[i].role==game::ship::plant::EngineSlotRole::AftMain &&
                static_cast<int>(i) ==
                    motorPrograms.shape.assistedWorkingThrustMotorIndex)
            {
                const auto& m=motorPrograms.motors[i];
                out.workingForwardAccelerationMps2+=
                    m.capacity01*m.ratedN/
                    std::max(1.0,motorPrograms.shape.massKg);
            }
    }
    return out;
}

// Newtonian scalar control budget from signed installed RCS authority.
// Server capability and client telemetry consumers both use THIS bound, so
// the controller normalizes against what the server actually executes.
inline void applyNewtonianRcsScalarBounds(
    ShipParams& p,
    const std::array<double,6>& positive,
    const std::array<double,6>& negative
) noexcept
{
    const double linearMin=std::min({
        positive[0],positive[1],positive[2],
        negative[0],negative[1],negative[2]
    });
    const double angularMin=std::min({
        positive[3],positive[4],positive[5],
        negative[3],negative[4],negative[5]
    });
    p.manoeuvreThrusterAuthorityExplicit=true;
    // The signed per-axis values travel alongside the scalar so a consumer
    // that normalizes per axis (ShipControlState::
    // rcsTranslationPerAxisAuthority) can use the strong directions.
    p.newtonianRcsPositiveMps2={
        std::max(0.0,positive[0]),std::max(0.0,positive[1]),
        std::max(0.0,positive[2])};
    p.newtonianRcsNegativeMps2={
        std::max(0.0,negative[0]),std::max(0.0,negative[1]),
        std::max(0.0,negative[2])};
    p.newtonianAngularPositiveRadPerSec2={
        std::max(0.0,positive[3]),std::max(0.0,positive[4]),
        std::max(0.0,positive[5])};
    p.newtonianAngularNegativeRadPerSec2={
        std::max(0.0,negative[3]),std::max(0.0,negative[4]),
        std::max(0.0,negative[5])};
    p.manoeuvreThrusterAccel=static_cast<float>(
        std::min(std::max(0.0,
            static_cast<double>(p.manoeuvreThrusterAccel)),
            std::max(0.0,linearMin)));
    p.angularAccel=static_cast<float>(
        std::min(std::max(0.0,
            static_cast<double>(p.angularAccel)),
            std::max(0.0,angularMin)));
}

[[nodiscard]] inline ShipParams applyAuthorityToControlParams(
    ShipParams p, const ShipControlAuthorityTelemetry& telemetry
) noexcept
{
    if(!telemetry.valid)
        return p;
    if(game::ship::plant::familyOf(telemetry.model)==
           game::ship::plant::PhysicalControlFamily::Newtonian)
    {
        applyNewtonianRcsScalarBounds(
            p,telemetry.positive,telemetry.negative);
        return p;
    }
    if(game::ship::plant::familyOf(telemetry.model)!=
           game::ship::plant::PhysicalControlFamily::Assisted)
        return p;
    // Conservative plan/controller limits: NO value may exceed physical
    // accepted motor-basis capability. Existing ShipParams API is kept as
    // an adapter until navigation directly takes the telemetry structure.
    // ShipDynamics reserves zero directional rating for legacy descriptors
    // ("fall back to the load envelope"). ZERO FROM TELEMETRY is different:
    // it is physical loss of the motor bank and must never trigger that
    // legacy fallback. Explicitly clear availability when the motor cannot
    // generate thrust; use the current load envelope only as the old-format
    // descriptor's rating before applying the real six-axis bound.
    const double structuralLoadMps2 =
        std::max(0.0, static_cast<double>(
            p.maxLinearGs > 0.0f ? p.maxLinearGs : p.maxGs
        )) * 9.80665;
    const double forwardRating =
        p.forwardMainEngineAccelerationMps2 > 0.0f
            ? static_cast<double>(p.forwardMainEngineAccelerationMps2)
            : structuralLoadMps2;
    const double reverseRating =
        p.reverseMainEngineAccelerationMps2 > 0.0f
            ? static_cast<double>(p.reverseMainEngineAccelerationMps2)
            : structuralLoadMps2;
    // Always plan along the installed, torque-neutral working thrust ray.
    // negative[2] is only the separate pure-body -Z diagnostic basis.
    const double forward = std::min({
        structuralLoadMps2, forwardRating,
        std::max(0.0, telemetry.workingForwardAccelerationMps2)
    });
    const double reverse = std::min({
        structuralLoadMps2, reverseRating,
        std::max(0.0, telemetry.positive[2])
    });
    p.forwardMainEngineAvailable =
        p.forwardMainEngineAvailable && forward > 1.0e-6;
    p.reverseMainEngineAvailable =
        p.reverseMainEngineAvailable && reverse > 1.0e-6;
    p.forwardMainEngineAccelerationMps2 = static_cast<float>(forward);
    p.reverseMainEngineAccelerationMps2 = static_cast<float>(reverse);
    p.strafeAccel=static_cast<float>(std::min({
        std::max(0.0,static_cast<double>(p.strafeAccel)),
        telemetry.positive[0],telemetry.negative[0],
        telemetry.positive[1],telemetry.negative[1]
    }));
    p.angularAccel=static_cast<float>(std::min({
        std::max(0.0,static_cast<double>(p.angularAccel)),
        telemetry.positive[3],telemetry.negative[3],
        telemetry.positive[4],telemetry.negative[4],
        telemetry.positive[5],telemetry.negative[5]
    }));
    return p;
}

// A committed trajectory must be revalidated against loss of installed
// physical capability. This checks signed axes (including damaged asymmetric
// turns), not just the overall hull health or a descriptor maximum.
// A modest tolerance avoids invalidating plans on thermal snapshot jitter.
// The caller decides whether to stop safely or request a new route.
[[nodiscard]] inline bool hasSignificantAssistedAuthorityLoss(
    const ShipControlAuthorityTelemetry& accepted,
    const ShipControlAuthorityTelemetry& live,
    double allowedFractionalLoss = 0.05
) noexcept
{
    if (!accepted.valid)
        return false;
    if (!live.valid || accepted.model != live.model)
        return true;
    const double fraction = std::clamp(
        allowedFractionalLoss, 0.0, 1.0
    );
    if (accepted.workingForwardAccelerationMps2 > 1.0e-6 &&
        live.workingForwardAccelerationMps2 <
            accepted.workingForwardAccelerationMps2 * (1.0-fraction))
        return true;
    for (std::size_t i = 0; i < accepted.positive.size(); ++i)
    {
        if (accepted.positive[i] > 1.0e-6 &&
            live.positive[i] < accepted.positive[i] * (1.0 - fraction))
            return true;
        if (accepted.negative[i] > 1.0e-6 &&
            live.negative[i] < accepted.negative[i] * (1.0 - fraction))
            return true;
    }
    return false;
}

} // namespace game::ship::telemetry
