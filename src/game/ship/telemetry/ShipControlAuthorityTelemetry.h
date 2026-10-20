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
// Server-computed motor mixing basis. The same twelve signed motor
// programs are used by the ACTUAL installed Assisted executor. Navigation
// may inspect concurrent acceleration+torque demands without synthesizing
// a fictitious independent lateral engine.
struct AssistedCoupledMotorAuthority
{
    static constexpr std::size_t MaxMotors = 16;
    static constexpr std::size_t ForceComponents = 3 * MaxMotors;
    static constexpr std::size_t SignedAxes = 12;

    bool valid = false;
    std::uint8_t motorCount = 0;
    std::array<float, MaxMotors> available01 {};
    std::array<double, SignedAxes> achievable {};
    std::array<std::array<float, ForceComponents>, SignedAxes> fractions {};
};

struct ShipControlAuthorityTelemetry
{
    bool valid = false;
    game::ship::plant::PhysicalControlModelId model =
        game::ship::plant::PhysicalControlModelId::AssistedI;
    std::uint64_t sourceActuatorRevision = 0;
    std::array<double,6> positive {};
    std::array<double,6> negative {};
    AssistedCoupledMotorAuthority coupled {};
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
    out.coupled.valid = true;
    out.coupled.motorCount =
        static_cast<std::uint8_t>(motorPrograms.count);
    for (std::size_t motor=0;motor<motorPrograms.count;++motor)
        out.coupled.available01[motor] =
            static_cast<float>(
                std::clamp(motorPrograms.motors[motor].capacity01,0.0,1.0));
    for (std::size_t signedAxis=0;
         signedAxis<out.coupled.SignedAxes;++signedAxis)
    {
        const auto& preset=motorPrograms.programs[signedAxis];
        out.coupled.achievable[signedAxis]=preset.achievable;
        for (std::size_t component=0;
             component<3*motorPrograms.count;++component)
        {
            out.coupled.fractions[signedAxis][component] =
                static_cast<float>(preset.forceFractions[component]);
        }
    }
    for(std::size_t axis=0;axis<6;++axis)
    {
        out.positive[axis]=std::max(
            0.0,motorPrograms.programs[2*axis].achievable
        );
        out.negative[axis]=std::max(
            0.0,motorPrograms.programs[2*axis+1].achievable
        );
    }
    return out;
}

[[nodiscard]] inline ShipParams applyAuthorityToControlParams(
    ShipParams p, const ShipControlAuthorityTelemetry& telemetry
) noexcept
{
    if(!telemetry.valid ||
       game::ship::plant::familyOf(telemetry.model)!=
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
    const double forward = std::min({
        structuralLoadMps2, forwardRating,
        std::max(0.0, telemetry.negative[2])
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
