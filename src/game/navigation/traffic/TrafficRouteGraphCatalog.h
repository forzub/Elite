#pragma once

#include <memory>
#include <string>

#include "src/game/navigation/traffic/TrafficRouteGraph.h"

namespace game::navigation::traffic
{

class TrafficRouteGraphCatalog final
{
public:
    bool load(
        const std::string& path =
            "assets/data/navigation/hub_traffic_route_graph.json"
    );

    const std::shared_ptr<const TrafficRouteGraph>& graph() const noexcept
    {
        return m_graph;
    }

    const std::string& failure() const noexcept
    {
        return m_failure;
    }

private:
    std::shared_ptr<const TrafficRouteGraph> m_graph;
    std::string m_failure;
};

} // namespace game::navigation::traffic
