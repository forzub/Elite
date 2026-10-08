#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation::traffic
{

// ============================================================================
// PROTECTED TRAFFIC TOPOLOGY CONTRACT
// ============================================================================
//
// LIVE NAVIGATION DEPENDS ON THIS LAYER BEING SEMANTICALLY STABLE.
//
// This graph describes WHICH infrastructure portals must be crossed and in
// WHICH ORDER. It does NOT contain sampled route geometry and RoutePlanner is
// not allowed to mutate it.
//
// DO NOT CASUALLY MODIFY THIS CONTRACT while working on follower gains,
// trajectory sampling, docking visuals, speed profiles, or obstacle routing.
// Changes require:
//   1) an explicit traffic-topology use case;
//   2) graph validation;
//   3) TrafficRouteGraphTests regression coverage.
//
// Once built, TrafficRouteGraph is immutable: there is no mutable node/edge
// access. Geometric planners consume resolved stages only.
// ============================================================================

enum class NavigationPortalRole : std::uint8_t
{
    TransitEntry = 0,
    TransitExit,
    DockingPort,
    HoldPoint,
    Generic
};

enum class TrafficRouteStageKind : std::uint8_t
{
    FreeApproach = 0,
    FreeSpace,
    MandatoryTransit,
    TerminalApproach
};

enum class TransitVolumeKind : std::uint8_t
{
    Cylinder = 0,
    Box
};

enum class TransitDirectionPolicy : std::uint8_t
{
    OneWay = 0,
    TwoWay
};

struct NavigationPortalDefinition
{
    std::string id;
    std::string hubModuleId;
    std::string semanticAnchorId;
    NavigationPortalRole role = NavigationPortalRole::Generic;

    // Crossing direction THROUGH the portal, not merely a mesh outward normal.
    glm::dvec3 crossingForwardLocal {0.0, 0.0, -1.0};
    glm::dvec3 upLocal {0.0, 1.0, 0.0};

    double requiredClearanceMeters = 0.0;
    double maxCrossingSpeedMps = 0.0;
};

struct MandatoryTransitZoneDefinition
{
    std::string id;
    std::string hubModuleId;
    TransitVolumeKind volumeKind = TransitVolumeKind::Cylinder;
    TransitDirectionPolicy directionPolicy = TransitDirectionPolicy::OneWay;

    std::string entryPortalId;
    std::string exitPortalId;

    // Local volume contract. For Cylinder, axis is the module-local Z axis and
    // radius/halfLength define the legal centerline tube after hull clearance.
    double radiusMeters = 0.0;
    double halfLengthMeters = 0.0;
    glm::dvec3 halfExtentsMeters {0.0};

    double requiredClearanceMeters = 0.0;
    double maxTransitSpeedMps = 0.0;

    // Future internal docks may terminate or originate a mandatory-transit
    // stage without weakening the entry/exit portal contract.
    std::vector<std::string> internalDockPortalIds;
};

struct TrafficRouteEdgeDefinition
{
    std::string id;
    std::string fromPortalId;
    std::string toPortalId;
    TrafficRouteStageKind kind = TrafficRouteStageKind::FreeSpace;

    // Required only for MandatoryTransit.
    std::string mandatoryZoneId;
};

struct TrafficRouteGraphDefinition
{
    std::string graphId;
    std::uint32_t schemaVersion = 1;
    std::vector<NavigationPortalDefinition> portals;
    std::vector<MandatoryTransitZoneDefinition> mandatoryZones;
    std::vector<TrafficRouteEdgeDefinition> edges;
};

struct TrafficRouteStage
{
    TrafficRouteStageKind kind = TrafficRouteStageKind::FreeSpace;
    std::string fromPortalId;
    std::string toPortalId;
    std::string mandatoryZoneId;
};

struct ResolvedTrafficRoute
{
    bool valid = false;
    std::string failure;
    std::vector<std::string> portalSequence;
    std::vector<TrafficRouteStage> stages;
};

class TrafficRouteGraph final
{
public:
    TrafficRouteGraph(const TrafficRouteGraph&) = delete;
    TrafficRouteGraph& operator=(const TrafficRouteGraph&) = delete;
    TrafficRouteGraph(TrafficRouteGraph&&) = delete;
    TrafficRouteGraph& operator=(TrafficRouteGraph&&) = delete;

    static std::shared_ptr<const TrafficRouteGraph> build(
        TrafficRouteGraphDefinition definition,
        std::string* failure = nullptr
    );

    const std::string& graphId() const noexcept
    {
        return m_definition.graphId;
    }

    std::uint32_t schemaVersion() const noexcept
    {
        return m_definition.schemaVersion;
    }

    const NavigationPortalDefinition* portal(
        const std::string& id
    ) const noexcept;

    const MandatoryTransitZoneDefinition* mandatoryZone(
        const std::string& id
    ) const noexcept;

    // Resolve semantic topology only. No coordinates, no smoothing, no route
    // samples. The result is safe to hand to a later geometric stage compiler.
    ResolvedTrafficRoute resolve(
        const std::string& firstRequiredPortalId,
        const std::string& destinationPortalId
    ) const;

private:
    explicit TrafficRouteGraph(TrafficRouteGraphDefinition definition);

    TrafficRouteGraphDefinition m_definition;
    std::unordered_map<std::string, std::size_t> m_portalIndex;
    std::unordered_map<std::string, std::size_t> m_zoneIndex;
    std::unordered_map<std::string, std::vector<std::size_t>> m_outgoingEdges;
};

} // namespace game::navigation::traffic
