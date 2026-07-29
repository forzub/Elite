#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct HubMapSnapshot; }

namespace game::system_map
{
class HubMapRenderContext;
class HubMapView;

class HubMapSceneRenderer
{
public:
    void render(
        HubMapView& view,
        HubMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) const;
};
}
