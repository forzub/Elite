#include "src/game/navigation/autopilot/HullAttitudeControl.h"
#include "src/game/navigation/autopilot/HullPoseGuidance.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>

namespace
{

using Pose =
    game::navigation::autopilot::HullPoseGuidance;
using Control =
    game::navigation::autopilot::HullAttitudeControl;

constexpr double Pi =
    3.1415926535897932384626433832795;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

glm::dvec3 rotateAroundX(const glm::dvec3& v, double a)
{
    const double c = std::cos(a);
    const double s = std::sin(a);
    return {
        v.x,
        v.y * c - v.z * s,
        v.y * s + v.z * c
    };
}

void testLargeRollErrorCommandsOnlyRoll()
{
    Pose::Request poseRequest;
    poseRequest.currentForward = {1.0, 0.0, 0.0};
    poseRequest.currentUp = {0.0, 1.0, 0.0};
    poseRequest.currentRight = {0.0, 0.0, 1.0};
    poseRequest.targetForward = {1.0, 0.0, 0.0};
    poseRequest.targetUp = {0.0, 0.0, 1.0};

    const auto pose = Pose::evaluate(poseRequest);
    require(pose.valid, "90-degree roll pose invalid");
    require(std::hypot(
                pose.pitchYawErrorLocalRad.x,
                pose.pitchYawErrorLocalRad.y) < 1.0e-12,
        "pure roll error leaked into pitch/yaw");

    Control::Request controlRequest;
    controlRequest.pitchYawErrorRad = {
        pose.pitchYawErrorLocalRad.x,
        pose.pitchYawErrorLocalRad.y
    };
    controlRequest.rollErrorRad = pose.rollErrorRad;
    controlRequest.pitchYawRateRadPerSec = {0.0, 0.0};
    controlRequest.rollRateRadPerSec = 0.0;
    controlRequest.maxPitchYawRateRadPerSec = {1.0, 1.0};
    controlRequest.maxRollRateRadPerSec = 1.0;
    controlRequest.angularAccelerationAuthorityRadPerSec2 = 1.0;
    controlRequest.deltaSeconds = 0.02;

    const auto control = Control::evaluate(controlRequest);
    require(control.valid, "90-degree roll control invalid");
    require(std::hypot(
                control.pitchYawInput.x,
                control.pitchYawInput.y) < 1.0e-12,
        "pure roll correction created pitch/yaw actuator input");
    require(std::abs(control.rollInput) > 0.99,
        "large roll error did not command hard roll authority");
}

void testClosedLoopRollConvergesAndStops()
{
    constexpr double Dt = 0.02;
    constexpr double Authority = 1.0;
    constexpr double MaxRollRate = 1.0;

    const glm::dvec3 forward(1.0, 0.0, 0.0);
    const glm::dvec3 baseUp(0.0, 1.0, 0.0);
    const glm::dvec3 baseRight(0.0, 0.0, 1.0);
    const glm::dvec3 targetUp(0.0, 0.0, 1.0);

    double rollAngle = 0.0;
    double rollRate = 0.0;

    for (int step = 0; step < 800; ++step)
    {
        Pose::Request poseRequest;
        poseRequest.currentForward = forward;
        poseRequest.currentUp = rotateAroundX(baseUp, rollAngle);
        poseRequest.currentRight = rotateAroundX(baseRight, rollAngle);
        poseRequest.targetForward = forward;
        poseRequest.targetUp = targetUp;

        const auto pose = Pose::evaluate(poseRequest);
        require(pose.valid, "closed-loop roll pose invalid");

        Control::Request controlRequest;
        controlRequest.pitchYawErrorRad = {
            pose.pitchYawErrorLocalRad.x,
            pose.pitchYawErrorLocalRad.y
        };
        controlRequest.rollErrorRad = pose.rollErrorRad;
        controlRequest.pitchYawRateRadPerSec = {0.0, 0.0};
        controlRequest.rollRateRadPerSec = rollRate;
        controlRequest.maxPitchYawRateRadPerSec = {1.0, 1.0};
        controlRequest.maxRollRateRadPerSec = MaxRollRate;
        controlRequest.angularAccelerationAuthorityRadPerSec2 = Authority;
        controlRequest.deltaSeconds = Dt;

        const auto control = Control::evaluate(controlRequest);
        require(control.valid, "closed-loop roll control invalid");
        require(std::hypot(
                    control.pitchYawInput.x,
                    control.pitchYawInput.y) < 1.0e-10,
            "closed-loop roll correction leaked into pitch/yaw");

        rollRate += control.rollInput * Authority * Dt;
        rollRate = std::clamp(
            rollRate,
            -MaxRollRate,
            MaxRollRate
        );
        rollAngle += rollRate * Dt;
    }

    const glm::dvec3 finalUp =
        rotateAroundX(baseUp, rollAngle);
    const double finalAlignment =
        std::clamp(glm::dot(finalUp, targetUp), -1.0, 1.0);
    const double finalError =
        std::acos(finalAlignment);

    require(finalError < 0.003,
        "closed-loop roll failed to align belly/up reference");
    require(std::abs(rollRate) < 0.01,
        "closed-loop roll reached alignment but failed to arrest roll rate");
}

}

int main()
{
    try
    {
        testLargeRollErrorCommandsOnlyRoll();
        testClosedLoopRollConvergesAndStops();

        std::cout
            << "HULL ROLL ALIGNMENT CONTRACT TESTS: PASS\n"
            << " - large dock-bottom error commands hard roll only\n"
            << " - roll converges to target up without pitch/yaw coupling\n"
            << " - residual roll rate is arrested at alignment\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "HULL ROLL ALIGNMENT CONTRACT TESTS: FAIL: "
            << e.what() << '\n';
        return 1;
    }
}
