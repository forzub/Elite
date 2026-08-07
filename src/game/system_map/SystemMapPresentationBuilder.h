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

        This component performs view synchronization only. Dynamic body poses
        and rotation phases are copied from the authoritative server snapshot;
        no celestial mechanics are evaluated on the client.
    */
    class SystemMapPresentationBuilder
    {
    public:
        SystemMapPresentation build(
            SystemMapView& view,
            const Viewport& viewport,
            const world::celestial::SystemMapSnapshot& system,
            double wallNowSeconds,
            bool updateHoverPresentation = true
        ) const;
    };
}
