#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct HubMapSnapshot; }
namespace game::system_map
{
class HubMapView;
struct HubMapPresentation;
}

namespace game::system_map
{
class HubMapRenderContext
{
public:
    virtual ~HubMapRenderContext() = default;

    virtual void renderHubMapPasses(
        const HubMapView& view,
        const HubMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) = 0;
};
}
