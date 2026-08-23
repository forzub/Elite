#pragma once

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

struct NavigationTrackedTacticalObject
{
    std::string objectId;
    std::string typeName;
    std::string displayName;
    int displayIndex = 0;
    glm::vec4 color {0.70f, 0.90f, 1.00f, 0.82f};
};

struct NavigationTrackedCelestialBody
{
    std::string trackingId;
    int systemId = -1;
    std::string bodyId;
    std::string typeName;
    std::string displayName;
    world::coordinates::WorldPosition worldPosition;
    glm::vec4 color {0.70f, 0.86f, 1.00f, 0.82f};
};

struct NavigationTrackedInfrastructure
{
    std::string trackingId;
    int systemId = -1;
    std::string stableObjectId;
    std::string typeName;
    std::string displayName;
    glm::vec4 color {0.70f, 0.86f, 1.00f, 0.82f};
};

struct NavigationTrackedSemanticAnchor
{
    std::string trackingId;
    int systemId = -1;
    std::string hubModuleId;
    std::string anchorId;
    std::string typeName;
    std::string displayName;
    glm::vec4 color {0.42f, 0.96f, 0.72f, 0.86f};
};

/*
    Transient client presentation tracking.

    Open tactical/celestial cards may contribute HUD markers, but this state is
    not player route intent and may be reconciled against card lifetime. Target
    numbers are deliberately allocated only to ships; hubs and celestial bodies
    may still be tracked but never receive a target number.
*/
class TargetTrackingState
{
public:
    void reconcileOpenCards(const std::vector<std::string>& openCardIds)
    {
        const std::unordered_set<std::string> open(
            openCardIds.begin(),
            openCardIds.end()
        );
        eraseMissing(m_tacticalObjects, open);
        eraseMissing(m_celestialBodies, open);
        eraseMissing(m_infrastructure, open);
        eraseMissing(m_semanticAnchors, open);
    }

    void rememberTacticalObject(
        std::string objectId,
        std::string typeName,
        std::string displayName,
        const glm::vec4& color,
        bool numberedShipTarget,
        int preferredDisplayIndex = 0
    )
    {
        if (objectId.empty())
            return;

        NavigationTrackedTacticalObject tracked;
        tracked.objectId = std::move(objectId);
        tracked.typeName = std::move(typeName);
        tracked.displayName = std::move(displayName);
        tracked.displayIndex =
            tracked.objectId == "player" || !numberedShipTarget
                ? 0
                : adoptDisplayIndex(tracked.objectId, preferredDisplayIndex);
        tracked.color = color;
        m_tacticalObjects[tracked.objectId] = std::move(tracked);
    }

    void rememberCelestialBody(
        std::string trackingId,
        int systemId,
        std::string bodyId,
        std::string typeName,
        std::string displayName,
        const world::coordinates::WorldPosition& worldPosition,
        const glm::vec4& color
    )
    {
        if (trackingId.empty() || bodyId.empty())
            return;

        NavigationTrackedCelestialBody tracked;
        tracked.trackingId = std::move(trackingId);
        tracked.systemId = systemId;
        tracked.bodyId = std::move(bodyId);
        tracked.typeName = std::move(typeName);
        tracked.displayName = std::move(displayName);
        tracked.worldPosition = worldPosition;
        tracked.color = color;
        m_celestialBodies[tracked.trackingId] = std::move(tracked);
    }

    const std::unordered_map<std::string, NavigationTrackedTacticalObject>&
    tacticalObjects() const noexcept
    {
        return m_tacticalObjects;
    }


    void rememberInfrastructure(
        std::string trackingId,
        int systemId,
        std::string stableObjectId,
        std::string typeName,
        std::string displayName,
        const glm::vec4& color
    )
    {
        if (trackingId.empty() || stableObjectId.empty())
            return;

        NavigationTrackedInfrastructure tracked;
        tracked.trackingId = std::move(trackingId);
        tracked.systemId = systemId;
        tracked.stableObjectId = std::move(stableObjectId);
        tracked.typeName = std::move(typeName);
        tracked.displayName = std::move(displayName);
        tracked.color = color;
        m_infrastructure[tracked.trackingId] = std::move(tracked);
    }

    void rememberSemanticAnchor(
        std::string trackingId,
        int systemId,
        std::string hubModuleId,
        std::string anchorId,
        std::string typeName,
        std::string displayName,
        const glm::vec4& color
    )
    {
        if (trackingId.empty() || hubModuleId.empty() || anchorId.empty())
            return;

        NavigationTrackedSemanticAnchor tracked;
        tracked.trackingId = std::move(trackingId);
        tracked.systemId = systemId;
        tracked.hubModuleId = std::move(hubModuleId);
        tracked.anchorId = std::move(anchorId);
        tracked.typeName = std::move(typeName);
        tracked.displayName = std::move(displayName);
        tracked.color = color;
        m_semanticAnchors[tracked.trackingId] = std::move(tracked);
    }

    const std::unordered_map<std::string, NavigationTrackedCelestialBody>&
    celestialBodies() const noexcept
    {
        return m_celestialBodies;
    }

    const std::unordered_map<std::string, NavigationTrackedInfrastructure>&
    infrastructure() const noexcept
    {
        return m_infrastructure;
    }

    const std::unordered_map<std::string, NavigationTrackedSemanticAnchor>&
    semanticAnchors() const noexcept
    {
        return m_semanticAnchors;
    }

private:
    template <typename MapT>
    static void eraseMissing(
        MapT& values,
        const std::unordered_set<std::string>& open
    )
    {
        for (auto it = values.begin(); it != values.end();)
        {
            if (open.find(it->first) == open.end())
                it = values.erase(it);
            else
                ++it;
        }
    }

    int adoptDisplayIndex(const std::string& stableId, int preferred)
    {
        if (preferred > 0)
        {
            m_displayIndices[stableId] = preferred;
            m_nextDisplayIndex = std::max(m_nextDisplayIndex, preferred + 1);
            return preferred;
        }
        return ensureDisplayIndex(stableId);
    }

    int ensureDisplayIndex(const std::string& stableId)
    {
        const auto found = m_displayIndices.find(stableId);
        if (found != m_displayIndices.end())
            return found->second;

        const int assigned = m_nextDisplayIndex++;
        m_displayIndices.emplace(stableId, assigned);
        return assigned;
    }

private:
    std::unordered_map<std::string, NavigationTrackedTacticalObject>
        m_tacticalObjects;
    std::unordered_map<std::string, NavigationTrackedCelestialBody>
        m_celestialBodies;
    std::unordered_map<std::string, NavigationTrackedInfrastructure>
        m_infrastructure;
    std::unordered_map<std::string, NavigationTrackedSemanticAnchor>
        m_semanticAnchors;
    std::unordered_map<std::string, int> m_displayIndices;
    int m_nextDisplayIndex = 1;
};

} // namespace game::navigation
