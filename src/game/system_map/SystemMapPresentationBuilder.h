#pragma once

#include "src/game/system_map/SystemMapPresentation.h"
#include "src/render/types/Viewport.h"

namespace world::celestial
{
    struct SystemMapSnapshot;
}

namespace game::system_map
{
    class SystemMapView;

    /*
        Updates persistent presentation state and builds one immutable frame.

        This is the only System-map component allowed to perform render-cycle
        synchronization such as system-change reset, initial camera fitting,
        presentation-clock advancement and stale-selection cleanup.
    */
    class SystemMapPresentationBuilder
    {
    public:
        SystemMapPresentation build(
            SystemMapView& view,
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            double wallNowSeconds
        ) const;
    };
}
