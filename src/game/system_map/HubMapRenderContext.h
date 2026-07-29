#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct HubMapSnapshot; }
namespace game::system_map { class HubMapView; }

namespace game::system_map
{
class HubMapRenderContext
{
public:
    virtual ~HubMapRenderContext() = default;

    virtual void renderHubMapPasses(
        HubMapView& view,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) = 0;
};
}
