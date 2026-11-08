#pragma once

#include "src/scene/EntityID.h"
#include "src/world/orbits/OrbitalMotion.h"
#include "src/game/world_state/InitialWorldState.h"

class GameSimulation;

namespace game::scene
{

struct GameSceneSetupConfig
{
    static constexpr bool PromoScene = false;
};

EntityId buildInitialScene(
    GameSimulation& sim,
    const game::world_state::InitialWorldState& initialState
);

// One orbital hub (its modules / docks) in the simulation's ACTIVE system - the same path as the
// authored hubs of initial_world_state.json; used for the hubs of world sites in other systems.
bool spawnOrbitalHub(
    GameSimulation& sim,
    const game::world_state::InitialWorldStateOrbitalHub& hub
);

} // namespace game::scene