#pragma once

#include <vector>

#include "src/world/navigation/NavigationContact.h"
#include "src/world/navigation/ObstacleAvoidance.h"

namespace world::navigation
{

NavigationContact makeContactFromObstacle(
    const NavigationObstacle& obstacle,
    uint32_t entityId = 0,
    NavigationContactKind kind = NavigationContactKind::Unknown
);

std::vector<NavigationContact> makeContactsFromObstacles(
    const std::vector<NavigationObstacle>& obstacles,
    NavigationContactKind kind = NavigationContactKind::Unknown
);

} // namespace world::navigation