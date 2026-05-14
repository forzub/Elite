#include "TacticalCollisionMonitor.h"

#include <cmath>

#include <glm/common.hpp>
#include <glm/gtx/norm.hpp>

namespace world::navigation
{

static glm::vec3 safeNormalizeOr(
    const glm::vec3& v,
    const glm::vec3& fallback
)
{
    if (glm::length2(v) < 0.000001f)
        return fallback;

    return glm::normalize(v);
}

TacticalCollisionResult evaluateTacticalCollisionRisk(
    const glm::vec3& agentPosition,
    const glm::vec3& agentVelocity,
    const NavigationAgentProfile& profile,
    const std::vector<NavigationContact>& contacts
)
{
    TacticalCollisionResult best;

    const float sensorRange2 =
        profile.sensorRange * profile.sensorRange;

    const float lookAhead =
        glm::max(profile.lookAheadTime, 0.01f);

    for (const auto& c : contacts)
    {
        const glm::vec3 toContact =
            c.center - agentPosition;

        const float distNow2 =
            glm::length2(toContact);

        if (distNow2 > sensorRange2)
            continue;

        const glm::vec3 relativeVelocity =
            c.linearVelocity - agentVelocity;

        const float rv2 =
            glm::length2(relativeVelocity);

        float tClosest = 0.0f;

        if (rv2 > 0.000001f)
        {
            tClosest =
                -glm::dot(toContact, relativeVelocity) / rv2;

            tClosest =
                glm::clamp(tClosest, 0.0f, lookAhead);
        }

        const glm::vec3 futureDelta =
            toContact + relativeVelocity * tClosest;

        const float closestDist =
            std::sqrt(glm::length2(futureDelta));

        const float spinPadding =
            glm::length(c.angularVelocity) * 0.15f;

        const float safeRadius =
            profile.bodyRadius +
            c.radius +
            spinPadding +
            profile.caution;

        const float risk =
            glm::clamp(
                1.0f - closestDist / glm::max(safeRadius, 0.001f),
                0.0f,
                1.0f
            );

        if (risk <= best.risk)
            continue;

        best.risk = risk;
        best.timeToClosest = tClosest;
        best.closestDistance = closestDist;
        best.contactEntityId = c.entityId;

        const glm::vec3 away =
            safeNormalizeOr(
                -futureDelta,
                safeNormalizeOr(agentPosition - c.center, glm::vec3(0, 1, 0))
            );

        best.avoidDirection = away;

        if (risk >= 0.80f)
            best.level = TacticalRiskLevel::Emergency;
        else if (risk >= 0.50f)
            best.level = TacticalRiskLevel::Danger;
        else if (risk >= 0.20f)
            best.level = TacticalRiskLevel::Caution;
        else
            best.level = TacticalRiskLevel::Clear;
    }

    return best;
}


TacticalCollisionResult evaluateTacticalCollisionRisk(
    const glm::vec3& agentPosition,
    const glm::vec3& agentVelocity,
    const NavigationAgentProfile& profile,
    const NavigationScene& scene
)
{
    return evaluateTacticalCollisionRisk(
        agentPosition,
        agentVelocity,
        profile,
        scene.contacts
    );
}



} // namespace world::navigation