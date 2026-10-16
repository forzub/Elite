#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

#include "src/game/ship/plant/ShipPhysicalControlModel.h"

namespace game::ship::plant
{

// Simplified actuator-to-motion bridge.
//
// Each row describes how one physical actuator contributes to the aggregate
// movement axes used by the accepted motion backend.  This is intentionally a
// cinematic/control authority model, not an engine or gas-dynamics solver.
//
// Healthy rows are normalized per axis, so a completely healthy construction
// reproduces the pre-matrix movement envelope exactly. Runtime damage only
// removes or reduces contributions; it can never create extra authority.
struct AuthorityContributionRow
{
    // Contributions that depend only on available thrust.
    KinematicAuthorityEstimate thrust;

    // Contributions that require main-nozzle vectoring to be operational.
    KinematicAuthorityEstimate vectoring;

    // Extra trajectory/attitude authority that exists only while a main bank
    // is producing thrust. Newtonian TVC lives here rather than pretending to
    // be independent RCS authority.
    PoweredMainVectoringEstimate poweredMainVectoring;
};

struct AuthorityMatrixResult
{
    bool valid = false;
    KinematicAuthorityEstimate authority;
    PoweredMainVectoringEstimate poweredMainVectoring;
};

namespace detail
{

struct AxisAccumulator
{
    double positiveNumerator = 0.0;
    double positiveDenominator = 0.0;
    double negativeNumerator = 0.0;
    double negativeDenominator = 0.0;
};

inline void accumulateAxis(
    AxisAccumulator& out,
    const AxisAuthority01& weight,
    double availability
) noexcept
{
    const double positive = std::max(0.0, weight.positive);
    const double negative = std::max(0.0, weight.negative);
    const double a = std::clamp(availability, 0.0, 1.0);

    out.positiveNumerator += positive * a;
    out.positiveDenominator += positive;
    out.negativeNumerator += negative * a;
    out.negativeDenominator += negative;
}

inline AxisAuthority01 normalizedAxis(
    const AxisAccumulator& value
) noexcept
{
    return {
        value.positiveDenominator > 1.0e-12
            ? std::clamp(
                  value.positiveNumerator / value.positiveDenominator,
                  0.0,
                  1.0
              )
            : 0.0,
        value.negativeDenominator > 1.0e-12
            ? std::clamp(
                  value.negativeNumerator / value.negativeDenominator,
                  0.0,
                  1.0
              )
            : 0.0
    };
}

struct KinematicAccumulator
{
    AxisAccumulator longitudinal;
    AxisAccumulator strafe;
    AxisAccumulator lift;
    AxisAccumulator pitch;
    AxisAccumulator yaw;
    AxisAccumulator roll;
};

inline void accumulateKinematic(
    KinematicAccumulator& out,
    const KinematicAuthorityEstimate& weight,
    double availability
) noexcept
{
    accumulateAxis(out.longitudinal, weight.longitudinal, availability);
    accumulateAxis(out.strafe, weight.strafe, availability);
    accumulateAxis(out.lift, weight.lift, availability);
    accumulateAxis(out.pitch, weight.pitch, availability);
    accumulateAxis(out.yaw, weight.yaw, availability);
    accumulateAxis(out.roll, weight.roll, availability);
}

inline KinematicAuthorityEstimate normalizedKinematic(
    const KinematicAccumulator& value
) noexcept
{
    KinematicAuthorityEstimate out;
    out.longitudinal = normalizedAxis(value.longitudinal);
    out.strafe = normalizedAxis(value.strafe);
    out.lift = normalizedAxis(value.lift);
    out.pitch = normalizedAxis(value.pitch);
    out.yaw = normalizedAxis(value.yaw);
    out.roll = normalizedAxis(value.roll);
    return out;
}

struct PoweredAccumulator
{
    double trajectoryStrafeNumerator = 0.0;
    double trajectoryStrafeDenominator = 0.0;
    double trajectoryLiftNumerator = 0.0;
    double trajectoryLiftDenominator = 0.0;
    double pitchNumerator = 0.0;
    double pitchDenominator = 0.0;
    double yawNumerator = 0.0;
    double yawDenominator = 0.0;
    double rollNumerator = 0.0;
    double rollDenominator = 0.0;
};

inline void accumulatePowered(
    PoweredAccumulator& out,
    const PoweredMainVectoringEstimate& weight,
    double availability
) noexcept
{
    const double a = std::clamp(availability, 0.0, 1.0);

    auto add = [a](
        double weightValue,
        double& numerator,
        double& denominator
    )
    {
        const double w = std::max(0.0, weightValue);
        numerator += w * a;
        denominator += w;
    };

    add(
        weight.trajectoryStrafe01,
        out.trajectoryStrafeNumerator,
        out.trajectoryStrafeDenominator
    );
    add(
        weight.trajectoryLift01,
        out.trajectoryLiftNumerator,
        out.trajectoryLiftDenominator
    );
    add(weight.pitch01, out.pitchNumerator, out.pitchDenominator);
    add(weight.yaw01, out.yawNumerator, out.yawDenominator);
    add(weight.roll01, out.rollNumerator, out.rollDenominator);
}

inline double normalizedPowered(
    double numerator,
    double denominator
) noexcept
{
    return denominator > 1.0e-12
        ? std::clamp(numerator / denominator, 0.0, 1.0)
        : 0.0;
}

} // namespace detail

[[nodiscard]] inline AuthorityMatrixResult evaluateAuthorityContributionMatrix(
    const std::vector<AuthorityContributionRow>& rows,
    const std::vector<EngineSlotState>& state
) noexcept
{
    AuthorityMatrixResult out;
    if (rows.size() != state.size())
        return out;

    detail::KinematicAccumulator kinematic;
    detail::PoweredAccumulator powered;

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        const double thrust = effectiveSlotThrust01(state[i]);
        const double vectoring = effectiveSlotVectoring01(state[i]);

        detail::accumulateKinematic(
            kinematic,
            rows[i].thrust,
            thrust
        );
        detail::accumulateKinematic(
            kinematic,
            rows[i].vectoring,
            vectoring
        );
        detail::accumulatePowered(
            powered,
            rows[i].poweredMainVectoring,
            vectoring
        );
    }

    out.authority = detail::normalizedKinematic(kinematic);
    out.poweredMainVectoring = {
        detail::normalizedPowered(
            powered.trajectoryStrafeNumerator,
            powered.trajectoryStrafeDenominator
        ),
        detail::normalizedPowered(
            powered.trajectoryLiftNumerator,
            powered.trajectoryLiftDenominator
        ),
        detail::normalizedPowered(
            powered.pitchNumerator,
            powered.pitchDenominator
        ),
        detail::normalizedPowered(
            powered.yawNumerator,
            powered.yawDenominator
        ),
        detail::normalizedPowered(
            powered.rollNumerator,
            powered.rollDenominator
        )
    };

    out.valid = true;
    return out;
}

[[nodiscard]] inline double weakestAxisAuthority(
    const AxisAuthority01& axis
) noexcept
{
    return std::min(axis.positive, axis.negative);
}

} // namespace game::ship::plant
