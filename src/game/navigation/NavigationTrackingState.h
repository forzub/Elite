#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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

enum class NavigationRouteAnchorKind
{
    FreeSpace = 0,
    CelestialBody,
    Hub,
    Ship,
    Infrastructure
};

enum class NavigationRouteMapKind
{
    Galaxy = 0,
    System,
    Detail,
    Hub
};

enum class NavigationWaypointTransitKind
{
    PassThrough = 0,
    Rendezvous
};

enum class NavigationArrivalMode
{
    SafeZone = 0,
    Follow,
    Formation,
    ParadeFormation
};

struct NavigationArrivalProfile
{
    NavigationArrivalMode mode = NavigationArrivalMode::SafeZone;
    // Zero means that the future trajectory solver chooses the safe radius
    // from target size / exclusion envelope rather than exposing engineering
    // detail in the normal route UI.
    double safeDistanceMeters = 0.0;
    glm::dvec3 relativeOffsetMeters {0.0};
    bool matchVelocity = true;
    bool formationMotionLock = false;
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

    NavigationRouteAnchorKind anchorKind =
        NavigationRouteAnchorKind::FreeSpace;
    NavigationRouteMapKind authoredMap =
        NavigationRouteMapKind::System;
    int authoredSystemId = -1;
    std::string authoredBodyId;
    std::string authoredHubId;

    // Dynamic targets retain semantic identity. worldPosition is merely the
    // latest presentation fallback and may be refreshed while the object is
    // visible; the future predictor resolves targetEntityId at arrival time.
    std::string targetEntityId;
    bool dynamicTarget = false;
    // A moving ship used as an intermediate route node is not a geometric
    // point. It is a rendezvous checkpoint: the future solver intercepts the
    // predicted ship state, matches velocity, then continues to the next node.
    NavigationWaypointTransitKind transitKind =
        NavigationWaypointTransitKind::PassThrough;

    bool showOnHud = true;
    NavigationArrivalProfile arrival;

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

    void setWaypointRouteMetadata(
        const std::string& sourceObjectId,
        NavigationRouteAnchorKind anchorKind,
        NavigationRouteMapKind authoredMap,
        int authoredSystemId,
        std::string authoredBodyId = {},
        std::string authoredHubId = {},
        std::string targetEntityId = {},
        bool dynamicTarget = false
    )
    {
        auto* waypoint = findWaypoint(sourceObjectId);
        if (!waypoint)
            return;
        waypoint->anchorKind = anchorKind;
        waypoint->authoredMap = authoredMap;
        waypoint->authoredSystemId = authoredSystemId;
        waypoint->authoredBodyId = std::move(authoredBodyId);
        waypoint->authoredHubId = std::move(authoredHubId);
        waypoint->targetEntityId = std::move(targetEntityId);
        waypoint->dynamicTarget = dynamicTarget;
        waypoint->transitKind =
            anchorKind == NavigationRouteAnchorKind::Ship && dynamicTarget
                ? NavigationWaypointTransitKind::Rendezvous
                : NavigationWaypointTransitKind::PassThrough;
    }

    bool routeVisibleOnHud() const noexcept
    {
        return m_routeVisibleOnHud;
    }

    void setRouteVisibleOnHud(bool visible) noexcept
    {
        m_routeVisibleOnHud = visible;
    }

    bool hasRoute() const noexcept
    {
        return std::any_of(
            m_waypoints.begin(),
            m_waypoints.end(),
            [](const NavigationWaypoint& waypoint)
            {
                return waypoint.role != NavigationWaypointRole::None;
            }
        );
    }

    std::size_t routeSize() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            m_waypoints.begin(),
            m_waypoints.end(),
            [](const NavigationWaypoint& waypoint)
            {
                return waypoint.role != NavigationWaypointRole::None;
            }
        ));
    }

    std::vector<const NavigationWaypoint*> orderedRouteWaypoints() const
    {
        std::vector<const NavigationWaypoint*> ordered;
        ordered.reserve(routeSize());
        const NavigationWaypoint* finish = nullptr;
        for (const auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
                ordered.push_back(&waypoint);
            else if (waypoint.role == NavigationWaypointRole::Finish)
                finish = &waypoint;
        }
        std::sort(
            ordered.begin(),
            ordered.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        if (finish)
            ordered.push_back(finish);
        return ordered;
    }

    void setWaypointHudVisible(
        const std::string& sourceObjectId,
        bool visible
    )
    {
        if (auto* waypoint = findWaypoint(sourceObjectId))
            waypoint->showOnHud = visible;
    }

    void setFinishArrivalMode(NavigationArrivalMode mode)
    {
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role != NavigationWaypointRole::Finish)
                continue;
            waypoint.arrival.mode = mode;
            // Even SAFE arrives co-moving with a dynamic target; otherwise
            // the player would cross the safety envelope immediately after
            // autopilot completion. SAFE changes distance/authority, not the
            // terminal velocity-match requirement.
            waypoint.arrival.matchVelocity = true;
            waypoint.arrival.formationMotionLock =
                mode == NavigationArrivalMode::ParadeFormation;
            return;
        }
    }

    void moveIntermediateWaypoint(
        const std::string& sourceObjectId,
        int targetSequence
    )
    {
        std::vector<NavigationWaypoint*> intermediates;
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
                intermediates.push_back(&waypoint);
        }
        std::sort(
            intermediates.begin(),
            intermediates.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        const auto found = std::find_if(
            intermediates.begin(),
            intermediates.end(),
            [&](const NavigationWaypoint* waypoint)
            {
                return waypoint->sourceObjectId == sourceObjectId;
            }
        );
        if (found == intermediates.end())
            return;

        NavigationWaypoint* moving = *found;
        intermediates.erase(found);
        const int bounded = std::clamp(
            targetSequence,
            1,
            static_cast<int>(intermediates.size()) + 1
        );
        intermediates.insert(
            intermediates.begin() + (bounded - 1),
            moving
        );
        for (std::size_t i = 0; i < intermediates.size(); ++i)
            intermediates[i]->sequence = static_cast<int>(i) + 1;
    }

    void removeRouteWaypoint(const std::string& sourceObjectId)
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

    void clearRoute()
    {
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(),
                m_waypoints.end(),
                [](const NavigationWaypoint& waypoint)
                {
                    return waypoint.role != NavigationWaypointRole::None;
                }
            ),
            m_waypoints.end()
        );
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
        // Open cards are transient presentation. Route intent is not. Once a
        // candidate becomes WAYPOINT/FINISH it survives card closure and map
        // changes until the user explicitly removes it from the route.
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(),
                m_waypoints.end(),
                [&](const NavigationWaypoint& waypoint)
                {
                    return waypoint.role == NavigationWaypointRole::None &&
                           open.find(waypoint.sourceObjectId) == open.end();
                }
            ),
            m_waypoints.end()
        );
    }

    void renumberIntermediateWaypoints()
    {
        std::vector<NavigationWaypoint*> ordered;
        int nextUnassigned = 1;
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
            {
                ordered.push_back(&waypoint);
                nextUnassigned = std::max(
                    nextUnassigned,
                    waypoint.sequence + 1
                );
            }
            else
            {
                waypoint.sequence = 0;
            }
        }

        for (auto* waypoint : ordered)
        {
            if (waypoint->sequence <= 0)
                waypoint->sequence = nextUnassigned++;
        }

        std::stable_sort(
            ordered.begin(),
            ordered.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        for (std::size_t i = 0; i < ordered.size(); ++i)
            ordered[i]->sequence = static_cast<int>(i) + 1;
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
    bool m_routeVisibleOnHud = true;
};

} // namespace game::navigation
