#pragma once

#include "src/game/presentation/EngineThrustHudPresentation.h"
#include "src/render/types/Viewport.h"
#include "src/game/simulation/ShipCoreStatus.h"

namespace render::cockpit
{

class EngineThrustIndicatorRenderer final
{
public:
    void renderThermalSystems(
        const game::ShipCoreStatus& status,
        const Viewport& viewport
    );
    void render(
        const game::presentation::EngineThrustHudPresentation& presentation,
        const Viewport& viewport
    );
};

} // namespace render::cockpit
