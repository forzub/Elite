// Rescue zones on the client (navigation/rules/RescueRules.h): where an SOS is heard. The current
// system's inhabited hubs come replicated with their positions; any system's inhabitation comes from the
// world-sites catalog (jump planning to a system the client does not see yet).
#include "src/game/SpaceStateInternal.h"
#include "src/game/localization/LocalizationService.h"
#include "src/game/navigation/rules/RescueRules.h"

#include <limits>

namespace l10n = game::localization;

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

rescue::Estimate SpaceState::rescueAt(const glm::dvec3& worldM, int systemId, std::string* nearestHubName,
                                      std::string* nearestHubId) const
{
    double best = std::numeric_limits<double>::infinity();
    std::string bestName, bestId;
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
                bestId = id;
            }
        }
    if (nearestHubName)
        *nearestHubName = bestName;
    if (nearestHubId)
        *nearestHubId = bestId;
    return rescue::estimate(std::isfinite(best) ? best : -1.0, systemHasRescueService(systemId));
}

std::string SpaceState::rescueWarningAt(const glm::dvec3& worldM, int systemId, const char* what) const
{
    std::string hub;
    const auto e = rescueAt(worldM, systemId, &hub);
    if (e.zone == rescue::Zone::Inside)
        return {};
    if (e.zone == rescue::Zone::NoService)
        return l10n::tr("cockpit.rescue.no_service", {{"who", what}},
                        "{who}: no rescue service in this system - only passing ships will hear an SOS");
    if (e.distanceToHubM > 0.0)
        return l10n::tr("cockpit.rescue.outside_zone_far",
                        {{"who", what}, {"hub", hub}, {"distance", l10n::formatNumber(e.distanceToHubM / rescue::LightSecondM, 0)},
                         {"zone", l10n::formatNumber(rescue::ZoneRadiusLs, 0)}},
                        "{who}: outside the rescue zone ({distance} ls to '{hub}', zone {zone}) - an SOS may go unheard");
    return l10n::tr("cockpit.rescue.outside_zone", {{"who", what}}, "{who}: outside the rescue zone - an SOS may go unheard");
}
