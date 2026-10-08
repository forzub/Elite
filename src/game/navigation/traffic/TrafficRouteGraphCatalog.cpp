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
    if (value == "volume_transit") return TrafficRouteStageKind::VolumeTransit;
    if (value == "terminal_approach") return TrafficRouteStageKind::TerminalApproach;
    return TrafficRouteStageKind::FreeSpace;
}

world::navigation::NavigationVolumePolicy parseVolumePolicy(
    const std::string& value
)
{
    if (value == "preferred")
        return world::navigation::NavigationVolumePolicy::PreferInside;
    return world::navigation::NavigationVolumePolicy::KeepInside;
}

TransitDirectionPolicy parseDirectionPolicy(const std::string& value)
{
    return value == "two_way"
        ? TransitDirectionPolicy::TwoWay
        : TransitDirectionPolicy::OneWay;
}

PortalConnectionKind parsePortalConnectionKind(
    const std::string& value
)
{
    return value == "mandatory_tangent_straight"
        ? PortalConnectionKind::MandatoryTangentStraight
        : PortalConnectionKind::Direct;
}

PortalConnectionDefinition readPortalConnection(
    const json& item,
    const char* key
)
{
    PortalConnectionDefinition out;
    if (!item.contains(key) || !item[key].is_object())
        return out;

    const auto& connection = item[key];
    out.kind =
        parsePortalConnectionKind(
            connection.value("kind", "direct")
        );
    out.mandatoryStraightMeters =
        connection.value("mandatory_straight_m", 0.0);
    return out;
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
            portal.inboundConnection =
                readPortalConnection(item, "inbound_connection");
            portal.outboundConnection =
                readPortalConnection(item, "outbound_connection");
            definition.portals.push_back(std::move(portal));
        }
    }

    if (root.contains("lanes") &&
        root["lanes"].is_array())
    {
        for (const auto& item : root["lanes"])
        {
            if (!item.is_object())
                continue;

            TrafficLaneDefinition lane;
            lane.id = item.value("id", "");
            lane.navigationVolumeId =
                item.value("navigation_volume_id", "");
            lane.directionPolicy =
                parseDirectionPolicy(
                    item.value("direction_policy", "one_way")
                );
            lane.entryPortalId =
                item.value("entry_portal_id", "");
            lane.exitPortalId =
                item.value("exit_portal_id", "");
            lane.policy =
                parseVolumePolicy(
                    item.value("policy", "required")
                );
            lane.maxTransitSpeedMps =
                item.value("max_transit_speed_mps", 0.0);
            lane.internalDockPortalIds =
                readStringArray(item, "internal_dock_portal_ids");
            definition.lanes.push_back(std::move(lane));
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
            edge.laneId =
                item.value("lane_id", "");
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
