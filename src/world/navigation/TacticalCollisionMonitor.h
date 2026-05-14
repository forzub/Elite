#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "src/world/navigation/NavigationAgentProfile.h"
#include "src/world/navigation/NavigationContact.h"
#include "src/world/navigation/NavigationScene.h"   

namespace world::navigation
{

enum class TacticalRiskLevel
{
    Clear,
    Caution,
    Danger,
    Emergency
};

struct TacticalCollisionResult
{
    TacticalRiskLevel level = TacticalRiskLevel::Clear;

    float risk = 0.0f;
    float timeToClosest = 0.0f;
    float closestDistance = 999999.0f;

    uint32_t contactEntityId = 0;

    glm::vec3 avoidDirection {0.0f};
};

TacticalCollisionResult evaluateTacticalCollisionRisk(
    const glm::vec3& agentPosition,
    const glm::vec3& agentVelocity,
    const NavigationAgentProfile& profile,
    const std::vector<NavigationContact>& contacts
);

TacticalCollisionResult evaluateTacticalCollisionRisk(
    const glm::vec3& agentPosition,
    const glm::vec3& agentVelocity,
    const NavigationAgentProfile& profile,
    const NavigationScene& scene
);

} // namespace world::navigation