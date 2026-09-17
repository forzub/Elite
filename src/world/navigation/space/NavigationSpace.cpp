#include "NavigationSpace.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

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

Bounds3d mergeBounds(const Bounds3d& a, const Bounds3d& b) noexcept
{
    return {
        {
            std::min(a.minMapMeters.x, b.minMapMeters.x),
            std::min(a.minMapMeters.y, b.minMapMeters.y),
            std::min(a.minMapMeters.z, b.minMapMeters.z)
        },
        {
            std::max(a.maxMapMeters.x, b.maxMapMeters.x),
            std::max(a.maxMapMeters.y, b.maxMapMeters.y),
            std::max(a.maxMapMeters.z, b.maxMapMeters.z)
        }
    };
}

Vec3d boundsCenter(const Bounds3d& bounds) noexcept
{
    return {
        0.5 * (bounds.minMapMeters.x + bounds.maxMapMeters.x),
        0.5 * (bounds.minMapMeters.y + bounds.maxMapMeters.y),
        0.5 * (bounds.minMapMeters.z + bounds.maxMapMeters.z)
    };
}

double axisValue(const Vec3d& value, int axis) noexcept
{
    if (axis == 1)
        return value.y;
    if (axis == 2)
        return value.z;
    return value.x;
}

double distance(const Vec3d& a, const Vec3d& b) noexcept
{
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    const double dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

double turnAngleRadians(
    const Vec3d& incomingPortalCenter,
    const Vec3d& currentRegionCenter,
    const Vec3d& outgoingPortalCenter
) noexcept
{
    const double ax = currentRegionCenter.x - incomingPortalCenter.x;
    const double ay = currentRegionCenter.y - incomingPortalCenter.y;
    const double az = currentRegionCenter.z - incomingPortalCenter.z;
    const double bx = outgoingPortalCenter.x - currentRegionCenter.x;
    const double by = outgoingPortalCenter.y - currentRegionCenter.y;
    const double bz = outgoingPortalCenter.z - currentRegionCenter.z;

    const double lengthA = std::sqrt(ax * ax + ay * ay + az * az);
    const double lengthB = std::sqrt(bx * bx + by * by + bz * bz);
    if (lengthA <= 1.0e-12 || lengthB <= 1.0e-12)
        return 0.0;

    const double cosine = std::clamp(
        (ax * bx + ay * by + az * bz) / (lengthA * lengthB),
        -1.0,
        1.0
    );
    return std::acos(cosine);
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

void validateCostPolicy(const NavigationSpace::CorridorCostPolicy& policy)
{
    if (!finite(policy.distanceWeight) || policy.distanceWeight < 0.0 ||
        !finite(policy.preferredClearanceMultiple) ||
        policy.preferredClearanceMultiple < 1.0 ||
        !finite(policy.clearancePenaltyMeters) ||
        policy.clearancePenaltyMeters < 0.0 ||
        !finite(policy.turnPenaltyMetersPerRadian) ||
        policy.turnPenaltyMetersPerRadian < 0.0)
    {
        throw std::invalid_argument(
            "NavigationSpace corridor cost policy must be finite and non-negative; preferred clearance multiple must be >= 1"
        );
    }
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
    using RegionSlot = std::size_t;
    using PortalSlot = std::size_t;
    using TurnStateSlot = std::size_t;
    static constexpr RegionSlot InvalidRegionSlot =
        std::numeric_limits<RegionSlot>::max();
    static constexpr PortalSlot InvalidPortalSlot =
        std::numeric_limits<PortalSlot>::max();
    static constexpr TurnStateSlot InvalidTurnStateSlot =
        std::numeric_limits<TurnStateSlot>::max();
    static constexpr std::size_t InvalidSpatialNode =
        std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t SpatialLeafSize = 8;

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

    struct AdjacencyEdge
    {
        PortalId portalId = 0;
        PortalSlot portalSlot = InvalidPortalSlot;
        RegionSlot neighborSlot = InvalidRegionSlot;
        TurnStateSlot arrivalTurnStateSlot = InvalidTurnStateSlot;
        double geometricMeters = 0.0;
        double availableClearanceMeters = 0.0;
    };

    struct TurnStateRecord
    {
        RegionSlot regionSlot = InvalidRegionSlot;
        PortalId incomingPortalId = 0;
        PortalSlot incomingPortalSlot = InvalidPortalSlot;
    };

    struct GraphIndex
    {
        std::map<RegionId, RegionSlot> regionSlots;
        std::vector<RegionId> slotRegionIds;
        std::vector<Vec3d> regionCenters;
        std::vector<double> regionCapacities;
        std::vector<std::uint8_t> regionInvalidated;
        std::vector<PortalId> slotPortalIds;
        std::vector<Vec3d> portalCenters;
        std::vector<std::uint8_t> portalInvalidated;
        std::vector<std::vector<AdjacencyEdge>> adjacency;
        std::vector<std::vector<PortalSlot>> incidentPortalSlots;
        std::vector<TurnStateRecord> turnStates;
        std::vector<std::size_t> turnAngleOffsets;
        std::vector<double> turnAnglesRadians;
    };

    struct SpatialNode
    {
        Bounds3d bounds {};
        std::size_t left = InvalidSpatialNode;
        std::size_t right = InvalidSpatialNode;
        std::size_t begin = 0;
        std::size_t end = 0;

        bool leaf() const noexcept
        {
            return left == InvalidSpatialNode && right == InvalidSpatialNode;
        }
    };

    struct SpatialIndex
    {
        std::vector<RegionSlot> slots;
        std::vector<SpatialNode> nodes;
    };

    Revision spaceRevision = 0;
    Revision sourceRevision = 0;
    std::map<RegionId, RegionState> regions;
    std::map<PortalId, PortalState> portals;
    GraphIndex graph;
    SpatialIndex spatial;

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

    static GraphIndex buildGraphIndex(
        const std::map<RegionId, RegionState>& regions,
        const std::map<PortalId, PortalState>& portals
    )
    {
        GraphIndex result;
        result.slotRegionIds.reserve(regions.size());
        result.regionCenters.reserve(regions.size());
        result.regionCapacities.reserve(regions.size());
        result.regionInvalidated.reserve(regions.size());
        result.adjacency.resize(regions.size());
        result.incidentPortalSlots.resize(regions.size());
        result.turnStates.reserve(portals.size() * 2);
        result.slotPortalIds.reserve(portals.size());
        result.portalCenters.reserve(portals.size());
        result.portalInvalidated.reserve(portals.size());

        RegionSlot slot = 0;
        for (const auto& entry : regions)
        {
            result.regionSlots.emplace(entry.first, slot++);
            result.slotRegionIds.push_back(entry.first);
            result.regionCenters.push_back(boundsCenter(entry.second.input.boundsMapMeters));
            result.regionCapacities.push_back(regionCapacity(entry.second.input));
            result.regionInvalidated.push_back(entry.second.invalidated ? 1u : 0u);
        }

        for (const auto& entry : portals)
        {
            result.slotPortalIds.push_back(entry.first);
            result.portalCenters.push_back(entry.second.input.centerMapMeters);
            result.portalInvalidated.push_back(entry.second.invalidated ? 1u : 0u);
        }

        auto addArrivalState = [&result](
            RegionSlot regionSlot,
            PortalId portalId,
            PortalSlot portalSlot
        ) {
            const TurnStateSlot stateSlot = result.turnStates.size();
            result.turnStates.push_back(
                TurnStateRecord{regionSlot, portalId, portalSlot}
            );
            return stateSlot;
        };

        PortalSlot portalSlot = 0;
        for (const auto& entry : portals)
        {
            const PortalId portalId = entry.first;
            const auto& portal = entry.second.input;
            const RegionSlot slotA = result.regionSlots.at(portal.regionA);
            const RegionSlot slotB = result.regionSlots.at(portal.regionB);
            const double geometricMeters =
                distance(result.regionCenters[slotA], portal.centerMapMeters) +
                distance(portal.centerMapMeters, result.regionCenters[slotB]);
            const double availableClearanceMeters = std::min({
                portal.clearanceRadiusMeters,
                result.regionCapacities[slotA],
                result.regionCapacities[slotB]
            });

            const TurnStateSlot arrivalAtB = addArrivalState(
                slotB,
                portalId,
                portalSlot
            );
            result.adjacency[slotA].push_back(
                AdjacencyEdge{
                    portalId,
                    portalSlot,
                    slotB,
                    arrivalAtB,
                    geometricMeters,
                    availableClearanceMeters
                }
            );
            if (portal.bidirectional)
            {
                const TurnStateSlot arrivalAtA = addArrivalState(
                    slotA,
                    portalId,
                    portalSlot
                );
                result.adjacency[slotB].push_back(
                    AdjacencyEdge{
                        portalId,
                        portalSlot,
                        slotA,
                        arrivalAtA,
                        geometricMeters,
                        availableClearanceMeters
                    }
                );
            }

            result.incidentPortalSlots[slotA].push_back(portalSlot);
            result.incidentPortalSlots[slotB].push_back(portalSlot);
            ++portalSlot;
        }

        result.turnAngleOffsets.reserve(result.turnStates.size() + 1);
        result.turnAngleOffsets.push_back(0);
        for (const auto& state : result.turnStates)
        {
            const Vec3d& currentCenter = result.regionCenters[state.regionSlot];
            const Vec3d& incomingPortalCenter =
                result.portalCenters[state.incomingPortalSlot];
            for (const auto& edge : result.adjacency[state.regionSlot])
            {
                result.turnAnglesRadians.push_back(
                    turnAngleRadians(
                        incomingPortalCenter,
                        currentCenter,
                        result.portalCenters[edge.portalSlot]
                    )
                );
            }
            result.turnAngleOffsets.push_back(result.turnAnglesRadians.size());
        }

        return result;
    }

    static const Bounds3d& regionBoundsForSlot(
        const std::map<RegionId, RegionState>& regions,
        const GraphIndex& graph,
        RegionSlot slot
    )
    {
        return regions.at(graph.slotRegionIds.at(slot)).input.boundsMapMeters;
    }

    static std::size_t buildSpatialNode(
        SpatialIndex& spatial,
        const std::map<RegionId, RegionState>& regions,
        const GraphIndex& graph,
        std::size_t begin,
        std::size_t end
    )
    {
        SpatialNode node;
        node.begin = begin;
        node.end = end;
        node.bounds = regionBoundsForSlot(regions, graph, spatial.slots.at(begin));

        Vec3d centroidMin = boundsCenter(node.bounds);
        Vec3d centroidMax = centroidMin;
        for (std::size_t i = begin + 1; i < end; ++i)
        {
            const Bounds3d& bounds = regionBoundsForSlot(
                regions,
                graph,
                spatial.slots[i]
            );
            node.bounds = mergeBounds(node.bounds, bounds);
            const Vec3d center = boundsCenter(bounds);
            centroidMin.x = std::min(centroidMin.x, center.x);
            centroidMin.y = std::min(centroidMin.y, center.y);
            centroidMin.z = std::min(centroidMin.z, center.z);
            centroidMax.x = std::max(centroidMax.x, center.x);
            centroidMax.y = std::max(centroidMax.y, center.y);
            centroidMax.z = std::max(centroidMax.z, center.z);
        }

        const std::size_t nodeIndex = spatial.nodes.size();
        spatial.nodes.push_back(node);

        const std::size_t count = end - begin;
        if (count <= SpatialLeafSize)
            return nodeIndex;

        const Vec3d span {
            centroidMax.x - centroidMin.x,
            centroidMax.y - centroidMin.y,
            centroidMax.z - centroidMin.z
        };
        int axis = 0;
        if (span.y > span.x && span.y >= span.z)
            axis = 1;
        else if (span.z > span.x && span.z > span.y)
            axis = 2;

        const std::size_t middle = begin + count / 2;
        std::nth_element(
            spatial.slots.begin() + static_cast<std::ptrdiff_t>(begin),
            spatial.slots.begin() + static_cast<std::ptrdiff_t>(middle),
            spatial.slots.begin() + static_cast<std::ptrdiff_t>(end),
            [&](RegionSlot a, RegionSlot b)
            {
                const double ca = axisValue(
                    boundsCenter(regionBoundsForSlot(regions, graph, a)),
                    axis
                );
                const double cb = axisValue(
                    boundsCenter(regionBoundsForSlot(regions, graph, b)),
                    axis
                );
                if (ca != cb)
                    return ca < cb;
                return a < b;
            }
        );

        const std::size_t left = buildSpatialNode(
            spatial,
            regions,
            graph,
            begin,
            middle
        );
        const std::size_t right = buildSpatialNode(
            spatial,
            regions,
            graph,
            middle,
            end
        );
        spatial.nodes[nodeIndex].left = left;
        spatial.nodes[nodeIndex].right = right;
        return nodeIndex;
    }

    static SpatialIndex buildSpatialIndex(
        const std::map<RegionId, RegionState>& regions,
        const GraphIndex& graph
    )
    {
        SpatialIndex result;
        result.slots.reserve(graph.slotRegionIds.size());
        for (RegionSlot slot = 0; slot < graph.slotRegionIds.size(); ++slot)
            result.slots.push_back(slot);

        if (!result.slots.empty())
        {
            result.nodes.reserve(result.slots.size() * 2);
            buildSpatialNode(
                result,
                regions,
                graph,
                0,
                result.slots.size()
            );
        }
        return result;
    }

    void collectPointCandidateSlots(
        const Vec3d& point,
        std::vector<RegionSlot>& output
    ) const
    {
        output.clear();
        if (spatial.nodes.empty())
            return;

        std::vector<std::size_t> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty())
        {
            const std::size_t nodeIndex = stack.back();
            stack.pop_back();
            const SpatialNode& node = spatial.nodes[nodeIndex];
            if (!contains(node.bounds, point))
                continue;

            if (node.leaf())
            {
                output.insert(
                    output.end(),
                    spatial.slots.begin() + static_cast<std::ptrdiff_t>(node.begin),
                    spatial.slots.begin() + static_cast<std::ptrdiff_t>(node.end)
                );
            }
            else
            {
                stack.push_back(node.right);
                stack.push_back(node.left);
            }
        }

        std::sort(output.begin(), output.end());
        output.erase(std::unique(output.begin(), output.end()), output.end());
    }

    void collectBoundsCandidateSlots(
        const Bounds3d& bounds,
        std::vector<RegionSlot>& output
    ) const
    {
        output.clear();
        if (spatial.nodes.empty())
            return;

        std::vector<std::size_t> stack;
        stack.reserve(64);
        stack.push_back(0);

        while (!stack.empty())
        {
            const std::size_t nodeIndex = stack.back();
            stack.pop_back();
            const SpatialNode& node = spatial.nodes[nodeIndex];
            if (!intersects(node.bounds, bounds))
                continue;

            if (node.leaf())
            {
                output.insert(
                    output.end(),
                    spatial.slots.begin() + static_cast<std::ptrdiff_t>(node.begin),
                    spatial.slots.begin() + static_cast<std::ptrdiff_t>(node.end)
                );
            }
            else
            {
                stack.push_back(node.right);
                stack.push_back(node.left);
            }
        }

        std::sort(output.begin(), output.end());
        output.erase(std::unique(output.begin(), output.end()), output.end());
    }

    const RegionState* findTraversableRegion(
        const Vec3d& point,
        double required,
        std::size_t* examined = nullptr
    ) const
    {
        std::vector<RegionSlot> candidates;
        collectPointCandidateSlots(point, candidates);

        for (RegionSlot slot : candidates)
        {
            if (examined)
                ++(*examined);

            const RegionId regionId = graph.slotRegionIds.at(slot);
            const RegionState& state = regions.at(regionId);
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
    auto graph = Impl::buildGraphIndex(regions, portals);
    auto spatial = Impl::buildSpatialIndex(regions, graph);

    impl_->regions = std::move(regions);
    impl_->portals = std::move(portals);
    impl_->graph = std::move(graph);
    impl_->spatial = std::move(spatial);
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
    auto graph = Impl::buildGraphIndex(regions, portals);
    auto spatial = Impl::buildSpatialIndex(regions, graph);

    impl_->regions = std::move(regions);
    impl_->portals = std::move(portals);
    impl_->graph = std::move(graph);
    impl_->spatial = std::move(spatial);
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
    std::vector<Impl::RegionSlot> candidates;
    impl_->collectBoundsCandidateSlots(boundsMapMeters, candidates);

    std::vector<Impl::RegionSlot> newlyInvalidatedSlots;
    for (Impl::RegionSlot slot : candidates)
    {
        const RegionId regionId = impl_->graph.slotRegionIds.at(slot);
        auto& state = impl_->regions.at(regionId);
        if (!state.invalidated && intersects(state.input.boundsMapMeters, boundsMapMeters))
        {
            state.invalidated = true;
            impl_->graph.regionInvalidated.at(slot) = 1u;
            result.invalidatedRegionIds.push_back(regionId);
            newlyInvalidatedSlots.push_back(slot);
        }
    }

    for (Impl::RegionSlot slot : newlyInvalidatedSlots)
    {
        for (Impl::PortalSlot portalSlot : impl_->graph.incidentPortalSlots.at(slot))
        {
            const PortalId portalId = impl_->graph.slotPortalIds.at(portalSlot);
            auto& state = impl_->portals.at(portalId);
            if (!state.invalidated)
            {
                state.invalidated = true;
                impl_->graph.portalInvalidated.at(portalSlot) = 1u;
                result.invalidatedPortalIds.push_back(portalId);
            }
        }
    }

    std::sort(result.invalidatedRegionIds.begin(), result.invalidatedRegionIds.end());
    std::sort(result.invalidatedPortalIds.begin(), result.invalidatedPortalIds.end());

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

    std::vector<Impl::RegionSlot> candidates;
    impl_->collectPointCandidateSlots(query.pointMapMeters, candidates);

    for (Impl::RegionSlot slot : candidates)
    {
        ++result.regionsExamined;
        const RegionId regionId = impl_->graph.slotRegionIds.at(slot);
        const auto& state = impl_->regions.at(regionId);
        if (state.invalidated || !contains(state.input.boundsMapMeters, query.pointMapMeters))
            continue;

        const double available = std::min(
            pointClearance(state.input.boundsMapMeters, query.pointMapMeters),
            state.input.clearanceRadiusMeters
        );

        if (firstContaining == 0)
        {
            firstContaining = regionId;
            firstAvailable = available;
        }

        if (available >= required)
        {
            result.traversable = true;
            result.regionId = regionId;
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

    const auto startSlotIt = impl_->graph.regionSlots.find(startId);
    const auto endSlotIt = impl_->graph.regionSlots.find(endId);
    if (startSlotIt == impl_->graph.regionSlots.end() ||
        endSlotIt == impl_->graph.regionSlots.end())
    {
        return result;
    }

    const Impl::RegionSlot startSlot = startSlotIt->second;
    const Impl::RegionSlot endSlot = endSlotIt->second;
    const std::size_t regionCount = impl_->graph.slotRegionIds.size();

    struct Prev
    {
        Impl::RegionSlot regionSlot = Impl::InvalidRegionSlot;
        PortalId portalId = 0;
    };

    std::vector<std::uint8_t> visited(regionCount, 0);
    std::vector<Prev> previous(regionCount);
    std::vector<Impl::RegionSlot> frontier;
    frontier.reserve(regionCount);
    std::size_t frontierHead = 0;

    frontier.push_back(startSlot);
    visited[startSlot] = 1;

    while (frontierHead < frontier.size())
    {
        const Impl::RegionSlot currentSlot = frontier[frontierHead++];
        ++result.diagnostics.regionsVisited;

        for (const auto& edge : impl_->graph.adjacency[currentSlot])
        {
            ++result.diagnostics.portalsExamined;
            const auto portalIt = impl_->portals.find(edge.portalId);
            if (portalIt == impl_->portals.end())
                continue;

            const auto& portalState = portalIt->second;
            if (portalState.invalidated ||
                portalState.input.clearanceRadiusMeters < required)
            {
                continue;
            }

            const Impl::RegionSlot neighborSlot = edge.neighborSlot;
            if (neighborSlot >= regionCount || visited[neighborSlot])
                continue;

            const RegionId neighborId = impl_->graph.slotRegionIds[neighborSlot];
            const auto regionIt = impl_->regions.find(neighborId);
            if (regionIt == impl_->regions.end() || regionIt->second.invalidated)
                continue;
            if (regionCapacity(regionIt->second.input) < required)
                continue;

            visited[neighborSlot] = 1;
            previous[neighborSlot] = Prev{currentSlot, edge.portalId};

            if (neighborSlot == endSlot)
            {
                std::vector<RegionId> reverseRegions;
                std::vector<PortalId> reversePortals;

                Impl::RegionSlot cursor = endSlot;
                reverseRegions.push_back(impl_->graph.slotRegionIds[cursor]);
                while (cursor != startSlot)
                {
                    const Prev prev = previous[cursor];
                    if (prev.regionSlot == Impl::InvalidRegionSlot)
                        return result;
                    reversePortals.push_back(prev.portalId);
                    cursor = prev.regionSlot;
                    reverseRegions.push_back(impl_->graph.slotRegionIds[cursor]);
                }

                result.regionPath.assign(reverseRegions.rbegin(), reverseRegions.rend());
                result.portalPath.assign(reversePortals.rbegin(), reversePortals.rend());
                result.found = true;
                return result;
            }

            frontier.push_back(neighborSlot);
        }
    }

    return result;
}

NavigationSpace::CostedCorridorResult NavigationSpace::queryCostedCorridor(
    const CorridorQuery& query,
    const CorridorCostPolicy& policy
) const
{
    const double required = requiredClearance(query.envelope);
    validateCostPolicy(policy);

    CostedCorridorResult result;
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

    const auto startSlotIt = impl_->graph.regionSlots.find(startId);
    const auto endSlotIt = impl_->graph.regionSlots.find(endId);
    if (startSlotIt == impl_->graph.regionSlots.end() ||
        endSlotIt == impl_->graph.regionSlots.end())
    {
        return result;
    }

    const Impl::RegionSlot startSlot = startSlotIt->second;
    const Impl::RegionSlot endSlot = endSlotIt->second;
    const std::size_t regionCount = impl_->graph.slotRegionIds.size();

    if (policy.turnPenaltyMetersPerRadian > 0.0)
    {
        struct QueueItem
        {
            double cost = 0.0;
            Impl::RegionSlot regionSlot = Impl::InvalidRegionSlot;
            PortalId incomingPortalId = 0;
            Impl::TurnStateSlot stateSlot = Impl::InvalidTurnStateSlot;
        };

        struct QueueGreater
        {
            bool operator()(const QueueItem& a, const QueueItem& b) const noexcept
            {
                if (a.cost != b.cost)
                    return a.cost > b.cost;
                if (a.regionSlot != b.regionSlot)
                    return a.regionSlot > b.regionSlot;
                if (a.incomingPortalId != b.incomingPortalId)
                    return a.incomingPortalId > b.incomingPortalId;
                return a.stateSlot > b.stateSlot;
            }
        };

        const double infinity = std::numeric_limits<double>::infinity();
        const std::size_t stateCount = impl_->graph.turnStates.size();
        std::vector<double> bestCost(stateCount, infinity);
        std::vector<Impl::TurnStateSlot> previous(
            stateCount,
            Impl::InvalidTurnStateSlot
        );
        std::vector<std::uint8_t> settled(stateCount, 0);
        std::vector<std::uint8_t> countedRegion(regionCount, 0);
        std::priority_queue<
            QueueItem,
            std::vector<QueueItem>,
            QueueGreater
        > frontier;

        if (startSlot >= impl_->graph.regionInvalidated.size() ||
            endSlot >= impl_->graph.regionInvalidated.size() ||
            impl_->graph.regionInvalidated[startSlot] ||
            impl_->graph.regionInvalidated[endSlot])
        {
            return result;
        }

        countedRegion[startSlot] = 1;
        ++result.diagnostics.regionsVisited;
        const double preferredClearance =
            required * policy.preferredClearanceMultiple;
        auto clearancePenaltyFor = [&](double availableClearance) noexcept {
            if (preferredClearance <= 0.0 ||
                availableClearance >= preferredClearance)
            {
                return 0.0;
            }
            const double shortfallFraction =
                (preferredClearance - availableClearance) / preferredClearance;
            return policy.clearancePenaltyMeters * shortfallFraction;
        };

        for (const auto& edge : impl_->graph.adjacency[startSlot])
        {
            ++result.diagnostics.portalsExamined;
            if (edge.neighborSlot >= regionCount ||
                edge.portalSlot >= impl_->graph.portalInvalidated.size() ||
                edge.arrivalTurnStateSlot >= stateCount)
            {
                continue;
            }
            if (impl_->graph.portalInvalidated[edge.portalSlot] ||
                impl_->graph.regionInvalidated[edge.neighborSlot] ||
                edge.availableClearanceMeters < required)
            {
                continue;
            }

            const double candidateCost =
                policy.distanceWeight * edge.geometricMeters +
                clearancePenaltyFor(edge.availableClearanceMeters);
            const Impl::TurnStateSlot nextStateSlot = edge.arrivalTurnStateSlot;
            if (candidateCost < bestCost[nextStateSlot])
            {
                bestCost[nextStateSlot] = candidateCost;
                const auto& state = impl_->graph.turnStates[nextStateSlot];
                frontier.push(QueueItem{
                    candidateCost,
                    state.regionSlot,
                    state.incomingPortalId,
                    nextStateSlot
                });
            }
        }

        Impl::TurnStateSlot finalStateSlot = Impl::InvalidTurnStateSlot;
        double finalCost = infinity;

        while (!frontier.empty())
        {
            const QueueItem current = frontier.top();
            frontier.pop();

            if (current.stateSlot >= stateCount || settled[current.stateSlot])
                continue;
            if (current.cost > bestCost[current.stateSlot])
                continue;

            const auto& currentState = impl_->graph.turnStates[current.stateSlot];
            if (currentState.regionSlot != current.regionSlot ||
                currentState.incomingPortalId != current.incomingPortalId ||
                currentState.regionSlot >= impl_->graph.regionInvalidated.size() ||
                currentState.incomingPortalSlot >= impl_->graph.portalInvalidated.size())
            {
                continue;
            }
            if (impl_->graph.regionInvalidated[currentState.regionSlot] ||
                impl_->graph.portalInvalidated[currentState.incomingPortalSlot])
            {
                continue;
            }

            settled[current.stateSlot] = 1;
            if (!countedRegion[currentState.regionSlot])
            {
                countedRegion[currentState.regionSlot] = 1;
                ++result.diagnostics.regionsVisited;
            }

            if (currentState.regionSlot == endSlot)
            {
                finalStateSlot = current.stateSlot;
                finalCost = current.cost;
                break;
            }

            const auto& edges = impl_->graph.adjacency[currentState.regionSlot];
            if (current.stateSlot + 1 >= impl_->graph.turnAngleOffsets.size())
                return result;
            const std::size_t turnOffset =
                impl_->graph.turnAngleOffsets[current.stateSlot];
            const std::size_t turnEnd =
                impl_->graph.turnAngleOffsets[current.stateSlot + 1];
            if (turnEnd - turnOffset != edges.size() ||
                turnEnd > impl_->graph.turnAnglesRadians.size())
            {
                return result;
            }

            for (std::size_t edgeIndex = 0; edgeIndex < edges.size(); ++edgeIndex)
            {
                const auto& edge = edges[edgeIndex];
                ++result.diagnostics.portalsExamined;
                if (edge.neighborSlot >= regionCount ||
                    edge.portalSlot >= impl_->graph.portalInvalidated.size() ||
                    edge.arrivalTurnStateSlot >= stateCount)
                {
                    continue;
                }
                if (impl_->graph.portalInvalidated[edge.portalSlot] ||
                    impl_->graph.regionInvalidated[edge.neighborSlot] ||
                    edge.availableClearanceMeters < required)
                {
                    continue;
                }

                const double turnPenalty =
                    policy.turnPenaltyMetersPerRadian *
                    impl_->graph.turnAnglesRadians[turnOffset + edgeIndex];
                const double candidateCost =
                    current.cost +
                    policy.distanceWeight * edge.geometricMeters +
                    clearancePenaltyFor(edge.availableClearanceMeters) +
                    turnPenalty;

                const Impl::TurnStateSlot nextStateSlot = edge.arrivalTurnStateSlot;
                if (settled[nextStateSlot] ||
                    candidateCost >= bestCost[nextStateSlot])
                {
                    continue;
                }

                bestCost[nextStateSlot] = candidateCost;
                previous[nextStateSlot] = current.stateSlot;
                const auto& nextState = impl_->graph.turnStates[nextStateSlot];
                frontier.push(QueueItem{
                    candidateCost,
                    nextState.regionSlot,
                    nextState.incomingPortalId,
                    nextStateSlot
                });
            }
        }

        if (finalStateSlot == Impl::InvalidTurnStateSlot || !finite(finalCost))
            return result;

        std::vector<RegionId> reverseRegions;
        std::vector<PortalId> reversePortals;
        Impl::TurnStateSlot cursor = finalStateSlot;
        reverseRegions.push_back(endId);

        while (true)
        {
            if (cursor >= stateCount)
                return result;
            const auto& state = impl_->graph.turnStates[cursor];
            if (state.incomingPortalId == 0)
                return result;

            reversePortals.push_back(state.incomingPortalId);
            const Impl::TurnStateSlot prev = previous[cursor];
            if (prev == Impl::InvalidTurnStateSlot)
            {
                reverseRegions.push_back(startId);
                break;
            }

            cursor = prev;
            const auto& prevState = impl_->graph.turnStates[cursor];
            reverseRegions.push_back(
                impl_->graph.slotRegionIds[prevState.regionSlot]
            );
        }

        result.regionPath.assign(reverseRegions.rbegin(), reverseRegions.rend());
        result.portalPath.assign(reversePortals.rbegin(), reversePortals.rend());
        result.totalCostMetersEquivalent = finalCost;
        result.found = true;
        return result;
    }

    struct Prev
    {
        Impl::RegionSlot regionSlot = Impl::InvalidRegionSlot;
        PortalId portalId = 0;
    };

    const double infinity = std::numeric_limits<double>::infinity();
    std::vector<double> bestCost(regionCount, infinity);
    std::vector<Prev> previous(regionCount);
    std::vector<std::uint8_t> settled(regionCount, 0);

    using QueueKey = std::pair<double, Impl::RegionSlot>;
    std::multimap<QueueKey, Impl::RegionSlot> frontier;
    bestCost[startSlot] = 0.0;
    frontier.emplace(QueueKey{0.0, startSlot}, startSlot);

    while (!frontier.empty())
    {
        const auto currentIt = frontier.begin();
        const double currentCost = currentIt->first.first;
        const Impl::RegionSlot currentSlot = currentIt->second;
        frontier.erase(currentIt);

        if (currentSlot >= regionCount || settled[currentSlot])
            continue;
        if (currentCost > bestCost[currentSlot])
            continue;

        settled[currentSlot] = 1;
        ++result.diagnostics.regionsVisited;
        if (currentSlot == endSlot)
            break;

        const RegionId currentId = impl_->graph.slotRegionIds[currentSlot];
        const auto currentRegionIt = impl_->regions.find(currentId);
        if (currentRegionIt == impl_->regions.end() || currentRegionIt->second.invalidated)
            continue;

        const double currentCapacity = regionCapacity(currentRegionIt->second.input);
        const Vec3d currentCenter = boundsCenter(
            currentRegionIt->second.input.boundsMapMeters
        );

        for (const auto& edge : impl_->graph.adjacency[currentSlot])
        {
            ++result.diagnostics.portalsExamined;
            if (edge.neighborSlot >= regionCount || settled[edge.neighborSlot])
                continue;

            const auto portalIt = impl_->portals.find(edge.portalId);
            if (portalIt == impl_->portals.end() || portalIt->second.invalidated)
                continue;

            const auto& portal = portalIt->second.input;
            const RegionId neighborId = impl_->graph.slotRegionIds[edge.neighborSlot];
            const auto neighborRegionIt = impl_->regions.find(neighborId);
            if (neighborRegionIt == impl_->regions.end() ||
                neighborRegionIt->second.invalidated)
            {
                continue;
            }

            const double neighborCapacity = regionCapacity(neighborRegionIt->second.input);
            const double availableClearance = std::min({
                portal.clearanceRadiusMeters,
                currentCapacity,
                neighborCapacity
            });
            if (availableClearance < required)
                continue;

            const Vec3d neighborCenter = boundsCenter(
                neighborRegionIt->second.input.boundsMapMeters
            );
            const double geometricMeters =
                distance(currentCenter, portal.centerMapMeters) +
                distance(portal.centerMapMeters, neighborCenter);

            double clearancePenalty = 0.0;
            const double preferredClearance =
                required * policy.preferredClearanceMultiple;
            if (preferredClearance > 0.0 && availableClearance < preferredClearance)
            {
                const double shortfallFraction =
                    (preferredClearance - availableClearance) / preferredClearance;
                clearancePenalty =
                    policy.clearancePenaltyMeters * shortfallFraction;
            }

            const double candidateCost =
                currentCost +
                policy.distanceWeight * geometricMeters +
                clearancePenalty;

            if (candidateCost < bestCost[edge.neighborSlot])
            {
                bestCost[edge.neighborSlot] = candidateCost;
                previous[edge.neighborSlot] = Prev{currentSlot, edge.portalId};
                frontier.emplace(
                    QueueKey{candidateCost, edge.neighborSlot},
                    edge.neighborSlot
                );
            }
        }
    }

    if (!finite(bestCost[endSlot]))
        return result;

    std::vector<RegionId> reverseRegions;
    std::vector<PortalId> reversePortals;
    Impl::RegionSlot cursor = endSlot;
    reverseRegions.push_back(impl_->graph.slotRegionIds[cursor]);
    while (cursor != startSlot)
    {
        const Prev prev = previous[cursor];
        if (prev.regionSlot == Impl::InvalidRegionSlot)
            return result;
        reversePortals.push_back(prev.portalId);
        cursor = prev.regionSlot;
        reverseRegions.push_back(impl_->graph.slotRegionIds[cursor]);
    }

    result.regionPath.assign(reverseRegions.rbegin(), reverseRegions.rend());
    result.portalPath.assign(reversePortals.rbegin(), reversePortals.rend());
    result.totalCostMetersEquivalent = bestCost[endSlot];
    result.found = true;
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
