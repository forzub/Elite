#include "NavigationSpace.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <stdexcept>
#include <utility>

namespace world::navigation
{
namespace
{

using Vec3d = NavigationSpace::Vec3d;
using Bounds3d = NavigationSpace::Bounds3d;
using AgentEnvelope = NavigationSpace::AgentEnvelope;
using RegionId = NavigationSpace::RegionId;
using PortalId = NavigationSpace::PortalId;

bool finite(double value) noexcept
{
    return std::isfinite(value);
}

bool finite(const Vec3d& v) noexcept
{
    return finite(v.x) && finite(v.y) && finite(v.z);
}

bool validBounds(const Bounds3d& bounds) noexcept
{
    return finite(bounds.minMapMeters) && finite(bounds.maxMapMeters) &&
        bounds.minMapMeters.x <= bounds.maxMapMeters.x &&
        bounds.minMapMeters.y <= bounds.maxMapMeters.y &&
        bounds.minMapMeters.z <= bounds.maxMapMeters.z;
}

bool contains(const Bounds3d& bounds, const Vec3d& p) noexcept
{
    return p.x >= bounds.minMapMeters.x && p.x <= bounds.maxMapMeters.x &&
        p.y >= bounds.minMapMeters.y && p.y <= bounds.maxMapMeters.y &&
        p.z >= bounds.minMapMeters.z && p.z <= bounds.maxMapMeters.z;
}

bool intersects(const Bounds3d& a, const Bounds3d& b) noexcept
{
    return
        a.minMapMeters.x <= b.maxMapMeters.x && a.maxMapMeters.x >= b.minMapMeters.x &&
        a.minMapMeters.y <= b.maxMapMeters.y && a.maxMapMeters.y >= b.minMapMeters.y &&
        a.minMapMeters.z <= b.maxMapMeters.z && a.maxMapMeters.z >= b.minMapMeters.z;
}

double pointClearance(const Bounds3d& bounds, const Vec3d& p) noexcept
{
    if (!contains(bounds, p))
        return 0.0;

    return std::min({
        p.x - bounds.minMapMeters.x,
        bounds.maxMapMeters.x - p.x,
        p.y - bounds.minMapMeters.y,
        bounds.maxMapMeters.y - p.y,
        p.z - bounds.minMapMeters.z,
        bounds.maxMapMeters.z - p.z
    });
}

double regionCapacity(
    const NavigationSpace::RegionInput& region
) noexcept
{
    const auto& b = region.boundsMapMeters;
    const double halfX = 0.5 * (b.maxMapMeters.x - b.minMapMeters.x);
    const double halfY = 0.5 * (b.maxMapMeters.y - b.minMapMeters.y);
    const double halfZ = 0.5 * (b.maxMapMeters.z - b.minMapMeters.z);
    return std::min({
        region.clearanceRadiusMeters,
        halfX,
        halfY,
        halfZ
    });
}

double requiredClearance(const AgentEnvelope& envelope)
{
    if (!finite(envelope.radiusMeters) ||
        !finite(envelope.additionalClearanceMeters) ||
        envelope.radiusMeters < 0.0 ||
        envelope.additionalClearanceMeters < 0.0)
    {
        throw std::invalid_argument("NavigationSpace agent envelope must be finite and non-negative");
    }

    return envelope.radiusMeters + envelope.additionalClearanceMeters;
}

void validateRegion(const NavigationSpace::RegionInput& region)
{
    if (region.regionId == 0)
        throw std::invalid_argument("NavigationSpace region id must be non-zero");
    if (!validBounds(region.boundsMapMeters))
        throw std::invalid_argument("NavigationSpace region bounds are invalid");
    if (!finite(region.clearanceRadiusMeters) || region.clearanceRadiusMeters < 0.0)
        throw std::invalid_argument("NavigationSpace region clearance must be finite and non-negative");
}

void validatePortal(const NavigationSpace::PortalInput& portal)
{
    if (portal.portalId == 0)
        throw std::invalid_argument("NavigationSpace portal id must be non-zero");
    if (portal.regionA == 0 || portal.regionB == 0 || portal.regionA == portal.regionB)
        throw std::invalid_argument("NavigationSpace portal endpoints are invalid");
    if (!finite(portal.centerMapMeters))
        throw std::invalid_argument("NavigationSpace portal center must be finite");
    if (!finite(portal.clearanceRadiusMeters) || portal.clearanceRadiusMeters < 0.0)
        throw std::invalid_argument("NavigationSpace portal clearance must be finite and non-negative");
}

} // namespace

class NavigationSpace::Impl
{
public:
    struct RegionState
    {
        RegionInput input;
        bool invalidated = false;
    };

    struct PortalState
    {
        PortalInput input;
        bool invalidated = false;
    };

    Revision spaceRevision = 0;
    Revision sourceRevision = 0;
    std::map<RegionId, RegionState> regions;
    std::map<PortalId, PortalState> portals;

    static void validatePortalReferences(
        const std::map<RegionId, RegionState>& regions,
        const std::map<PortalId, PortalState>& portals
    )
    {
        for (const auto& entry : portals)
        {
            const auto& portal = entry.second.input;
            if (regions.find(portal.regionA) == regions.end() ||
                regions.find(portal.regionB) == regions.end())
            {
                throw std::invalid_argument(
                    "NavigationSpace portal references an unknown region"
                );
            }
        }
    }

    const RegionState* findTraversableRegion(
        const Vec3d& point,
        double required,
        std::size_t* examined = nullptr
    ) const
    {
        for (const auto& entry : regions)
        {
            if (examined)
                ++(*examined);

            const RegionState& state = entry.second;
            if (state.invalidated || !contains(state.input.boundsMapMeters, point))
                continue;

            const double available = std::min(
                pointClearance(state.input.boundsMapMeters, point),
                state.input.clearanceRadiusMeters
            );
            if (available >= required)
                return &state;
        }
        return nullptr;
    }
};

NavigationSpace::NavigationSpace()
    : impl_(std::make_unique<Impl>())
{
}

NavigationSpace::~NavigationSpace() = default;
NavigationSpace::NavigationSpace(NavigationSpace&&) noexcept = default;
NavigationSpace& NavigationSpace::operator=(NavigationSpace&&) noexcept = default;

void NavigationSpace::replaceStaticWorld(StaticSpaceUpdate update)
{
    std::map<RegionId, Impl::RegionState> regions;
    std::map<PortalId, Impl::PortalState> portals;

    for (const auto& region : update.regions)
    {
        validateRegion(region);
        if (!regions.emplace(region.regionId, Impl::RegionState{region, false}).second)
            throw std::invalid_argument("NavigationSpace duplicate region id");
    }

    for (const auto& portal : update.portals)
    {
        validatePortal(portal);
        if (!portals.emplace(portal.portalId, Impl::PortalState{portal, false}).second)
            throw std::invalid_argument("NavigationSpace duplicate portal id");
    }

    Impl::validatePortalReferences(regions, portals);

    impl_->regions = std::move(regions);
    impl_->portals = std::move(portals);
    impl_->sourceRevision = update.sourceRevision;
    ++impl_->spaceRevision;
}

void NavigationSpace::applyLocalPatch(LocalPatch patch)
{
    auto regions = impl_->regions;
    auto portals = impl_->portals;

    for (PortalId portalId : patch.removePortalIds)
        portals.erase(portalId);

    for (RegionId regionId : patch.removeRegionIds)
    {
        regions.erase(regionId);
        for (auto it = portals.begin(); it != portals.end();)
        {
            const auto& portal = it->second.input;
            if (portal.regionA == regionId || portal.regionB == regionId)
                it = portals.erase(it);
            else
                ++it;
        }
    }

    for (const auto& region : patch.upsertRegions)
    {
        validateRegion(region);
        regions[region.regionId] = Impl::RegionState{region, false};
    }

    for (const auto& portal : patch.upsertPortals)
    {
        validatePortal(portal);
        portals[portal.portalId] = Impl::PortalState{portal, false};
    }

    Impl::validatePortalReferences(regions, portals);

    impl_->regions = std::move(regions);
    impl_->portals = std::move(portals);
    impl_->sourceRevision = patch.sourceRevision;
    ++impl_->spaceRevision;
}

NavigationSpace::InvalidationResult NavigationSpace::invalidateBounds(
    const Bounds3d& boundsMapMeters,
    Revision sourceRevision
)
{
    if (!validBounds(boundsMapMeters))
        throw std::invalid_argument("NavigationSpace invalidation bounds are invalid");

    InvalidationResult result;

    for (auto& entry : impl_->regions)
    {
        auto& state = entry.second;
        if (!state.invalidated && intersects(state.input.boundsMapMeters, boundsMapMeters))
        {
            state.invalidated = true;
            result.invalidatedRegionIds.push_back(entry.first);
        }
    }

    for (auto& entry : impl_->portals)
    {
        auto& state = entry.second;
        const bool endpointInvalid =
            impl_->regions.at(state.input.regionA).invalidated ||
            impl_->regions.at(state.input.regionB).invalidated;
        if (!state.invalidated && endpointInvalid)
        {
            state.invalidated = true;
            result.invalidatedPortalIds.push_back(entry.first);
        }
    }

    if (!result.invalidatedRegionIds.empty() || !result.invalidatedPortalIds.empty())
    {
        impl_->sourceRevision = sourceRevision;
        ++impl_->spaceRevision;
    }

    result.spaceRevision = impl_->spaceRevision;
    result.sourceRevision = impl_->sourceRevision;
    return result;
}

NavigationSpace::PointQueryResult NavigationSpace::queryPoint(
    const PointQuery& query
) const
{
    const double required = requiredClearance(query.envelope);

    PointQueryResult result;
    result.spaceRevision = impl_->spaceRevision;
    result.sourceRevision = impl_->sourceRevision;

    RegionId firstContaining = 0;
    double firstAvailable = 0.0;

    for (const auto& entry : impl_->regions)
    {
        ++result.regionsExamined;
        const auto& state = entry.second;
        if (state.invalidated || !contains(state.input.boundsMapMeters, query.pointMapMeters))
            continue;

        const double available = std::min(
            pointClearance(state.input.boundsMapMeters, query.pointMapMeters),
            state.input.clearanceRadiusMeters
        );

        if (firstContaining == 0)
        {
            firstContaining = entry.first;
            firstAvailable = available;
        }

        if (available >= required)
        {
            result.traversable = true;
            result.regionId = entry.first;
            result.availableClearanceMeters = available;
            return result;
        }
    }

    result.regionId = firstContaining;
    result.availableClearanceMeters = firstAvailable;
    return result;
}

NavigationSpace::CorridorResult NavigationSpace::queryCorridor(
    const CorridorQuery& query
) const
{
    const double required = requiredClearance(query.envelope);

    CorridorResult result;
    result.spaceRevision = impl_->spaceRevision;
    result.sourceRevision = impl_->sourceRevision;

    std::size_t locateExamined = 0;
    const Impl::RegionState* startState = impl_->findTraversableRegion(
        query.startMapMeters,
        required,
        &locateExamined
    );
    const Impl::RegionState* endState = impl_->findTraversableRegion(
        query.endMapMeters,
        required,
        &locateExamined
    );
    result.diagnostics.regionsVisited += locateExamined;

    if (!startState || !endState)
        return result;

    const RegionId startId = startState->input.regionId;
    const RegionId endId = endState->input.regionId;

    if (startId == endId)
    {
        result.found = true;
        result.regionPath.push_back(startId);
        return result;
    }

    struct Prev
    {
        RegionId regionId = 0;
        PortalId portalId = 0;
    };

    std::queue<RegionId> frontier;
    std::map<RegionId, bool> visited;
    std::map<RegionId, Prev> previous;

    frontier.push(startId);
    visited[startId] = true;

    while (!frontier.empty())
    {
        const RegionId current = frontier.front();
        frontier.pop();
        ++result.diagnostics.regionsVisited;

        for (const auto& portalEntry : impl_->portals)
        {
            ++result.diagnostics.portalsExamined;
            const PortalId portalId = portalEntry.first;
            const auto& portalState = portalEntry.second;
            const auto& portal = portalState.input;

            if (portalState.invalidated || portal.clearanceRadiusMeters < required)
                continue;

            RegionId neighbor = 0;
            if (portal.regionA == current)
                neighbor = portal.regionB;
            else if (portal.bidirectional && portal.regionB == current)
                neighbor = portal.regionA;
            else
                continue;

            const auto regionIt = impl_->regions.find(neighbor);
            if (regionIt == impl_->regions.end() || regionIt->second.invalidated)
                continue;
            if (regionCapacity(regionIt->second.input) < required)
                continue;
            if (visited[neighbor])
                continue;

            visited[neighbor] = true;
            previous[neighbor] = Prev{current, portalId};

            if (neighbor == endId)
            {
                std::vector<RegionId> reverseRegions;
                std::vector<PortalId> reversePortals;

                RegionId cursor = endId;
                reverseRegions.push_back(cursor);
                while (cursor != startId)
                {
                    const Prev prev = previous.at(cursor);
                    reversePortals.push_back(prev.portalId);
                    cursor = prev.regionId;
                    reverseRegions.push_back(cursor);
                }

                result.regionPath.assign(reverseRegions.rbegin(), reverseRegions.rend());
                result.portalPath.assign(reversePortals.rbegin(), reversePortals.rend());
                result.found = true;
                return result;
            }

            frontier.push(neighbor);
        }
    }

    return result;
}

NavigationSpace::Stats NavigationSpace::stats() const noexcept
{
    Stats result;
    result.spaceRevision = impl_->spaceRevision;
    result.sourceRevision = impl_->sourceRevision;
    result.regionCount = impl_->regions.size();
    result.portalCount = impl_->portals.size();

    for (const auto& entry : impl_->regions)
    {
        if (entry.second.invalidated)
            ++result.invalidatedRegionCount;
    }
    for (const auto& entry : impl_->portals)
    {
        if (entry.second.invalidated)
            ++result.invalidatedPortalCount;
    }

    return result;
}

} // namespace world::navigation
