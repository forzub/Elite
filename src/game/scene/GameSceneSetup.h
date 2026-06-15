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

EntityId buildInitialScene(GameSimulation& sim);

} // namespace game::scene