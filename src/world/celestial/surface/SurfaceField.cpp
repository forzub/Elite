#include "src/world/celestial/surface/SurfaceField.h"

#include <cstdio>

namespace world::celestial::surface
{
const char* materialName(SurfaceMaterial material)
{
    switch (material)
    {
        case SurfaceMaterial::Rock: return "rock";
        case SurfaceMaterial::Basalt: return "basalt";
        case SurfaceMaterial::Regolith: return "regolith";
        case SurfaceMaterial::Sand: return "sand";
        case SurfaceMaterial::Soil: return "soil";
        case SurfaceMaterial::Vegetation: return "vegetation";
        case SurfaceMaterial::Ice: return "ice";
        case SurfaceMaterial::Snow: return "snow";
        case SurfaceMaterial::Salt: return "salt";
        case SurfaceMaterial::Lava: return "lava";
        default: return "unknown";
    }
}

const char* biomeName(Biome biome)
{
    switch (biome)
    {
        case Biome::Ocean: return "ocean";
        case Biome::Lake: return "lake";
        case Biome::IceSheet: return "ice_sheet";
        case Biome::Snow: return "snow";
        case Biome::Tundra: return "tundra";
        case Biome::BorealForest: return "boreal_forest";
        case Biome::TemperateForest: return "temperate_forest";
        case Biome::Grassland: return "grassland";
        case Biome::Savanna: return "savanna";
        case Biome::Desert: return "desert";
        case Biome::Rainforest: return "rainforest";
        case Biome::Barren: return "barren";
        case Biome::Volcanic: return "volcanic";
        case Biome::IcePlain: return "ice_plain";
        case Biome::GasDeck: return "gas_deck";
    }
    return "unknown";
}

const char* featureClassName(FeatureClass featureClass)
{
    switch (featureClass)
    {
        case FeatureClass::None: return "none";
        case FeatureClass::Continent: return "continent";
        case FeatureClass::MountainRange: return "mountain_range";
        case FeatureClass::ImpactCrater: return "impact_crater";
        case FeatureClass::Volcano: return "volcano";
        case FeatureClass::Lake: return "lake";
        case FeatureClass::IceCap: return "ice_cap";
        case FeatureClass::Storm: return "storm";
    }
    return "unknown";
}

std::string featureLabel(FeatureClass featureClass, std::uint64_t id)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%s:%016llx", featureClassName(featureClass),
                  static_cast<unsigned long long>(id));
    return buffer;
}
} // namespace world::celestial::surface
