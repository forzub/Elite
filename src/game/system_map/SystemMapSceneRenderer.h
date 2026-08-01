#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct PlayerNavigationState;
    struct SystemMapSnapshot;
}

namespace game::system_map
{
    struct SystemMapPresentation;
    class SystemMapRenderContext;
    class SystemMapView;

    /*
        System draw-pass orchestrator.

        It consumes immutable view and presentation state, owns no OpenGL
        objects and performs no persistent state synchronization. Frame-local
        pick data is written through SystemMapRenderContext.
    */
    class SystemMapSceneRenderer
    {
    public:
        void render(
            const SystemMapView& view,
            SystemMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            const world::celestial::PlayerNavigationState& navigation,
            const SystemMapPresentation& presentation
        ) const;
    };
}
