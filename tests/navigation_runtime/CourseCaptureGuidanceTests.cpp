#include "src/game/navigation/autopilot/CourseCaptureGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using Guidance =
    game::navigation::autopilot::CourseCaptureGuidance;
using Segment =
    game::navigation::planner::RouteCurveSegment;
using Kind =
    game::navigation::planner::RouteCurveKind;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Segment straight(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double s0,
    double s1
)
{
    Segment out;
    out.kind = Kind::Line;
    out.startProgressMeters = s0;
    out.endProgressMeters = s1;
    out.startMeters = a;
    out.endMeters = b;
    out.startForward = glm::normalize(b - a);
    out.endForward = out.startForward;
    return out;
}

void testCenteredKeepsExactTangent()
{
    std::vector<Segment> route {
        straight({0.0,0.0,0.0},{1000.0,0.0,0.0},0.0,1000.0)
    };

    Guidance::Request request;
    request.positionMapMeters = {100.0,0.0,0.5};
    request.actualVelocityMapMps = {50.0,0.0,0.0};
    request.routeCurves = &route;
    request.currentRouteProgressMeters = 100.0;
    request.centeringDeadbandMeters = 2.0;
    request.courseResponseSeconds = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "centered guidance invalid");
    require(!out.captureActive, "centered craft activated capture");
    require(
        std::abs(out.desiredCourseMap.x - 1.0) < 1.0e-12 &&
        std::abs(out.desiredCourseMap.y) < 1.0e-12 &&
        std::abs(out.desiredCourseMap.z) < 1.0e-12,
        "centered craft did not keep exact tangent"
    );
}

void testParallelOffsetBuildsSmoothCapture()
{
    std::vector<Segment> route {
        straight({0.0,0.0,0.0},{3000.0,0.0,0.0},0.0,3000.0)
    };

    Guidance::Request request;
    request.positionMapMeters = {100.0,0.0,20.0};
    request.actualVelocityMapMps = {50.0,0.0,0.0};
    request.routeCurves = &route;
    request.currentRouteProgressMeters = 100.0;
    request.centeringDeadbandMeters = 2.0;
    request.courseResponseSeconds = 2.0;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "parallel capture invalid");
    require(out.captureActive, "parallel offset did not activate capture");
    require(
        out.meetingRouteProgressMeters >
            request.currentRouteProgressMeters,
        "capture meeting point was not ahead"
    );
    require(
        out.desiredCourseMap.z < -1.0e-4,
        "capture course did not turn toward centerline"
    );
    require(
        out.courseErrorRad > 1.0e-5,
        "capture emitted no course correction"
    );

    const glm::dvec3 startDerivative =
        out.captureP1 - out.captureP0;
    const glm::dvec3 endDerivative =
        out.captureP3 - out.captureP2;

    require(
        glm::dot(
            glm::normalize(startDerivative),
            glm::dvec3(1.0,0.0,0.0)
        ) > 0.999999,
        "capture start tangent does not match actual velocity"
    );
    require(
        glm::dot(
            glm::normalize(endDerivative),
            out.meetingTangentMap
        ) > 0.999999,
        "capture end tangent does not match route tangent"
    );
}

void testHigherSpeedMovesMeetingFarther()
{
    std::vector<Segment> route {
        straight({0.0,0.0,0.0},{5000.0,0.0,0.0},0.0,5000.0)
    };

    Guidance::Request slow;
    slow.positionMapMeters = {100.0,0.0,20.0};
    slow.actualVelocityMapMps = {20.0,0.0,0.0};
    slow.routeCurves = &route;
    slow.currentRouteProgressMeters = 100.0;
    slow.centeringDeadbandMeters = 2.0;
    slow.courseResponseSeconds = 2.0;

    auto fast = slow;
    fast.actualVelocityMapMps = {80.0,0.0,0.0};

    const auto a = Guidance::evaluate(slow);
    const auto b = Guidance::evaluate(fast);

    require(a.valid && b.valid, "speed scaling case invalid");
    require(
        b.meetingRouteProgressMeters >
            a.meetingRouteProgressMeters + 1.0,
        "faster craft did not receive farther meeting point"
    );
}

void testMeetingJoinIsNearTangent()
{
    std::vector<Segment> route {
        straight({0.0,0.0,0.0},{3000.0,0.0,0.0},0.0,3000.0)
    };

    Guidance::Request request;
    request.positionMapMeters = {100.0,0.0,50.0};
    request.actualVelocityMapMps = {40.0,0.0,0.0};
    request.routeCurves = &route;
    request.currentRouteProgressMeters = 100.0;
    request.centeringDeadbandMeters = 2.0;
    request.courseResponseSeconds = 2.0;
    request.maximumMeetingJoinAngleRad = 0.20;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "meeting-angle case invalid");
    require(
        out.meetingJoinAngleRad <=
            request.maximumMeetingJoinAngleRad + 1.0e-9,
        "capture selected a route crossing instead of near-tangent join"
    );
}

}

int main()
{
    try
    {
        testCenteredKeepsExactTangent();
        testParallelOffsetBuildsSmoothCapture();
        testHigherSpeedMovesMeetingFarther();
        testMeetingJoinIsNearTangent();

        std::cout
            << "COURSE CAPTURE GUIDANCE TESTS: PASS\n"
            << " - centered craft keeps exact route tangent\n"
            << " - displaced craft gets smooth velocity-tangent capture\n"
            << " - meeting distance scales with speed/response\n"
            << " - capture joins route near-tangentially\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "COURSE CAPTURE GUIDANCE TESTS: FAIL: "
            << error.what() << '\n';
        return 1;
    }
}
