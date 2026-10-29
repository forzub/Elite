#include "src/world/generation/InterstellarGenerator.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/DescriptorJson.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/StellarSystemGenerator.h"

namespace world::generation
{

namespace
{

using namespace astro;
using ojson = nlohmann::ordered_json;

constexpr double CubicCmPerCubicLy = LightYearMeters * LightYearMeters * LightYearMeters * 1.0e6;

std::string cellTag(long long x, long long y, long long z)
{
    std::ostringstream s;
    s << (x < 0 ? "m" : "p") << std::llabs(x) << (y < 0 ? "m" : "p") << std::llabs(y)
      << (z < 0 ? "m" : "p") << std::llabs(z);
    return s.str();
}

ojson provenance(std::uint64_t seed, const UniverseGenerationVersions& v, const char* model)
{
    return ojson {{"generator", "world::generation::Interstellar"}, {"seed", hexSeed(seed)},
                  {"interstellar_version", v.interstellar}, {"model", model}};
}

// Peculiar velocity relative to the Local Standard of Rest (thin-disk dispersion).
ojson velocityJson(RandomStream& rs)
{
    return ojson::array({rs.normal(0.0, 30.0), rs.normal(0.0, 20.0), rs.normal(0.0, 15.0)});
}

double coolingTeffK(double massJupiter, double ageGyr)
{
    // Burrows et al. 1997-like: substellar objects cool as they age.
    return 100.0 * std::pow(massJupiter, 0.55) * std::pow(std::max(ageGyr, 0.05) / 5.0, -0.32);
}

struct ClassRate
{
    const char* type;
    double perCell;
};

// Materialised notable members per 25-ly cell (the physical populations are
// far denser: ~1 Jupiter-mass rogue per few stars, Sumi et al. 2023).
constexpr ClassRate Rates[] = {
    {"rogue_planet_giant", 0.15}, {"rogue_planet_terrestrial", 0.12}, {"binary_rogue_planet", 0.01},
    {"rogue_moon", 0.05}, {"brown_dwarf", 0.08}, {"white_dwarf_remnant", 0.02}, {"neutron_star", 0.002},
    {"interstellar_comet", 0.06}, {"debris_group", 0.02},
};

} // namespace

glm::dvec3 galacticToIcrsLy(double lDeg, double bDeg, double d)
{
    const double l = lDeg * DegToRad, b = bDeg * DegToRad;
    const glm::dvec3 g(d * std::cos(b) * std::cos(l), d * std::cos(b) * std::sin(l), d * std::sin(b));
    // Transpose of the ICRS -> Galactic rotation.
    const double r[3][3] = {
        {-0.0548755604, -0.8734370902, -0.4838350155},
        {0.4941094279, -0.4448296300, 0.7469822445},
        {-0.8676661490, -0.1980763734, 0.4559837762}};
    return {r[0][0] * g.x + r[1][0] * g.y + r[2][0] * g.z,
            r[0][1] * g.x + r[1][1] * g.y + r[2][1] * g.z,
            r[0][2] * g.x + r[1][2] * g.y + r[2][2] * g.z};
}

std::vector<InterstellarObject> generateInterstellarObjects(
    std::uint64_t universeSeed,
    const std::vector<glm::dvec3>& systems,
    const InterstellarSettings& s,
    const UniverseGenerationVersions& v)
{
    std::vector<InterstellarObject> out;
    const long long n = static_cast<long long>(std::ceil(s.radiusLy / s.cellSizeLy));
    for (long long cx = -n; cx < n; ++cx)
        for (long long cy = -n; cy < n; ++cy)
            for (long long cz = -n; cz < n; ++cz)
            {
                const std::uint64_t cellSeed = galacticCellSeed(universeSeed, cx, cy, cz);
                const glm::dvec3 origin(cx * s.cellSizeLy, cy * s.cellSizeLy, cz * s.cellSizeLy);
                for (const auto& rate : Rates)
                {
                    RandomStream rs(cellSeed, std::string("interstellar_") + rate.type);
                    const int count = rs.poisson(rate.perCell);
                    for (int k = 0; k < count; ++k)
                    {
                        const std::uint64_t seed = deriveSeed(cellSeed, rate.type, static_cast<std::uint64_t>(k));
                        RandomStream os(seed, "object");
                        const glm::dvec3 p = origin + glm::dvec3(os.uniform(), os.uniform(), os.uniform()) * s.cellSizeLy;
                        const double dSol = glm::length(p);
                        if (dSol > s.radiusLy || dSol < 2.0)
                            continue;
                        bool nearSystem = false;
                        for (const auto& sp : systems)
                            nearSystem = nearSystem || glm::length(p - sp) < s.minDistanceToSystemLy;
                        if (nearSystem)
                            continue;
                        const std::string type = rate.type;
                        if (type == "brown_dwarf" && dSol < s.surveyCompleteBrownDwarfLy)
                            continue; // WISE would have found it
                        InterstellarObject o;
                        o.id = "ism_obj_" + cellTag(cx, cy, cz) + "_" + type + "_" + std::to_string(k);
                        o.positionLy = p;
                        const double age = os.uniform(0.5, 10.0);
                        ojson props;
                        props["age_gyr"] = age;
                        props["velocity_lsr_kms"] = velocityJson(os);
                        std::ostringstream name;
                        if (type == "rogue_planet_giant" || type == "binary_rogue_planet")
                        {
                            const double mj = os.logUniform(0.3, 10.0);
                            const double teff = coolingTeffK(mj, age);
                            o.objectType = type == "binary_rogue_planet" ? "binary_rogue_planet" : "rogue_planet";
                            props["mass_kg"] = mj * JupiterMassKg;
                            props["mu_m3s2"] = G * mj * JupiterMassKg;
                            props["radius_km"] = JupiterRadiusM / 1000.0 * std::pow(mj, -0.04);
                            props["teff_k"] = teff;
                            props["body_class"] = mj > 3.0 ? "super_jupiter" : "gas_giant";
                            props["composition"] = {{"H2", 0.86}, {"He", 0.136}, {"CH4", 0.003}};
                            props["origin"] = os.chance(0.7) ? "ejected_by_planet_scattering" : "formed_in_isolation";
                            if (type == "binary_rogue_planet")
                            {
                                props["companion_mass_kg"] = mj * os.uniform(0.3, 1.0) * JupiterMassKg;
                                props["separation_au"] = os.logUniform(5.0, 200.0);
                            }
                            name << "Nomad " << (type == "binary_rogue_planet" ? "binary " : "") << "J-" << out.size() + 1;
                            o.tags = {"rogue", "cold", "infrared_only"};
                        }
                        else if (type == "rogue_planet_terrestrial")
                        {
                            const double me = os.logUniform(0.3, 10.0);
                            const bool hydrogen = me > 1.0 && os.chance(0.5);
                            o.objectType = "rogue_planet";
                            props["mass_kg"] = me * EarthMassKg;
                            props["mu_m3s2"] = G * me * EarthMassKg;
                            props["radius_km"] = EarthRadiusM / 1000.0 * std::pow(me, 0.27);
                            props["teff_k"] = os.uniform(20.0, 45.0);
                            props["body_class"] = me > 2.0 ? "super_earth" : "rocky";
                            // A retained primordial H2 envelope can keep a subsurface ocean liquid
                            // without a star (Stevenson 1999).
                            props["atmosphere"] = hydrogen ? "primordial_h2_greenhouse" : "frozen_out";
                            props["hydrosphere"] = hydrogen ? "possible_subsurface_ocean" : "ice";
                            props["origin"] = "ejected_by_planet_scattering";
                            name << "Nomad T-" << out.size() + 1;
                            o.tags = {"rogue", "frozen", "dark"};
                        }
                        else if (type == "rogue_moon")
                        {
                            const double me = os.logUniform(0.001, 0.1);
                            o.objectType = "rogue_moon";
                            props["mass_kg"] = me * EarthMassKg;
                            props["mu_m3s2"] = G * me * EarthMassKg;
                            props["radius_km"] = std::cbrt(3.0 * me * EarthMassKg / (4.0 * Pi * 2000.0)) / 1000.0;
                            props["teff_k"] = os.uniform(10.0, 25.0);
                            props["composition"] = {{"water_ice", 0.5}, {"silicate", 0.45}, {"organics", 0.05}};
                            props["origin"] = "stripped_during_planet_ejection";
                            name << "Drifter M-" << out.size() + 1;
                            o.tags = {"rogue", "icy"};
                        }
                        else if (type == "brown_dwarf")
                        {
                            const double mj = os.uniform(13.0, 75.0);
                            const double teff = std::min(2400.0, coolingTeffK(mj, age) * 2.2);
                            o.objectType = "brown_dwarf";
                            props["mass_kg"] = mj * JupiterMassKg;
                            props["mu_m3s2"] = G * mj * JupiterMassKg;
                            props["radius_km"] = JupiterRadiusM / 1000.0 * 0.95;
                            props["teff_k"] = teff;
                            props["spectral_type"] = teff > 1300.0 ? "L" : teff > 550.0 ? "T" : "Y";
                            props["composition"] = {{"H2", 0.86}, {"He", 0.136}, {"CH4", teff < 1300.0 ? 0.004 : 0.0}};
                            name << "Ember " << props["spectral_type"].get<std::string>() << "-" << out.size() + 1;
                            o.tags = {"substellar", "infrared"};
                        }
                        else if (type == "white_dwarf_remnant" || type == "neutron_star")
                        {
                            const bool ns = type == "neutron_star";
                            const double ms = ns ? os.uniform(1.2, 2.0) : os.uniform(0.55, 0.9);
                            o.objectType = "stellar_remnant";
                            props["remnant"] = ns ? "neutron_star" : "white_dwarf";
                            props["mass_kg"] = ms * SunMassKg;
                            props["mu_m3s2"] = G * ms * SunMassKg;
                            props["radius_km"] = ns ? 12.0 : 0.0127 * SunRadiusM / 1000.0 * std::pow(ms / 0.6, -1.0 / 3.0);
                            props["teff_k"] = ns ? 5.0e5 : os.uniform(3500.0, 6000.0);
                            props["age_gyr"] = ns ? os.uniform(0.05, 2.0) : os.uniform(6.0, 11.0);
                            name << (ns ? "Pulsar remnant R-" : "Cinder W-") << out.size() + 1;
                            o.tags = {"remnant", ns ? "radio" : "faint"};
                        }
                        else if (type == "interstellar_comet")
                        {
                            const double km = os.logUniform(0.5, 40.0);
                            o.objectType = "interstellar_comet";
                            props["radius_km"] = km;
                            props["mass_kg"] = 4.0 / 3.0 * Pi * std::pow(km * 1000.0, 3.0) * 600.0;
                            props["composition"] = {{"water_ice", 0.45}, {"co_co2_ices", 0.25}, {"silicate_dust", 0.2}, {"organics", 0.1}};
                            props["origin"] = "ejected_planetesimal";
                            name << "Wanderer C-" << out.size() + 1;
                            o.tags = {"comet", "volatile_rich"};
                        }
                        else
                        {
                            o.objectType = "debris_group";
                            props["members_materialised"] = 0;
                            props["estimated_members_above_100m"] = std::floor(os.logUniform(100.0, 5000.0));
                            props["extent_au"] = os.logUniform(10.0, 1000.0);
                            props["total_mass_kg"] = os.logUniform(1.0e15, 1.0e18);
                            props["composition"] = {{"silicate", 0.5}, {"metal", 0.2}, {"water_ice", 0.2}, {"organics", 0.1}};
                            props["origin"] = "disrupted_planetary_system_ejecta";
                            name << "Shoal D-" << out.size() + 1;
                            o.tags = {"debris", "resource"};
                        }
                        o.name = name.str();
                        props["gameplay_gravity"] = o.objectType != "debris_group";
                        props["provenance"] = provenance(seed, v, "cell population, notable member");
                        o.properties = props;
                        o.description = "Generated interstellar object (" + o.objectType + ").";
                        out.push_back(std::move(o));
                    }
                }
            }
    return out;
}

std::vector<InterstellarObject> generateInterstellarMedium(
    std::uint64_t universeSeed,
    const InterstellarSettings& s,
    const UniverseGenerationVersions& v)
{
    std::vector<InterstellarObject> out;
    struct Region
    {
        const char* id;
        const char* name;
        const char* phase;
        double l, b, distanceLy;
        glm::dvec3 radiiLy;
        double densityCm3, temperatureK, ionization, molecular, dustToGas, ice;
        const char* source;
    };
    // Authored local environment (approximate geometry from the cited work).
    const Region authored[] = {
        {"ism_local_bubble", "Local Bubble", "hot_ionized", 0.0, 0.0, 0.0, {300.0, 250.0, 500.0},
         0.005, 1.0e6, 1.0, 0.0, 0.0, 0.0, "Frisch 2011; Zucker et al. 2022 (cavity ~100 pc)"},
        {"ism_local_interstellar_cloud", "Local Interstellar Cloud", "warm_partially_ionized", 170.0, -10.0, 5.0,
         {9.0, 6.0, 5.0}, 0.25, 7500.0, 0.3, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (LIC; Sol at its edge)"},
        {"ism_g_cloud", "G Cloud", "warm_partially_ionized", 315.0, 0.0, 4.0, {5.0, 4.0, 3.0},
         0.2, 5500.0, 0.3, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (towards alpha Centauri)"},
        {"ism_blue_cloud", "Blue Cloud", "warm_neutral", 230.0, -15.0, 6.0, {3.0, 2.0, 2.0},
         0.15, 3000.0, 0.1, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (towards Sirius)"},
        {"ism_leo_cold_cloud", "Leo Cold Cloud", "cold_neutral_dense", 225.0, 44.0, 60.0, {6.0, 2.0, 0.5},
         3000.0, 20.0, 1.0e-4, 0.1, 0.01, 0.3, "Peek et al. 2011; Meyer et al. 2006 (11-24 pc)"},
    };
    auto visual = [](const std::string& phase) {
        if (phase == "hot_ionized") return ojson {{"false_color_rgb", {0.55, 0.35, 0.95}}, {"alpha", 0.03}};
        if (phase == "warm_partially_ionized") return ojson {{"false_color_rgb", {0.25, 0.8, 0.8}}, {"alpha", 0.12}};
        if (phase == "warm_neutral") return ojson {{"false_color_rgb", {0.35, 0.55, 0.95}}, {"alpha", 0.12}};
        return ojson {{"false_color_rgb", {0.9, 0.95, 1.0}}, {"alpha", 0.25}};
    };
    auto add = [&](const std::string& id, const std::string& name, const std::string& phase, const glm::dvec3& c,
                   const glm::dvec3& radii, double n, double t, double ion, double mol, double dust, double ice,
                   const std::string& source, std::uint64_t seed, bool authoredRegion) {
        InterstellarObject o;
        o.id = id;
        o.name = name;
        o.objectType = "interstellar_medium_region";
        o.positionLy = c;
        o.description = "Interstellar medium region (" + phase + ").";
        o.tags = {"ism", phase};
        RandomStream rs(seed, "ism_shape");
        const double volumeLy3 = 4.0 / 3.0 * Pi * radii.x * radii.y * radii.z;
        const double massKg = n * CubicCmPerCubicLy * volumeLy3 * ProtonMassKg * 1.4;
        ojson p;
        p["geometry"] = {{"shape", "ellipsoid"}, {"radii_ly", {radii.x, radii.y, radii.z}},
                         {"orientation_quat_wxyz", {1.0, 0.0, 0.0, 0.0}}};
        p["phase"] = phase;
        p["density_profile"] = {{"mean_density_cm3", n}, {"profile", "gaussian_core"},
                                {"core_fraction", rs.uniform(0.3, 0.6)}, {"clumping", rs.uniform(0.1, 0.6)},
                                {"turbulence_kms", t > 1.0e5 ? 50.0 : t > 1000.0 ? rs.uniform(1.5, 3.0) : rs.uniform(0.3, 1.0)}};
        p["temperature_k"] = t;
        p["ionization_fraction"] = ion;
        p["mass_fraction"] = {{"H", 0.71}, {"He", 0.275}, {"metals", 0.015}};
        p["molecular_fraction"] = mol;
        p["dust_to_gas"] = dust;
        p["ice_mantle_fraction"] = ice;
        p["metallicity_feh"] = 0.0;
        ojson trace = ojson::object();
        if (t < 100.0)
            trace = {{"CO", 1.0e-5}, {"H2O_ice", 1.0e-6}, {"CO2_ice", 2.0e-7}, {"CH3OH_ice", 5.0e-8},
                     {"NH3", 1.0e-8}, {"silicate_dust", dust * 0.6}, {"carbonaceous_dust", dust * 0.4}};
        else if (t < 2.0e4)
            trace = {{"silicate_dust", dust * 0.6}, {"carbonaceous_dust", dust * 0.4}, {"C+", 1.0e-4}, {"Mg+", 2.0e-6},
                     {"Fe+", 1.0e-6}};
        else
            trace = {{"O VI", 2.0e-8}, {"C IV", 1.0e-8}};
        p["trace_species"] = trace;
        p["total_mass_kg"] = massKg;
        p["gameplay_gravity"] = false; // diffuse: negligible on ship scales
        // Resources follow from density, phase and ices (relative scoop yields).
        p["resource_profile"] = {{"hydrogen_scoop_relative", n / 0.25},
                                 {"volatile_ice_relative", ice * n / 3000.0},
                                 {"dust_relative", dust * n / 0.25 / 0.006}};
        p["visualization"] = visual(phase);
        p["provenance"] = authoredRegion ? ojson {{"source", source}, {"geometry", "approximate"}}
                                         : provenance(seed, v, "procedural warm cloudlet");
        o.properties = p;
        out.push_back(std::move(o));
    };
    for (const auto& r : authored)
        add(r.id, r.name, r.phase, galacticToIcrsLy(r.l, r.b, r.distanceLy), r.radiiLy, r.densityCm3, r.temperatureK,
            r.ionization, r.molecular, r.dustToGas, r.ice, r.source, deriveSeed(universeSeed, r.id), true);

    // Rare procedural warm cloudlets inside the bubble (Redfield & Linsky find
    // ~15 within 15 pc; beyond the authored ones we add a sparse population).
    const long long n = static_cast<long long>(std::ceil(s.radiusLy / s.cellSizeLy));
    int index = 0;
    for (long long cx = -n; cx < n; ++cx)
        for (long long cy = -n; cy < n; ++cy)
            for (long long cz = -n; cz < n; ++cz)
            {
                const std::uint64_t cellSeed = galacticCellSeed(universeSeed, cx, cy, cz);
                RandomStream rs(cellSeed, "ism_cloudlets");
                const int count = rs.poisson(0.03);
                for (int k = 0; k < count; ++k)
                {
                    const std::uint64_t seed = deriveSeed(cellSeed, "ism_cloudlet", static_cast<std::uint64_t>(k));
                    RandomStream cs(seed, "cloudlet");
                    const glm::dvec3 c = glm::dvec3(cx, cy, cz) * s.cellSizeLy +
                        glm::dvec3(cs.uniform(), cs.uniform(), cs.uniform()) * s.cellSizeLy;
                    if (glm::length(c) > s.radiusLy || glm::length(c) < 15.0)
                        continue;
                    const glm::dvec3 radii(cs.uniform(1.0, 5.0), cs.uniform(0.7, 3.0), cs.uniform(0.5, 2.0));
                    add("ism_cloudlet_" + cellTag(cx, cy, cz) + "_" + std::to_string(k),
                        "Cloudlet " + std::to_string(++index), "warm_partially_ionized", c, radii,
                        cs.uniform(0.05, 0.3), cs.uniform(4000.0, 9000.0), cs.uniform(0.2, 0.5), 0.0, 0.006, 0.0,
                        "", seed, false);
                }
            }
    return out;
}

} // namespace world::generation
