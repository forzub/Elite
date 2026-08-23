#include "src/game/navigation/DockingPortRuntimeStateCatalog.h"

#include <fstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::navigation
{
namespace
{
using json = nlohmann::json;

DockingOperationalState parseOperational(const std::string& value)
{
    if (value == "online") return DockingOperationalState::Online;
    if (value == "offline") return DockingOperationalState::Offline;
    if (value == "damaged") return DockingOperationalState::Damaged;
    return DockingOperationalState::Unknown;
}

DockingOccupancyState parseOccupancy(const std::string& value)
{
    if (value == "free") return DockingOccupancyState::Free;
    if (value == "occupied") return DockingOccupancyState::Occupied;
    if (value == "reserved") return DockingOccupancyState::Reserved;
    return DockingOccupancyState::Unknown;
}

DockingAccessState parseAccess(const std::string& value)
{
    if (value == "allowed") return DockingAccessState::Allowed;
    if (value == "clearance_required") return DockingAccessState::ClearanceRequired;
    if (value == "denied") return DockingAccessState::Denied;
    return DockingAccessState::Unknown;
}

} // namespace

std::string DockingPortRuntimeStateCatalog::key(
    const std::string& hubModuleId,
    const std::string& anchorId
)
{
    return hubModuleId + "\n" + anchorId;
}

bool DockingPortRuntimeStateCatalog::load(const std::string& path)
{
    std::ifstream in(path);
    if (!in.is_open() &&
        path == "assets/data/navigation/hub_docking_runtime_test.json")
    {
        in.open("../assets/data/navigation/hub_docking_runtime_test.json");
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

    if (!root.contains("ports") || !root["ports"].is_array())
        return false;

    std::unordered_map<std::string, DockingPortRuntimeState> parsed;
    for (const json& item : root["ports"])
    {
        if (!item.is_object())
            continue;

        DockingPortRuntimeState state;
        state.hubModuleId = item.value("module_id", "");
        state.anchorId = item.value("anchor_id", "");
        state.operational = parseOperational(item.value("operational", "unknown"));
        state.occupancy = parseOccupancy(item.value("occupancy", "unknown"));
        state.access = parseAccess(item.value("access", "unknown"));

        if (state.hubModuleId.empty() || state.anchorId.empty())
            continue;

        parsed[key(state.hubModuleId, state.anchorId)] = std::move(state);
    }

    m_states = std::move(parsed);
    return true;
}

const DockingPortRuntimeState* DockingPortRuntimeStateCatalog::find(
    const std::string& hubModuleId,
    const std::string& anchorId
) const noexcept
{
    const auto it = m_states.find(key(hubModuleId, anchorId));
    return it == m_states.end() ? nullptr : &it->second;
}

} // namespace game::navigation
