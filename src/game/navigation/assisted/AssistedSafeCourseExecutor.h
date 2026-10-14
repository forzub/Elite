#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/AutopilotControlApi.h"

namespace game::navigation::assisted
{

// Assisted DOCKING ENTRY safe-course executor.
//
// HARD CONTRACT -- DO NOT turn this back into a trajectory follower.
//
// The docking-entry sequence is intentionally simple:
//   1. remove commanded linear acceleration;
//   2. prove the current ballistic course or select ONE collision-free course;
//   3. turn the VELOCITY VECTOR toward that selected course;
//   4. once acquired, remove linear acceleration again and coast.
//
// This module does not own route geometry, ETA, replanning, planner progress,
// position tracking, or a time-indexed recovery arc.  A deviation from an
// imaginary circle is therefore NEVER a reason to choose another direction.
//
// The ordinary Assisted route autopilot remains a separate system.  This
// helper merely builds the small, bounded course command used before route
// ownership begins.
class AssistedSafeCourseExecutor final
{
public:
    struct Input
    {
        glm::dvec3 actualVelocityMapMps {0.0};
        glm::dvec3 selectedCourseMap {0.0, 0.0, -1.0};

        glm::dvec3 forwardMap {0.0, 0.0, -1.0};
        glm::dvec3 rightMap {1.0, 0.0, 0.0};
        glm::dvec3 upMap {0.0, 1.0, 0.0};

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        // Speed at which the safe direction was selected.  Assisted may lose
        // a little energy during the turn, but this prevents the entry helper
        // from deliberately commanding a slowdown.
        double selectedSpeedMps = 0.0;

        double deltaSeconds = 0.0;
    };

    struct Output
    {
        bool valid = false;
        bool acquired = false;
        double courseErrorRad = 0.0;
        autopilot::AutopilotControlRequest request;
    };

    [[nodiscard]] static Output evaluate(const Input& in) noexcept
    {
        Output out;

        const double actualSpeed = glm::length(in.actualVelocityMapMps);
        const double selectedLength = glm::length(in.selectedCourseMap);
        if (!std::isfinite(actualSpeed) ||
            !std::isfinite(selectedLength) ||
            selectedLength <= 1.0e-9)
        {
            return out;
        }

        const glm::dvec3 target =
            in.selectedCourseMap / selectedLength;

        glm::dvec3 current = target;
        if (actualSpeed > 0.5)
            current = in.actualVelocityMapMps / actualSpeed;

        const double cosine =
            std::clamp(glm::dot(current, target), -1.0, 1.0);
        out.courseErrorRad = std::acos(cosine);

        // Course acquisition tolerance, not route geometry.  This exists only
        // to decide when the pre-route turn is finished.
        constexpr double AcquireErrorRad =
            2.0 * 3.14159265358979323846 / 180.0;

        out.valid = true;
        out.acquired = out.courseErrorRad <= AcquireErrorRad;
        if (out.acquired)
            return out;

        // Do not point the hull straight at a distant safe direction in one
        // control sample.  Assisted FA-on would then classify most of the
        // existing inertial velocity as lateral slip and destroy its energy.
        // Instead keep a small course lead and let the real Assisted law bend
        // the velocity vector progressively.
        constexpr double MaxLeadRad =
            8.0 * 3.14159265358979323846 / 180.0;
        const double step = std::min(out.courseErrorRad, MaxLeadRad);

        glm::dvec3 axis = glm::cross(current, target);
        double axisLength = glm::length(axis);
        if (axisLength <= 1.0e-9)
        {
            const glm::dvec3 basis =
                std::abs(current.y) < 0.9
                    ? glm::dvec3(0.0, 1.0, 0.0)
                    : glm::dvec3(1.0, 0.0, 0.0);
            axis = glm::cross(current, basis);
            axisLength = glm::length(axis);
        }
        if (axisLength <= 1.0e-9)
            return {};

        axis /= axisLength;

        // Rodrigues rotation: advance the requested course only by MaxLeadRad.
        const glm::dvec3 intermediate =
            current * std::cos(step) +
            glm::cross(axis, current) * std::sin(step) +
            axis * glm::dot(axis, current) * (1.0 - std::cos(step));

        const double requestedSpeed =
            in.selectedSpeedMps > 0.5
                ? in.selectedSpeedMps
                : actualSpeed;

        auto& request = out.request;
        request.desiredVelocityMapMps =
            glm::normalize(intermediate) * requestedSpeed;
        request.desiredLinearAccelerationMapMps2 = glm::dvec3(0.0);
        request.desiredForwardMap = glm::normalize(intermediate);
        request.desiredUpMap = in.upMap;
        request.desiredCourseAngularRateMapRadPerSec = glm::dvec3(0.0);
        request.desiredRollRateRadPerSec = 0.0;

        request.actualVelocityMapMps = in.actualVelocityMapMps;
        request.forwardMap = in.forwardMap;
        request.rightMap = in.rightMap;
        request.upMap = in.upMap;
        request.pitchRateRadPerSec = in.pitchRateRadPerSec;
        request.yawRateRadPerSec = in.yawRateRadPerSec;
        request.rollRateRadPerSec = in.rollRateRadPerSec;
        request.stopRequested = false;
        request.terminalAttitudeHold = false;
        request.deltaSeconds = std::max(1.0e-4, in.deltaSeconds);

        return out;
    }
};

} // namespace game::navigation::assisted
