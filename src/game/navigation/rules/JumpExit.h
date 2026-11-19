#pragma once

// The ONE rule for the default exit body of an interstellar jump (audit 2026-11-19: GameServer and the jump
// procedure's fallback each had their own copy with different tie-breaks). The pilot named no body: the planet with
// the most world sites (where people are), then a canon (observed / authored) one, then the heaviest; planets of
// radius >= 1000 km only. Each caller builds the candidates from its own source (the star atlas / the gravity bodies);
// the world sites come from the process-wide catalog (loaded once).
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "src/world/celestial/WorldSitesCatalog.h"

namespace game::navigation::rules
{
struct JumpExitCandidate
{
    std::string id;
    bool planet = false;        // a planet (not the star, not a belt / reservoir)
    double radiusM = 0.0;
    double massKg = 0.0;
    bool observed = false;      // canon (observed / authored)
};

inline constexpr double JumpExitMinRadiusM = 1.0e6;

inline std::string pickJumpExitBody(int systemId, const std::vector<JumpExitCandidate>& candidates)
{
    std::map<std::string, int> sitesOnBody;
    for (const auto& s : world::celestial::WorldSitesCatalog::shared().sitesInSystem(systemId))
        ++sitesOnBody[s.parentBodyId];
    std::string best;
    std::tuple<int, int, double> bestKey {-1, -1, -1.0};
    for (const auto& c : candidates)
    {
        if (!c.planet || c.radiusM < JumpExitMinRadiusM)
            continue;
        const auto n = sitesOnBody.find(c.id);
        const std::tuple<int, int, double> key {n == sitesOnBody.end() ? 0 : n->second, c.observed ? 1 : 0, c.massKg};
        if (key > bestKey)
        {
            bestKey = key;
            best = c.id;
        }
    }
    return best;
}
} // namespace game::navigation::rules
