#include "src/game/navigation/autopilot/VelocityCourseGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Guidance =
    game::navigation::autopilot::VelocityCourseGuidance;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testVelocityDirectionOwnsCourse()
{
    Guidance::Request request;
    request.actualVelocityMapMps = {0.0, 0.0, 50.0};
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};
    request.targetCourseMap = {0.0, 0.0, 1.0};

    const auto result = Guidance::evaluate(request);
    require(result.valid, "guidance invalid");
    require(
        result.usingVelocityDirection,
        "moving craft did not use velocity direction"
    );
    require(
        result.courseErrorRad < 1.0e-9,
        "hull nose incorrectly contaminated route course error"
    );
    require(
        std::abs(result.pitchYawErrorLocalRad.x) < 1.0e-9 &&
        std::abs(result.pitchYawErrorLocalRad.y) < 1.0e-9,
        "zero course error emitted pitch/yaw correction"
    );
}

void testCourseErrorIgnoresHullAlignment()
{
    Guidance::Request request;
    request.actualVelocityMapMps = {50.0, 0.0, 0.0};
    request.currentForward = {0.0, 0.0, 1.0};
    request.currentRight = {1.0, 0.0, 0.0};
    request.currentUp = {0.0, 1.0, 0.0};
    request.targetCourseMap = {0.0, 0.0, 1.0};

    const auto result = Guidance::evaluate(request);
    require(result.valid, "guidance invalid");
    require(
        std::abs(
            result.courseErrorRad -
            1.57079632679489661923
        ) < 1.0e-9,
        "course error did not follow velocity vector"
    );
}

void testStoppedFallbackIsDeterministic()
{
    Guidance::Request request;
    request.actualVelocityMapMps = {0.0, 0.0, 0.0};
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};
    request.targetCourseMap = {1.0, 0.0, 0.0};

    const auto result = Guidance::evaluate(request);
    require(result.valid, "stopped fallback invalid");
    require(
        !result.usingVelocityDirection,
        "zero velocity claimed to have a course direction"
    );
    require(
        result.courseErrorRad < 1.0e-9,
        "stopped fallback did not use hull forward"
    );
}

}

int main()
{
    try
    {
        testVelocityDirectionOwnsCourse();
        testCourseErrorIgnoresHullAlignment();
        testStoppedFallbackIsDeterministic();
        std::cout
            << "VELOCITY COURSE GUIDANCE TESTS: PASS\n"
            << " - moving route course is velocity-vector owned\n"
            << " - hull nose does not define course error\n"
            << " - zero-speed fallback is deterministic\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "VELOCITY COURSE GUIDANCE TESTS: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
