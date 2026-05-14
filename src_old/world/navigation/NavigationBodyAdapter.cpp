#include "NavigationBodyAdapter.h"

#include "src/game/ship/core/ShipCore.h"

#include <glm/common.hpp>


namespace world::navigation
{

std::vector<NavigationBody> buildNavigationBodiesForShip(
    const game::ship::core::ShipCore& ship
)
{
    std::vector<NavigationBody> out;

    const auto& hitComponent =
        ship.hitComponent();

    bool hasBounds = false;

    glm::vec3 localMin(0.0f);
    glm::vec3 localMax(0.0f);

    for (const auto& v : hitComponent.volumes)
    {
        if (v.destroyed)
            continue;

        if (v.supportLinkVolume)
            continue;

        const glm::vec3 mn =
            v.center - v.halfSize;

        const glm::vec3 mx =
            v.center + v.halfSize;

        if (!hasBounds)
        {
            localMin = mn;
            localMax = mx;
            hasBounds = true;
        }
        else
        {
            localMin = glm::min(localMin, mn);
            localMax = glm::max(localMax, mx);
        }
    }

    if (!hasBounds)
        return out;

    NavigationBody body;

    body.debugName = "ship_main_body";

    body.shape =
        NavigationBodyShape::Box;

    body.center =
        (localMin + localMax) * 0.5f;

    body.halfExtents =
        (localMax - localMin) * 0.5f;

    body.dynamic = true;

    out.push_back(body);

    return out;
}

} // namespace world::navigation