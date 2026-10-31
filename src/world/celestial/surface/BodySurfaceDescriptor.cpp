#include "src/world/celestial/surface/BodySurfaceDescriptor.h"

#include <algorithm>
#include <cmath>
#include <fstream>

#include <nlohmann/json.hpp>

#include "src/world/celestial/surface/SurfaceIdentity.h"

namespace world::celestial::surface
{
namespace
{
    using nlohmann::json;

    double number(const json& j, const char* key, double fallback)
    {
        const auto it = j.find(key);
        return it != j.end() && it->is_number() ? it->get<double>() : fallback;
    }

    std::string text(const json& j, const char* key)
    {
        const auto it = j.find(key);
        return it != j.end() && it->is_string() ? it->get<std::string>() : std::string();
    }

    std::uint64_t seedOf(const json& j)
    {
        const auto it = j.find("seed");
        if (it == j.end())
            return 0;
        if (it->is_number_unsigned())
            return it->get<std::uint64_t>();
        if (it->is_string())
            return std::stoull(it->get<std::string>(), nullptr, 0);
        return 0;
    }

    std::vector<std::pair<std::string, double>> fractions(const json& j, const char* key)
    {
        std::vector<std::pair<std::string, double>> out;
        const auto it = j.find(key);
        if (it == j.end() || !it->is_object())
            return out;
        for (const auto& [name, value] : it->items())
            if (value.is_number())
                out.push_back({name, value.get<double>()});
        // Stable order whatever the JSON object order.
        std::sort(out.begin(), out.end());
        return out;
    }

    bool readJson(const std::string& path, json& out, std::string* error)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            if (error)
                *error = "cannot open " + path;
            return false;
        }
        out = json::parse(in, nullptr, false);
        if (out.is_discarded())
        {
            if (error)
                *error = "invalid JSON " + path;
            return false;
        }
        return true;
    }

    const json& section(const json& j, const char* key)
    {
        static const json empty = json::object();
        const auto it = j.find(key);
        return it != j.end() && it->is_object() ? *it : empty;
    }
}

bool loadGeneratedBody(const std::string& path, BodySurfaceDescriptor& out, std::string* error)
{
    json j;
    if (!readJson(path, j, error))
        return false;
    out = BodySurfaceDescriptor {};
    out.id = text(j, "id");
    out.name = text(j, "designation");
    out.seed = seedOf(j);
    out.kind = text(j, "kind");
    out.origin = text(j, "origin");

    const json& physical = section(j, "physical");
    out.radiusM = number(physical, "radius_km", 1000.0) * 1000.0;
    out.surfaceGravityMs2 = number(physical, "surface_gravity_ms2", 1.0);
    out.bodyClass = text(physical, "body_class");

    const json& thermal = section(j, "thermal");
    out.meanTemperatureK = number(thermal, "mean_surface_temperature_k", 250.0);
    out.minTemperatureK = number(thermal, "min_surface_temperature_k", out.meanTemperatureK);
    out.maxTemperatureK = number(thermal, "max_surface_temperature_k", out.meanTemperatureK);

    const json& atmosphere = section(j, "atmosphere");
    out.surfacePressurePa = number(atmosphere, "surface_pressure_pa", 0.0);
    out.cloudPotential = number(atmosphere, "cloud_potential", 0.0);
    out.atmosphereComposition = fractions(atmosphere, "composition");

    const json& geophysics = section(j, "geophysics");
    out.tectonicRegime = text(geophysics, "tectonic_regime");
    out.volcanism = text(geophysics, "volcanism");
    out.hydrosphere = text(geophysics, "hydrosphere");

    const json& spin = section(j, "spin");
    out.obliquityDeg = number(spin, "obliquity_deg", 0.0);
    out.rotationPeriodHours = number(spin, "rotation_period_hours", 24.0);

    const json& surface = section(j, "surface_generation");
    out.terrainProfile = text(surface, "terrain_profile");
    out.reliefAmplitudeKm = number(surface, "relief_amplitude_km", 1.0);
    out.roughness = number(surface, "roughness", 0.5);
    out.craterDensity = number(surface, "crater_density", 0.0);
    out.resurfacingRate = number(surface, "resurfacing_rate", 0.0);
    out.erosionPotential = number(surface, "erosion_potential", 0.0);
    out.glaciationPotential = number(surface, "glaciation_potential", 0.0);
    out.weatherPotential = number(surface, "weather_potential", 0.0);
    out.oceanCoverage = number(surface, "ocean_coverage", 0.0);
    out.iceCoverage = number(surface, "ice_coverage", 0.0);
    out.surfaceMaterials = fractions(surface, "surface_materials");

    const json& bulk = section(j, "bulk_composition");
    out.ironCore = number(bulk, "iron_core", 0.0);
    out.silicateMantle = number(bulk, "silicate_mantle", 0.0);
    out.waterIce = number(bulk, "water_ice", 0.0);
    out.otherVolatiles = number(bulk, "other_volatiles", 0.0);
    out.hydrogenHelium = number(bulk, "hydrogen_helium_envelope", 0.0);

    const json& resources = section(j, "resources");
    out.orbitalAccessIndex = number(section(resources, "industrial_suitability"), "orbital_access_index", 0.0);

    // Curated observed bodies (Sol) keep their imported maps in the game;
    // the procedural provider is still a valid view of the same physics.
    out.source = SurfaceSourceKind::ProceduralFromDescriptor;
    return true;
}

SurfaceArchetype surfaceArchetype(const BodySurfaceDescriptor& b)
{
    const std::string& c = b.bodyClass;
    if (c == "gas_giant" || c == "super_jupiter")
        return SurfaceArchetype::GasGiant;
    if (c == "ice_giant" || c == "sub_neptune")
        return SurfaceArchetype::IceGiant;
    const std::string& p = b.terrainProfile;
    if (p == "gas_giant_bands")
        return SurfaceArchetype::GasGiant;
    if (p == "ice_giant_bands")
        return SurfaceArchetype::IceGiant;
    if (p == "lava_ocean" || (p == "volcanic_plains" && b.meanTemperatureK > 600.0) ||
        b.volcanism == "extreme")
        return SurfaceArchetype::Volcanic;
    const bool liquidWater = (b.hydrosphere == "surface_ocean" || b.hydrosphere == "global_ocean") &&
                             b.meanTemperatureK > 250.0 && b.meanTemperatureK < 400.0;
    if (liquidWater && b.surfacePressurePa > 600.0)
        return SurfaceArchetype::Terrestrial;
    if (p == "cratered_ice" || p == "ice_shell_fractured" || p == "cryovolcanic_ice" ||
        p == "hydrocarbon_lakes" || b.hydrosphere == "ice_shell" || b.bodyClass == "dwarf_icy")
        return SurfaceArchetype::Icy;
    if (b.surfacePressurePa > 100.0)
        return SurfaceArchetype::DryAtmospheric;
    return SurfaceArchetype::AirlessRocky;
}

const char* archetypeName(SurfaceArchetype a)
{
    switch (a)
    {
        case SurfaceArchetype::Terrestrial: return "terrestrial";
        case SurfaceArchetype::AirlessRocky: return "airless_rocky";
        case SurfaceArchetype::DryAtmospheric: return "dry_atmospheric";
        case SurfaceArchetype::Volcanic: return "volcanic";
        case SurfaceArchetype::Icy: return "icy";
        case SurfaceArchetype::GasGiant: return "gas_giant";
        case SurfaceArchetype::IceGiant: return "ice_giant";
    }
    return "unknown";
}

bool hasSolidSurface(const BodySurfaceDescriptor& b)
{
    const SurfaceArchetype a = surfaceArchetype(b);
    return a != SurfaceArchetype::GasGiant && a != SurfaceArchetype::IceGiant;
}

SurfaceDetailPolicy surfaceDetailPolicy(const BodySurfaceDescriptor& b)
{
    SurfaceDetailPolicy p;
    if (!hasSolidSurface(b))
    {
        p.solidSurface = false;
        p.groundReachable = false;
        // The cloud deck down to features of a few km (storm cells).
        p.finestVertexSpacingM = std::max(2000.0, b.radiusM * 5.0e-5);
        p.reason = "no solid surface: cloud deck only";
        return p;
    }
    // Reachability of the ground for any realistic lander: crushing
    // pressure, gravity a crew cannot land in, or a molten surface.
    const double bar = b.surfacePressurePa / 1.0e5;
    if (bar > 100.0)
        p.reason = "surface pressure " + std::to_string(static_cast<int>(bar)) + " bar";
    else if (b.surfaceGravityMs2 > 30.0)
        p.reason = "surface gravity " + std::to_string(static_cast<int>(b.surfaceGravityMs2)) + " m/s2";
    else if (b.meanTemperatureK > 1000.0 || b.terrainProfile == "lava_ocean")
        p.reason = "molten surface";
    if (!p.reason.empty())
    {
        p.groundReachable = false;
        p.finestVertexSpacingM = 250.0; // high-altitude overflight
        return p;
    }
    p.finestVertexSpacingM = 0.25;
    p.reason = "reachable ground";
    return p;
}

// ---- small bodies ----------------------------------------------------------

bool loadGeneratedReservoir(const std::string& path, ReservoirSurfaceDescriptor& out, std::string* error)
{
    json j;
    if (!readJson(path, j, error))
        return false;
    out = ReservoirSurfaceDescriptor {};
    out.id = text(j, "id");
    out.kind = text(j, "kind");
    out.seed = seedOf(j);
    out.innerAu = number(j, "inner_au", 0.0);
    out.outerAu = number(j, "outer_au", 0.0);
    const json& size = section(j, "size_distribution");
    out.sizeSlope = number(size, "slope", -3.5);
    out.minBodyKm = number(size, "min_body_km", 0.001);
    out.maxBodyKm = number(size, "max_body_km", 100.0);
    out.composition = fractions(j, "composition");
    return true;
}

int asteroidCandidatesInCell(const ReservoirSurfaceDescriptor& reservoir, const glm::ivec3& cell)
{
    const std::uint64_t h = latticeHash(deriveSeed(reservoir.seed, "asteroid_cells"), cell.x, cell.y, cell.z);
    return 1 + static_cast<int>(unitFromHash(h) * 6.0);
}

SmallBodyDescriptor asteroidInReservoir(const ReservoirSurfaceDescriptor& reservoir, const glm::ivec3& cell,
                                        int candidate)
{
    SmallBodyDescriptor out;
    const std::uint64_t cellSeed =
        latticeHash(deriveSeed(reservoir.seed, "asteroid"), cell.x, cell.y, cell.z);
    out.seed = deriveSeed(cellSeed, "candidate", static_cast<std::uint64_t>(candidate));
    out.id = reservoir.id + "/asteroid:" + std::to_string(cell.x) + "," + std::to_string(cell.y) + "," +
             std::to_string(cell.z) + ":" + std::to_string(candidate);

    // Size: the reservoir's power law N(>D) ~ D^(slope + 1) between the
    // visible minimum (100 m: a landmark-scale object) and its maximum.
    const double slope = std::min(-1.1, reservoir.sizeSlope + 1.0);
    const double dMin = std::max(0.1, reservoir.minBodyKm);
    const double dMax = std::max(dMin * 2.0, std::min(reservoir.maxBodyKm, 60.0));
    const double u = unitFromHash(out.seed, 0);
    const double a = std::pow(dMin, slope);
    const double b = std::pow(dMax, slope);
    const double diameterKm = std::pow(a + u * (b - a), 1.0 / slope);
    out.meanRadiusM = diameterKm * 500.0;

    // Composition: the reservoir's mixture, one draw per body.
    double total = 0.0;
    for (const auto& [name, fraction] : reservoir.composition)
        total += fraction;
    double pick = unitFromHash(out.seed, 1) * std::max(total, 1.0e-9);
    for (const auto& [name, fraction] : reservoir.composition)
    {
        pick -= fraction;
        if (pick > 0.0)
            continue;
        if (name.find("metal") != std::string::npos)
            out.composition = SmallBodyComposition::Metallic;
        else if (name.find("ice") != std::string::npos)
            out.composition = SmallBodyComposition::Icy;
        else if (name.find("carbon") != std::string::npos)
            out.composition = SmallBodyComposition::Carbonaceous;
        else
            out.composition = SmallBodyComposition::Silicate;
        break;
    }
    return out;
}

const char* compositionName(SmallBodyComposition c)
{
    switch (c)
    {
        case SmallBodyComposition::Carbonaceous: return "carbonaceous";
        case SmallBodyComposition::Silicate: return "silicate";
        case SmallBodyComposition::Metallic: return "metallic";
        case SmallBodyComposition::Icy: return "icy";
    }
    return "unknown";
}
} // namespace world::celestial::surface
