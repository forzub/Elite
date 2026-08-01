#pragma once

#include "src/render/types/Viewport.h"

namespace world::celestial { struct DetailMapSnapshot; }

namespace game::system_map
{
class DetailMapRenderContext;
struct DetailMapPresentation;

class DetailMapSceneRenderer
{
public:
    void render(
        const DetailMapPresentation& presentation,
        DetailMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    ) const;
};
}
