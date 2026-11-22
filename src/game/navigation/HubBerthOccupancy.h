#pragma once

// The hub cities' landing places taken now, as the client last received them (ObjectSnapshot::occupiedBerths:
// "<address>#<bay>@<ship>[!]"), for every view that shows them - the HUD capture zones, the cards' buttons, the map's
// markers (author 2026-11-22: «каждый док или площадка должны быть под диспетчером»; «места для посадок должны быть на
// карте»). Filled by SpaceState each frame; read-only elsewhere.
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace game::navigation
{
struct HubBerthOccupancy
{
    std::unordered_map<std::string, std::vector<std::string>> byFrameModule;
    std::uint32_t playerShipId = 0;

    static HubBerthOccupancy& instance()
    {
        static HubBerthOccupancy registry;
        return registry;
    }
    // the places of `address` in that city taken by others (our own place is not "taken" for us)
    int takenByOthers(const std::string& frameModuleId, const std::string& address) const
    {
        const auto it = byFrameModule.find(frameModuleId);
        if (it == byFrameModule.end())
            return 0;
        int n = 0;
        const std::string prefix = address + "#";
        const std::string own = "@" + std::to_string(playerShipId);
        for (const auto& b : it->second)
        {
            if (b.compare(0, prefix.size(), prefix) != 0)
                continue;
            const auto at = b.find('@');
            const std::string who = at == std::string::npos ? std::string() : b.substr(at, b.find('!', at) - at);
            if (who != own)
                ++n;
        }
        return n;
    }
};
} // namespace game::navigation
