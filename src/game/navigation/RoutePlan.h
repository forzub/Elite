#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/identity/ShipInstanceId.h"
#include "src/world/coordinates/WorldPosition.h"

namespace game::navigation
{

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
    double safeDistanceMeters = 0.0;
    glm::dvec3 relativeOffsetMeters {0.0};
    bool matchVelocity = true;
    bool formationMotionLock = false;
};

/*
    Durable semantic identity of a route target.

    This is intentionally independent from renderer/card/object IDs. Ships use
    ShipInstanceId; the materialized simulation entity handle is transient and is
    never stored here. Hubs/bodies/infrastructure use stable authored/domain
    IDs. Free-space nodes use canonical WorldPosition.
*/
struct RouteTargetRef
{
    NavigationRouteAnchorKind kind = NavigationRouteAnchorKind::FreeSpace;
    int systemId = -1;
    ShipInstanceId shipInstanceId = 0;
    std::string stableObjectId;
    world::coordinates::WorldPosition spatialWorldPosition;

    bool valid() const noexcept
    {
        switch (kind)
        {
            case NavigationRouteAnchorKind::Ship:
                return shipInstanceId != 0;
            case NavigationRouteAnchorKind::CelestialBody:
            case NavigationRouteAnchorKind::Hub:
            case NavigationRouteAnchorKind::Infrastructure:
                return !stableObjectId.empty();
            case NavigationRouteAnchorKind::FreeSpace:
                return true;
        }
        return false;
    }
};

inline bool sameWorldPosition(
    const world::coordinates::WorldPosition& a,
    const world::coordinates::WorldPosition& b
) noexcept
{
    return a.cell.x == b.cell.x &&
           a.cell.y == b.cell.y &&
           a.cell.z == b.cell.z &&
           a.localMeters.x == b.localMeters.x &&
           a.localMeters.y == b.localMeters.y &&
           a.localMeters.z == b.localMeters.z;
}

inline bool sameRouteTarget(
    const RouteTargetRef& a,
    const RouteTargetRef& b
) noexcept
{
    if (a.kind != b.kind)
        return false;

    switch (a.kind)
    {
        case NavigationRouteAnchorKind::Ship:
            return a.shipInstanceId != 0 &&
                   a.shipInstanceId == b.shipInstanceId;
        case NavigationRouteAnchorKind::CelestialBody:
        case NavigationRouteAnchorKind::Hub:
        case NavigationRouteAnchorKind::Infrastructure:
            return a.systemId == b.systemId &&
                   !a.stableObjectId.empty() &&
                   a.stableObjectId == b.stableObjectId;
        case NavigationRouteAnchorKind::FreeSpace:
            return sameWorldPosition(
                a.spatialWorldPosition,
                b.spatialWorldPosition
            );
    }
    return false;
}

struct NavigationWaypoint
{
    std::uint64_t id = 0;

    // Mutable presentation binding only. It may change after rematerialization
    // without changing route identity.
    std::string sourceObjectId;
    RouteTargetRef target;

    NavigationWaypointRole role = NavigationWaypointRole::None;
    int sequence = 0;

    NavigationRouteMapKind authoredMap = NavigationRouteMapKind::System;
    int authoredSystemId = -1;
    std::string authoredBodyId;
    std::string authoredHubId;

    bool dynamicTarget = false;
    NavigationWaypointTransitKind transitKind =
        NavigationWaypointTransitKind::PassThrough;

    bool showOnHud = true;
    NavigationArrivalProfile arrival;

    // Latest presentation fallback. It is not semantic identity.
    world::coordinates::WorldPosition worldPosition;
    std::string address;
    std::string displayName;
};

class RoutePlan
{
public:
    NavigationWaypoint& rememberCandidate(
        const RouteTargetRef& target,
        std::string sourceObjectId,
        const world::coordinates::WorldPosition& worldPosition,
        std::string address,
        std::string displayName = {}
    )
    {
        if (!target.valid())
            throw std::runtime_error("route target identity is invalid");

        NavigationWaypoint* waypoint = findByTarget(target);

        if (!waypoint)
        {
            NavigationWaypoint created;
            created.id = m_nextWaypointId++;
            created.target = target;
            m_waypoints.push_back(std::move(created));
            waypoint = &m_waypoints.back();
        }

        waypoint->target = target;
        waypoint->sourceObjectId = std::move(sourceObjectId);
        waypoint->worldPosition = worldPosition;
        waypoint->address = std::move(address);
        waypoint->displayName = std::move(displayName);
        return *waypoint;
    }

    NavigationWaypoint* findById(std::uint64_t id)
    {
        const auto it = std::find_if(
            m_waypoints.begin(), m_waypoints.end(),
            [&](const NavigationWaypoint& waypoint) { return waypoint.id == id; }
        );
        return it == m_waypoints.end() ? nullptr : &*it;
    }

    const NavigationWaypoint* findById(std::uint64_t id) const
    {
        return const_cast<RoutePlan*>(this)->findById(id);
    }

    NavigationWaypoint* findBySourceObjectId(const std::string& sourceObjectId)
    {
        const auto it = std::find_if(
            m_waypoints.begin(), m_waypoints.end(),
            [&](const NavigationWaypoint& waypoint)
            {
                return !sourceObjectId.empty() &&
                       waypoint.sourceObjectId == sourceObjectId;
            }
        );
        return it == m_waypoints.end() ? nullptr : &*it;
    }

    const NavigationWaypoint* findBySourceObjectId(
        const std::string& sourceObjectId
    ) const
    {
        return const_cast<RoutePlan*>(this)->findBySourceObjectId(sourceObjectId);
    }

    NavigationWaypoint* findByTarget(const RouteTargetRef& target)
    {
        const auto it = std::find_if(
            m_waypoints.begin(), m_waypoints.end(),
            [&](const NavigationWaypoint& waypoint)
            {
                return sameRouteTarget(waypoint.target, target);
            }
        );
        return it == m_waypoints.end() ? nullptr : &*it;
    }

    const NavigationWaypoint* findByTarget(const RouteTargetRef& target) const
    {
        return const_cast<RoutePlan*>(this)->findByTarget(target);
    }

    void bindPresentationSource(
        std::uint64_t nodeId,
        std::string sourceObjectId
    )
    {
        if (auto* waypoint = findById(nodeId))
            waypoint->sourceObjectId = std::move(sourceObjectId);
    }

    void toggleRole(std::uint64_t nodeId, NavigationWaypointRole role)
    {
        auto* waypoint = findById(nodeId);
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
            m_waypoints.begin(), m_waypoints.end(),
            [](const NavigationWaypoint& waypoint)
            {
                return waypoint.role == NavigationWaypointRole::Finish;
            }
        );
    }

    bool routeVisibleOnHud() const noexcept { return m_routeVisibleOnHud; }
    void setRouteVisibleOnHud(bool visible) noexcept { m_routeVisibleOnHud = visible; }

    bool hasRoute() const noexcept
    {
        return std::any_of(
            m_waypoints.begin(), m_waypoints.end(),
            [](const NavigationWaypoint& waypoint)
            {
                return waypoint.role != NavigationWaypointRole::None;
            }
        );
    }

    std::size_t routeSize() const noexcept
    {
        return static_cast<std::size_t>(std::count_if(
            m_waypoints.begin(), m_waypoints.end(),
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
            ordered.begin(), ordered.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        if (finish)
            ordered.push_back(finish);
        return ordered;
    }

    void setWaypointHudVisible(std::uint64_t nodeId, bool visible)
    {
        if (auto* waypoint = findById(nodeId))
            waypoint->showOnHud = visible;
    }

    void setFinishArrivalMode(NavigationArrivalMode mode)
    {
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role != NavigationWaypointRole::Finish)
                continue;
            waypoint.arrival.mode = mode;
            waypoint.arrival.matchVelocity = true;
            waypoint.arrival.formationMotionLock =
                mode == NavigationArrivalMode::ParadeFormation;
            return;
        }
    }

    void moveIntermediateWaypoint(std::uint64_t nodeId, int targetSequence)
    {
        std::vector<NavigationWaypoint*> intermediates;
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
                intermediates.push_back(&waypoint);
        }
        std::sort(
            intermediates.begin(), intermediates.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        const auto found = std::find_if(
            intermediates.begin(), intermediates.end(),
            [&](const NavigationWaypoint* waypoint)
            {
                return waypoint->id == nodeId;
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
        intermediates.insert(intermediates.begin() + bounded - 1, moving);
        for (std::size_t i = 0; i < intermediates.size(); ++i)
            intermediates[i]->sequence = static_cast<int>(i) + 1;
    }

    void removeRouteWaypoint(std::uint64_t nodeId)
    {
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(), m_waypoints.end(),
                [&](const NavigationWaypoint& waypoint)
                {
                    return waypoint.id == nodeId;
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
                m_waypoints.begin(), m_waypoints.end(),
                [](const NavigationWaypoint& waypoint)
                {
                    return waypoint.role != NavigationWaypointRole::None;
                }
            ),
            m_waypoints.end()
        );
    }

    void pruneTransientCandidates(const std::vector<std::string>& openCardIds)
    {
        m_waypoints.erase(
            std::remove_if(
                m_waypoints.begin(), m_waypoints.end(),
                [&](const NavigationWaypoint& waypoint)
                {
                    return waypoint.role == NavigationWaypointRole::None &&
                        std::find(
                            openCardIds.begin(), openCardIds.end(),
                            waypoint.sourceObjectId
                        ) == openCardIds.end();
                }
            ),
            m_waypoints.end()
        );
    }

    const std::vector<NavigationWaypoint>& waypoints() const noexcept
    {
        return m_waypoints;
    }

private:
    void renumberIntermediateWaypoints()
    {
        std::vector<NavigationWaypoint*> ordered;
        int nextUnassigned = 1;
        for (auto& waypoint : m_waypoints)
        {
            if (waypoint.role == NavigationWaypointRole::Intermediate)
            {
                ordered.push_back(&waypoint);
                nextUnassigned = std::max(nextUnassigned, waypoint.sequence + 1);
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
            ordered.begin(), ordered.end(),
            [](const NavigationWaypoint* a, const NavigationWaypoint* b)
            {
                return a->sequence < b->sequence;
            }
        );
        for (std::size_t i = 0; i < ordered.size(); ++i)
            ordered[i]->sequence = static_cast<int>(i) + 1;
    }

private:
    std::vector<NavigationWaypoint> m_waypoints;
    std::uint64_t m_nextWaypointId = 1;
    bool m_routeVisibleOnHud = true;
};

} // namespace game::navigation
