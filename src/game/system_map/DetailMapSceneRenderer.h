#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct DetailMapSnapshot; }

namespace game::system_map
{
class DetailMapRenderContext;
class DetailMapView;
struct DetailMapPresentation;

class DetailMapSceneRenderer
{
public:
    void render(
        const DetailMapView& view,
        const DetailMapPresentation& presentation,
        DetailMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) const;
};
}
