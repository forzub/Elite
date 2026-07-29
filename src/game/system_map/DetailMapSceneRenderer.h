#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct DetailMapSnapshot; }

namespace game::system_map
{
class DetailMapRenderContext;
class DetailMapView;

class DetailMapSceneRenderer
{
public:
    void render(
        DetailMapView& view,
        DetailMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) const;
};
}
