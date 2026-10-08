#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/RouteFrameField.h"
#include "src/game/navigation/TwoPointRollGeometry.h"

namespace game::navigation::autopilot
{

// VERIFIED WORKING CONTRACT — LIVE TESTED 2026-10-08.
//
// This function owns the complete hull-to-tunnel roll relationship.
//
// DO NOT MODIFY THIS MECHANIC AS PART OF UNRELATED WORK ON:
//   * route following;
//   * pitch/yaw steering;
//   * speed/braking;
//   * capture guidance;
//   * planner geometry;
//   * rendering.
//
// The mechanic is intentionally based on TWO POINTS for both objects:
//   hull   = center + radial "bottom" point;
//   tunnel = center + radial "bottom" point.
//
// The visible tunnel's live roll phase and phase rate are the ONLY dynamic
// roll reference. The hull must follow that same reference; do not reconstruct
// another dock angle, integrate a second roll phase, or substitute Euler
// angles. Change this code only for a concrete reproducing roll-sync failure
// covered by a regression test.
class HullTunnelRollGuidance final
{
public:
    struct Request
    {
        glm::dvec3 hullCenter {0.0};
        glm::dvec3 hullForward {0.0, 0.0, -1.0};
        glm::dvec3 hullUp {0.0, 1.0, 0.0};

        glm::dvec3 tunnelCenter {0.0};
        glm::dvec3 tunnelForward {0.0, 0.0, -1.0};
        glm::dvec3 canonicalTunnelUp {0.0, 1.0, 0.0};

        double liveTunnelRollPhaseRad = 0.0;
        double liveTunnelRollRateRadPerSec = 0.0;

        double rollResponseSeconds = 1.0;
        double maxRollRateRadPerSec = 0.0;
    };

    struct Result
    {
        bool valid = false;
        glm::dvec3 desiredTunnelUp {0.0, 1.0, 0.0};
        double signedRollErrorRad = 0.0;
        double desiredRollRateRadPerSec = 0.0;
    };

    [[nodiscard]] static Result evaluate(
        const Request& request
    ) noexcept
    {
        Result out;

        if (!finiteVec(request.hullCenter) ||
            !finiteVec(request.hullForward) ||
            !finiteVec(request.hullUp) ||
            !finiteVec(request.tunnelCenter) ||
            !finiteVec(request.tunnelForward) ||
            !finiteVec(request.canonicalTunnelUp) ||
            !std::isfinite(request.liveTunnelRollPhaseRad) ||
            !std::isfinite(request.liveTunnelRollRateRadPerSec) ||
            !std::isfinite(request.rollResponseSeconds) ||
            !std::isfinite(request.maxRollRateRadPerSec))
        {
            return out;
        }

        const glm::dvec3 hullForward =
            normalizedOr(
                request.hullForward,
                glm::dvec3(0.0, 0.0, -1.0)
            );
        const glm::dvec3 tunnelForward =
            normalizedOr(
                request.tunnelForward,
                hullForward
            );

        out.desiredTunnelUp =
            RouteFrameField::rotateUpAroundForward(
                tunnelForward,
                request.canonicalTunnelUp,
                request.liveTunnelRollPhaseRad
            );

        const auto hullReference =
            TwoPointRollGeometry::fromCenterAndUp(
                request.hullCenter,
                request.hullUp
            );
        const auto tunnelReference =
            TwoPointRollGeometry::fromCenterAndUp(
                request.tunnelCenter,
                out.desiredTunnelUp
            );

        out.signedRollErrorRad =
            TwoPointRollGeometry::signedPhase(
                hullForward,
                hullReference,
                tunnelReference
            );

        const double maxRollRate =
            std::max(0.0, request.maxRollRateRadPerSec);
        const double responseSeconds =
            std::max(1.0e-9, request.rollResponseSeconds);

        out.desiredRollRateRadPerSec =
            maxRollRate > 1.0e-9
                ? std::clamp(
                    request.liveTunnelRollRateRadPerSec +
                        out.signedRollErrorRad / responseSeconds,
                    -maxRollRate,
                    maxRollRate
                  )
                : 0.0;

        out.valid =
            finiteVec(out.desiredTunnelUp) &&
            std::isfinite(out.signedRollErrorRad) &&
            std::isfinite(out.desiredRollRateRadPerSec);

        return out;
    }

private:
    [[nodiscard]] static bool finiteVec(
        const glm::dvec3& value
    ) noexcept
    {
        return
            std::isfinite(value.x) &&
            std::isfinite(value.y) &&
            std::isfinite(value.z);
    }

    [[nodiscard]] static glm::dvec3 normalizedOr(
        const glm::dvec3& value,
        const glm::dvec3& fallback
    ) noexcept
    {
        const double length = glm::length(value);
        if (std::isfinite(length) && length > 1.0e-12)
            return value / length;

        const double fallbackLength = glm::length(fallback);
        return
            std::isfinite(fallbackLength) &&
            fallbackLength > 1.0e-12
                ? fallback / fallbackLength
                : glm::dvec3(0.0, 0.0, -1.0);
    }
};

} // namespace game::navigation::autopilot
