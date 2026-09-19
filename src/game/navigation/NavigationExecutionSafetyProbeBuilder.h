#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include <glm/glm.hpp>

#include "src/world/navigation/local/PhysicalManeuverHorizon.h"

namespace game::navigation
{

// Pure kinematic construction for execution-safety probes.
//
// This class deliberately knows nothing about NavigationSpace, HitVolumes,
// revisions, diagnostics, GameSimulation or authoritative state. It converts
// explicit immutable kinematic inputs into explicit probe geometry. The
// stateful shell is responsible only for asking authoritative geometry whether
// those probe segments are traversable.
class NavigationExecutionSafetyProbeBuilder final
{
public:
    static constexpr std::size_t kExecutedForecastSamples = 12;

    struct StoppingReserveQuery
    {
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        double controlResponseReserveSeconds = 0.0;
        double brakingAccelerationMetersPerSecond2 = 0.0;
    };

    struct StoppingReserveProbe
    {
        bool valid = false;
        bool active = false;
        glm::dvec3 startMapMeters {0.0};
        glm::dvec3 endMapMeters {0.0};
        double responseSeconds = 0.0;
        double distanceMeters = 0.0;
    };

    struct ConstantAccelerationQuery
    {
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 velocityMapMetersPerSecond {0.0};
        glm::dvec3 accelerationMapMetersPerSecond2 {0.0};
        double durationSeconds = 0.0;
        double maximumDistanceMeters = 0.0;
    };

    struct ConstantAccelerationProbe
    {
        bool valid = false;
        bool active = false;
        glm::dvec3 startMapMeters {0.0};
        glm::dvec3 endMapMeters {0.0};
    };

    struct SampledForecast
    {
        bool valid = false;
        bool active = false;
        std::array<
            glm::dvec3,
            kExecutedForecastSamples + 1
        > pointsMapMeters {};
        std::size_t pointCount = 0;
    };

    [[nodiscard]] static StoppingReserveProbe buildStoppingReserve(
        const StoppingReserveQuery& query
    ) noexcept
    {
        StoppingReserveProbe result;
        result.startMapMeters = query.positionMapMeters;
        result.endMapMeters = query.positionMapMeters;

        if (!finite(query.positionMapMeters) ||
            !finite(query.velocityMapMetersPerSecond) ||
            !finiteNonNegative(query.controlResponseReserveSeconds) ||
            !std::isfinite(query.brakingAccelerationMetersPerSecond2) ||
            query.brakingAccelerationMetersPerSecond2 <= 0.0)
        {
            return result;
        }

        const double speed =
            glm::length(query.velocityMapMetersPerSecond);
        if (!std::isfinite(speed))
            return result;

        result.valid = true;
        if (speed <= kEpsilon)
            return result;

        world::navigation::PhysicalManeuverHorizon::Query horizonQuery;
        horizonQuery.speedMetersPerSecond = speed;
        horizonQuery.accelerationMagnitudeMetersPerSecond2 = 0.0;
        horizonQuery.snapshotAgeSeconds = 0.0;
        horizonQuery.controlResponseReserveSeconds =
            query.controlResponseReserveSeconds;
        horizonQuery.brakingAccelerationMetersPerSecond2 =
            query.brakingAccelerationMetersPerSecond2;
        horizonQuery.turnDistanceMeters = 0.0;
        horizonQuery.safetyMarginMeters = 0.0;
        horizonQuery.minimumDistanceMeters = 0.0;
        horizonQuery.minimumLookAheadSeconds = 0.0;

        const auto horizon =
            world::navigation::PhysicalManeuverHorizon::evaluate(
                horizonQuery
            );
        if (!horizon.valid)
        {
            result.valid = false;
            return result;
        }

        result.responseSeconds = horizon.responseSeconds;
        result.distanceMeters =
            horizon.responseDistanceMeters +
            horizon.brakingDistanceMeters;

        const glm::dvec3 direction =
            query.velocityMapMetersPerSecond / speed;
        result.endMapMeters =
            query.positionMapMeters +
            direction * result.distanceMeters;
        result.active = result.distanceMeters > kEpsilon;
        return result;
    }

    [[nodiscard]] static ConstantAccelerationProbe
    buildConstantAccelerationProbe(
        const ConstantAccelerationQuery& query
    ) noexcept
    {
        ConstantAccelerationProbe result;
        result.startMapMeters = query.positionMapMeters;
        result.endMapMeters = query.positionMapMeters;

        if (!validConstantAccelerationQuery(query))
            return result;

        result.valid = true;
        if (query.durationSeconds <= kEpsilon)
            return result;

        result.endMapMeters =
            query.positionMapMeters +
            boundedDisplacement(query, query.durationSeconds);
        const glm::dvec3 delta =
            result.endMapMeters - result.startMapMeters;
        result.active =
            glm::dot(delta, delta) > kEpsilon * kEpsilon;
        return result;
    }

    [[nodiscard]] static SampledForecast
    buildSampledConstantAccelerationForecast(
        const ConstantAccelerationQuery& query
    ) noexcept
    {
        SampledForecast result;
        result.pointsMapMeters[0] = query.positionMapMeters;
        result.pointCount = 1;

        if (!validConstantAccelerationQuery(query))
            return result;

        result.valid = true;
        if (query.durationSeconds <= kEpsilon)
            return result;

        for (std::size_t sample = 1;
             sample <= kExecutedForecastSamples;
             ++sample)
        {
            const double t =
                query.durationSeconds *
                static_cast<double>(sample) /
                static_cast<double>(kExecutedForecastSamples);

            result.pointsMapMeters[sample] =
                query.positionMapMeters +
                boundedDisplacement(query, t);
        }

        result.pointCount =
            kExecutedForecastSamples + 1;
        result.active = true;
        return result;
    }

private:
    static constexpr double kEpsilon = 1.0e-6;

    [[nodiscard]] static bool finite(
        const glm::dvec3& value
    ) noexcept
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    [[nodiscard]] static bool finiteNonNegative(
        double value
    ) noexcept
    {
        return std::isfinite(value) && value >= 0.0;
    }

    [[nodiscard]] static bool validConstantAccelerationQuery(
        const ConstantAccelerationQuery& query
    ) noexcept
    {
        return
            finite(query.positionMapMeters) &&
            finite(query.velocityMapMetersPerSecond) &&
            finite(query.accelerationMapMetersPerSecond2) &&
            finiteNonNegative(query.durationSeconds) &&
            finiteNonNegative(query.maximumDistanceMeters);
    }

    [[nodiscard]] static glm::dvec3 boundedDisplacement(
        const ConstantAccelerationQuery& query,
        double timeSeconds
    ) noexcept
    {
        glm::dvec3 displacement =
            query.velocityMapMetersPerSecond * timeSeconds +
            0.5 *
                query.accelerationMapMetersPerSecond2 *
                timeSeconds * timeSeconds;

        const double distance = glm::length(displacement);
        if (query.maximumDistanceMeters > 0.0 &&
            distance > query.maximumDistanceMeters &&
            distance > kEpsilon)
        {
            displacement *=
                query.maximumDistanceMeters / distance;
        }
        return displacement;
    }
};

} // namespace game::navigation
