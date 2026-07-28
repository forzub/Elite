#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
    struct PlayerNavigationState;
}

namespace game::system_map
{
    class GalaxyMapView;
    class GalaxyMapRenderContext;

    /*
        Galaxy draw-pass orchestrator.

        It owns no OpenGL objects. Every shared GPU operation is performed
        through GalaxyMapRenderContext, whose implementation remains the
        SystemMapRenderer facade during the incremental extraction.
    */
    class GalaxyMapRenderer
    {
    public:
        void render(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation
        ) const;
    };
}
