#pragma once

#include <algorithm>
#include <cmath>

#include <glm/glm.hpp>

namespace game::navigation::autopilot
{

// Gentle in-corridor centerline guidance.
//
// This is NOT CourseCaptureGuidance and must not become an emergency recovery
// controller.  It only removes persistent parallel offset while the craft is
// still inside the accepted tracking corridor.
//
// Contract:
// - exact tangent inside the centering deadband;
// - continuous correction outside the deadband;
// - correction magnitude rises smoothly with corridor occupancy;
// - desired lateral closing speed is bounded by physical lateral authority;
// - caller retains CourseCaptureGuidance for actual corridor escape.
class InCorridorCenteringGuidance final
{
public:
    struct Request
    {
        glm::dvec3 routeTangentMap {0.0, 0.0, -1.0};
        glm::dvec3 crossTrackErrorMapMeters {0.0};
        double actualSpeedMps = 0.0;
        double deadbandMeters = 0.0;
        double corridorToleranceMeters = 0.0;
        double lateralAccelerationAuthorityMps2 = 0.0;
        double responseSeconds = 0.0;
    };

    struct Result
    {
        bool valid = false;
        glm::dvec3 desiredCourseMap {0.0, 0.0, -1.0};
        double correctionAngleRad = 0.0;
        double desiredClosingSpeedMps = 0.0;
        double occupancy01 = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& in) noexcept
    {
        Result out;

        const double tangentLength = glm::length(in.routeTangentMap);
        const double errorMeters = glm::length(in.crossTrackErrorMapMeters);
        if (!std::isfinite(tangentLength) ||
            tangentLength <= 1.0e-12 ||
            !std::isfinite(errorMeters) ||
            !std::isfinite(in.actualSpeedMps) ||
            in.actualSpeedMps < 0.0 ||
            !std::isfinite(in.deadbandMeters) ||
            !std::isfinite(in.corridorToleranceMeters) ||
            !std::isfinite(in.lateralAccelerationAuthorityMps2) ||
            !std::isfinite(in.responseSeconds))
        {
            return out;
        }

        const glm::dvec3 tangent = in.routeTangentMap / tangentLength;
        out.desiredCourseMap = tangent;
        out.valid = true;

        const double tolerance =
            std::max(in.corridorToleranceMeters, 1.0e-6);
        const double deadband =
            std::clamp(in.deadbandMeters, 0.0, tolerance);

        if (errorMeters <= deadband + 1.0e-9 ||
            in.actualSpeedMps <= 0.5)
        {
            return out;
        }

        const double span = std::max(1.0e-6, tolerance - deadband);
        const double rawOccupancy =
            std::clamp((errorMeters - deadband) / span, 0.0, 1.0);
        out.occupancy01 =
            rawOccupancy * rawOccupancy * (3.0 - 2.0 * rawOccupancy);

        const double response =
            std::max(0.25, in.responseSeconds);

        // Aim to remove only the excess outside the deadband.  Physical
        // lateral authority caps how fast that capture may happen.
        const double proportionalClosingSpeed =
            (errorMeters - deadband) / response;
        const double authorityClosingSpeed =
            std::max(0.0, in.lateralAccelerationAuthorityMps2) *
            response;

        out.desiredClosingSpeedMps =
            std::min(proportionalClosingSpeed, authorityClosingSpeed) *
            out.occupancy01;

        if (out.desiredClosingSpeedMps <= 1.0e-9)
            return out;

        const glm::dvec3 toCenter =
            -in.crossTrackErrorMapMeters / errorMeters;

        const glm::dvec3 desiredVelocityDirection =
            tangent * in.actualSpeedMps +
            toCenter * out.desiredClosingSpeedMps;

        const double desiredLength =
            glm::length(desiredVelocityDirection);
        if (!std::isfinite(desiredLength) ||
            desiredLength <= 1.0e-12)
        {
            out.valid = false;
            return out;
        }

        out.desiredCourseMap =
            desiredVelocityDirection / desiredLength;

        out.correctionAngleRad =
            std::acos(
                std::clamp(
                    glm::dot(tangent, out.desiredCourseMap),
                    -1.0,
                    1.0
                )
            );

        return out;
    }
};

} // namespace game::navigation::autopilot
