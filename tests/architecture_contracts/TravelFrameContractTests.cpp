#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

#include "src/game/navigation/TravelFrameSystem.h"

namespace
{

void require(bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void requireNear(
    const glm::dvec3& a,
    const glm::dvec3& b,
    double epsilon,
    const std::string& message)
{
    if (glm::length(a - b) > epsilon)
        throw std::runtime_error(message);
}

game::navigation::KinematicFrame makeReferenceFrame()
{
    game::navigation::KinematicFrame frame;
    frame.systemId = 7;
    frame.frameId = "hub_alpha";
    frame.originMeters = glm::dvec3(1000.0, -2000.0, 3000.0);
    frame.linearVelocityMps = glm::dvec3(10.0, 20.0, 30.0);
    frame.linearAccelerationMps2 = glm::dvec3(0.1, -0.2, 0.3);
    frame.localToWorldBasis = glm::dmat3(1.0);
    frame.angularVelocityWorldRadPerSecond = glm::dvec3(0.0, 0.0, 0.01);
    frame.angularAccelerationWorldRadPerSecond2 = glm::dvec3(0.0, 0.0, 0.001);
    frame.valid = true;
    return frame;
}

void testMatchCopiesKinematicsButKeepsOwnedIdentity()
{
    game::navigation::DynamicMotionState motion;
    const auto reference = makeReferenceFrame();

    require(
        game::navigation::TravelFrameSystem::matchToReference(
            motion,
            reference,
            "ship_travel_42",
            reference.frameId
        ),
        "travel frame failed to match valid reference"
    );

    require(motion.travelFrame.valid, "owned travel frame is invalid after match");
    require(motion.travelFrame.frameId == "ship_travel_42", "travel frame aliased reference identity");
    require(motion.matchedToReferenceFrame, "match flag not set");
    require(motion.matchedReferenceFrameId == reference.frameId, "matched reference identity lost");
    require(motion.systemId == reference.systemId, "system membership not inherited from matched reference");

    requireNear(motion.travelFrame.originMeters, reference.originMeters, 1e-12, "origin mismatch after match");
    requireNear(motion.travelFrame.linearVelocityMps, reference.linearVelocityMps, 1e-12, "velocity mismatch after match");
    requireNear(motion.travelFrame.linearAccelerationMps2, reference.linearAccelerationMps2, 1e-12, "acceleration mismatch after match");
    requireNear(motion.travelFrame.angularVelocityWorldRadPerSecond, reference.angularVelocityWorldRadPerSecond, 1e-12, "angular velocity mismatch after match");
}

void testRefreshPreservesOwnedIdentityAndUpdatesKinematics()
{
    game::navigation::DynamicMotionState motion;
    auto reference = makeReferenceFrame();

    require(
        game::navigation::TravelFrameSystem::matchToReference(
            motion,
            reference,
            "ship_travel_42",
            reference.frameId
        ),
        "initial match failed"
    );

    reference.originMeters += glm::dvec3(50.0, 60.0, 70.0);
    reference.linearVelocityMps += glm::dvec3(1.0, 2.0, 3.0);
    reference.linearAccelerationMps2 += glm::dvec3(0.01, 0.02, 0.03);

    require(
        game::navigation::TravelFrameSystem::refreshMatchedReference(
            motion,
            reference,
            reference.frameId
        ),
        "matched travel frame did not refresh"
    );

    require(motion.travelFrame.frameId == "ship_travel_42", "refresh replaced owned frame identity");
    requireNear(motion.travelFrame.originMeters, reference.originMeters, 1e-12, "refresh origin mismatch");
    requireNear(motion.travelFrame.linearVelocityMps, reference.linearVelocityMps, 1e-12, "refresh velocity mismatch");
    requireNear(motion.travelFrame.linearAccelerationMps2, reference.linearAccelerationMps2, 1e-12, "refresh acceleration mismatch");
}

void testDetachPreservesInstantaneousKinematics()
{
    game::navigation::DynamicMotionState motion;
    const auto reference = makeReferenceFrame();

    require(
        game::navigation::TravelFrameSystem::matchToReference(
            motion,
            reference,
            "ship_travel_42",
            reference.frameId
        ),
        "initial match failed"
    );

    const auto before = motion.travelFrame;
    game::navigation::TravelFrameSystem::detach(motion);

    require(!motion.matchedToReferenceFrame, "detach did not clear match flag");
    require(motion.matchedReferenceFrameId.empty(), "detach retained external reference identity");
    require(motion.travelFrame.frameId == before.frameId, "detach changed owned frame identity");
    requireNear(motion.travelFrame.originMeters, before.originMeters, 1e-12, "detach changed origin");
    requireNear(motion.travelFrame.linearVelocityMps, before.linearVelocityMps, 1e-12, "detach changed velocity");
    requireNear(motion.travelFrame.linearAccelerationMps2, before.linearAccelerationMps2, 1e-12, "detach changed acceleration");
}

void testLocalWorldStateIsContinuousAcrossOwnershipBoundary()
{
    game::navigation::DynamicMotionState motion;
    const auto reference = makeReferenceFrame();

    require(
        game::navigation::TravelFrameSystem::matchToReference(
            motion,
            reference,
            "ship_travel_42",
            reference.frameId
        ),
        "initial match failed"
    );

    const glm::dvec3 localPosition(250.0, -80.0, 15.0);
    const glm::dvec3 localVelocity(12.0, 3.0, -4.0);

    const glm::dvec3 worldPositionBefore =
        motion.travelFrame.localToWorldPosition(localPosition);
    const glm::dvec3 worldVelocityBefore =
        motion.travelFrame.localToWorldVelocity(localPosition, localVelocity);

    game::navigation::TravelFrameSystem::detach(motion);

    const glm::dvec3 worldPositionAfter =
        motion.travelFrame.localToWorldPosition(localPosition);
    const glm::dvec3 worldVelocityAfter =
        motion.travelFrame.localToWorldVelocity(localPosition, localVelocity);

    requireNear(worldPositionAfter, worldPositionBefore, 1e-12, "detach teleported local state");
    requireNear(worldVelocityAfter, worldVelocityBefore, 1e-12, "detach changed physical velocity");
}

} // namespace

int main()
{
    try
    {
        testMatchCopiesKinematicsButKeepsOwnedIdentity();
        testRefreshPreservesOwnedIdentityAndUpdatesKinematics();
        testDetachPreservesInstantaneousKinematics();
        testLocalWorldStateIsContinuousAcrossOwnershipBoundary();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Travel-frame contract test failed: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Travel-frame contract tests passed.\n";
    return EXIT_SUCCESS;
}
