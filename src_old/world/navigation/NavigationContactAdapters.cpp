#include "NavigationContactAdapters.h"

namespace world::navigation
{

NavigationContact makeContactFromObstacle(
    const NavigationObstacle& obstacle,
    uint32_t entityId,
    NavigationContactKind kind
)
{
    NavigationContact c;

    c.entityId = entityId;
    c.ownerEntityId = 0;
    c.kind = kind;

    c.center = obstacle.center;
    c.radius = obstacle.radius;

    c.linearVelocity = glm::vec3(0.0f);
    c.angularVelocity = glm::vec3(0.0f);

    c.danger = 1.0f;
    c.dynamic = false;
    c.predictable = true;

    return c;
}

std::vector<NavigationContact> makeContactsFromObstacles(
    const std::vector<NavigationObstacle>& obstacles,
    NavigationContactKind kind
)
{
    std::vector<NavigationContact> out;
    out.reserve(obstacles.size());

    uint32_t syntheticId = 1;

    for (const auto& obstacle : obstacles)
    {
        out.push_back(
            makeContactFromObstacle(
                obstacle,
                syntheticId++,
                kind
            )
        );
    }

    return out;
}

} // namespace world::navigation