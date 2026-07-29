#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct PlayerNavigationState;
    struct SystemMapSnapshot;
}

namespace game::system_map
{
    class SystemMapRenderContext;
    class SystemMapView;

    /*
        System draw-pass orchestrator.

        It owns no OpenGL objects and no input state. Persistent presentation
        state lives in SystemMapView; frame-local pick data is written through
        SystemMapRenderContext into SystemMapFrameData.
    */
    class SystemMapSceneRenderer
    {
    public:
        void render(
            SystemMapView& view,
            SystemMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            const world::celestial::PlayerNavigationState& navigation
        ) const;

    private:
        double resolvePresentationTimeSeconds(
            SystemMapView& view,
            const world::celestial::SystemMapSnapshot& system,
            double wallNowSeconds
        ) const;
    };
}
