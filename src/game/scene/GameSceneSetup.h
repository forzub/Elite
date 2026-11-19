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

// The construction stages of the hub cities follow the universe time (src/model_asset/HubExpansion.h): when the
// stages of a spawned city change (a new game day), its hit volumes are re-assembled - at most one city per call
// (the server calls it every step). Returns true when a city was refreshed.
bool refreshHubCityConstructionCollision(GameSimulation& sim);

} // namespace game::scene