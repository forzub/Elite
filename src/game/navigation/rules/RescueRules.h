#pragma once

// Rescue zones and the SOS (author 2026-11-08): "в жилых зонах есть спасательные службы - это их задача;
// надо выводить предупреждение в терминале HUD, при расчёте маршрута или при старте автопилота: при полётах
// вдали от обитаемых зон есть риск, что SOS не будет услышан. В обитаемой зоне капсулу подберут минут через
// 10-20, а там - как повезёт."
//
// A rescue zone is the comms reach of an inhabited hub: canon world_sites.json comms.coverage_ls gives a
// station 30 light-seconds (trade hub 60, military 90, bridge beacon 120); the conservative station value is
// used for every hub. Inside it the hub's SAR service hears the SOS and picks the capsule up in 10-20 minutes
// (longer toward the edge). Outside it the SOS reaches only ships that happen to pass; a system without an
// inhabited hub has no rescue service at all.

#include <algorithm>
#include <cstdint>

namespace game::navigation::rescue
{

inline constexpr double LightSecondM = 299792458.0;
inline constexpr double ZoneRadiusLs = 30.0;
inline constexpr double ZoneRadiusM = ZoneRadiusLs * LightSecondM;
inline constexpr double PickupMinutesMin = 10.0;
inline constexpr double PickupMinutesMax = 20.0;

enum class Zone : std::uint8_t
{
    Inside = 0,     // a hub's SAR service hears the SOS
    Outside = 1,    // the system has rescue services, but not here
    NoService = 2   // no inhabited hub in the system
};

struct Estimate
{
    Zone zone = Zone::NoService;
    double distanceToHubM = -1.0;    // the nearest inhabited hub (-1: none known)
    double pickupMinutes = -1.0;     // inside a zone
};

// nearestHubM < 0: no inhabited hub known in the system
inline Estimate estimate(double nearestHubM, bool systemHasService)
{
    Estimate e;
    e.distanceToHubM = nearestHubM;
    if (!systemHasService)
        return e;
    if (nearestHubM >= 0.0 && nearestHubM <= ZoneRadiusM)
    {
        e.zone = Zone::Inside;
        e.pickupMinutes = PickupMinutesMin + (PickupMinutesMax - PickupMinutesMin) * std::clamp(nearestHubM / ZoneRadiusM, 0.0, 1.0);
        return e;
    }
    e.zone = Zone::Outside;
    return e;
}

} // namespace game::navigation::rescue
