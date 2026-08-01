#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct HubMapSnapshot; }

namespace game::system_map
{
class HubMapRenderContext;
class HubMapView;
struct HubMapPresentation;

class HubMapSceneRenderer
{
public:
    void render(
        const HubMapView& view,
        const HubMapPresentation& presentation,
        HubMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::HubMapSnapshot& snapshot
    ) const;
};
}
