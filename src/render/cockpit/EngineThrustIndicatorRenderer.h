#pragma once

#include "src/game/presentation/EngineThrustHudPresentation.h"
#include "src/render/types/Viewport.h"

namespace render::cockpit
{

class EngineThrustIndicatorRenderer final
{
public:
    void render(
        const game::presentation::EngineThrustHudPresentation& presentation,
        const Viewport& viewport
    );
};

} // namespace render::cockpit
