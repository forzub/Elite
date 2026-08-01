/* Standalone translation unit for the local-map subsystem. */
#include "src/game/system_map/DetailMapRenderContext.h"
#include "src/game/system_map/DetailMapSceneRenderer.h"
#include "src/game/system_map/DetailMapView.h"
#include "src/game/system_map/LocalMapPresentation.h"

namespace game::system_map
{
void DetailMapSceneRenderer::render(
    const DetailMapView& view,
    const DetailMapPresentation& presentation,
    DetailMapRenderContext& context,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& snapshot
) const
{
    context.renderDetailMapPasses(
        view,
        presentation,
        viewport,
        snapshot
    );
}
}
