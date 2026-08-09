#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "src/game/simulation/activation/SpatialBounds.h"

namespace game::simulation::activation
{

struct KinematicPoint
{
    std::array<double, 3> positionMeters {0.0, 0.0, 0.0};
    std::array<double, 3> velocityMetersPerSecond {0.0, 0.0, 0.0};
    SpatialBounds bounds {};
};

struct InteractionHorizonPolicy
{
    // How far into the future the server asks whether two entities can affect
    // one another. This is intentionally independent of radar/sensor range.
    double lookAheadSeconds = 5.0;

    // Broad-phase safety padding around the two conservative object bounds.
    double safetyMarginMeters = 25.0;

    // Optional gameplay effect reach (weapon/effect/etc.). Zero means purely
    // physical contact. Sensor visibility must not be encoded here.
    double gameplayRangeMeters = 0.0;
};

struct InteractionPrediction
{
    double currentCenterDistanceMeters = 0.0;
    double currentSurfaceDistanceMeters = 0.0;

    double timeToClosestSeconds = 0.0;
    double closestCenterDistanceMeters = 0.0;
    double closestSurfaceDistanceMeters = 0.0;

    double interactionEnvelopeMeters = 0.0;

    bool currentlyWithinEnvelope = false;
    bool entersEnvelopeWithinHorizon = false;
    bool closingAtSampleTime = false;
};

namespace detail
{
inline std::array<double, 3> subtract(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b
) noexcept
{
    return {
        a[0] - b[0],
        a[1] - b[1],
        a[2] - b[2]
    };
}

inline std::array<double, 3> addScaled(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b,
    double scale
) noexcept
{
    return {
        a[0] + b[0] * scale,
        a[1] + b[1] * scale,
        a[2] + b[2] * scale
    };
}

inline double dot(
    const std::array<double, 3>& a,
    const std::array<double, 3>& b
) noexcept
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

inline double length(const std::array<double, 3>& v) noexcept
{
    return std::sqrt(std::max(0.0, dot(v, v)));
}
} // namespace detail

inline InteractionPrediction evaluateInteractionHorizon(
    const KinematicPoint& a,
    const KinematicPoint& b,
    const InteractionHorizonPolicy& policy
) noexcept
{
    const auto relativePosition = detail::subtract(
        b.positionMeters,
        a.positionMeters
    );

    const auto relativeVelocity = detail::subtract(
        b.velocityMetersPerSecond,
        a.velocityMetersPerSecond
    );

    const double currentCenterDistance =
        detail::length(relativePosition);

    const double combinedPhysicalRadius =
        std::max(0.0, a.bounds.interactionRadiusMeters) +
        std::max(0.0, b.bounds.interactionRadiusMeters);

    const double envelope =
        combinedPhysicalRadius +
        std::max(0.0, policy.safetyMarginMeters) +
        std::max(0.0, policy.gameplayRangeMeters);

    const double relativeSpeedSquared =
        detail::dot(relativeVelocity, relativeVelocity);

    const double radialVelocityDot =
        detail::dot(relativePosition, relativeVelocity);

    double timeToClosest = 0.0;
    if (relativeSpeedSquared > 1e-12)
    {
        timeToClosest =
            -radialVelocityDot / relativeSpeedSquared;

        timeToClosest = std::clamp(
            timeToClosest,
            0.0,
            std::max(0.0, policy.lookAheadSeconds)
        );
    }

    const auto closestDelta = detail::addScaled(
        relativePosition,
        relativeVelocity,
        timeToClosest
    );

    const double closestCenterDistance =
        detail::length(closestDelta);

    InteractionPrediction result;
    result.currentCenterDistanceMeters = currentCenterDistance;
    result.currentSurfaceDistanceMeters = std::max(
        0.0,
        currentCenterDistance - combinedPhysicalRadius
    );
    result.timeToClosestSeconds = timeToClosest;
    result.closestCenterDistanceMeters = closestCenterDistance;
    result.closestSurfaceDistanceMeters = std::max(
        0.0,
        closestCenterDistance - combinedPhysicalRadius
    );
    result.interactionEnvelopeMeters = envelope;
    result.currentlyWithinEnvelope =
        currentCenterDistance <= envelope;
    result.entersEnvelopeWithinHorizon =
        closestCenterDistance <= envelope;
    result.closingAtSampleTime = radialVelocityDot < 0.0;

    return result;
}

} // namespace game::simulation::activation
