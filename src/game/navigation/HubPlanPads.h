#pragma once

// The hub city plan's addresses and landing pads (author 2026-11-21: «каждому блоку адрес ... развозить по заявке»;
// «каждому модулю города посадочную площадку - на свободных участках, на крышах, на ножках ... как гараж на 1-2
// места»). Header-only: the plan loader (HubPlanCatalog.cpp, the game) and the city assembler (HubCityAssembly.cpp, the
// game and the mesh editor) place the same pads from the same plan. The pad modules: tools/models/units/hub_pads.py.

#include <algorithm>
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
// the pad's capture zone (author 2026-11-21: «прозрачный куб с белёсыми рёбрами, появляется когда до площадки метров
// 100»): a box over the deck - half side, height; shown within HubPadZoneShowMeters of the pad
inline constexpr double HubPadZoneHalf = 25.0;
inline constexpr double HubPadZoneHeight = 60.0;
inline constexpr double HubPadZoneShowMeters = 100.0;

// the pad module folder of a building's pad ("" none)
std::string hubPadModuleFolder(const HubPlanModule& m);

// fills block.address, module.address / number and the pads (kind, touchdown point, module origin + yaw)
void assignHubPlanAddressesAndPads(std::vector<HubPlanBlock>& blocks, std::vector<HubPlanModule>& modules);
} // namespace game::navigation

#include "src/game/navigation/HubPlanCatalog.h"

namespace game::navigation
{
inline std::string hubPadModuleFolder(const HubPlanModule& m)
{
    switch (m.padKind)
    {
        case HubPlanModule::PadKind::Roof: return "hubl_pad_roof";
        case HubPlanModule::PadKind::Stilts: return "hubl_pad_stilts";
        case HubPlanModule::PadKind::Garage: return m.padPlaces >= 2 ? "hubl_garage2" : "hubl_garage1";
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
        // a dock / pad / pier of its own: no pad
        if (module.module.find("dock") != std::string::npos || module.module.find("pad") != std::string::npos ||
            module.module.find("pier") != std::string::npos)
            continue;
        const double half = 0.5 * std::min(module.rect.z - module.rect.x, module.rect.w - module.rect.y);
        const double height = module.under ? -module.y0 : module.y1;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        const double side = module.under ? -1.0 : 1.0;
        if (half >= 0.5 * HubPadDeckSide && height > 60.0)
        {
            // a tall building with a roof wide enough for the 36 m deck: the pad on its roof (the lower tier: its
            // underside, the module upside down)
            module.padKind = Kind::Roof;
            module.padRadiusMeters = 0.5 * HubPadDeckSide;
            const double roof = module.under ? module.y0 : module.y1;
            module.padOrigin = glm::dvec3(centre.x, roof, centre.y);
            module.padPosition = glm::dvec3(centre.x, roof + side * HubPadRoofDeckTop, centre.y);
        }
        else if (half >= 15.0 && height <= 60.0)
            module.padKind = Kind::Garage;   // a low wide one: a 1-2 place garage beside it (a free plot below)
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
        const bool garage = module.padKind == Kind::Garage;
        if ((module.padKind != Kind::Stilts && !garage) || module.block < 0)
            continue;
        const auto& block = blocks[static_cast<std::size_t>(module.block)];
        const double clear = HubPadClearMeters;
        const glm::dvec2 centre(0.5 * (module.rect.x + module.rect.z), 0.5 * (module.rect.y + module.rect.w));
        std::vector<const HubPlanModule*> near;   // this block, this tier: the buildings and the free-plot pads so far
        for (const auto& other : modules)
            if (other.block == module.block && other.under == module.under)
                near.push_back(&other);
        double best = 1.0e30;
        glm::dvec2 at(0.0);
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
            module.padYawDeg = glm::degrees(std::atan2(-to.x, -to.y));
        }
    }
}
} // namespace game::navigation
