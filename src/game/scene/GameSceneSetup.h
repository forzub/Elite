#pragma once

#include "src/scene/EntityID.h"

class GameSimulation;

namespace game::scene
{

struct GameSceneSetupConfig
{
    static constexpr bool PromoScene = false;
};

EntityId buildInitialScene(GameSimulation& sim);

} // namespace game::scene