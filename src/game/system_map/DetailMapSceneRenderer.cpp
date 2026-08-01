/* Standalone translation unit for the local-map subsystem. */
#include "src/game/system_map/DetailMapRenderContext.h"
#include "src/game/system_map/DetailMapSceneRenderer.h"
#include "src/game/system_map/LocalMapPresentation.h"

namespace game::system_map
{
void DetailMapSceneRenderer::render(
    const DetailMapPresentation& presentation,
    DetailMapRenderContext& context,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& snapshot
) const
{
    context.renderDetailMapPasses(
        presentation,
        viewport,
        snapshot
    );
}
}
