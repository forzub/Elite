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
    struct SystemMapSceneFrame;
    class SystemMapRenderContext;
    class SystemMapView;

    struct SystemMapSceneRenderOptions
    {
        // Presentation-only: hiding map objects must not change map data,
        // selection, picking, or authoritative navigation state.
        bool drawObjects = true;
    };

    /*
        System draw-pass orchestrator.

        It consumes immutable view and presentation state, owns no OpenGL
        objects and performs no persistent state synchronization. CPU geometry
        and pick data arrive in the same prepared SystemMapSceneFrame.
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
            const SystemMapPresentation& presentation,
            const SystemMapSceneFrame& frame,
            const SystemMapSceneRenderOptions& options
        ) const;
    };
}
