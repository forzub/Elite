#include "game/navigation/AcceptedManeuverProgramBuilder.h"

#include <cmath>
#include <glm/gtc/quaternion.hpp>
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

void testStoragePageBoundaryPreservesAngularState()
{
    const ShipParams params = makeParams();

    world::navigation::Trajectory trajectory;
    trajectory.status = world::navigation::TrajectoryStatus::Ready;
    trajectory.systemId = 0;
    trajectory.frameId = "hub";
    trajectory.startUniverseTimeSeconds = 200.0;

    constexpr std::size_t SampleCount = 20;
    constexpr double Dt = 0.25;
    constexpr double AngularRate = 0.08;

    for (std::size_t i = 0; i < SampleCount; ++i)
    {
        const double time = Dt * static_cast<double>(i);
        world::navigation::TrajectorySample sample;
        sample.universeTimeSeconds =
            trajectory.startUniverseTimeSeconds + time;
        sample.timeOffsetSeconds = time;
        sample.pathProgressMeters = static_cast<double>(i);
        sample.sourcePathProgressMeters = static_cast<double>(i);
        sample.positionMeters = {0.0, 0.0, -static_cast<double>(i)};
        sample.velocityMps = {0.0, 0.0, -4.0};
        sample.accelerationMps2 = {0.0, 0.0, 0.0};
        sample.orientation = glm::angleAxis(
            AngularRate * time,
            glm::dvec3(0.0, 1.0, 0.0)
        );
        sample.speedMps = 4.0;
        trajectory.samples.push_back(sample);
    }

    trajectory.durationSeconds =
        trajectory.samples.back().timeOffsetSeconds;
    trajectory.lengthMeters =
        trajectory.samples.back().pathProgressMeters;

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.objectiveRevision = 9;
    request.firstProgramRevision = 30;
    request.capabilityRevision = 5;
    request.hasInitialAngularVelocity = true;
    request.initialAngularVelocityMapRadPerSec =
        {0.0, AngularRate, 0.0};
    request.hasTerminalAngularVelocity = true;
    request.terminalAngularVelocityMapRadPerSec =
        {0.0, AngularRate, 0.0};

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(result.valid, "builder rejected smooth paged rotation");
    require(result.pages.size() == 2,
            "20 samples should require exactly two overlapping storage pages");

    const auto& first = result.pages[0];
    const auto& second = result.pages[1];
    const auto& firstBoundary =
        first.samples[first.sampleCount - 1];
    const auto& secondBoundary =
        second.samples[0];

    require(glm::length(
                firstBoundary.angularVelocityMapRadPerSecond -
                secondBoundary.angularVelocityMapRadPerSecond) <
            1.0e-10,
            "storage page boundary reset or changed angular velocity");
    require(glm::length(
                firstBoundary.angularAccelerationFeedForwardMapRadPerSec2 -
                secondBoundary.angularAccelerationFeedForwardMapRadPerSec2) <
            1.0e-10,
            "storage page boundary reset or changed angular acceleration");
    require(!first.completionTriggersReplan &&
            second.completionTriggersReplan,
            "storage page boundary became a semantic maneuver completion");
    require(
        second.family ==
            game::navigation::AcceptedManeuverProgram::
                ManeuverFamily::PrecisionTransit,
        "pre-capture final page must remain transit, not physical capture"
    );
}

void testAssistedUsesGameFlightLawInsteadOfRcsAllocation()
{
    ShipParams params = makeParams();
    params.strafeAccel = 30.0f;
    params.manoeuvreThrusterAccel = 2.0f;

    auto trajectory = makeTrajectory();
    for (auto& sample : trajectory.samples)
    {
        sample.accelerationMps2 = {8.0, 0.0, 0.0};
    }

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.controlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    request.referenceMode =
        game::navigation::AcceptedManeuverProgram::
            ReferenceMode::SpatialCorridor;
    request.objectiveRevision = 12;
    request.firstProgramRevision = 40;
    request.capabilityRevision = 6;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(result.valid,
            "Assisted route was incorrectly rejected by precision-RCS limit");
    require(result.pages.size() == 1,
            "small Assisted route unexpectedly paged");

    const auto& program = result.pages.front();
    require(
        program.translationMode ==
            game::navigation::AcceptedManeuverProgram::
                TranslationMode::AssistedVelocity,
        "Assisted program did not select the game-flight execution mode"
    );
    require(
        program.referenceMode ==
            game::navigation::AcceptedManeuverProgram::
                ReferenceMode::SpatialCorridor,
        "builder lost the spatial-corridor execution contract"
    );
}

void testAssistedRequestsRefinementForMotionEnvelope()
{
    ShipParams params = makeParams();
    params.strafeAccel = 20.0f;
    params.manoeuvreThrusterAccel = 2.0f;

    auto trajectory = makeTrajectory();
    for (auto& sample : trajectory.samples)
    {
        sample.accelerationMps2 = {80.0, 0.0, 0.0};
    }

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.controlLaw = game::navigation::LocalFlightControlLaw::Assisted;
    request.objectiveRevision = 14;
    request.firstProgramRevision = 60;
    request.capabilityRevision = 8;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(!result.valid,
            "Assisted accepted a lateral acceleration it cannot execute");
    require(
        result.feedback.disposition ==
            game::navigation::AcceptedManeuverProgramBuilder::
                ValidationDisposition::NeedsRefinement,
        "recoverable Assisted envelope violation became a hard failure"
    );
    require(
        result.feedback.refinement ==
            game::navigation::AcceptedManeuverProgramBuilder::
                RefinementKind::IncreaseTurnRadius,
        "lateral Assisted envelope violation did not request radius/speed refinement"
    );
    require(
        result.feedback.requiredValue > result.feedback.availableValue &&
        result.feedback.recommendedScale < 1.0,
        "refinement feedback lost quantitative required/available guidance"
    );
    require(
        result.failureReason.rfind(
            "assisted-motion-envelope-infeasible",
            0
        ) == 0,
        "Assisted impossible-motion rejection exposed the wrong reason"
    );
    require(
        result.failureReason.find("sample=") != std::string::npos &&
        result.failureReason.find("along_mps2=") != std::string::npos &&
        result.failureReason.find("transverse_mps2=") != std::string::npos &&
        result.failureReason.find("lateral_authority_mps2=") !=
            std::string::npos,
        "Assisted impossible-motion rejection lost its diagnostic context"
    );
}


void testNewtonianVectorDemandUsesMainNotPrecisionRcs()
{
    ShipParams params = makeParams();
    params.manoeuvreThrusterAccel = 0.0f;

    auto trajectory = makeTrajectory();
    for (auto& sample : trajectory.samples)
    {
        // Pure path-normal acceleration. A Newtonian craft serves this by
        // rotating the aft-main thrust vector, not by spending translation RCS.
        sample.accelerationMps2 = {5.0, 0.0, 0.0};
    }

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.controlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    request.objectiveRevision = 13;
    request.firstProgramRevision = 50;
    request.capabilityRevision = 7;
    request.policy.linearFeedbackReserveMps2 = 4.0;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(
        result.valid,
        "Newtonian path-normal main-vector demand was incorrectly tied to RCS authority"
    );
}

void testNewtonianBackwardDemandUsesFlipAndBurnMain()
{
    ShipParams params = makeParams();
    params.reverseMainEngineAvailable = false;
    params.reverseMainEngineAccelerationMps2 = 0.0f;
    params.manoeuvreThrusterAccel = 0.0f;

    auto trajectory = makeTrajectory();
    for (auto& sample : trajectory.samples)
    {
        // Opposite the nominal hull-forward sample. Rear-only Newtonian
        // propulsion can still provide this acceleration after a 180-deg flip.
        sample.accelerationMps2 = {0.0, 0.0, 5.0};
    }

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.controlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    request.objectiveRevision = 13;
    request.firstProgramRevision = 50;
    request.policy.linearFeedbackReserveMps2 = 4.0;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(
        result.valid,
        "rear-only Newtonian ship rejected acceleration that is reachable by flip-and-burn"
    );
}

void testNewtonianDemandAboveAftMainMagnitudeIsRejected()
{
    ShipParams params = makeParams();
    params.manoeuvreThrusterAccel = 100.0f;

    auto trajectory = makeTrajectory();
    for (auto& sample : trajectory.samples)
        sample.accelerationMps2 = {30.0, 0.0, 0.0};

    game::navigation::AcceptedManeuverProgramBuilder::Request request;
    request.trajectory = &trajectory;
    request.shipPhysics = &params;
    request.controlLaw = game::navigation::LocalFlightControlLaw::Newtonian;
    request.objectiveRevision = 14;
    request.firstProgramRevision = 60;
    request.policy.linearFeedbackReserveMps2 = 4.0;

    const auto result =
        game::navigation::AcceptedManeuverProgramBuilder::build(request);

    require(
        !result.valid &&
        result.failureReason == "newtonian-motion-envelope-infeasible",
        "Newtonian accepted an acceleration vector above aft-main magnitude"
    );
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
    require(result.failureReason == "angular-kinematics-infeasible",
            "builder did not expose the angular rejection reason");
    require(result.pages.empty(),
            "rejected terminal spin leaked executable program pages");
}

} // namespace

int main()
{
    try
    {
        testTerminalAngularVelocityIsAcceptedAndPreserved();
        testStoragePageBoundaryPreservesAngularState();
        testAssistedUsesGameFlightLawInsteadOfRcsAllocation();
        testAssistedRequestsRefinementForMotionEnvelope();
        testNewtonianVectorDemandUsesMainNotPrecisionRcs();
        testNewtonianBackwardDemandUsesFlipAndBurnMain();
        testNewtonianDemandAboveAftMainMagnitudeIsRejected();
        testImpossibleTerminalSpinIsRejected();

        std::cout
            << "ACCEPTED MANEUVER PROGRAM BUILDER TESTS: PASS\n"
            << " - trajectory -> immutable accepted program\n"
            << " - rotating terminal angular velocity retained\n"
            << " - storage-page angular state remains continuous\n"
            << " - Assisted uses game-flight velocity control, not route RCS\n"
            << " - recoverable Assisted acceleration violation requests planner refinement\n"
            << " - Newtonian route-vector acceleration belongs to aft main, not precision RCS\n"
            << " - rear-only Newtonian braking is accepted through flip-and-burn\n"
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
