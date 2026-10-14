#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>

#include "src/game/navigation/autopilot/InCorridorCenteringGuidance.h"

using game::navigation::autopilot::InCorridorCenteringGuidance;

namespace
{

constexpr double Pi = 3.14159265358979323846;

double angleBetween(const glm::dvec3& a, const glm::dvec3& b)
{
    return std::acos(
        std::clamp(
            glm::dot(glm::normalize(a), glm::normalize(b)),
            -1.0,
            1.0
        )
    );
}

} // namespace

int main()
{
    {
        InCorridorCenteringGuidance::Request r;
        r.routeTangentMap = {0.0, 0.0, -1.0};
        r.crossTrackErrorMapMeters = {0.0, 5.0, 0.0};
        r.actualSpeedMps = 220.0;
        r.deadbandMeters = 6.4;
        r.corridorToleranceMeters = 64.0;
        r.lateralAccelerationAuthorityMps2 = 20.0;
        r.responseSeconds = 1.0;

        const auto out = InCorridorCenteringGuidance::evaluate(r);
        assert(out.valid);
        assert(std::abs(out.desiredClosingSpeedMps) < 1.0e-12);
        assert(angleBetween(out.desiredCourseMap, r.routeTangentMap) < 1.0e-12);
    }

    {
        InCorridorCenteringGuidance::Request r;
        r.routeTangentMap = {0.0, 0.0, -1.0};
        r.crossTrackErrorMapMeters = {0.0, 52.0, 0.0};
        r.actualSpeedMps = 220.0;
        r.deadbandMeters = 6.4;
        r.corridorToleranceMeters = 64.0;
        r.lateralAccelerationAuthorityMps2 = 20.0;
        r.responseSeconds = 1.0;

        const auto out = InCorridorCenteringGuidance::evaluate(r);
        assert(out.valid);
        assert(out.desiredClosingSpeedMps > 0.0);
        assert(out.desiredClosingSpeedMps <= 20.0 + 1.0e-9);

        // Positive Y error must produce a negative-Y course component.
        assert(out.desiredCourseMap.y < 0.0);

        // Gentle means single-digit degrees, not an emergency capture turn.
        assert(out.correctionAngleRad > 0.0);
        assert(out.correctionAngleRad < 6.0 * Pi / 180.0);
    }

    {
        InCorridorCenteringGuidance::Request near;
        near.routeTangentMap = {0.0, 0.0, -1.0};
        near.crossTrackErrorMapMeters = {0.0, 16.0, 0.0};
        near.actualSpeedMps = 220.0;
        near.deadbandMeters = 6.4;
        near.corridorToleranceMeters = 64.0;
        near.lateralAccelerationAuthorityMps2 = 20.0;
        near.responseSeconds = 1.0;

        auto far = near;
        far.crossTrackErrorMapMeters = {0.0, 52.0, 0.0};

        const auto nearOut =
            InCorridorCenteringGuidance::evaluate(near);
        const auto farOut =
            InCorridorCenteringGuidance::evaluate(far);

        assert(nearOut.valid && farOut.valid);
        assert(
            nearOut.desiredClosingSpeedMps <
            farOut.desiredClosingSpeedMps
        );
        assert(
            nearOut.correctionAngleRad <
            farOut.correctionAngleRad
        );
    }

    {
        InCorridorCenteringGuidance::Request r;
        r.routeTangentMap = {0.0, 0.0, 0.0};
        r.crossTrackErrorMapMeters = {0.0, 52.0, 0.0};
        r.actualSpeedMps = 220.0;
        r.deadbandMeters = 6.4;
        r.corridorToleranceMeters = 64.0;
        r.lateralAccelerationAuthorityMps2 = 20.0;
        r.responseSeconds = 1.0;

        assert(!InCorridorCenteringGuidance::evaluate(r).valid);
    }

    std::cout << "IN-CORRIDOR CENTERING GUIDANCE: PASS\n";
    std::cout << " - zero correction inside deadband\n";
    std::cout << " - correction points toward center\n";
    std::cout << " - correction is smooth and physically bounded\n";
    return 0;
}
