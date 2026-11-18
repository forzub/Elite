#pragma once

#include <cstdint>
#include <string>

namespace game::simulation
{

/*
    Damage of a hub city's buildings (author 2026-11-18: «а город вообще имеет систему учета повреждений?» -> «да, делай
    учёт повреждений»). The server owns, per city object (ObjectType::HubBeltCity) and per assembly node (one building or
    module of the generated city), the energy the weapons put into it; crossing thresholds the node changes VARIANT:
      Missile / Laser  - the first stage: the damaged module <folder>_dmg_missile / _dmg_laser (the kind of the hit that
                         crossed it: a shaped warhead -> missile; a laser or a kinetic slug -> laser (+ railgun holes));
      Gutted           - 4x the first threshold: the facade torn off (<folder>_dmg_gutted);
      Destroyed        - 12x: the module is gone (its girders, roads, crossings are infrastructure and stay).
    A module without damaged variants (tools/models/units/hub_damage.py builds them for the residential / office
    buildings) goes straight from Intact to Destroyed at the last threshold. Only the changed nodes are replicated
    (ObjectSnapshot::buildingStates); the client hides the node and draws the variant's own package in its place.
*/
enum class CityBuildingVariant : std::uint8_t
{
    Intact = 0,
    Missile = 1,
    Laser = 2,
    Gutted = 3,
    Destroyed = 4
};

struct CityBuildingState
{
    std::uint32_t node = 0;      // semantic node index in the city package
    std::uint8_t variant = 0;    // CityBuildingVariant
};

// the module folder suffix of a damaged variant ("" for Intact / Destroyed)
inline const char* cityVariantSuffix(std::uint8_t variant)
{
    switch (static_cast<CityBuildingVariant>(variant))
    {
        case CityBuildingVariant::Missile: return "_dmg_missile";
        case CityBuildingVariant::Laser: return "_dmg_laser";
        case CityBuildingVariant::Gutted: return "_dmg_gutted";
        default: return "";
    }
}

// a city collision volume id is "<node id>.<module folder>.<k>" (tools/models/pads.ref_set_doc): the module folder
inline std::string cityModuleFolderOfCollision(const std::string& collisionId)
{
    const auto a = collisionId.find('.');
    const auto b = collisionId.rfind('.');
    return a == std::string::npos || b == std::string::npos || b <= a ? std::string() : collisionId.substr(a + 1, b - a - 1);
}

// the city's infrastructure (the lattice it hangs on): never damaged here
inline bool cityInfrastructureFolder(const std::string& folder)
{
    for (const char* k : {"girder", "road", "tube", "monorail", "crossing", "junction", "canopy"})
        if (folder.find(k) != std::string::npos)
            return true;
    return false;
}

} // namespace game::simulation
