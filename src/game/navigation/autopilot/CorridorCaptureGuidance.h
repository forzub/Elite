#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Pure corridor-position geometry.
//
// Responsibility:
//   current position + current centerline frame + authored look-ahead point
//       -> temporary forward direction required to return to centerline.
//
// It does not know hull attitude dynamics, speed envelopes, engines, throttle,
// angular authority or ShipControlState.
class CorridorCaptureGuidance final
{
public:
    struct Request
    {
        glm::dvec3 positionMapMeters {0.0};
        glm::dvec3 currentRoutePointMapMeters {0.0};
        glm::dvec3 currentRouteTangentMap {0.0, 0.0, -1.0};
        glm::dvec3 lookAheadPointMapMeters {0.0};
        double centeringDeadbandMeters = 0.0;
    };

    struct Result
    {
        bool valid = false;
        bool captureActive = false;

        glm::dvec3 desiredForwardMap {0.0, 0.0, -1.0};

        double crossTrackErrorMeters = 0.0;
        double lookAheadOffTangentMeters = 0.0;
        double captureAngleRad = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;

        const glm::dvec3 tangent =
            normalizedOr(
                request.currentRouteTangentMap,
                {0.0, 0.0, -1.0}
            );

        const glm::dvec3 positionError =
            request.positionMapMeters -
            request.currentRoutePointMapMeters;
        const glm::dvec3 crossError =
            positionError -
            tangent * glm::dot(positionError, tangent);
        out.crossTrackErrorMeters = glm::length(crossError);

        const glm::dvec3 steeringRay =
            request.lookAheadPointMapMeters -
            request.positionMapMeters;
        const double steeringDistance = glm::length(steeringRay);

        if (!(std::isfinite(steeringDistance) &&
              steeringDistance > 1.0e-9) ||
            !std::isfinite(out.crossTrackErrorMeters))
        {
            return out;
        }

        const glm::dvec3 rayDirection = steeringRay / steeringDistance;
        const glm::dvec3 offTangent =
            steeringRay -
            tangent * glm::dot(steeringRay, tangent);
        out.lookAheadOffTangentMeters = glm::length(offTangent);

        const double deadband =
            std::max(0.0, request.centeringDeadbandMeters);

        // Route curvature is not a corridor error. A future point on any
        // real arc/Bezier segment is naturally off the CURRENT tangent, so
        // using lookAheadOffTangentMeters to activate capture makes a centered
        // craft steer along a chord and cut the authored curve.
        //
        // Capture exists only to recover actual cross-track displacement.
        // While centered, Assisted flight follows the exact local tangent.
        out.captureActive =
            out.crossTrackErrorMeters > deadband;

        out.desiredForwardMap =
            out.captureActive ? rayDirection : tangent;

        out.captureAngleRad =
            angleBetween(tangent, out.desiredForwardMap);

        out.valid =
            finiteVec(out.desiredForwardMap) &&
            std::isfinite(out.lookAheadOffTangentMeters) &&
            std::isfinite(out.captureAngleRad);

        return out;
    }

private:
    [[nodiscard]] static bool finiteVec(const glm::dvec3& v) noexcept
    {
        return
            std::isfinite(v.x) &&
            std::isfinite(v.y) &&
            std::isfinite(v.z);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;
        return fallback;
    }

    [[nodiscard]] static double angleBetween(
        const glm::dvec3& a,
        const glm::dvec3& b
    ) noexcept
    {
        return std::acos(
            std::clamp(
                glm::dot(
                    normalizedOr(a, {0.0, 0.0, -1.0}),
                    normalizedOr(b, {0.0, 0.0, -1.0})
                ),
                -1.0,
                1.0
            )
        );
    }
};

} // namespace game::navigation::autopilot
