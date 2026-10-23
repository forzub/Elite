#include "src/game/navigation/DynamicMotionSystem.h"
#include "src/game/navigation/assisted/AssistedSafeCourseExecutor.h"
#include "src/game/shared/SharedShipPhysics.h"
#include "src/game/ship/core/ShipParams.h"
#include "src/game/ship/core/ShipTransform.h"
#include "src/world/WorldParams.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr double Pi = 3.1415926535897932384626433832795;
constexpr double Dt = 0.02;

using Executor =
    game::navigation::assisted::AssistedSafeCourseExecutor;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

glm::dvec3 normalized(const glm::dvec3& v)
{
    const double len = glm::length(v);
    if (!(std::isfinite(len) && len > 1.0e-12))
        throw std::runtime_error("invalid test direction");
    return v / len;
}

void setBasis(
    ShipTransform& transform,
    const glm::dvec3& forward,
    const glm::dvec3& upHint
)
{
    const glm::dvec3 f = normalized(forward);
    glm::dvec3 r = glm::cross(f, upHint);
    if (glm::length(r) <= 1.0e-9)
        r = glm::cross(f, glm::dvec3(0.0, 0.0, 1.0));
    r = normalized(r);
    const glm::dvec3 u = normalized(glm::cross(r, f));

    transform.orientation = glm::mat4(1.0f);
    transform.orientation[0] = glm::vec4(glm::vec3(r), 0.0f);
    transform.orientation[1] = glm::vec4(glm::vec3(u), 0.0f);
    transform.orientation[2] = glm::vec4(glm::vec3(-f), 0.0f);
}

ShipParams assistedParams()
{
    ShipParams p {};
    p.maxPitchRate = 2.5f;
    p.maxYawRate = 2.5f;
    p.maxRollRate = 3.0f;
    p.angularAccel = 3.0f;
    p.angularDamping = 2.5f;

    p.maxCombatSpeed = 500.0f;
    p.maxCruiseSpeed = 1000.0f;
    p.throttleAccel = 5.0f;
    p.assistedMinimumTargetSpeedChangeRateMps2 = 1.0f;
    p.assistedTargetSpeedChangeRateFractionPerSecond = 0.10f;

    p.forwardMainEngineAvailable = true;
    p.reverseMainEngineAvailable = true;
    p.forwardMainEngineAccelerationMps2 = 73.549875f;
    p.reverseMainEngineAccelerationMps2 = 73.549875f;

    p.autoLevelStrength = 0.0f;
    p.strafeAccel = 73.549875f;
    p.strafeDamping = 6.0f;
    p.maxStrafeSpeed = 80.0f;
    p.manoeuvreThrusterAccel = 0.0f;

    p.maxGs = 5.0f;
    p.maxLinearGs = 7.5f;
    p.turnRadius = 20.0f;
    p.stopSpeedEpsilonMps = 0.05f;

    p.massKg = 260000.0;
    p.pitchInertiaKgM2 = 11219866.6666667;
    p.yawInertiaKgM2 = 25324866.6666667;
    p.rollInertiaKgM2 = 15188333.3333333;
    return p;
}

double angleBetween(
    const glm::dvec3& a,
    const glm::dvec3& b
)
{
    return std::acos(
        std::clamp(
            glm::dot(normalized(a), normalized(b)),
            -1.0,
            1.0
        )
    );
}

void testSafeCourseExecutorConvergesWithoutPlanner()
{
    const ShipParams params = assistedParams();

    game::navigation::KinematicFrame frame;
    frame.systemId = 1;
    frame.frameId = "assisted-safe-course-isolation";
    frame.originMeters = {0.0, 0.0, 0.0};
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.valid = true;

    ShipTransform transform {};
    transform.motion.mode = game::navigation::MotionMode::HubTactical;
    transform.motion.systemId = 1;
    transform.motion.travelFrame = frame;
    transform.motion.localControlLaw =
        game::navigation::LocalFlightControlLaw::Assisted;
    transform.motion.localVelocityMps = {300.0, 0.0, 0.0};
    transform.motion.worldVelocityMps = {300.0, 0.0, 0.0};
    transform.motion.targetForwardSpeedMps = 300.0;
    transform.motion.assistedTargetSpeedHold = true;

    // Live hot-start shape: velocity remains on +X while the hull starts
    // sixty degrees away from it.
    const double hullOffset = 60.0 * Pi / 180.0;
    const glm::dvec3 initialHullForward(
        std::cos(hullOffset),
        0.0,
        std::sin(hullOffset)
    );
    setBasis(transform, initialHullForward, {0.0, 1.0, 0.0});

    // Fixed safe-course decision. There is intentionally no obstacle checker,
    // planner, route binder or route follower in this test.
    const double safeTurn = 35.0 * Pi / 180.0;
    const glm::dvec3 selectedCourse(
        std::cos(safeTurn),
        0.0,
        std::sin(safeTurn)
    );

    WorldParams world {};

    bool acquired = false;
    double maxAngularSpeed = 0.0;
    double maxCourseError = 0.0;
    double maxHullVelocityError = 0.0;

    for (int tick = 0; tick < 1000; ++tick)
    {
        Executor::Input in;
        in.actualVelocityMapMps = transform.motion.localVelocityMps;
        in.selectedCourseMap = selectedCourse;
        in.forwardMap = glm::dvec3(transform.forward());
        in.rightMap = glm::dvec3(transform.right());
        in.upMap = glm::dvec3(transform.up());
        in.pitchRateRadPerSec = transform.pitchRate;
        in.yawRateRadPerSec = transform.yawRate;
        in.rollRateRadPerSec = transform.rollRate;
        in.selectedSpeedMps = 300.0;
        in.maxPitchRateRadPerSec = params.maxPitchRate;
        in.maxYawRateRadPerSec = params.maxYawRate;
        in.maxRollRateRadPerSec = params.maxRollRate;
        in.angularAccelerationAuthorityRadPerSec2 = params.angularAccel;
        in.deltaSeconds = Dt;

        const auto out = Executor::evaluate(in);
        require(out.valid, "safe-course executor emitted invalid output");

        const double angularSpeed =
            std::sqrt(
                transform.pitchRate * transform.pitchRate +
                transform.yawRate * transform.yawRate +
                transform.rollRate * transform.rollRate
            );
        maxAngularSpeed = std::max(maxAngularSpeed, angularSpeed);
        maxCourseError = std::max(maxCourseError, out.courseErrorRad);

        const double speed =
            glm::length(transform.motion.localVelocityMps);
        if (speed > 0.5)
        {
            maxHullVelocityError = std::max(
                maxHullVelocityError,
                angleBetween(
                    glm::dvec3(transform.forward()),
                    transform.motion.localVelocityMps
                )
            );
        }

        if (out.acquired)
        {
            acquired = true;
            break;
        }

        SharedShipPhysics::integrate(
            transform,
            params,
            out.control,
            world,
            static_cast<float>(Dt)
        );

        game::navigation::DynamicMotionSystem::applyLocalFrameInput(
            transform.motion,
            frame,
            params,
            static_cast<float>(Dt),
            out.control.targetSpeedRate,
            out.control.cruiseActive,
            out.control.forwardInput,
            out.control.liftInput,
            out.control.strafeInput,
            transform.forward(),
            transform.right(),
            transform.up()
        );

        game::navigation::DynamicMotionSystem::updateLocalFrameMotion(
            transform.motion,
            transform.worldPosition,
            frame,
            params,
            Dt
        );
        transform.syncLegacyPositionFromWorld();
    }

    const double finalCourseError =
        angleBetween(transform.motion.localVelocityMps, selectedCourse);
    const double finalHullVelocityError =
        angleBetween(
            glm::dvec3(transform.forward()),
            transform.motion.localVelocityMps
        );
    const double finalAngularSpeed =
        std::sqrt(
            transform.pitchRate * transform.pitchRate +
            transform.yawRate * transform.yawRate +
            transform.rollRate * transform.rollRate
        );

    std::cout
        << "[SAFE-COURSE-ISOLATION]"
        << " acquired=" << (acquired ? 1 : 0)
        << " final_course_err_deg=" << finalCourseError * 180.0 / Pi
        << " final_hull_velocity_err_deg="
        << finalHullVelocityError * 180.0 / Pi
        << " final_omega_radps=" << finalAngularSpeed
        << " max_omega_radps=" << maxAngularSpeed
        << " max_course_err_deg=" << maxCourseError * 180.0 / Pi
        << " max_hull_velocity_err_deg="
        << maxHullVelocityError * 180.0 / Pi
        << "\n";

    require(
        acquired,
        "isolated Assisted safe-course executor did not converge"
    );
    require(
        finalCourseError < 2.1 * Pi / 180.0,
        "isolated executor acquired without selected velocity course"
    );
    require(
        finalHullVelocityError < 2.1 * Pi / 180.0,
        "isolated executor acquired with hull/velocity slip"
    );
    require(
        finalAngularSpeed < 0.011,
        "isolated executor acquired without settling angular rate"
    );
}


void testEngineOutCourseAcquisitionUsesWorkingThrustAxis()
{
    constexpr double offset=49.50776982*Pi/180.0;
    Executor::Input in;
    in.actualVelocityMapMps={0.0,0.0,-150.0};
    in.selectedCourseMap={0.0,0.0,-1.0};
    in.forwardMap={0.0,0.0,-1.0};
    in.rightMap={1.0,0.0,0.0};
    in.upMap={0.0,1.0,0.0};
    in.workingThrustAxisBody={
        -std::sin(offset),0.0,-std::cos(offset)};
    in.maxPitchRateRadPerSec=2.0;
    in.maxYawRateRadPerSec=2.0;
    in.maxRollRateRadPerSec=2.0;
    in.angularAccelerationAuthorityRadPerSec2=1.0;
    in.deltaSeconds=0.02;

    const auto wrong=Executor::evaluate(in);
    require(wrong.valid && !wrong.acquired &&
            wrong.hullToVelocityErrorRad>0.8 &&
            std::abs(wrong.control.yawInput)>0.01f,
            "engine-out safe-course accepted nose-aligned wrong thrust axis");

    // 49.5 degree hull yaw cancels the -49.5 degree physical motor axis.
    // The nose now disagrees with course, but actual THRUST is course-aligned.
    in.forwardMap={std::sin(offset),0.0,-std::cos(offset)};
    in.rightMap={std::cos(offset),0.0,std::sin(offset)};
    const auto right=Executor::evaluate(in);
    require(right.valid && right.acquired &&
            right.hullToVelocityErrorRad<1.0e-6,
            "engine-out safe-course cannot acquire displaced-nose attitude");
    in.workingThrustAxisBody={0.0,0.0,-1.0};
    const auto healthy=Executor::evaluate(in);
    require(healthy.valid && !healthy.acquired,
            "nominal intact ship stopped treating its nose as working axis");
}

} // namespace

int main()
{
    try
    {
        testSafeCourseExecutorConvergesWithoutPlanner();
        testEngineOutCourseAcquisitionUsesWorkingThrustAxis();
        std::cout
            << "ASSISTED SAFE COURSE EXECUTOR ISOLATION: PASS\n"
            << " - no obstacle checker in execution loop\n"
            << " - no route planner or entry binder\n"
            << " - no route follower/client route autopilot\n"
            << " - fixed selected course executes through ordinary Assisted physics\n";
        return 0;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "ASSISTED SAFE COURSE EXECUTOR ISOLATION: FAIL: "
            << e.what() << "\n";
        return 1;
    }
}
