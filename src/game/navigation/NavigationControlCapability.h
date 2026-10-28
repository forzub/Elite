#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <glm/glm.hpp>

#include "src/game/navigation/LocalFlightControlLaw.h"
#include "src/game/ship/core/ShipDynamics.h"
#include "src/game/ship/core/ShipParams.h"

namespace game::navigation
{

// One read-only FACTUAL view of the ship's control capability for
// navigation (route request, profile, manoeuvre compiler, pilot).
//
// It owns no physics: it only re-presents the authoritative capability
// already published in ShipParams (the transitional adapter of the server's
// ShipCapabilitySnapshot + installed control-authority telemetry). Every
// navigation consumer reads THESE numbers instead of converting ShipParams
// itself, so planner, compiler and pilot see one truth.
//
// No policy here: shares such as "plan with 85 % of main", "turn at 40 % of
// the installed rate" or feedback reserves are applied by the consumer.
// Zero means zero: a missing actuator is never replaced by a small invented
// authority; consumers must refuse or wait instead.
struct NavigationControlCapability
{
    bool valid = false;
    LocalFlightControlLaw law = defaultLocalFlightControlLaw();
    std::uint64_t sourceRevision = 0;

    // Main engine along the working thrust axis (body; healthy -Z).
    bool mainAvailable = false;
    double mainForwardMps2 = 0.0;
    double mainReverseMps2 = 0.0;
    glm::dvec3 workingThrustAxisBody {0.0, 0.0, -1.0};
    double gimbalPitchRad = 0.0;
    double gimbalYawRad = 0.0;
    // Side force from nozzle vectoring alone (main * sin(gimbal)).
    double mainVectoringLateralMps2 = 0.0;

    // Signed translational RCS, body X right, Y up, Z aft (nose = -Z).
    // positive.z = toward the stern, negative.z = toward the nose.
    glm::dvec3 rcsPositiveMps2 {0.0};
    glm::dvec3 rcsNegativeMps2 {0.0};
    bool rcsPerAxisKnown = false;
    // Weakest translational direction (the scalar bound every direction
    // can deliver).
    double rcsScalarMps2 = 0.0;

    // Angular authority: signed per axis (x = pitch, y = yaw, z = roll)
    // where the installed telemetry provides it, else the symmetric scalar.
    glm::dvec3 angularPositiveRadPerSec2 {0.0};
    glm::dvec3 angularNegativeRadPerSec2 {0.0};
    double angularScalarRadPerSec2 = 0.0;
    glm::dvec3 maxAngularRateRadPerSec {0.0};
    double maxAngularSpeedRadPerSec = 0.0;

    double controlledSpeedLimitMps = 0.0;

    // Translational RCS along one body direction: axis 0 = right, 1 = up,
    // 2 = forward (nose, body -Z); `sign` selects the direction (same
    // convention as the pilot's ShipControlState forward/strafe/lift).
    [[nodiscard]] double rcsDirectionMps2(int axis, double sign) const noexcept
    {
        if (!rcsPerAxisKnown)
            return rcsScalarMps2;
        switch (axis)
        {
            case 0: return sign >= 0.0 ? rcsPositiveMps2.x : rcsNegativeMps2.x;
            case 1: return sign >= 0.0 ? rcsPositiveMps2.y : rcsNegativeMps2.y;
            default: return sign >= 0.0 ? rcsNegativeMps2.z : rcsPositiveMps2.z;
        }
    }

    // Translation is possible at all (main or RCS) and the hull can turn.
    [[nodiscard]] bool canManoeuvre() const noexcept
    {
        return valid && (mainForwardMps2 > 0.0 || rcsScalarMps2 > 0.0) &&
            angularScalarRadPerSec2 > 0.0 && maxAngularSpeedRadPerSec > 0.0;
    }
};

[[nodiscard]] inline NavigationControlCapability makeNavigationControlCapability(
    const ShipParams& params,
    LocalFlightControlLaw law,
    std::uint64_t sourceRevision = 0
) noexcept
{
    const auto nonNegative = [](double v) noexcept
    {
        return std::isfinite(v) ? std::max(0.0, v) : 0.0;
    };
    NavigationControlCapability out;
    out.law = law;
    out.sourceRevision = sourceRevision;

    out.mainForwardMps2 =
        nonNegative(game::ship::forwardMainAccelerationLimitMps2(params));
    out.mainReverseMps2 =
        nonNegative(game::ship::reverseMainAccelerationLimitMps2(params));
    out.mainAvailable = out.mainForwardMps2 > 0.0;
    if (params.assistedWorkingThrustAxisEmergency &&
        glm::length(params.assistedWorkingThrustAxisBody) > 1.0e-9)
        out.workingThrustAxisBody =
            glm::normalize(params.assistedWorkingThrustAxisBody);
    out.gimbalPitchRad = nonNegative(game::ship::mainGimbalPitchLimitRad(params));
    out.gimbalYawRad = nonNegative(game::ship::mainGimbalYawLimitRad(params));
    out.mainVectoringLateralMps2 = nonNegative(
        game::ship::newtonianMainVectoringLateralAccelerationLimitMps2(params));

    out.rcsPerAxisKnown = game::ship::newtonianRcsPerAxisKnown(params);
    out.rcsScalarMps2 =
        nonNegative(game::ship::manoeuvreAccelerationLimitMps2(params));
    if (out.rcsPerAxisKnown)
    {
        out.rcsPositiveMps2 = glm::max(params.newtonianRcsPositiveMps2, glm::dvec3(0.0));
        out.rcsNegativeMps2 = glm::max(params.newtonianRcsNegativeMps2, glm::dvec3(0.0));
    }
    else
    {
        out.rcsPositiveMps2 = glm::dvec3(out.rcsScalarMps2);
        out.rcsNegativeMps2 = glm::dvec3(out.rcsScalarMps2);
    }

    out.angularScalarRadPerSec2 =
        nonNegative(game::ship::angularAccelerationLimitRadPerSec2(params));
    const glm::dvec3 signedPositive = params.newtonianAngularPositiveRadPerSec2;
    const glm::dvec3 signedNegative = params.newtonianAngularNegativeRadPerSec2;
    if (signedPositive.x + signedPositive.y + signedPositive.z +
            signedNegative.x + signedNegative.y + signedNegative.z > 1.0e-9)
    {
        out.angularPositiveRadPerSec2 = glm::max(signedPositive, glm::dvec3(0.0));
        out.angularNegativeRadPerSec2 = glm::max(signedNegative, glm::dvec3(0.0));
    }
    else
    {
        out.angularPositiveRadPerSec2 = glm::dvec3(out.angularScalarRadPerSec2);
        out.angularNegativeRadPerSec2 = glm::dvec3(out.angularScalarRadPerSec2);
    }
    out.maxAngularRateRadPerSec = {
        nonNegative(game::ship::pitchRateLimitRadPerSec(params)),
        nonNegative(game::ship::yawRateLimitRadPerSec(params)),
        nonNegative(game::ship::rollRateLimitRadPerSec(params))};
    out.maxAngularSpeedRadPerSec =
        nonNegative(game::ship::maximumAngularSpeedRadPerSec(params));
    out.controlledSpeedLimitMps =
        nonNegative(game::ship::controlledSpeedLimitMps(params));
    out.valid = true;
    return out;
}

} // namespace game::navigation
