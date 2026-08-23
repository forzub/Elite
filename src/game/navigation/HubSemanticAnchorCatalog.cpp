#include "src/game/navigation/HubSemanticAnchorCatalog.h"

#include <fstream>

#include <nlohmann/json.hpp>
#include <utility>

namespace game::navigation
{
namespace
{
using json = nlohmann::json;

HubSemanticAnchorKind parseKind(const std::string& value)
{
    if (value == "docking_port") return HubSemanticAnchorKind::DockingPort;
    if (value == "landing_pad") return HubSemanticAnchorKind::LandingPad;
    if (value == "transit_gate") return HubSemanticAnchorKind::TransitGate;
    if (value == "cargo_access") return HubSemanticAnchorKind::CargoAccess;
    if (value == "service_access") return HubSemanticAnchorKind::ServiceAccess;
    if (value == "attack_point") return HubSemanticAnchorKind::AttackPoint;
    if (value == "sensor_array") return HubSemanticAnchorKind::SensorArray;
    return HubSemanticAnchorKind::NavigationReference;
}

glm::dvec3 readVec3(
    const json& parent,
    const char* key,
    const glm::dvec3& fallback
)
{
    if (!parent.contains(key) || !parent[key].is_array() ||
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

} // namespace

bool HubSemanticAnchorCatalog::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_semantic_anchors.json")
    {
        in.open("../assets/data/navigation/hub_semantic_anchors.json");
    }

    if (!in.is_open())
        return false;

    json root;
    try
    {
        in >> root;
    }
    catch (...)
    {
        return false;
    }

    if (!root.contains("modules") || !root["modules"].is_array())
        return false;

    std::unordered_map<std::string, std::vector<HubSemanticAnchorDefinition>> parsed;

    for (const json& module : root["modules"])
    {
        if (!module.is_object())
            continue;

        const std::string moduleId = module.value("module_id", "");
        if (moduleId.empty() || !module.contains("anchors") ||
            !module["anchors"].is_array())
        {
            continue;
        }

        auto& out = parsed[moduleId];
        for (const json& item : module["anchors"])
        {
            if (!item.is_object())
                continue;

            HubSemanticAnchorDefinition anchor;
            anchor.id = item.value("id", "");
            anchor.hubModuleId = moduleId;
            anchor.kind = parseKind(item.value("kind", "navigation_reference"));
            anchor.localPositionMeters = readVec3(
                item,
                "local_position_m",
                glm::dvec3(0.0)
            );
            anchor.localForward = readVec3(
                item,
                "local_forward",
                glm::dvec3(0.0, 0.0, -1.0)
            );
            anchor.localUp = readVec3(
                item,
                "local_up",
                glm::dvec3(0.0, 1.0, 0.0)
            );
            anchor.extentMeters = readVec3(
                item,
                "extent_m",
                glm::dvec3(0.0)
            );
            anchor.requiredClearanceMeters =
                item.value("required_clearance_m", 0.0);
            anchor.maxEntrySpeedMps = item.value("max_entry_speed_mps", 0.0);
            anchor.enabled = item.value("enabled", true);

            if (!anchor.id.empty())
                out.push_back(std::move(anchor));
        }
    }

    m_byModule = std::move(parsed);
    return true;
}

const std::vector<HubSemanticAnchorDefinition>&
HubSemanticAnchorCatalog::anchorsForModule(
    const std::string& hubModuleId
) const noexcept
{
    static const std::vector<HubSemanticAnchorDefinition> empty;
    const auto it = m_byModule.find(hubModuleId);
    return it == m_byModule.end() ? empty : it->second;
}

const HubSemanticAnchorDefinition* HubSemanticAnchorCatalog::find(
    const std::string& hubModuleId,
    const std::string& anchorId
) const noexcept
{
    const auto& anchors = anchorsForModule(hubModuleId);
    for (const auto& anchor : anchors)
    {
        if (anchor.id == anchorId)
            return &anchor;
    }
    return nullptr;
}

} // namespace game::navigation
