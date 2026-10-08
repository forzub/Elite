#include "src/game/navigation/traffic/TrafficRouteGraphCatalog.h"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::navigation::traffic
{
namespace
{
using json = nlohmann::json;

NavigationPortalRole parsePortalRole(const std::string& value)
{
    if (value == "transit_entry") return NavigationPortalRole::TransitEntry;
    if (value == "transit_exit") return NavigationPortalRole::TransitExit;
    if (value == "docking_port") return NavigationPortalRole::DockingPort;
    if (value == "hold_point") return NavigationPortalRole::HoldPoint;
    return NavigationPortalRole::Generic;
}

TrafficRouteStageKind parseStageKind(const std::string& value)
{
    if (value == "free_approach") return TrafficRouteStageKind::FreeApproach;
    if (value == "mandatory_transit") return TrafficRouteStageKind::MandatoryTransit;
    if (value == "terminal_approach") return TrafficRouteStageKind::TerminalApproach;
    return TrafficRouteStageKind::FreeSpace;
}

TransitVolumeKind parseVolumeKind(const std::string& value)
{
    return value == "box"
        ? TransitVolumeKind::Box
        : TransitVolumeKind::Cylinder;
}

TransitDirectionPolicy parseDirectionPolicy(const std::string& value)
{
    return value == "two_way"
        ? TransitDirectionPolicy::TwoWay
        : TransitDirectionPolicy::OneWay;
}

glm::dvec3 readVec3(
    const json& parent,
    const char* key,
    const glm::dvec3& fallback
)
{
    if (!parent.contains(key) ||
        !parent[key].is_array() ||
        parent[key].size() != 3)
    {
        return fallback;
    }

    return {
        parent[key][0].get<double>(),
        parent[key][1].get<double>(),
        parent[key][2].get<double>()
    };
}

std::vector<std::string> readStringArray(
    const json& parent,
    const char* key
)
{
    std::vector<std::string> out;
    if (!parent.contains(key) || !parent[key].is_array())
        return out;

    for (const auto& value : parent[key])
    {
        if (value.is_string())
            out.push_back(value.get<std::string>());
    }
    return out;
}

} // namespace

bool TrafficRouteGraphCatalog::load(const std::string& path)
{
    m_graph.reset();
    m_failure.clear();

    std::ifstream in(path);
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_traffic_route_graph.json")
    {
        in.open("../assets/data/navigation/hub_traffic_route_graph.json");
    }
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_traffic_route_graph.json")
    {
        in.open("src/assets/data/navigation/hub_traffic_route_graph.json");
    }

    if (!in.is_open())
    {
        m_failure = "cannot open traffic route graph data";
        return false;
    }

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception& error)
    {
        m_failure =
            std::string("traffic route graph JSON parse failed: ") +
            error.what();
        return false;
    }

    TrafficRouteGraphDefinition definition;
    definition.schemaVersion = root.value("version", 0u);
    definition.graphId = root.value("graph_id", "");

    if (root.contains("portals") && root["portals"].is_array())
    {
        for (const auto& item : root["portals"])
        {
            if (!item.is_object())
                continue;

            NavigationPortalDefinition portal;
            portal.id = item.value("id", "");
            portal.hubModuleId = item.value("module_id", "");
            portal.semanticAnchorId =
                item.value("semantic_anchor_id", "");
            portal.role =
                parsePortalRole(item.value("role", "generic"));
            portal.crossingForwardLocal =
                readVec3(
                    item,
                    "crossing_forward_local",
                    glm::dvec3(0.0, 0.0, -1.0)
                );
            portal.upLocal =
                readVec3(
                    item,
                    "up_local",
                    glm::dvec3(0.0, 1.0, 0.0)
                );
            portal.requiredClearanceMeters =
                item.value("required_clearance_m", 0.0);
            portal.maxCrossingSpeedMps =
                item.value("max_crossing_speed_mps", 0.0);
            definition.portals.push_back(std::move(portal));
        }
    }

    if (root.contains("mandatory_zones") &&
        root["mandatory_zones"].is_array())
    {
        for (const auto& item : root["mandatory_zones"])
        {
            if (!item.is_object())
                continue;

            MandatoryTransitZoneDefinition zone;
            zone.id = item.value("id", "");
            zone.hubModuleId = item.value("module_id", "");
            zone.volumeKind =
                parseVolumeKind(item.value("volume_kind", "cylinder"));
            zone.directionPolicy =
                parseDirectionPolicy(
                    item.value("direction_policy", "one_way")
                );
            zone.entryPortalId = item.value("entry_portal_id", "");
            zone.exitPortalId = item.value("exit_portal_id", "");
            zone.radiusMeters = item.value("radius_m", 0.0);
            zone.halfLengthMeters = item.value("half_length_m", 0.0);
            zone.halfExtentsMeters =
                readVec3(
                    item,
                    "half_extents_m",
                    glm::dvec3(0.0)
                );
            zone.requiredClearanceMeters =
                item.value("required_clearance_m", 0.0);
            zone.maxTransitSpeedMps =
                item.value("max_transit_speed_mps", 0.0);
            zone.internalDockPortalIds =
                readStringArray(item, "internal_dock_portal_ids");
            definition.mandatoryZones.push_back(std::move(zone));
        }
    }

    if (root.contains("edges") && root["edges"].is_array())
    {
        for (const auto& item : root["edges"])
        {
            if (!item.is_object())
                continue;

            TrafficRouteEdgeDefinition edge;
            edge.id = item.value("id", "");
            edge.fromPortalId = item.value("from_portal_id", "");
            edge.toPortalId = item.value("to_portal_id", "");
            edge.kind =
                parseStageKind(item.value("kind", "free_space"));
            edge.mandatoryZoneId =
                item.value("mandatory_zone_id", "");
            definition.edges.push_back(std::move(edge));
        }
    }

    m_graph =
        TrafficRouteGraph::build(
            std::move(definition),
            &m_failure
        );
    return static_cast<bool>(m_graph);
}

} // namespace game::navigation::traffic
