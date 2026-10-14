#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include "src/game/navigation/autopilot/InCorridorCenteringGuidance.h"

using game::navigation::autopilot::InCorridorCenteringGuidance;

namespace
{

constexpr double Pi = 3.14159265358979323846;

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

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
        require(out.valid, "centering result invalid");
        require(std::abs(out.desiredClosingSpeedMps) < 1.0e-12, "deadband produced closing speed");
        require(angleBetween(out.desiredCourseMap, r.routeTangentMap) < 1.0e-12, "deadband altered tangent");
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
        require(out.valid, "centering result invalid");
        require(out.desiredClosingSpeedMps > 0.0, "centering produced no closing speed");
        require(out.desiredClosingSpeedMps <= 20.0 + 1.0e-9, "centering exceeded lateral authority");

        // Positive Y error must produce a negative-Y course component.
        require(out.desiredCourseMap.y < 0.0, "centering did not point toward center");

        // Gentle means single-digit degrees, not an emergency capture turn.
        require(out.correctionAngleRad > 0.0, "centering produced zero correction angle");
        require(out.correctionAngleRad < 6.0 * Pi / 180.0, "centering correction became emergency-sized");
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

        require(nearOut.valid && farOut.valid, "near/far centering result invalid");
        require(
            nearOut.desiredClosingSpeedMps <
                farOut.desiredClosingSpeedMps,
            "centering strength did not increase with offset"
        );
        require(
            nearOut.correctionAngleRad <
                farOut.correctionAngleRad,
            "centering angle did not increase with offset"
        );
    }

    {
        // Regression from live Assisted stress telemetry: with ~64 m corridor
        // tolerance and ~18 m residual offset, centering must still have
        // meaningful authority. The old double-smoothstep attenuation reduced
        // this to a fraction of a metre per second and visually stranded the
        // craft around 20 m from centerline.
        InCorridorCenteringGuidance::Request r;
        r.routeTangentMap = {0.0, 0.0, -1.0};
        r.crossTrackErrorMapMeters = {0.0, 18.0, 0.0};
        r.actualSpeedMps = 350.0;
        r.deadbandMeters = 6.4;
        r.corridorToleranceMeters = 64.0;
        r.lateralAccelerationAuthorityMps2 = 20.0;
        r.responseSeconds = 2.0;

        const auto out = InCorridorCenteringGuidance::evaluate(r);
        require(out.valid, "centering result invalid");
        require(out.desiredClosingSpeedMps > 5.0, "18 m residual centering authority too weak");
        require(out.desiredCourseMap.y < 0.0, "centering did not point toward center");
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

        require(!InCorridorCenteringGuidance::evaluate(r).valid, "zero tangent was accepted");
    }

    std::cout << "IN-CORRIDOR CENTERING GUIDANCE: PASS\n";
    std::cout << " - zero correction inside deadband\n";
    std::cout << " - correction points toward center\n";
    std::cout << " - correction is smooth and physically bounded\n";
    return 0;
}
