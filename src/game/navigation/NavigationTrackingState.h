#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

enum class NavigationTrackedKind
{
    TacticalObject = 0,
    CelestialBody,
    RouteWaypoint
};

enum class NavigationWaypointRole
{
    None = 0,
    Finish,
    Intermediate
};

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
    int displayIndex = 0;
    world::coordinates::WorldPosition worldPosition;
    glm::vec4 color {0.70f, 0.86f, 1.00f, 0.82f};
};

struct NavigationWaypoint
{
    std::uint64_t id = 0;
    std::string sourceObjectId;
    NavigationWaypointRole role = NavigationWaypointRole::None;
    int sequence = 0;
    int displayIndex = 0;
    world::coordinates::WorldPosition worldPosition;
    std::string address;
    std::string displayName;
};

/*
    Player-private navigation memory.

    This state is presentation/navigation intent only.  It is deliberately
    absent from replication and authoritative simulation.  Tactical tracking
    follows open map cards, celestial tracking follows open System-map body
    cards, while waypoint cards survive independently as explicit route intent.
*/
class NavigationTrackingState
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
        eraseMissingWaypoints(open);
        renumberIntermediateWaypoints();
    }

    void rememberTacticalObject(
        std::string objectId,
        std::string typeName,
        std::string displayName,
        const glm::vec4& color,
        int displayIndex = 0
    )
    {
        if (objectId.empty())
            return;

        NavigationTrackedTacticalObject tracked;
        tracked.objectId = std::move(objectId);
        tracked.typeName = std::move(typeName);
        tracked.displayName = std::move(displayName);
        tracked.displayIndex = tracked.objectId == "player"
            ? 0
            : adoptDisplayIndex(tracked.objectId, displayIndex);
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
        const glm::vec4& color,
        int displayIndex = 0
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
        tracked.displayIndex = adoptDisplayIndex(tracked.trackingId, displayIndex);
        tracked.worldPosition = worldPosition;
        tracked.color = color;
        m_celestialBodies[tracked.trackingId] = std::move(tracked);
    }

    NavigationWaypoint& rememberWaypointCandidate(
        std::string sourceObjectId,
        const world::coordinates::WorldPosition& worldPosition,
        std::string address,
        std::string displayName = "Space target"
    )
    {
        if (sourceObjectId.empty())
            throw std::runtime_error("waypoint sourceObjectId must not be empty");

        auto found = std::find_if(
            m_waypoints.begin(),
            m_waypoints.end(),
            [&](const NavigationWaypoint& waypoint)
            {
                return waypoint.sourceObjectId == sourceObjectId;
            }
        );

        if (found == m_waypoints.end())
        {
            NavigationWaypoint waypoint;
            waypoint.id = m_nextWaypointId++;
            waypoint.sourceObjectId = sourceObjectId;
            waypoint.displayIndex = ensureDisplayIndex(sourceObjectId);
            m_waypoints.push_back(std::move(waypoint));
            found = std::prev(m_waypoints.end());
        }

        found->worldPosition = worldPosition;
        found->address = std::move(address);
        found->displayName = std::move(displayName);
        return *found;
    }

    void toggleWaypointRole(
        const std::string& sourceObjectId,
        NavigationWaypointRole role
    )
    {
        auto* waypoint = findWaypoint(sourceObjectId);
        if (!waypoint)
            return;

        if (role == NavigationWaypointRole::Finish)
        {
            if (waypoint->role == NavigationWaypointRole::Finish)
            {
                waypoint->role = NavigationWaypointRole::None;
                renumberIntermediateWaypoints();
                return;
            }

            for (auto& existing : m_waypoints)
            {
                if (existing.role == NavigationWaypointRole::Finish)
                    existing.role = NavigationWaypointRole::None;
            }
            waypoint->role = NavigationWaypointRole::Finish;
            renumberIntermediateWaypoints();
            return;
        }

        if (role == NavigationWaypointRole::Intermediate)
        {
            waypoint->role =
                waypoint->role == NavigationWaypointRole::Intermediate
                    ? NavigationWaypointRole::None
                    : NavigationWaypointRole::Intermediate;
            renumberIntermediateWaypoints();
        }
    }

    bool hasFinishWaypoint() const noexcept
    {
        return std::any_of(
            m_waypoints.begin(),
            m_waypoints.end(),
            [](const NavigationWaypoint& waypoint)
            {
                return waypoint.role == NavigationWaypointRole::Finish;
            }
        );
    }

    const NavigationWaypoint* findWaypoint(
        const std::string& sourceObjectId
    ) const
    {
        const auto it = std::find_if(
            m_waypoints.begin(),
            m_waypoints.end(),
            [&](const NavigationWaypoint& waypoint)
            {
                return waypoint.sourceObjectId == sourceObjectId;
            }
        );
        return it == m_waypoints.end() ? nullptr : &(*it);
    }

    NavigationWaypoint* findWaypoint(
        const std::string& sourceObjectId
    )
    {
        return const_cast<NavigationWaypoint*>(
            static_cast<const NavigationTrackingState*>(this)->findWaypoint(sourceObjectId)
        );
    }

    void forgetWaypointCandidate(const std::string& sourceObjectId)
    {
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(),
                m_waypoints.end(),
                [&](const NavigationWaypoint& waypoint)
                {
                    return waypoint.sourceObjectId == sourceObjectId;
                }
            ),
            m_waypoints.end()
        );
        renumberIntermediateWaypoints();
    }


    NavigationWaypoint& setFinishWaypoint(
        const world::coordinates::WorldPosition& worldPosition,
        std::string address,
        std::string displayName = "Finish"
    )
    {
        auto& waypoint = rememberWaypointCandidate(
            "finish:legacy",
            worldPosition,
            std::move(address),
            std::move(displayName)
        );
        if (waypoint.role != NavigationWaypointRole::Finish)
        {
            toggleWaypointRole(
                waypoint.sourceObjectId,
                NavigationWaypointRole::Finish
            );
        }
        return waypoint;
    }

    const std::unordered_map<std::string, NavigationTrackedTacticalObject>&
    tacticalObjects() const noexcept
    {
        return m_tacticalObjects;
    }

    const std::unordered_map<std::string, NavigationTrackedCelestialBody>&
    celestialBodies() const noexcept
    {
        return m_celestialBodies;
    }

    const std::vector<NavigationWaypoint>& waypoints() const noexcept
    {
        return m_waypoints;
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

    void eraseMissingWaypoints(const std::unordered_set<std::string>& open)
    {
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(),
                m_waypoints.end(),
                [&](const NavigationWaypoint& waypoint)
                {
                    return open.find(waypoint.sourceObjectId) == open.end();
                }
            ),
            m_waypoints.end()
        );
    }

    void renumberIntermediateWaypoints()
    {
        int nextSequence = 1;
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
                waypoint.sequence = nextSequence++;
            else
                waypoint.sequence = 0;
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
    std::vector<NavigationWaypoint> m_waypoints;
    std::unordered_map<std::string, int> m_displayIndices;
    std::uint64_t m_nextWaypointId = 1;
    int m_nextDisplayIndex = 1;
};

} // namespace game::navigation
