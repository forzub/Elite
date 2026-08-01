#pragma once

#include <glm/glm.hpp>

#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct GalaxyMapSnapshot;
    struct PlayerNavigationState;
}

namespace game::system_map
{
    class GalaxyMapView;
    struct GalaxyMapCameraSnapshot;
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

    private:
        void drawNavigationGrid(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const GalaxyMapCameraSnapshot& camera
        ) const;

        void drawLabels(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation,
            const glm::mat4& mvp
        ) const;

        void drawPlayerMarker(
            GalaxyMapView& view,
            GalaxyMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::GalaxyMapSnapshot& galaxy,
            const world::celestial::PlayerNavigationState& navigation,
            const glm::mat4& mvp
        ) const;
    };
}
