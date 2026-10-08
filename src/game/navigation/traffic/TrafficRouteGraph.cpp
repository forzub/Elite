#include "src/game/navigation/traffic/TrafficRouteGraph.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_set>
#include <utility>

namespace game::navigation::traffic
{
namespace
{

bool validVector(const glm::dvec3& value) noexcept
{
    return
        std::isfinite(value.x) &&
        std::isfinite(value.y) &&
        std::isfinite(value.z) &&
        glm::length(value) > 1.0e-9;
}

void setFailure(std::string* failure, const std::string& value)
{
    if (failure)
        *failure = value;
}

} // namespace

TrafficRouteGraph::TrafficRouteGraph(
    TrafficRouteGraphDefinition definition
)
    : m_definition(std::move(definition))
{
    for (std::size_t i = 0; i < m_definition.portals.size(); ++i)
        m_portalIndex.emplace(m_definition.portals[i].id, i);

    for (std::size_t i = 0; i < m_definition.lanes.size(); ++i)
        m_laneIndex.emplace(m_definition.lanes[i].id, i);

    for (std::size_t i = 0; i < m_definition.edges.size(); ++i)
    {
        m_outgoingEdges[m_definition.edges[i].fromPortalId]
            .push_back(i);
    }
}

std::shared_ptr<const TrafficRouteGraph> TrafficRouteGraph::build(
    TrafficRouteGraphDefinition definition,
    std::string* failure
)
{
    if (failure)
        failure->clear();

    if (definition.schemaVersion != 1)
    {
        setFailure(failure, "unsupported traffic graph schema version");
        return {};
    }

    if (definition.graphId.empty())
    {
        setFailure(failure, "traffic graph id is empty");
        return {};
    }

    std::unordered_set<std::string> portalIds;
    for (const auto& portal : definition.portals)
    {
        if (portal.id.empty())
        {
            setFailure(failure, "traffic portal id is empty");
            return {};
        }

        if (!portalIds.insert(portal.id).second)
        {
            setFailure(
                failure,
                "duplicate traffic portal id: " + portal.id
            );
            return {};
        }

        if (portal.hubModuleId.empty() ||
            portal.semanticAnchorId.empty())
        {
            setFailure(
                failure,
                "traffic portal lacks semantic anchor binding: " +
                    portal.id
            );
            return {};
        }

        if (!validVector(portal.crossingForwardLocal) ||
            !validVector(portal.upLocal))
        {
            setFailure(
                failure,
                "traffic portal has invalid frame vectors: " +
                    portal.id
            );
            return {};
        }

        if (portal.requiredClearanceMeters < 0.0 ||
            portal.maxCrossingSpeedMps < 0.0 ||
            !std::isfinite(portal.requiredClearanceMeters) ||
            !std::isfinite(portal.maxCrossingSpeedMps))
        {
            setFailure(
                failure,
                "traffic portal has invalid numeric limits: " +
                    portal.id
            );
            return {};
        }
    }

    std::unordered_set<std::string> laneIds;
    for (const auto& lane : definition.lanes)
    {
        if (lane.id.empty())
        {
            setFailure(failure, "traffic lane id is empty");
            return {};
        }

        if (!laneIds.insert(lane.id).second)
        {
            setFailure(
                failure,
                "duplicate traffic lane id: " + lane.id
            );
            return {};
        }

        if (lane.navigationVolumeId.empty())
        {
            setFailure(
                failure,
                "traffic lane has no navigation volume: " + lane.id
            );
            return {};
        }

        if (!portalIds.count(lane.entryPortalId) ||
            !portalIds.count(lane.exitPortalId))
        {
            setFailure(
                failure,
                "traffic lane references unknown entry/exit portal: " +
                    lane.id
            );
            return {};
        }

        if (lane.entryPortalId == lane.exitPortalId)
        {
            setFailure(
                failure,
                "traffic lane entry and exit are identical: " +
                    lane.id
            );
            return {};
        }

        if (lane.policy !=
                world::navigation::NavigationVolumePolicy::KeepInside &&
            lane.policy !=
                world::navigation::NavigationVolumePolicy::PreferInside)
        {
            setFailure(
                failure,
                "traffic lane policy must be KeepInside or PreferInside: " +
                    lane.id
            );
            return {};
        }

        if (lane.maxTransitSpeedMps < 0.0 ||
            !std::isfinite(lane.maxTransitSpeedMps))
        {
            setFailure(
                failure,
                "traffic lane has invalid speed limit: " + lane.id
            );
            return {};
        }

        for (const auto& dockPortalId : lane.internalDockPortalIds)
        {
            if (!portalIds.count(dockPortalId))
            {
                setFailure(
                    failure,
                    "traffic lane references unknown internal dock portal: " +
                        dockPortalId
                );
                return {};
            }
        }
    }

    std::unordered_set<std::string> edgeIds;
    for (const auto& edge : definition.edges)
    {
        if (edge.id.empty())
        {
            setFailure(failure, "traffic edge id is empty");
            return {};
        }

        if (!edgeIds.insert(edge.id).second)
        {
            setFailure(
                failure,
                "duplicate traffic edge id: " + edge.id
            );
            return {};
        }

        if (!portalIds.count(edge.fromPortalId) ||
            !portalIds.count(edge.toPortalId))
        {
            setFailure(
                failure,
                "traffic edge references unknown portal: " + edge.id
            );
            return {};
        }

        if (edge.fromPortalId == edge.toPortalId)
        {
            setFailure(
                failure,
                "traffic edge forms a zero-length semantic loop: " + edge.id
            );
            return {};
        }

        if (edge.kind == TrafficRouteStageKind::VolumeTransit)
        {
            const auto laneIt =
                std::find_if(
                    definition.lanes.begin(),
                    definition.lanes.end(),
                    [&](const TrafficLaneDefinition& lane)
                    {
                        return lane.id == edge.laneId;
                    }
                );

            if (laneIt == definition.lanes.end())
            {
                setFailure(
                    failure,
                    "volume-transit edge references unknown lane: " +
                        edge.id
                );
                return {};
            }

            const auto& lane = *laneIt;
            const bool authoredForward =
                edge.fromPortalId == lane.entryPortalId &&
                edge.toPortalId == lane.exitPortalId;
            const bool authoredReverse =
                edge.fromPortalId == lane.exitPortalId &&
                edge.toPortalId == lane.entryPortalId;

            if (!authoredForward &&
                !(lane.directionPolicy == TransitDirectionPolicy::TwoWay &&
                  authoredReverse))
            {
                setFailure(
                    failure,
                    "volume-transit edge violates lane portal ordering: " +
                        edge.id
                );
                return {};
            }
        }
        else if (!edge.laneId.empty())
        {
            setFailure(
                failure,
                "non-volume-transit edge unexpectedly references a lane: " +
                    edge.id
            );
            return {};
        }
    }

    for (const auto& lane : definition.lanes)
    {
        const bool represented =
            std::any_of(
                definition.edges.begin(),
                definition.edges.end(),
                [&](const TrafficRouteEdgeDefinition& edge)
                {
                    return
                        edge.kind == TrafficRouteStageKind::VolumeTransit &&
                        edge.laneId == lane.id;
                }
            );

        if (!represented)
        {
            setFailure(
                failure,
                "traffic lane has no route edge: " + lane.id
            );
            return {};
        }
    }

    return std::shared_ptr<const TrafficRouteGraph>(
        new TrafficRouteGraph(std::move(definition))
    );
}

const NavigationPortalDefinition* TrafficRouteGraph::portal(
    const std::string& id
) const noexcept
{
    const auto it = m_portalIndex.find(id);
    if (it == m_portalIndex.end())
        return nullptr;

    return &m_definition.portals[it->second];
}

const TrafficLaneDefinition* TrafficRouteGraph::lane(
    const std::string& id
) const noexcept
{
    const auto it = m_laneIndex.find(id);
    if (it == m_laneIndex.end())
        return nullptr;

    return &m_definition.lanes[it->second];
}

ResolvedTrafficRoute TrafficRouteGraph::resolve(
    const std::string& firstRequiredPortalId,
    const std::string& destinationPortalId
) const
{
    ResolvedTrafficRoute out;

    if (!portal(firstRequiredPortalId))
    {
        out.failure =
            "unknown first required portal: " + firstRequiredPortalId;
        return out;
    }

    if (!portal(destinationPortalId))
    {
        out.failure =
            "unknown destination portal: " + destinationPortalId;
        return out;
    }

    struct Previous
    {
        std::string portalId;
        std::size_t edgeIndex = 0;
        bool hasValue = false;
    };

    std::deque<std::string> pending;
    std::unordered_set<std::string> visited;
    std::unordered_map<std::string, Previous> previous;

    pending.push_back(firstRequiredPortalId);
    visited.insert(firstRequiredPortalId);

    while (!pending.empty())
    {
        const std::string current = pending.front();
        pending.pop_front();

        if (current == destinationPortalId)
            break;

        const auto outgoingIt = m_outgoingEdges.find(current);
        if (outgoingIt == m_outgoingEdges.end())
            continue;

        for (const std::size_t edgeIndex : outgoingIt->second)
        {
            const auto& edge = m_definition.edges[edgeIndex];
            if (!visited.insert(edge.toPortalId).second)
                continue;

            previous[edge.toPortalId] = {
                current,
                edgeIndex,
                true
            };
            pending.push_back(edge.toPortalId);
        }
    }

    if (!visited.count(destinationPortalId))
    {
        out.failure =
            "no traffic topology path from " +
            firstRequiredPortalId + " to " + destinationPortalId;
        return out;
    }

    std::vector<std::size_t> reversedEdges;
    std::string cursor = destinationPortalId;

    while (cursor != firstRequiredPortalId)
    {
        const auto previousIt = previous.find(cursor);
        if (previousIt == previous.end() ||
            !previousIt->second.hasValue)
        {
            out.failure =
                "traffic graph predecessor chain is incomplete";
            return out;
        }

        reversedEdges.push_back(previousIt->second.edgeIndex);
        cursor = previousIt->second.portalId;
    }

    std::reverse(reversedEdges.begin(), reversedEdges.end());

    out.portalSequence.push_back(firstRequiredPortalId);

    TrafficRouteStage approach;
    approach.kind = TrafficRouteStageKind::FreeApproach;
    approach.toPortalId = firstRequiredPortalId;
    out.stages.push_back(std::move(approach));

    for (const std::size_t edgeIndex : reversedEdges)
    {
        const auto& edge = m_definition.edges[edgeIndex];

        TrafficRouteStage stage;
        stage.kind = edge.kind;
        stage.fromPortalId = edge.fromPortalId;
        stage.toPortalId = edge.toPortalId;
        stage.laneId = edge.laneId;

        if (!edge.laneId.empty())
        {
            const auto* laneDef = lane(edge.laneId);
            if (!laneDef)
            {
                out.failure =
                    "resolved edge lost lane definition: " + edge.id;
                return out;
            }

            stage.volumeConstraint.volumeId =
                laneDef->navigationVolumeId;
            stage.volumeConstraint.policy = laneDef->policy;
            stage.volumeConstraint.maxSpeedMps =
                laneDef->maxTransitSpeedMps;
        }

        out.stages.push_back(std::move(stage));
        out.portalSequence.push_back(edge.toPortalId);
    }

    out.valid = true;
    return out;
}

} // namespace game::navigation::traffic
