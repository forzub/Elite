#pragma once

// The hub city plan's addresses and landing pads (author 2026-11-21: «каждому блоку адрес ... развозить по заявке»;
// «каждому модулю города посадочную площадку - на свободных участках, на крышах, на ножках ... как гараж на 1-2
// места»). Header-only: the plan loader (HubPlanCatalog.cpp, the game) and the city assembler (HubCityAssembly.cpp, the
// game and the mesh editor) place the same pads from the same plan. The pad modules: tools/models/units/hub_pads.py.

#include <algorithm>
#include <glm/glm.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace game::navigation
{
struct HubPlanBlock;
struct HubPlanModule;

// the pad modules' measures (tools/models/units/hub_pads.py)
inline constexpr double HubPadDeckSide = 36.0;          // hubl_pad_roof / hubl_pad_stilts deck
inline constexpr double HubPadRoofDeckTop = 2.2;        // the roof pad's deck top over the roof
inline constexpr double HubPadStiltsDeckTop = 25.0;     // the stilts pad's deck top over the plane
inline constexpr double HubPadClearMeters = 30.0;       // a pad on a free plot: no building nearer than this
// the repair hangar (hubl_repair_hangar: 90 x 50 x 22 m, the door 80 x 18 m on its +Z): its free plot, the bay's touchdown
// (the wide one, author 2026-11-22: «да, делай ангар пошире» - 2 places for ships up to ~40 m: 110 x 60 x 26 m, the
// door 100 x 22 m)
inline constexpr double HubHangarClearMeters = 62.0;
inline constexpr double HubHangarBayZ = 2.0;             // a bay's centre from the box centre (toward the door)
inline constexpr double HubHangarBayX = 26.5;            // the two bays left / right of the box centre
inline constexpr double HubHangarFloorTop = 1.8;         // the scissor lift's top
inline constexpr double HubHangarDoorHalfDepth = 30.0;   // the door plane from the box centre
inline constexpr double HubHangarEntryHeight = 11.0;     // flown in at half the door's height
inline constexpr int HubHangarPlaces = 2;
// (hubHangarDoorForward / hubHangarRight(yaw) removed 2026-11-27: they ignored the under tier - hubPadAxisForward /
// hubPadAxisRight below are the model's true axes)
// the yaw that turns a pad model's +Z (its door / apron front) to the plan direction (dx, dz) on its tier
inline double hubPadYawFacing(bool under, double dx, double dz)
{
    return glm::degrees(std::atan2(dx, under ? -dz : dz));
}
// the pad's capture zone (author 2026-11-21: «прозрачный куб с белёсыми рёбрами, появляется когда до площадки метров
// 100»): a box over the deck - half side, height; shown within HubPadZoneShowMeters of the pad
inline constexpr double HubPadZoneHalf = 25.0;
inline constexpr double HubPadZoneHeight = 60.0;
inline constexpr double HubPadZoneShowMeters = 100.0;
// the landing legs (author 2026-11-21): a touchdown on them up to HubPadTouchdownMaxMps is a landing, from
// HubPadLegsBreakMps they break; the leg tips are HubPadLegLengthMeters under the hull's bottom
inline constexpr double HubPadTouchdownMaxMps = 2.5;
inline constexpr double HubPadLegsBreakMps = 4.0;
inline constexpr double HubPadLegLengthMeters = 0.5;

// the pad module folder of a building's pad ("" none)
std::string hubPadModuleFolder(const HubPlanModule& m);

// fills block.address, module.address / number and the pads (kind, touchdown point, module origin + yaw)
void assignHubPlanAddressesAndPads(std::vector<HubPlanBlock>& blocks, std::vector<HubPlanModule>& modules);
} // namespace game::navigation

#include "src/game/navigation/HubPlanCatalog.h"

namespace game::navigation
{
// a pad model's own axes in the PLAN, exactly as HubExpansion turns the model: R = Rx(under ? 180 : 0) * Ry(padYawDeg).
// An under-tier pad hangs upside down: its +X / +Z come out mirrored in z (audit A5 2026-11-27 - the bays and doors of
// under-tier hangars / garages were computed with the upper-tier axes)
inline glm::dvec3 hubPadAxisRight(const HubPlanModule& pad)
{
    const double yaw = glm::radians(pad.padYawDeg);
    return glm::dvec3(std::cos(yaw), 0.0, pad.under ? std::sin(yaw) : -std::sin(yaw));
}
inline glm::dvec3 hubPadAxisForward(const HubPlanModule& pad)
{
    const double yaw = glm::radians(pad.padYawDeg);
    return glm::dvec3(std::sin(yaw), 0.0, pad.under ? -std::cos(yaw) : std::cos(yaw));
}
inline std::string hubPadModuleFolder(const HubPlanModule& m)
{
    switch (m.padKind)
    {
        case HubPlanModule::PadKind::Roof: return "hubl_pad_roof";
        case HubPlanModule::PadKind::Stilts: return "hubl_pad_stilts";
        case HubPlanModule::PadKind::Garage: return m.padPlaces >= 2 ? "hubl_garage2" : "hubl_garage1";
        case HubPlanModule::PadKind::Hangar: return "hubl_repair_hangar";
        default: return {};
    }
}

inline void assignHubPlanAddressesAndPads(std::vector<HubPlanBlock>& blocks, std::vector<HubPlanModule>& modules)
{
    using Kind = HubPlanModule::PadKind;
    // addresses: the block = the grid column letter + the row number ("C4"); a building = its number in the block, the
    // lower tier with a U ("C4-07", "C4-U03")
    for (auto& block : blocks)
        block.address = std::string(1, static_cast<char>('A' + std::clamp(block.i, 0, 25))) + std::to_string(block.j + 1);
    std::vector<int> upper(blocks.size(), 0), lower(blocks.size(), 0);
    for (auto& module : modules)
    {
        if (module.block < 0 || module.pier)
            continue;
        const auto k = static_cast<std::size_t>(module.block);
        module.number = module.under ? ++lower[k] : ++upper[k];
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s-%s%02d", blocks[k].address.c_str(), module.under ? "U" : "", module.number);
        module.address = buf;
        // a power plant: nobody lands on a reactor hall (author 2026-11-21: a pad «висит в воздухе» over the power block)
        if (blocks[k].zone == "power")
            continue;
        // a dock / pad / pier of its own: no pad
        if (module.module.find("dock") != std::string::npos || module.module.find("pad") != std::string::npos ||
            module.module.find("pier") != std::string::npos)
            continue;
        const double half = 0.5 * std::min(module.rect.z - module.rect.x, module.rect.w - module.rect.y);
        const double height = module.under ? -module.y0 : module.y1;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        const double side = module.under ? -1.0 : 1.0;
        // a ROOF pad is the expensive exception, not the rule (author 2026-11-22: «ты их абсолютно все разместил над крышей
        // - туда действительно нет смысла лететь по зелёному коридору»): only a tall building of a downtown / hotel /
        // services / canopy block, and only every third of them; the rest berth at the base (a garage box, a pad on
        // legs on a free plot) - reached along the lanes down the street canyons
        const std::string& zoneHere = blocks[k].zone;
        const bool premium = zoneHere == "downtown" || zoneHere == "hotel" || zoneHere == "services" || zoneHere == "canopy";
        const bool roofSlot = premium && (module.number % 3) == 1;
        if (half >= 0.5 * HubPadDeckSide && height > 60.0 && roofSlot)
        {
            // a tall building with a roof wide enough for the 36 m deck: the pad on its roof (the lower tier: its
            // underside, the module upside down)
            module.padKind = Kind::Roof;
            module.padRadiusMeters = 0.5 * HubPadDeckSide;
            const double roof = module.under ? module.y0 : module.y1;
            module.padOrigin = glm::dvec3(centre.x, roof, centre.y);
            module.padPosition = glm::dvec3(centre.x, roof + side * HubPadRoofDeckTop, centre.y);
        }
        else if ((zoneHere == "repair" || zoneHere == "rig" || zoneHere == "port" || zoneHere == "logistics" ||
                  zoneHere == "storage") && module.number == 1)
            module.padKind = Kind::Hangar;   // a repair block: its repair hangar (3 bays) on a free plot - flown into
        else if (half >= 15.0)
            module.padKind = Kind::Garage;   // a wide one: a 1-2 place garage box beside it at the base (a free plot)
        else
            module.padKind = Kind::Stilts;   // a narrow one (or a tall narrow one): a pad on legs beside it
        if (module.padKind == Kind::Garage)
            module.padPlaces = half >= 30.0 ? 2 : 1;
        module.pad = true;
        // the expensive districts have the pad service (automatic landing), the cheap ones / the slums are hand-flown
        const std::string& zone = blocks[k].zone;
        module.autoLanding = zone == "downtown" || zone == "hotel" || zone == "residential" || zone == "services" ||
                             zone == "canopy" || zone == "dock" || zone == "repair" || zone == "refuel";
    }
    // the free-plot pads (on legs, garages): the nearest free plot of the block on a 10 m grid - no building within
    // HubPadClearMeters, no other free-plot pad within twice that; none within 250 m: no pad
    for (auto& module : modules)
    {
        const bool hangar = module.padKind == Kind::Hangar;
        const bool garage = module.padKind == Kind::Garage || hangar;
        if ((module.padKind != Kind::Stilts && !garage) || module.block < 0)
            continue;
        const auto& block = blocks[static_cast<std::size_t>(module.block)];
        const double clear = hangar ? HubHangarClearMeters : HubPadClearMeters;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        std::vector<const HubPlanModule*> near;   // this block, this tier: the buildings and the free-plot pads so far
        for (const auto& other : modules)
            if (other.block == module.block && other.under == module.under)
                near.push_back(&other);
        double best = 1.0e30;
        glm::dvec2 at(0.0);
        double hangarYaw = 0.0;
        if (hangar)
        {
            // the wide hangar by its own rectangle (110 x 60 m + 5 m), square to the block, its door away from the
            // building - the 62 m circle found no plot in any block (Earth hub, 2026-11-22)
            constexpr double HalfW = 55.0 + 5.0, HalfD = 30.0 + 5.0;
            for (double x = block.centre.x - block.halfMeters; x <= block.centre.x + block.halfMeters; x += 10.0)
                for (double z = block.centre.y - block.halfMeters; z <= block.centre.y + block.halfMeters; z += 10.0)
                    for (int o = 0; o < 2; ++o)
                    {
                        const double hx = o == 0 ? HalfW : HalfD, hz = o == 0 ? HalfD : HalfW;
                        if (x - hx < block.centre.x - block.halfMeters || x + hx > block.centre.x + block.halfMeters ||
                            z - hz < block.centre.y - block.halfMeters || z + hz > block.centre.y + block.halfMeters)
                            continue;
                        bool free = true;
                        for (const HubPlanModule* other : near)
                        {
                            if (x + hx > other->rect.x && x - hx < other->rect.z && z + hz > other->rect.y && z - hz < other->rect.w)
                                free = false;
                            else if (other != &module && other->padRadiusMeters > 0.0 &&
                                     std::abs(other->padPosition.x - x) < hx + other->padRadiusMeters &&
                                     std::abs(other->padPosition.z - z) < hz + other->padRadiusMeters)
                                free = false;
                            if (!free)
                                break;
                        }
                        const double d = glm::length(glm::dvec2(x, z) - centre);
                        if (free && d < best)
                        {
                            best = d;
                            at = glm::dvec2(x, z);
                            // the door (+Z of the module): o 0 - the width along x, the door faces +-z; o 1 - faces +-x
                            hangarYaw = o == 0 ? (z >= centre.y ? 0.0 : 180.0) : (x >= centre.x ? 90.0 : 270.0);
                        }
                    }
        }
        else
        for (double x = block.centre.x - block.halfMeters + clear; x <= block.centre.x + block.halfMeters - clear; x += 10.0)
            for (double z = block.centre.y - block.halfMeters + clear; z <= block.centre.y + block.halfMeters - clear; z += 10.0)
            {
                bool free = true;
                for (const HubPlanModule* o : near)
                {
                    const auto& other = *o;
                    if (x > other.rect.x - clear && x < other.rect.z + clear && z > other.rect.y - clear && z < other.rect.w + clear)
                        free = false;
                    else if (o != &module && (other.padKind == Kind::Stilts || other.padKind == Kind::Garage) &&
                             other.padRadiusMeters > 0.0 &&
                             glm::length(glm::dvec2(other.padPosition.x, other.padPosition.z) - glm::dvec2(x, z)) < 2.0 * clear)
                        free = false;
                    if (!free)
                        break;
                }
                const double d = glm::length(glm::dvec2(x, z) - centre);
                if (free && d < best)
                {
                    best = d;
                    at = glm::dvec2(x, z);
                }
            }
        if (best > 250.0)
        {
            module.pad = false;
            module.padKind = Kind::None;
            continue;
        }
        const double side = module.under ? -1.0 : 1.0;
        module.padRadiusMeters = 18.0;
        module.padOrigin = glm::dvec3(at.x, 0.0, at.y);
        // the stilts deck 25 m up; the garage apron at the plane (its hangar behind it, its back to the building:
        // the module's -Z toward the building)
        module.padPosition = glm::dvec3(at.x, garage ? 0.0 : side * HubPadStiltsDeckTop, at.y);
        if (garage)
        {
            const glm::dvec2 to = centre - at;
            module.padYawDeg = hubPadYawFacing(module.under, -to.x, -to.y);   // +Z away from the building, on its tier
        }
        if (hangar)
        {
            // hangarYaw was chosen as the upper-tier door direction (sin, cos): the same direction on this tier
            const double door = glm::radians(hangarYaw);
            module.padYawDeg = hubPadYawFacing(module.under, std::sin(door), std::cos(door));
            module.autoLanding = true;   // the hangar's crew brings every ship in (the zone label "A")
        }
        if (hangar)
        {
            // the touchdown: the middle bay, inside (the module's door is its +Z, away from the building)
            const glm::dvec3 fwd = hubPadAxisForward(module);   // the module +Z in the plan (its true axes)
            module.padPosition = glm::dvec3(at.x + fwd.x * HubHangarBayZ, side * HubHangarFloorTop, at.y + fwd.z * HubHangarBayZ);
            module.padRadiusMeters = 13.0;
        }
    }
}
} // namespace game::navigation

namespace game::navigation
{
// The pad service's route in the city frame (author 2026-11-21: «кобра пролетела сквозь башню - нет обхода башни»;
// «нет рамок, показывающих маршрут»): from `from` up / down to the transit height (includeClimb), across to over the
// pad - round every building whose top comes within 40 m of the transit height (its footprint grown by 60 m, passed
// by its nearest free corner) - ending over the pad at the transit height. The server flies it, the HUD draws it.
// the landing places of a module (author 2026-11-22: «каждый док или площадка должны быть под диспетчером»)
inline int hubBerthPlaces(const HubPlanModule& m)
{
    switch (m.padKind)
    {
        case HubPlanModule::PadKind::Hangar: return HubHangarPlaces;
        case HubPlanModule::PadKind::Garage: return std::max(1, m.padPlaces);
        case HubPlanModule::PadKind::None: return 0;
        default: return 1;
    }
}
// a landing place's price per hour (author 2026-11-22: «посадочные места - платные. чем ниже тем дешевле, чем выше -
// тем приоритетнее. оплата - почасовая»): by its level (garage < stilts < hangar < roof; the lower side 25 % less) and
// its district (downtown / hotel the dearest)
inline int hubBerthPricePerHour(const HubPlan& plan, const HubPlanModule& m)
{
    double base = 0.0;
    switch (m.padKind)
    {
        case HubPlanModule::PadKind::Garage: base = 40.0; break;
        case HubPlanModule::PadKind::Stilts: base = 70.0; break;
        case HubPlanModule::PadKind::Hangar: base = 120.0; break;
        case HubPlanModule::PadKind::Roof: base = 160.0; break;
        default: return 0;
    }
    if (m.under)
        base *= 0.75;
    std::string zone;
    if (m.block >= 0 && static_cast<std::size_t>(m.block) < plan.blocks().size())
        zone = plan.blocks()[static_cast<std::size_t>(m.block)].zone;
    const double k = zone == "downtown" || zone == "hotel"    ? 1.8
                   : zone == "services" || zone == "canopy"   ? 1.4
                   : zone == "dock"                           ? 1.2
                   : zone == "residential"                    ? 1.0
                                                              : 0.8;
    return static_cast<int>(std::round(base * k / 5.0) * 5.0);
}

// the hub dock's berth fee per hour: a pressurised hall with service, the dearest place in a city (author 2026-11-22:
// «над белыми и красными зонами захвата площадок и доков писать стоимость в час»)
inline int hubDockPricePerHour(const HubPlan& plan)
{
    (void)plan;
    return 250;
}

// the 2-place garage (hubl_garage2, tools/models/units/hub_pads.py: G2_W 40 m apron, its two marked places at x = +-w/4,
// the module origin at the apron's centre): a place's centre from the apron centre along the module +X
inline constexpr double HubGarage2PlaceX = 10.0;
// a place's touchdown point (PLAN strip metres): a hangar's bay / a 2-place garage's place left / right of the centre
// line; the others: the pad itself. (Audit A5 2026-11-27: the garage's two places were reserved apart by the dispatcher
// but landed on ONE point - only the hangar used `bay`.)
inline glm::dvec3 hubBerthTouchdown(const HubPlanModule& pad, int bay)
{
    if (pad.padKind == HubPlanModule::PadKind::Hangar)
        return pad.padPosition + hubPadAxisRight(pad) * (bay == 0 ? -HubHangarBayX : HubHangarBayX);
    if (pad.padKind == HubPlanModule::PadKind::Garage && pad.padPlaces >= 2)
        return pad.padPosition + hubPadAxisRight(pad) * (bay == 0 ? -HubGarage2PlaceX : HubGarage2PlaceX);
    return pad.padPosition;
}
// a hangar's entry point: outside its door, 35 m off in front of the bay, at the entry height (city frame)
inline glm::dvec3 hubHangarDoorFront(const HubPlanModule& pad, int bay = 0)
{
    const double side = pad.under ? -1.0 : 1.0;
    const glm::dvec3 fwd = hubPadAxisForward(pad);
    const glm::dvec3 t = hubBerthTouchdown(pad, bay);
    return glm::dvec3(t.x, 0.0, t.z) + fwd * (HubHangarDoorHalfDepth - HubHangarBayZ + 35.0) +
           glm::dvec3(0.0, side * HubHangarEntryHeight, 0.0);
}
// inside, over the bay at the entry height
inline glm::dvec3 hubHangarBayEntry(const HubPlanModule& pad, int bay = 0)
{
    const double side = pad.under ? -1.0 : 1.0;
    const glm::dvec3 t = hubBerthTouchdown(pad, bay);
    return glm::dvec3(t.x, side * HubHangarEntryHeight, t.z);
}

inline std::vector<glm::dvec3> hubPadRoute(const HubPlan& plan, const HubPlanModule& pad, const glm::dvec3& from, double transit,
                                           bool includeClimb)
{
    const glm::dvec3 out = pad.under ? glm::dvec3(0.0, -1.0, 0.0) : glm::dvec3(0.0, 1.0, 0.0);
    const auto atTransit = [&](const glm::dvec3& p) { return p + out * (transit - glm::dot(p, out)); };
    struct Rect
    {
        double x0, z0, x1, z1;
    };
    std::vector<Rect> tall;
    for (const auto& m : plan.modules())
    {
        if (m.under != pad.under)
            continue;
        // not the pad's own tower: its goal is over it - as an obstacle it was never reached and the service flew the
        // ship back and forth between its corners (a tower-top pad, live 2026-11-22)
        if (&m == &pad ||
            (pad.padPosition.x > std::min(m.rect.x, m.rect.z) && pad.padPosition.x < std::max(m.rect.x, m.rect.z) &&
             pad.padPosition.z > std::min(m.rect.y, m.rect.w) && pad.padPosition.z < std::max(m.rect.y, m.rect.w)))
            continue;
        const double top = pad.under ? -m.y0 : m.y1;
        if (top + 40.0 < transit)
            continue;
        tall.push_back({std::min(m.rect.x, m.rect.z) - 60.0, std::min(m.rect.y, m.rect.w) - 60.0, std::max(m.rect.x, m.rect.z) + 60.0,
                        std::max(m.rect.y, m.rect.w) + 60.0});
    }
    const auto inside = [&](double x, double z, const Rect* skip) {
        for (const auto& r : tall)
            if (&r != skip && x > r.x0 && x < r.x1 && z > r.z0 && z < r.z1)
                return true;
        return false;
    };
    // the first rect the segment a -> b crosses (slab test in xz), nearest to a
    const auto blocking = [&](const glm::dvec2& a, const glm::dvec2& b, double& tHit) -> const Rect* {
        const Rect* best = nullptr;
        tHit = 2.0;
        const glm::dvec2 d = b - a;
        for (const auto& r : tall)
        {
            double t0 = 0.0, t1 = 1.0;
            bool miss = false;
            for (int k = 0; k < 2 && !miss; ++k)
            {
                const double lo = k == 0 ? r.x0 : r.z0, hi = k == 0 ? r.x1 : r.z1;
                if (std::abs(d[k]) < 1.0e-9)
                {
                    miss = a[k] <= lo || a[k] >= hi;
                    continue;
                }
                double u = (lo - a[k]) / d[k], v = (hi - a[k]) / d[k];
                if (u > v)
                    std::swap(u, v);
                t0 = std::max(t0, u);
                t1 = std::min(t1, v);
                miss = t0 >= t1;
            }
            if (!miss && t0 < tHit)
            {
                tHit = t0;
                best = &r;
            }
        }
        return best;
    };
    std::vector<glm::dvec3> route;
    const glm::dvec3 start = atTransit(from);
    if (includeClimb)
        route.push_back(start);
    // a repair hangar is flown INTO: the route ends in front of its door (the service goes down there and in)
    const glm::dvec3 goal = atTransit(pad.padKind == HubPlanModule::PadKind::Hangar ? hubHangarDoorFront(pad) : pad.padPosition);
    glm::dvec2 at(start.x, start.z);
    const glm::dvec2 end(goal.x, goal.z);
    for (int guard = 0; guard < 12; ++guard)
    {
        double t = 0.0;
        const Rect* r = blocking(at, end, t);
        if (!r)
            break;
        // its corner that keeps the way shortest and is not inside another tall footprint
        const glm::dvec2 corners[4] = {{r->x0 - 1.0, r->z0 - 1.0}, {r->x1 + 1.0, r->z0 - 1.0}, {r->x1 + 1.0, r->z1 + 1.0},
                                       {r->x0 - 1.0, r->z1 + 1.0}};
        double best = 1.0e30;
        glm::dvec2 via = corners[0];
        for (const auto& c : corners)
        {
            double tt = 0.0;
            if (inside(c.x, c.y, r) || glm::length(c - at) < 1.0 || blocking(at, c, tt) == r)
                continue;   // inside another footprint, or reached only through this one
            const double len = glm::length(c - at) + glm::length(end - c);
            if (len < best)
            {
                best = len;
                via = c;
            }
        }
        if (best > 1.0e29)
            break;   // boxed in: straight on (the transit height is the fallback)
        route.push_back(glm::dvec3(via.x, goal.y, via.y));
        at = via;
    }
    route.push_back(goal);
    return route;
}
} // namespace game::navigation
