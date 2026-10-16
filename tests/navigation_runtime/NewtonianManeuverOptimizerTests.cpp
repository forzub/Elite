#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/navigation/planner/newtonian/NewtonianManeuverOptimizer.h"

namespace
{
using namespace game::navigation::planner::newtonian;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

double length(const glm::dvec3& v)
{
    return glm::length(v);
}

NewtonianPlannerLimits limits()
{
    NewtonianPlannerLimits out;
    out.mainAccelerationMps2 = 40.0;
    out.maxAngularVelocityRadPerSec = 1.0;
    out.angularAccelerationRadPerSec2 = 1.0;
    return out;
}

bool contains(
    const NewtonianManeuverPlan& plan,
    NewtonianManeuverPhaseKind kind
)
{
    for (const auto& phase : plan.phases)
    {
        if (phase.kind == kind)
            return true;
    }
    return false;
}

void testBrakeRequiresFlipAndMainBurn()
{
    NewtonianPlannerState initial;
    initial.velocityMps = {0.0, 0.0, 400.0};
    initial.forward = {0.0, 0.0, 1.0};

    NewtonianPlannerTarget target;
    target.desiredVelocityMps = {0.0, 0.0, 0.0};

    const auto plan =
        NewtonianManeuverOptimizer::optimize(
            initial,
            target,
            limits()
        );

    require(plan.valid, "braking plan invalid");
    require(
        contains(plan, NewtonianManeuverPhaseKind::RotateForThrust),
        "braking plan did not rotate for reverse thrust"
    );
    require(
        contains(plan, NewtonianManeuverPhaseKind::MainBurn),
        "braking plan did not use aft main burn"
    );
    require(
        length(plan.predictedTerminalState.velocityMps) < 1.0e-6,
        "braking plan did not remove velocity"
    );

    bool sawReverseBurn = false;
    for (const auto& phase : plan.phases)
    {
        if (phase.kind != NewtonianManeuverPhaseKind::MainBurn)
            continue;
        sawReverseBurn =
            glm::dot(
                glm::normalize(phase.targetForward),
                glm::dvec3(0.0, 0.0, -1.0)
            ) > 0.999;
    }
    require(
        sawReverseBurn,
        "braking plan did not point hull/main thrust opposite velocity"
    );
}

void testVelocityTurnUsesPhysicalMainThrustDirection()
{
    NewtonianPlannerState initial;
    initial.velocityMps = {0.0, 0.0, 400.0};
    initial.forward = {0.0, 0.0, 1.0};

    NewtonianPlannerTarget target;
    target.desiredVelocityMps = {400.0, 0.0, 0.0};

    const auto plan =
        NewtonianManeuverOptimizer::optimize(
            initial,
            target,
            limits()
        );

    require(plan.valid, "velocity-turn plan invalid");

    const glm::dvec3 error =
        plan.predictedTerminalState.velocityMps -
        target.desiredVelocityMps;

    require(
        length(error) < 1.0e-6,
        "velocity-turn plan did not reach target velocity"
    );

    bool sawDiagonalBurn = false;
    const glm::dvec3 expected =
        glm::normalize(glm::dvec3(400.0, 0.0, -400.0));

    for (const auto& phase : plan.phases)
    {
        if (phase.kind != NewtonianManeuverPhaseKind::MainBurn)
            continue;

        sawDiagonalBurn =
            glm::dot(
                glm::normalize(phase.targetForward),
                expected
            ) > 0.999;
    }

    require(
        sawDiagonalBurn,
        "velocity-turn plan did not orient main thrust along delta-v"
    );
}

void testFinalAttitudeIsSeparateFromTranslation()
{
    NewtonianPlannerState initial;
    initial.velocityMps = {0.0, 0.0, 300.0};
    initial.forward = {0.0, 0.0, 1.0};

    NewtonianPlannerTarget target;
    target.desiredVelocityMps = {0.0, 0.0, 0.0};
    target.finalForwardRequired = true;
    target.desiredForward = {1.0, 0.0, 0.0};

    const auto plan =
        NewtonianManeuverOptimizer::optimize(
            initial,
            target,
            limits()
        );

    require(plan.valid, "terminal-attitude plan invalid");
    require(
        contains(plan, NewtonianManeuverPhaseKind::Reorient),
        "terminal attitude was not authored as a separate reorientation"
    );
    require(
        glm::dot(
            glm::normalize(plan.predictedTerminalState.forward),
            glm::normalize(target.desiredForward)
        ) > 0.999999,
        "terminal forward orientation was not reached"
    );
    require(
        length(plan.predictedTerminalState.velocityMps) < 1.0e-6,
        "terminal reorientation changed translational target"
    );
}

void testInvalidAuthorityIsRejected()
{
    NewtonianPlannerState initial;
    NewtonianPlannerTarget target;
    auto bad = limits();
    bad.mainAccelerationMps2 = 0.0;

    const auto plan =
        NewtonianManeuverOptimizer::optimize(
            initial,
            target,
            bad
        );

    require(!plan.valid, "zero-main-authority plan was accepted");
}
}

int main()
{
    try
    {
        testBrakeRequiresFlipAndMainBurn();
        testVelocityTurnUsesPhysicalMainThrustDirection();
        testFinalAttitudeIsSeparateFromTranslation();
        testInvalidAuthorityIsRejected();

        std::cout
            << "NEWTONIAN MANEUVER OPTIMIZER: PASS\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "NEWTONIAN MANEUVER OPTIMIZER: FAIL: "
            << e.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
