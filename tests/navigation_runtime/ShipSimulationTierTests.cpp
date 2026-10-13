#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "src/game/simulation/ShipSimulationTier.h"

namespace
{

using Tier = game::simulation::ShipSimulationTier;
using Actor = game::simulation::ShipActorKind;
using Lifecycle = game::simulation::ShipSimulationLifecycleState;

void require(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void testIdentityAndSimulationDetailAreIndependent()
{
    Lifecycle state;
    state.tier = Tier::TrafficShell;
    state.actorKind = Actor::TrafficAutopilot;
    state.pilotPresent = true;
    state.formationId = 17;
    state.formationSlot = 2;

    require(!state.acceptsAiControl(), "traffic shell received full AI control");

    state.materializeForInteraction();

    require(
        state.tier == Tier::MaterializedShip,
        "interaction did not materialize the ship"
    );
    require(
        state.actorKind == Actor::NpcPilot,
        "anonymous traffic actor did not resolve to an NPC pilot"
    );
    require(state.pilotPresent, "pilot disappeared during materialization");
    require(
        state.formationId == 17 && state.formationSlot == 2,
        "materialization lost formation membership"
    );
    require(state.acceptsAiControl(), "materialized piloted ship cannot run AI");
}

void testPilotEjectionCreatesDerelictSemantics()
{
    Lifecycle state;
    state.tier = Tier::MaterializedShip;
    state.actorKind = Actor::NpcPilot;
    state.pilotPresent = true;
    state.formationId = 5;
    state.formationSlot = 1;

    state.onPilotEjected();

    require(state.tier == Tier::Derelict, "ejected hull is not DERELICT");
    require(state.actorKind == Actor::None, "derelict retained pilot authority");
    require(!state.pilotPresent, "derelict still reports pilot present");
    require(state.formationId == 0, "derelict remained in formation");
    require(!state.acceptsAiControl(), "derelict still accepts AI commands");

    state.dematerializeBackground();
    require(
        state.tier == Tier::WreckShell,
        "pilotless derelict did not reduce to WRECK_SHELL"
    );
}

void testPilotedBackgroundReturnsToTrafficShell()
{
    Lifecycle state;
    state.tier = Tier::MaterializedShip;
    state.actorKind = Actor::NpcPilot;
    state.pilotPresent = true;

    state.dematerializeBackground();

    require(
        state.tier == Tier::TrafficShell,
        "piloted background ship did not reduce to TRAFFIC_SHELL"
    );
}

void testCoarseDamageIsBounded()
{
    game::simulation::CoarseShipDamageState damage;
    damage.set(-2.0, 0.4, 1.8, 0.25);

    require(damage.structuralIntegrity01 == 0.0, "structure clamp failed");
    require(damage.mainThrustAuthority01 == 0.4, "main thrust changed");
    require(damage.mainVectoringAuthority01 == 1.0, "vectoring clamp failed");
    require(damage.manoeuvreAuthority01 == 0.25, "manoeuvre authority changed");
}

} // namespace

int main()
{
    try
    {
        testIdentityAndSimulationDetailAreIndependent();
        testPilotEjectionCreatesDerelictSemantics();
        testPilotedBackgroundReturnsToTrafficShell();
        testCoarseDamageIsBounded();

        std::cout
            << "SHIP SIMULATION TIER: PASS\n"
            << " - actor identity is independent from simulation detail\n"
            << " - interaction promotes shell to materialized ship\n"
            << " - pilot ejection removes AI and formation ownership\n"
            << " - background derelict reduces to wreck shell\n"
            << " - shell damage aggregate remains bounded\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& error)
    {
        std::cerr
            << "SHIP SIMULATION TIER: FAIL: "
            << error.what()
            << "\n";
        return EXIT_FAILURE;
    }
}
