#pragma once

#include <algorithm>
#include <cmath>

namespace render::celestial
{

struct CloudMotionPolicyInput
{
    double minimumWindSpeedMps = 0.0;
    double maximumWindSpeedMps = 0.0;
    double predominantDirectionDeg = 0.0;
    double planetRadiusMeters = 1.0;
    double baseHeightKm = 0.0;

    double authoredVisualUvPerSecond = 0.0;
    double referenceMeanWindMps = 0.0;
    double physicalTimeScale = 1.0;
    double debugSpeedMultiplier = 1.0;
};

struct CloudMotionPolicyResult
{
    double meanWindSpeedMps = 0.0;
    double physicalLongitudeUvPerSecond = 0.0;
    double baseVisualUvPerSecond = 0.0;
    double layerWindRatio = 1.0;
    double driftUvPerSecond = 0.0;
    double morphologySpeedMultiplier = 1.0;
};

inline CloudMotionPolicyResult resolveCloudMotionPolicy(
    const CloudMotionPolicyInput& input
)
{
    constexpr double Pi = 3.141592653589793238462643383279502884;

    CloudMotionPolicyResult result;

    result.meanWindSpeedMps =
        (input.minimumWindSpeedMps + input.maximumWindSpeedMps) * 0.5;

    const double directionRadians =
        input.predominantDirectionDeg * Pi / 180.0;

    const double longitudinalWindMps =
        result.meanWindSpeedMps * std::sin(directionRadians);

    const double cloudRadiusMeters =
        std::max(
            1.0,
            input.planetRadiusMeters + input.baseHeightKm * 1000.0
        );

    result.physicalLongitudeUvPerSecond =
        longitudinalWindMps /
        (2.0 * Pi * cloudRadiusMeters);

    result.baseVisualUvPerSecond =
        std::abs(input.authoredVisualUvPerSecond) > 1.0e-12
            ? input.authoredVisualUvPerSecond
            : result.physicalLongitudeUvPerSecond * input.physicalTimeScale;

    const double referenceWindMps =
        std::max(0.001, input.referenceMeanWindMps);

    result.layerWindRatio =
        std::clamp(
            result.meanWindSpeedMps / referenceWindMps,
            0.25,
            4.0
        );

    const double debugSpeed =
        std::clamp(
            input.debugSpeedMultiplier,
            0.0,
            100000.0
        );

    result.driftUvPerSecond =
        result.baseVisualUvPerSecond *
        result.layerWindRatio *
        debugSpeed;

    result.morphologySpeedMultiplier =
        debugSpeed <= 0.0
            ? 0.0
            : std::clamp(
                std::sqrt(debugSpeed),
                0.25,
                2.0
            );

    return result;
}

} // namespace render::celestial
