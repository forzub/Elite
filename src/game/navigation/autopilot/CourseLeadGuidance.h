#pragma once

#include <algorithm>
#include <cmath>

namespace game::navigation::autopilot
{

// Pure route-preview horizon policy.
//
// Responsibility:
//   current scalar speed + measured Assisted course response
//       -> bounded distance ahead on the authored route.
//
// It owns no steering, hull attitude, speed envelope or actuator control.
class CourseLeadGuidance final
{
public:
    struct Request
    {
        double actualSpeedMps = 0.0;
        double courseResponseSeconds = 0.0;
    };

    struct Result
    {
        bool valid = false;
        double lookAheadMeters = 0.0;
    };

    [[nodiscard]] static Result evaluate(const Request& request) noexcept
    {
        Result out;
        if (!std::isfinite(request.actualSpeedMps) ||
            !std::isfinite(request.courseResponseSeconds) ||
            request.actualSpeedMps < 0.0 ||
            request.courseResponseSeconds < 0.0)
        {
            return out;
        }

        // Preserve the proven corridor follower's useful preview band.
        // 50 m prevents a low-speed craft from waiting until curve entry.
        // 250 m prevents high-speed look-ahead from becoming a long chord
        // across authored route geometry.
        constexpr double MinimumLookAheadMeters = 50.0;
        constexpr double MaximumLookAheadMeters = 250.0;
        constexpr double MinimumPreviewSeconds = 1.5;

        const double responseSeconds =
            std::max(
                MinimumPreviewSeconds,
                request.courseResponseSeconds
            );

        out.lookAheadMeters =
            std::clamp(
                std::max(
                    MinimumLookAheadMeters,
                    request.actualSpeedMps * responseSeconds
                ),
                MinimumLookAheadMeters,
                MaximumLookAheadMeters
            );
        out.valid = true;
        return out;
    }
};

} // namespace game::navigation::autopilot
