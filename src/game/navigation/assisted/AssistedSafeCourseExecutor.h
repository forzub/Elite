#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/ship/core/ShipControlState.h"

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
        // The installed drive's physical working axis in ship BODY space.
        // Healthy Assisted is the ordinary {-Z} nose-axis special case.
        glm::dvec3 workingThrustAxisBody {0.0,0.0,-1.0};

        double pitchRateRadPerSec = 0.0;
        double yawRateRadPerSec = 0.0;
        double rollRateRadPerSec = 0.0;

        // Speed at which the safe direction was selected.  Assisted may lose
        // a little energy during the turn, but this prevents the entry helper
        // from deliberately commanding a slowdown.
        double selectedSpeedMps = 0.0;

        double maxPitchRateRadPerSec = 0.0;
        double maxYawRateRadPerSec = 0.0;
        double maxRollRateRadPerSec = 0.0;
        double angularAccelerationAuthorityRadPerSec2 = 0.0;
        double deltaSeconds = 0.0;
    };

    struct Output
    {
        bool valid = false;
        bool acquired = false;
        double courseErrorRad = 0.0;
        double hullToVelocityErrorRad = 0.0;
        ShipControlState control;
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

        const glm::dvec3 nose =
            normalizedOr(in.forwardMap,current);
        const glm::dvec3 right =
            normalizedOr(in.rightMap,{1.0,0.0,0.0});
        const glm::dvec3 up =
            normalizedOr(in.upMap,{0.0,1.0,0.0});
        const double bodyLength=glm::length(in.workingThrustAxisBody);
        if(!std::isfinite(bodyLength) || bodyLength<=1.0e-9)
            return {}; // Missing physical axis cannot become healthy nose.
        const glm::dvec3 bodyAxis=in.workingThrustAxisBody/bodyLength;
        const glm::dvec3 workingRay=
            right*bodyAxis.x+up*bodyAxis.y-nose*bodyAxis.z;
        const double workingLength=glm::length(workingRay);
        if(!std::isfinite(workingLength) || workingLength<=1.0e-9)
            return {};
        const glm::dvec3 workingForward=workingRay/workingLength;
        // For normal Assisted this is exactly the hull/velocity error;
        // after damage it reports the TRUE thrust-axis/velocity error.
        out.hullToVelocityErrorRad = std::acos(
            std::clamp(glm::dot(workingForward,current),-1.0,1.0)
        );

        // Both requirements are physical, not visual: velocity follows the
        // selected safe course, and the REAL attainable propulsion direction
        // (nose only for an intact craft) follows the velocity. Never demand
        // that an engine-out hull nose point along the offset thrust ray.
        constexpr double AcquireErrorRad =
            2.0 * 3.14159265358979323846 / 180.0;
        constexpr double AcquireAngularRateRadPerSec = 0.01;
        const double angularSpeedRadPerSec =
            std::sqrt(
                in.pitchRateRadPerSec * in.pitchRateRadPerSec +
                in.yawRateRadPerSec * in.yawRateRadPerSec +
                in.rollRateRadPerSec * in.rollRateRadPerSec
            );

        out.valid = true;
        out.acquired =
            out.courseErrorRad <= AcquireErrorRad &&
            out.hullToVelocityErrorRad <= AcquireErrorRad &&
            angularSpeedRadPerSec <= AcquireAngularRateRadPerSec;

        if (out.acquired)
            return out;

        // Manual Assisted semantics:
        //
        //   pilot turns HULL -> ordinary FA-on bends VELOCITY toward hull.
        //
        // The safe-course executor therefore emits ordinary pitch/yaw/roll
        // inputs only. It must NEVER feed VelocityCourseGuidance or a direct
        // navigation acceleration path. Those are route-execution semantics
        // and create a second controller fighting the physical Assisted law.
        constexpr double MaxLeadRad =
            8.0 * 3.14159265358979323846 / 180.0;
        const double step = std::min(out.courseErrorRad, MaxLeadRad);

        glm::dvec3 intermediate = target;
        if (out.courseErrorRad > 1.0e-9)
        {
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
            intermediate =
                current * std::cos(step) +
                glm::cross(axis, current) * std::sin(step) +
                axis * glm::dot(axis, current) *
                    (1.0 - std::cos(step));
        }

        // ONE steering reference for every Assisted configuration:
        // the installed working thrust direction. In normal condition it
        // is identical to the hull nose. Project the world-space error onto
        // the HULL controls (not an auxiliary axis-aligned coordinate frame).
        const glm::dvec3 targetForward=glm::normalize(intermediate);
        const glm::dvec3 crossAxis=
            glm::cross(workingForward,targetForward);
        const double sine=glm::length(crossAxis);
        const double angle=std::atan2(sine,
            std::clamp(glm::dot(workingForward,targetForward),-1.0,1.0));
        glm::dvec3 correction(0.0);
        if(sine>1.0e-10)
            correction=crossAxis*(angle/sine);
        else if(angle>3.14159265358979323846-1.0e-8)
            correction=up*angle;

        autopilot::HullAttitudeControl::Request attitudeRequest;
        attitudeRequest.pitchYawErrorRad = {
            glm::dot(correction,right),
            glm::dot(correction,up)
        };
        attitudeRequest.rollErrorRad = 0.0;
        attitudeRequest.pitchYawRateRadPerSec = {
            in.pitchRateRadPerSec,
            in.yawRateRadPerSec
        };
        attitudeRequest.desiredPitchYawRateRadPerSec = {0.0, 0.0};
        attitudeRequest.rollRateRadPerSec = in.rollRateRadPerSec;
        attitudeRequest.desiredRollRateRadPerSec = 0.0;
        attitudeRequest.maxPitchYawRateRadPerSec = {
            std::max(0.0, in.maxPitchRateRadPerSec),
            std::max(0.0, in.maxYawRateRadPerSec)
        };
        attitudeRequest.maxRollRateRadPerSec =
            std::max(0.0, in.maxRollRateRadPerSec);
        attitudeRequest.pitchYawAngularAccelerationAuthorityRadPerSec2 =
            std::max(1.0e-9, in.angularAccelerationAuthorityRadPerSec2);
        attitudeRequest.rollAngularAccelerationAuthorityRadPerSec2 =
            std::max(1.0e-9, in.angularAccelerationAuthorityRadPerSec2);
        attitudeRequest.deltaSeconds =
            std::max(1.0e-4, in.deltaSeconds);
        // The ordinary Assisted pilot uses the damped 0.5 s attitude servo.
        // Leaving this field zero selects the legacy dt^-1 bang-bang law:
        // it saturates yaw to +/-1 when a damaged drive delivers less
        // angular acceleration than the nominal ship envelope promises.
        // This is the SAME Assisted control law, not a second controller.
        attitudeRequest.pitchYawResponseSeconds = 0.50;

        const auto attitude =
            autopilot::HullAttitudeControl::evaluate(attitudeRequest);
        if (!attitude.valid)
            return {};

        out.control.pitchInput =
            static_cast<float>(attitude.pitchYawInput.x);
        out.control.yawInput =
            static_cast<float>(attitude.pitchYawInput.y);
        out.control.rollInput =
            static_cast<float>(attitude.rollInput);

        // No throttle/translation command. Translation remains on the exact
        // ordinary Assisted manual path, so its physical FA-on controller is
        // the ONLY system allowed to pull velocity toward the rotated hull.
        out.control.targetSpeedRate = 0.0f;
        return out;
    }

private:
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

} // namespace game::navigation::assisted
