#pragma once

// THE ACTUAL EXISTENCE of a hub city's landing pad (spatial audit A6, 2026-11-27). The static plan (HubPlanCatalog: the
// `pad` flag) says where a pad is DESIGNED; whether it EXISTS now is the city's physical package: HubExpansion builds a
// pad's node ("pad_<address>") only when its building is finished (a roof pad over a building still at its frame stage is
// left out), and the city damage keeps a variant per node. So one rule, the same on the server (its StaticObject's hit
// volumes + its damage state) and on the client (the city volumes assembled on the client + the replicated
// buildingStates):
//   built   - the package has volumes labelled "pad_<address>.<folder>.<k>" (their assemblyNode is the pad's node)
//   usable  - built, and its node neither Gutted nor Destroyed (a missile / laser hit leaves the deck usable)
// The dispatcher, the manual capture zone, the map's landing action and the HUD zones all ask this.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "src/game/simulation/CityDamage.h"

namespace game::simulation
{
struct CityPadPresence
{
    bool built = false;
    std::int32_t node = -1;
    std::uint8_t variant = 0;   // CityBuildingVariant
    bool usable() const noexcept
    {
        return built && variant != static_cast<std::uint8_t>(CityBuildingVariant::Gutted) &&
               variant != static_cast<std::uint8_t>(CityBuildingVariant::Destroyed);
    }
};

// address -> the pad's node index, from the city package's volume labels ("pad_<address>.<folder>.<k>")
template <typename Volumes>
std::unordered_map<std::string, std::int32_t> cityPadNodes(const Volumes& volumes)
{
    std::unordered_map<std::string, std::int32_t> out;
    for (const auto& v : volumes)
    {
        const std::string& label = v.m_label;
        if (label.rfind("pad_", 0) != 0 || v.assemblyNode < 0)
            continue;
        const auto dot = label.find('.', 4);
        if (dot == std::string::npos)
            continue;
        out.emplace(label.substr(4, dot - 4), v.assemblyNode);
    }
    return out;
}

// the pad's presence from the node index map and the city's damaged nodes
inline CityPadPresence cityPadPresence(const std::unordered_map<std::string, std::int32_t>& padNodes,
                                       const std::vector<CityBuildingState>& damaged, const std::string& address)
{
    CityPadPresence out;
    const auto it = padNodes.find(address);
    if (it == padNodes.end())
        return out;
    out.built = true;
    out.node = it->second;
    for (const auto& s : damaged)
        if (static_cast<std::int32_t>(s.node) == out.node)
            out.variant = s.variant;
    return out;
}
} // namespace game::simulation
