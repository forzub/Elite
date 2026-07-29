/* Included from SystemMapRenderer.cpp during phase 4 extraction. */
#include "src/game/system_map/DetailMapRenderContext.h"
#include "src/game/system_map/DetailMapSceneRenderer.h"
#include "src/game/system_map/DetailMapView.h"

namespace game::system_map
{
void DetailMapSceneRenderer::render(
    DetailMapView& view,
    DetailMapRenderContext& context,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& snapshot
) const
{
    view.beginScene(snapshot);
    context.renderDetailMapPasses(view, viewport, snapshot);
}
}
