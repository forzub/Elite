#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/navigation/NpcNavigationIntentController.h"

namespace
{

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testFormationSlotTracksMovingReference()
{
    game::navigation::NpcNavigationKinematicState state;
    state.positionSystemMeters = {100.0, 0.0, 0.0};
    state.relativeSystemVelocityMps = {0.0, 0.0, 0.0};
    state.forwardSystem = {0.0, 0.0, -1.0};

    NpcNavigationGoal goal;
    goal.mode = NpcNavigationGoalMode::TrackFormationSlot;
    goal.desiredPositionSystemMeters = {120.0, 0.0, 0.0};
    goal.desiredVelocitySystemMps = {0.0, 0.0, 180.0};
    goal.positionResponsePerSecond = 0.5;
    goal.maximumPositionCorrectionSpeedMps = 50.0;

    const glm::dvec3 desired =
        game::navigation::NpcNavigationIntentController::
            desiredVelocityForGoal(state, goal);

    require(
        std::abs(desired.x - 10.0) < 1.0e-9,
        "formation position correction is wrong"
    );
    require(
        std::abs(desired.z - 180.0) < 1.0e-9,
        "formation base velocity was not preserved"
    );
}

void testFormationCorrectionIsBounded()
{
    game::navigation::NpcNavigationKinematicState state;
    state.positionSystemMeters = {0.0, 0.0, 0.0};

    NpcNavigationGoal goal;
    goal.mode = NpcNavigationGoalMode::TrackFormationSlot;
    goal.desiredPositionSystemMeters = {10000.0, 0.0, 0.0};
    goal.desiredVelocitySystemMps = {0.0, 0.0, 180.0};
    goal.positionResponsePerSecond = 1.0;
    goal.maximumPositionCorrectionSpeedMps = 55.0;

    const glm::dvec3 desired =
        game::navigation::NpcNavigationIntentController::
            desiredVelocityForGoal(state, goal);
    const glm::dvec3 correction =
        desired - goal.desiredVelocitySystemMps;

    require(
        std::abs(glm::length(correction) - 55.0) < 1.0e-9,
        "formation correction exceeded configured bound"
    );
}

} // namespace

int main()
{
    try
    {
        testFormationSlotTracksMovingReference();
        testFormationCorrectionIsBounded();
        std::cout
            << "NPC FORMATION GOAL: PASS\n"
            << " - moving slot velocity and position correction compose\n"
            << " - catch-up correction is bounded\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NPC FORMATION GOAL: FAIL: "
                  << error.what() << "\n";
        return EXIT_FAILURE;
    }
}
