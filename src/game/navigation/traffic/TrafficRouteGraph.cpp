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

bool finitePositive(double value) noexcept
{
    return std::isfinite(value) && value > 0.0;
}

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
    {
        m_portalIndex.emplace(
            m_definition.portals[i].id,
            i
        );
    }

    for (std::size_t i = 0; i < m_definition.mandatoryZones.size(); ++i)
    {
        m_zoneIndex.emplace(
            m_definition.mandatoryZones[i].id,
            i
        );
    }

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

    std::unordered_set<std::string> zoneIds;
    for (const auto& zone : definition.mandatoryZones)
    {
        if (zone.id.empty())
        {
            setFailure(failure, "mandatory transit zone id is empty");
            return {};
        }

        if (!zoneIds.insert(zone.id).second)
        {
            setFailure(
                failure,
                "duplicate mandatory transit zone id: " + zone.id
            );
            return {};
        }

        if (!portalIds.count(zone.entryPortalId) ||
            !portalIds.count(zone.exitPortalId))
        {
            setFailure(
                failure,
                "mandatory transit zone references unknown entry/exit portal: " +
                    zone.id
            );
            return {};
        }

        if (zone.entryPortalId == zone.exitPortalId)
        {
            setFailure(
                failure,
                "mandatory transit zone entry and exit are identical: " +
                    zone.id
            );
            return {};
        }

        if (zone.requiredClearanceMeters < 0.0 ||
            zone.maxTransitSpeedMps < 0.0 ||
            !std::isfinite(zone.requiredClearanceMeters) ||
            !std::isfinite(zone.maxTransitSpeedMps))
        {
            setFailure(
                failure,
                "mandatory transit zone has invalid limits: " + zone.id
            );
            return {};
        }

        if (zone.volumeKind == TransitVolumeKind::Cylinder)
        {
            if (!finitePositive(zone.radiusMeters) ||
                !finitePositive(zone.halfLengthMeters))
            {
                setFailure(
                    failure,
                    "cylindrical mandatory transit zone has invalid dimensions: " +
                        zone.id
                );
                return {};
            }
        }
        else
        {
            if (!(zone.halfExtentsMeters.x > 0.0 &&
                  zone.halfExtentsMeters.y > 0.0 &&
                  zone.halfExtentsMeters.z > 0.0) ||
                !std::isfinite(zone.halfExtentsMeters.x) ||
                !std::isfinite(zone.halfExtentsMeters.y) ||
                !std::isfinite(zone.halfExtentsMeters.z))
            {
                setFailure(
                    failure,
                    "box mandatory transit zone has invalid dimensions: " +
                        zone.id
                );
                return {};
            }
        }

        for (const auto& dockPortalId : zone.internalDockPortalIds)
        {
            if (!portalIds.count(dockPortalId))
            {
                setFailure(
                    failure,
                    "mandatory transit zone references unknown internal dock portal: " +
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

        if (edge.kind == TrafficRouteStageKind::MandatoryTransit)
        {
            const auto zoneIt =
                std::find_if(
                    definition.mandatoryZones.begin(),
                    definition.mandatoryZones.end(),
                    [&](const MandatoryTransitZoneDefinition& zone)
                    {
                        return zone.id == edge.mandatoryZoneId;
                    }
                );

            if (zoneIt == definition.mandatoryZones.end())
            {
                setFailure(
                    failure,
                    "mandatory traffic edge references unknown zone: " +
                        edge.id
                );
                return {};
            }

            const auto& zone = *zoneIt;
            const bool authoredForward =
                edge.fromPortalId == zone.entryPortalId &&
                edge.toPortalId == zone.exitPortalId;
            const bool authoredReverse =
                edge.fromPortalId == zone.exitPortalId &&
                edge.toPortalId == zone.entryPortalId;

            if (!authoredForward &&
                !(zone.directionPolicy == TransitDirectionPolicy::TwoWay &&
                  authoredReverse))
            {
                setFailure(
                    failure,
                    "mandatory traffic edge violates zone portal ordering: " +
                        edge.id
                );
                return {};
            }
        }
        else if (!edge.mandatoryZoneId.empty())
        {
            setFailure(
                failure,
                "non-mandatory traffic edge unexpectedly owns a zone: " +
                    edge.id
            );
            return {};
        }
    }

    // Every mandatory zone must be represented by at least one mandatory edge.
    for (const auto& zone : definition.mandatoryZones)
    {
        const bool represented =
            std::any_of(
                definition.edges.begin(),
                definition.edges.end(),
                [&](const TrafficRouteEdgeDefinition& edge)
                {
                    return
                        edge.kind ==
                            TrafficRouteStageKind::MandatoryTransit &&
                        edge.mandatoryZoneId == zone.id;
                }
            );

        if (!represented)
        {
            setFailure(
                failure,
                "mandatory transit zone has no route edge: " + zone.id
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

const MandatoryTransitZoneDefinition* TrafficRouteGraph::mandatoryZone(
    const std::string& id
) const noexcept
{
    const auto it = m_zoneIndex.find(id);
    if (it == m_zoneIndex.end())
        return nullptr;

    return &m_definition.mandatoryZones[it->second];
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

    // The current ship position is intentionally outside immutable topology.
    // Resolver therefore prepends one synthetic free-approach stage.
    out.stages.push_back({
        TrafficRouteStageKind::FreeApproach,
        std::string(),
        firstRequiredPortalId,
        std::string()
    });

    for (const std::size_t edgeIndex : reversedEdges)
    {
        const auto& edge = m_definition.edges[edgeIndex];
        out.stages.push_back({
            edge.kind,
            edge.fromPortalId,
            edge.toPortalId,
            edge.mandatoryZoneId
        });
        out.portalSequence.push_back(edge.toPortalId);
    }

    out.valid = true;
    return out;
}

} // namespace game::navigation::traffic
