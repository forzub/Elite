#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct DetailMapSnapshot; }
namespace game::system_map
{
class DetailMapView;
struct DetailMapPresentation;
}

namespace game::system_map
{
class DetailMapRenderContext
{
public:
    virtual ~DetailMapRenderContext() = default;

    virtual void renderDetailMapPasses(
        const DetailMapView& view,
        const DetailMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) = 0;
};
}
