#include "src/game/navigation/autopilot/HullPoseGuidance.h"
#include "src/game/navigation/autopilot/AssistedAutopilot.h"

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


void testEmergencyPilotTargetsWorkingAxisNotNose()
{
    using namespace game::navigation::autopilot;
    constexpr double offAxis=49.50776982*Pi/180.0;
    ShipParams physics {};
    physics.maxYawRate=2.5f;
    physics.maxPitchRate=2.5f;
    physics.maxRollRate=2.5f;
    physics.angularAccel=2.0f;
    physics.maxGs=5.0f;
    physics.turnRadius=20.0f;
    physics.maxCombatSpeed=500.0f;
    physics.maxCruiseSpeed=500.0f;
    physics.throttleAccel=5.0f;
    physics.forwardMainEngineAvailable=true;
    physics.forwardMainEngineAccelerationMps2=25.0f;
    physics.reverseMainEngineAvailable=true;
    physics.reverseMainEngineAccelerationMps2=25.0f;
    physics.assistedWorkingThrustAxisEmergency=true;
    physics.assistedWorkingThrustAxisBody={
        -std::sin(offAxis),0.0,-std::cos(offAxis)};
    physics.assistedWorkingThrustMotorIndex=1;

    AutopilotControlRequest req;
    req.deltaSeconds=0.02;
    req.actualVelocityMapMps={0.0,0.0,-100.0};
    req.desiredVelocityMapMps=req.actualVelocityMapMps;
    req.desiredForwardMap={0.0,0.0,-1.0};

    AssistedAutopilot pilot;
    const auto start=pilot.update(req,physics);
    require(std::abs(start.yawInput)>0.01f,
        "engine-out pilot did not seek offset working-thrust alignment");

    // Hull yawed away from the route by the required compensation angle:
    // nose is NOT on course, but the real thrust vector IS on course.
    // Demanding nose alignment here is precisely the diagnosed bug.
    req.forwardMap={std::sin(offAxis),0.0,-std::cos(offAxis)};
    req.rightMap={std::cos(offAxis),0.0,std::sin(offAxis)};
    AssistedAutopilot aligned;
    const auto settled=aligned.update(req,physics);
    require(std::abs(settled.yawInput)<0.01f &&
            std::abs(settled.pitchInput)<0.01f,
        "engine-out pilot fought physically correct displaced-nose pose");

    // Healthy Assisted must still align the NOSE with the velocity course.
    physics.assistedWorkingThrustAxisEmergency=false;
    physics.assistedWorkingThrustAxisBody={0.0,0.0,-1.0};
    physics.assistedWorkingThrustMotorIndex=-1;
    AssistedAutopilot healthy;
    const auto nominal=healthy.update(req,physics);
    require(std::abs(nominal.yawInput)>0.01f,
        "healthy Assisted nose-on-course behavior unexpectedly changed");
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
        testEmergencyPilotTargetsWorkingAxisNotNose();

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
