#include "game/navigation/AcceptedManeuverProgramBuilder.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

ShipParams makeParams()
{
    ShipParams params {};
    params.maxPitchRate = 1.0f;
    params.maxYawRate = 1.0f;
    params.maxRollRate = 1.0f;
    params.angularAccel = 2.0f;
    params.angularDamping = 3.0f;

    params.maxCombatSpeed = 100.0f;
    params.maxCruiseSpeed = 1000.0f;
    params.throttleAccel = 20.0f;
    params.forwardMainEngineAvailable = true;
    params.reverseMainEngineAvailable = true;
    params.forwardMainEngineAccelerationMps2 = 20.0f;
    params.reverseMainEngineAccelerationMps2 = 20.0f;

    params.strafeAccel = 5.0f;
    params.strafeDamping = 4.0f;
    params.maxStrafeSpeed = 20.0f;
    params.manoeuvreThrusterAccel = 5.0f;
    params.maxGs = 5.0f;
    params.maxLinearGs = 5.0f;
    params.turnRadius = 20.0f;
    return params;
}

world::navigation::Trajectory makeTrajectory()
{
    world::navigation::Trajectory trajectory;
    trajectory.status = world::navigation::TrajectoryStatus::Ready;
    trajectory.systemId = 0;
    trajectory.frameId = "hub";
    trajectory.startUniverseTimeSeconds = 100.0;
    trajectory.durationSeconds = 2.0;
    trajectory.lengthMeters = 10.0;

    for (int i = 0; i < 3; ++i)
    {
        world::navigation::TrajectorySample sample;
        sample.universeTimeSeconds = 100.0 + i;
        sample.timeOffsetSeconds = static_cast<double>(i);
        sample.pathProgressMeters = 5.0 * i;
        sample.sourcePathProgressMeters = 5.0 * i;
        sample.positionMeters = {0.0, 0.0, -5.0 * i};
        sample.velocityMps = {0.0, 0.0, -5.0};
        sample.accelerationMps2 = {0.0, 0.0, 0.0};
        sample.orientation = glm::dquat(1.0, 0.0, 0.0, 0.0);
        sample.speedMps = 5.0;
        trajectory.samples.push_back(sample);
    }

    return trajectory;
}

void testTerminalAngularVelocityIsAcceptedAndPreserved()
{
    const ShipParams params = makeParams();
    const auto trajectory = makeTrajectory();

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.objectiveRevision = 7;
    request.firstProgramRevision = 11;
    request.capabilityRevision = 3;
    request.hasTerminalAngularVelocity = true;
    request.terminalAngularVelocityMapRadPerSec = {0.1, 0.0, 0.0};
    request.policy.linearFeedbackReserveMps2 = 0.5;
    request.policy.angularFeedbackReserveRadPerSec2 = 0.25;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(result.valid, "builder rejected feasible rotating terminal");
    require(result.pages.size() == 1, "small trajectory unexpectedly paged");

    const auto& program = result.pages.front();
    require(program.valid, "accepted program is invalid");
    require(program.objectiveRevision == 7, "objective revision lost");
    require(program.revision == 11, "program revision lost");
    require(program.actuatorProgramFeasible,
            "zero-feed-forward trajectory reported infeasible actuators");
    require(program.completionTriggersReplan,
            "final storage page must own objective completion");

    const auto& terminal =
        program.samples[program.sampleCount - 1];
    require(std::abs(
                terminal.angularVelocityMapRadPerSecond.x - 0.1) <
            1.0e-12,
            "terminal dock angular velocity was not preserved");
    require(glm::length(
                terminal.angularAccelerationFeedForwardMapRadPerSec2) <=
            program.capability.maxAngularAccelerationRadPerSec2 + 1.0e-6,
            "accepted terminal angular acceleration exceeded capability");
}

void testImpossibleTerminalSpinIsRejected()
{
    const ShipParams params = makeParams();
    const auto trajectory = makeTrajectory();

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.objectiveRevision = 8;
    request.firstProgramRevision = 20;
    request.capabilityRevision = 4;
    request.hasTerminalAngularVelocity = true;
    request.terminalAngularVelocityMapRadPerSec = {5.0, 0.0, 0.0};

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(!result.valid,
            "builder accepted terminal angular speed beyond ship capability");
    require(result.pages.empty(),
            "rejected terminal spin leaked executable program pages");
}

} // namespace

int main()
{
    try
    {
        testTerminalAngularVelocityIsAcceptedAndPreserved();
        testImpossibleTerminalSpinIsRejected();

        std::cout
            << "ACCEPTED MANEUVER PROGRAM BUILDER TESTS: PASS\n"
            << " - trajectory -> immutable accepted program\n"
            << " - rotating terminal angular velocity retained\n"
            << " - impossible terminal spin rejected before Follower\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "Accepted maneuver program builder test failed: "
            << error.what() << '\n';
        return 1;
    }
}
