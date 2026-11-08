// Rescue zones on the client (navigation/rules/RescueRules.h): where an SOS is heard. The current
// system's inhabited hubs come replicated with their positions; any system's inhabitation comes from the
// world-sites catalog (jump planning to a system the client does not see yet).
#include "src/game/SpaceStateInternal.h"
#include "src/game/navigation/rules/RescueRules.h"

#include <limits>

namespace rescue = game::navigation::rescue;

bool SpaceState::systemHasRescueService(int systemId) const
{
    if (!m_client)
        return false;
    // an inhabited hub (people, docks) runs a SAR service; the replicated hubs count too (Sol's network)
    for (const auto& s : m_client->catalogs().worldSites().sitesInSystem(systemId))
        if (s.population > 0 && s.docks > 0)
            return true;
    for (const auto& [id, hub] : m_client->world().hubs())
        if (hub.systemId == systemId)
            return true;
    return false;
}

rescue::Estimate SpaceState::rescueAt(const glm::dvec3& worldM, int systemId, std::string* nearestHubName) const
{
    double best = std::numeric_limits<double>::infinity();
    std::string bestName;
    if (m_client)
        for (const auto& [id, hub] : m_client->world().hubs())
        {
            if (hub.systemId != systemId)
                continue;
            const double d = glm::length(world::coordinates::fullMeters(hub.worldPosition) - worldM);
            if (d < best)
            {
                best = d;
                bestName = hub.name.empty() ? id : hub.name;
            }
        }
    if (nearestHubName)
        *nearestHubName = bestName;
    return rescue::estimate(std::isfinite(best) ? best : -1.0, systemHasRescueService(systemId));
}

std::string SpaceState::rescueWarningAt(const glm::dvec3& worldM, int systemId, const char* what) const
{
    std::string hub;
    const auto e = rescueAt(worldM, systemId, &hub);
    if (e.zone == rescue::Zone::Inside)
        return {};
    if (e.zone == rescue::Zone::NoService)
        return std::string(what) + ": в системе нет спасательной службы - SOS услышат только случайные корабли";
    char b[256];
    if (e.distanceToHubM > 0.0)
        std::snprintf(b, sizeof b, "%s: вне зоны спасения (до «%s» %.0f св. с, зона %.0f) - SOS могут не услышать", what, hub.c_str(),
                      e.distanceToHubM / rescue::LightSecondM, rescue::ZoneRadiusLs);
    else
        std::snprintf(b, sizeof b, "%s: вне зоны спасения - SOS могут не услышать", what);
    return b;
}
