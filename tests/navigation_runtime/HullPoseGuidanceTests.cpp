#include "src/game/navigation/autopilot/HullPoseGuidance.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{

using Guidance =
    game::navigation::autopilot::HullPoseGuidance;

constexpr double Pi =
    3.1415926535897932384626433832795;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

double magnitudePitchYaw(const glm::dvec3& e)
{
    return std::hypot(e.x, e.y);
}

void testIdentity()
{
    Guidance::Request request;
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};
    request.targetForward = request.currentForward;
    request.targetUp = request.currentUp;

    const auto out = Guidance::evaluate(request);
    require(out.valid, "identity pose invalid");
    require(magnitudePitchYaw(out.pitchYawErrorLocalRad) < 1.0e-12,
        "identity pose produced pitch/yaw error");
    require(std::abs(out.rollErrorRad) < 1.0e-12,
        "identity pose produced roll error");
}

void testYawPitchNeverCreatesRoll()
{
    Guidance::Request request;
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};

    const double a = 30.0 * Pi / 180.0;
    request.targetForward = {
        std::cos(a),
        0.0,
        std::sin(a)
    };
    request.targetUp = {0.0, 1.0, 0.0};

    const auto out = Guidance::evaluate(request);
    require(out.valid, "yaw-only pose invalid");
    require(magnitudePitchYaw(out.pitchYawErrorLocalRad) > 0.4,
        "yaw-only pose lost pitch/yaw demand");
    require(std::abs(out.pitchYawErrorLocalRad.z) < 1.0e-12,
        "pitch/yaw channel leaked into roll component");
    require(std::abs(out.rollErrorRad) < 1.0e-12,
        "yaw-only target created roll demand");
}

void testRollNeverCreatesPitchYaw()
{
    Guidance::Request request;
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};

    request.targetForward = request.currentForward;
    request.targetUp = {0.0, 0.0, 1.0};

    const auto out = Guidance::evaluate(request);
    require(out.valid, "roll-only pose invalid");
    require(magnitudePitchYaw(out.pitchYawErrorLocalRad) < 1.0e-12,
        "roll-only target created pitch/yaw demand");
    require(std::abs(out.rollErrorRad) > 1.5,
        "roll-only target lost roll demand");
}

void testCombinedErrorsRemainIndependent()
{
    Guidance::Request request;
    request.currentForward = {1.0, 0.0, 0.0};
    request.currentRight = {0.0, 0.0, 1.0};
    request.currentUp = {0.0, 1.0, 0.0};

    const double yaw = 25.0 * Pi / 180.0;
    request.targetForward = {
        std::cos(yaw),
        0.0,
        std::sin(yaw)
    };

    // Tunnel bottom/up asks for a 90-degree roll. Forward remains independently
    // owned by the pitch/yaw channel.
    request.targetUp = {0.0, 0.0, 1.0};

    const auto out = Guidance::evaluate(request);
    require(out.valid, "combined pose invalid");
    require(magnitudePitchYaw(out.pitchYawErrorLocalRad) > 0.3,
        "combined pose lost pitch/yaw demand");
    require(std::abs(out.rollErrorRad) > 1.0,
        "combined pose lost independent roll demand");
    require(std::abs(out.pitchYawErrorLocalRad.z) < 1.0e-12,
        "combined pose leaked roll into pitch/yaw vector");
}

}

int main()
{
    try
    {
        testIdentity();
        testYawPitchNeverCreatesRoll();
        testRollNeverCreatesPitchYaw();
        testCombinedErrorsRemainIndependent();

        std::cout
            << "HULL POSE GUIDANCE TESTS: PASS\n"
            << " - forward alignment owns pitch/yaw only\n"
            << " - tunnel up/down alignment owns roll only\n"
            << " - roll cannot alter corridor steering\n"
            << " - pitch/yaw cannot alter hull roll target\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "HULL POSE GUIDANCE TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
