#include "src/game/navigation/autopilot/CorridorCaptureGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
using Guidance =
    game::navigation::autopilot::CorridorCaptureGuidance;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testCenteredStraightKeepsTangent()
{
    Guidance::Request request;
    request.positionMapMeters = {0.0, 0.0, 0.0};
    request.currentRoutePointMapMeters = {0.0, 0.0, 0.0};
    request.currentRouteTangentMap = {1.0, 0.0, 0.0};
    request.lookAheadPointMapMeters = {100.0, 0.0, 0.0};
    request.centeringDeadbandMeters = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "centered straight invalid");
    require(!out.captureActive,
        "centered straight incorrectly activated capture");
    require(std::abs(out.desiredForwardMap.x - 1.0) < 1.0e-12,
        "centered straight changed tangent");
    require(std::abs(out.captureAngleRad) < 1.0e-12,
        "centered straight produced capture angle");
}

void testCenteredCurveKeepsExactTangent()
{
    Guidance::Request request;
    request.positionMapMeters = {0.0, 0.0, 0.0};
    request.currentRoutePointMapMeters = {0.0, 0.0, 0.0};
    request.currentRouteTangentMap = {1.0, 0.0, 0.0};

    // A future point on a real arc is necessarily off the current tangent.
    // Being centered on the authored curve must still mean: fly the tangent,
    // not the chord to that future point.
    request.lookAheadPointMapMeters = {40.0, 0.0, 20.0};
    request.centeringDeadbandMeters = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "centered curve invalid");
    require(!out.captureActive,
        "centered curve incorrectly activated chord capture");
    require(std::abs(out.desiredForwardMap.x - 1.0) < 1.0e-12 &&
            std::abs(out.desiredForwardMap.y) < 1.0e-12 &&
            std::abs(out.desiredForwardMap.z) < 1.0e-12,
        "centered curve did not keep exact authored tangent");
    require(std::abs(out.captureAngleRad) < 1.0e-12,
        "centered curve produced a chord steering angle");
}

void testParallelOffsetCreatesReturnDirection()
{
    Guidance::Request request;
    request.positionMapMeters = {0.0, 0.0, 20.0};
    request.currentRoutePointMapMeters = {0.0, 0.0, 0.0};
    request.currentRouteTangentMap = {1.0, 0.0, 0.0};
    request.lookAheadPointMapMeters = {100.0, 0.0, 0.0};
    request.centeringDeadbandMeters = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "offset capture invalid");
    require(out.captureActive,
        "parallel offset did not activate capture");
    require(out.crossTrackErrorMeters > 19.9,
        "parallel offset cross-track wrong");
    require(out.desiredForwardMap.z < -0.1,
        "parallel offset did not steer toward centerline");
    require(out.captureAngleRad > 0.1,
        "parallel offset capture angle too small");
}

void testDeadbandDoesNotHunt()
{
    Guidance::Request request;
    request.positionMapMeters = {0.0, 0.0, 0.5};
    request.currentRoutePointMapMeters = {0.0, 0.0, 0.0};
    request.currentRouteTangentMap = {1.0, 0.0, 0.0};
    request.lookAheadPointMapMeters = {100.0, 0.0, 0.0};
    request.centeringDeadbandMeters = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "deadband case invalid");
    require(!out.captureActive,
        "sub-deadband error activated capture");
    require(std::abs(out.captureAngleRad) < 1.0e-12,
        "sub-deadband error changed heading");
}

}

int main()
{
    try
    {
        testCenteredStraightKeepsTangent();
        testCenteredCurveKeepsExactTangent();
        testParallelOffsetCreatesReturnDirection();
        testDeadbandDoesNotHunt();

        std::cout
            << "CORRIDOR CAPTURE GUIDANCE TESTS: PASS\n"
            << " - centered straight keeps route tangent\n"
            << " - centered curve keeps exact tangent, never a chord\n"
            << " - parallel offset produces return heading\n"
            << " - deadband prevents centerline hunting\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "CORRIDOR CAPTURE GUIDANCE TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
