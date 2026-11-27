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
// (infrastructureStateOf below: the presence in the one state vocabulary)

// THE ONE INFRASTRUCTURE STATE (stabilization stage 5, A6): what a designed piece of a city IS now, the same words on
// the server and the client. Unknown = the asker has no current package (the client while its copy of the package is
// assembling, failed, or was built for an EARLIER construction stage than the one in force - the server decides then).
enum class InfrastructureState : std::uint8_t
{
    Unknown = 0,
    NotBuilt,    // designed; its building not finished in the package of the stage in force
    Existing,    // built, intact
    Damaged,     // built, hit (missile / laser variants) - still usable
    Destroyed    // gutted / destroyed - not usable
};
[[nodiscard]] inline bool infrastructureUsable(InfrastructureState s) noexcept
{
    return s == InfrastructureState::Existing || s == InfrastructureState::Damaged;
}

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
// the presence in the one vocabulary (a package the asker HAS, for the stage in force)
[[nodiscard]] inline InfrastructureState infrastructureStateOf(const CityPadPresence& p) noexcept
{
    if (!p.built)
        return InfrastructureState::NotBuilt;
    if (p.variant == static_cast<std::uint8_t>(CityBuildingVariant::Gutted) ||
        p.variant == static_cast<std::uint8_t>(CityBuildingVariant::Destroyed))
        return InfrastructureState::Destroyed;
    return p.variant != 0 ? InfrastructureState::Damaged : InfrastructureState::Existing;
}
} // namespace game::simulation
