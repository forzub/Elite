#pragma once

#include "src/game/system_map/SystemMapSceneFrame.h"

namespace world::celestial
{
    struct SystemMapSnapshot;
}

namespace game::system_map
{
    class SystemMapView;
    struct SystemMapPresentation;

    /* Builds all CPU geometry needed by both picking and System rendering. */
    class SystemMapSceneFrameBuilder
    {
    public:
        SystemMapSceneFrame build(
            const SystemMapView& view,
            const SystemMapRenderContext& context,
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            const SystemMapPresentation& presentation
        ) const;
    };
}
