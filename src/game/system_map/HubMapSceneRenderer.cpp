/* Standalone translation unit for the local-map subsystem. */
#include "src/game/system_map/HubMapRenderContext.h"
#include "src/game/system_map/HubMapSceneRenderer.h"
#include "src/game/system_map/LocalMapPresentation.h"

namespace game::system_map
{
void HubMapSceneRenderer::render(
    const HubMapPresentation& presentation,
    HubMapRenderContext& context,
    const Viewport& viewport,
    const world::celestial::HubMapSnapshot& snapshot
) const
{
    context.renderHubMapPasses(
        presentation,
        viewport,
        snapshot
    );
}
}
