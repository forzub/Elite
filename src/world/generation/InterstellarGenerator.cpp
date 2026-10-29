#include "src/world/generation/InterstellarGenerator.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/DescriptorJson.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/InterstellarNoise.h"
#include "src/world/generation/StellarSystemGenerator.h"
#include "src/world/interstellar/ExtractionIndices.h"

namespace world::generation
{

namespace
{

using namespace astro;
using namespace interstellar_noise;
using ojson = nlohmann::ordered_json;
using world::coordinates::GridCell3;

constexpr double CubicCmPerCubicLy = LightYearMeters * LightYearMeters * LightYearMeters * 1.0e6;
constexpr double LyPerPc = 3.261563777;
constexpr double MeanParticleMassKg = 1.4 * ProtonMassKg; // H + He per H atom

ojson provenance(std::uint64_t seed, const UniverseGenerationVersions& v, const char* model)
{
    return ojson {{"generator", "world::generation::Interstellar"}, {"seed", hexSeed(seed)},
                  {"interstellar_version", v.interstellar}, {"model", model}};
}

// Five-character base-36 designation of a seed (display names only; ids
// carry the cell tag). Independent of every other cell.
std::string shortCode(std::uint64_t seed)
{
    static const char* digits = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
    std::string s;
    std::uint64_t x = splitMix64(seed);
    for (int i = 0; i < 5; ++i)
    {
        s += digits[x % 34];
        x /= 34;
    }
    return s;
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

ojson traceSpecies(double temperatureK, double dust)
{
    if (temperatureK < 100.0)
        return {{"CO", 1.0e-5}, {"H2O_ice", 1.0e-6}, {"CO2_ice", 2.0e-7}, {"CH3OH_ice", 5.0e-8},
                {"NH3", 1.0e-8}, {"silicate_dust", dust * 0.6}, {"carbonaceous_dust", dust * 0.4}};
    if (temperatureK < 2.0e4)
        return {{"silicate_dust", dust * 0.6}, {"carbonaceous_dust", dust * 0.4}, {"C+", 1.0e-4},
                {"Mg+", 2.0e-6}, {"Fe+", 1.0e-6}};
    return {{"O VI", 2.0e-8}, {"C IV", 1.0e-8}};
}

/*
    Extraction profile of a medium, derived only from its physical state.
    Mass densities per km^3 and what a reference collector (1 km^2 effective
    area at 100 km/s) gathers per day. Indices are log-scaled against a dense
    molecular clump (n ~ 1e3 cm^-3): the diffuse hot Local Bubble is
    effectively worthless as a fuel source; dense cold / molecular gas has a
    real volatile potential; supernova-enriched dust carries heavy elements.
*/
ojson extractionProfile(double densityCm3, double dustToGas, double iceMantle, double enrichment)
{
    const auto e = world::interstellar::extractionIndices(densityCm3, dustToGas, iceMantle, enrichment);
    const double rhoKgM3 = e.gasKgPerKm3 * 1.0e-9;
    return {{"gas_kg_per_km3", e.gasKgPerKm3},
            {"hydrogen_kg_per_km3", e.gasKgPerKm3 * 0.71},
            {"dust_kg_per_km3", e.dustKgPerKm3},
            {"ice_kg_per_km3", e.iceKgPerKm3},
            {"heavy_elements_kg_per_km3", e.heavyElementsKgPerKm3},
            {"collector_kg_per_day_1km2_100kms", rhoKgM3 * 1.0e6 * 1.0e5 * 86400.0},
            {"hydrogen_fuel_index", e.hydrogenFuel},
            {"volatile_index", e.volatiles},
            {"heavy_element_index", e.heavyElements}};
}

// Uniform random rotation (Shoemake), as a matrix and a wxyz quaternion.
glm::dmat3 randomRotation(RandomStream& rs, glm::dvec4& quatWxyz)
{
    const double u1 = rs.uniform(), u2 = rs.uniform() * 2.0 * Pi, u3 = rs.uniform() * 2.0 * Pi;
    const double a = std::sqrt(1.0 - u1), b = std::sqrt(u1);
    const double x = a * std::sin(u2), y = a * std::cos(u2), z = b * std::sin(u3), w = b * std::cos(u3);
    quatWxyz = {w, x, y, z};
    return glm::dmat3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y),
                      2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x),
                      2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
}

/*
    IrregularDensityVolume: relative density 0..1 on a coarse lattice. The
    bounding ellipsoid (radii, rotation) sets the scale; seeded fractal
    noise warps the boundary, carves holes and adds clumps.
*/
DensityLattice irregularVolume(std::uint64_t seed, const glm::dvec3& centre, const glm::dvec3& radii,
                               const glm::dmat3& rotation, double peakDensityCm3, int maxResolution = 20)
{
    DensityLattice L;
    const double rmax = std::max({radii.x, radii.y, radii.z});
    const double rmin = std::min({radii.x, radii.y, radii.z});
    const double extent = rmax * 1.35;
    const int n = std::clamp(static_cast<int>(std::ceil(2.0 * extent / (rmax / 7.0))), 8, maxResolution);
    L.voxelLy = 2.0 * extent / n;
    L.dims = {n, n, n};
    L.originLy = centre - glm::dvec3(extent);
    L.encoding = "relative";
    L.peakDensityCm3 = peakDensityCm3;
    L.values.resize(static_cast<std::size_t>(n) * n * n);
    const glm::dmat3 inverse = glm::transpose(rotation);
    const std::uint64_t warpSeed = deriveSeed(seed, "volume_warp");
    const std::uint64_t detailSeed = deriveSeed(seed, "volume_detail");
    for (int k = 0; k < n; ++k)
        for (int j = 0; j < n; ++j)
            for (int i = 0; i < n; ++i)
            {
                const glm::dvec3 x = L.originLy + (glm::dvec3(i, j, k) + 0.5) * L.voxelLy;
                const glm::dvec3 q = (inverse * (x - centre)) / radii;
                const double r2 = glm::dot(q, q);
                const double warp = fbm(warpSeed, x / std::max(rmin * 0.7, 0.5), 3);
                const double detail = fbm(detailSeed, x / std::max(rmin * 0.3, 0.3), 3);
                double v = 1.0 - r2 * (0.65 + 0.7 * warp);
                v = smoothstep(0.0, 0.4, v) * (0.3 + 0.7 * detail);
                if (detail < 0.22)
                    v *= 0.15; // holes
                L.values[static_cast<std::size_t>(i + n * (j + n * k))] =
                    static_cast<std::uint8_t>(std::lround(255.0 * std::clamp(v, 0.0, 1.0)));
            }
    return L;
}

struct RegionSpec
{
    std::string id;
    std::string name;
    char phase = 'p';
    glm::dvec3 centre {0.0};
    glm::dvec3 radiiLy {1.0};
    double peakDensityCm3 = 0.1;
    double temperatureK = 7000.0;
    double ionization = 0.3;
    double molecular = 0.0;
    double dustToGas = 0.006;
    double ice = 0.0;
    double enrichment = 0.0;
    std::string source;
    std::uint64_t seed = 0;
    bool authored = false;
    // observed | procedural_inferred | procedural_generated
    std::string provenanceClass = "procedural_generated";
    ojson formation;             // local physical state that formed it (procedural)
};

InterstellarObject regionObject(const RegionSpec& r, const UniverseGenerationVersions& v)
{
    RandomStream rs(r.seed, "ism_shape");
    glm::dvec4 quat;
    const glm::dmat3 rotation = randomRotation(rs, quat);
    // Lattice detail by phase: faint warm cloudlets are numerous and smooth,
    // dense molecular structures keep the finest lattice.
    const int resolution = r.authored ? 20 : r.phase == 'p' ? 10 : r.phase == 'c' ? 14 : 20;
    const DensityLattice L = irregularVolume(r.seed, r.centre, r.radiiLy, rotation, r.peakDensityCm3, resolution);
    double sum = 0.0;
    int filled = 0;
    for (const std::uint8_t b : L.values)
    {
        sum += b / 255.0;
        filled += b > 12;
    }
    const double voxelVolume = L.voxelLy * L.voxelLy * L.voxelLy;
    const double massKg = sum * voxelVolume * r.peakDensityCm3 * CubicCmPerCubicLy * MeanParticleMassKg;
    const double meanDensity = filled > 0 ? r.peakDensityCm3 * sum / filled : 0.0;

    InterstellarObject o;
    o.id = r.id;
    o.name = r.name;
    o.objectType = "interstellar_medium_region";
    o.positionLy = r.centre;
    o.description = std::string("Interstellar medium region (") + mediumPhaseName(r.phase) + ").";
    o.tags = {"ism", mediumPhaseName(r.phase)};
    ojson p;
    p["geometry"] = {{"shape", "density_lattice"},
                     {"bounding_ellipsoid", {{"radii_ly", {r.radiiLy.x, r.radiiLy.y, r.radiiLy.z}},
                                             {"orientation_quat_wxyz", {quat.x, quat.y, quat.z, quat.w}}}},
                     {"lattice", latticeJson(L)}};
    p["phase"] = mediumPhaseName(r.phase);
    p["density_profile"] = {{"peak_density_cm3", r.peakDensityCm3}, {"mean_density_cm3", meanDensity},
                            {"filled_voxel_fraction", static_cast<double>(filled) / L.values.size()},
                            {"turbulence_kms", r.temperatureK > 1000.0 ? rs.uniform(1.5, 3.0) : rs.uniform(0.3, 1.0)}};
    p["temperature_k"] = r.temperatureK;
    p["ionization_fraction"] = r.ionization;
    p["mass_fraction"] = {{"H", 0.71}, {"He", 0.275}, {"metals", 0.015 * (1.0 + r.enrichment)}};
    p["molecular_fraction"] = r.molecular;
    p["dust_to_gas"] = r.dustToGas;
    p["ice_mantle_fraction"] = r.ice;
    p["supernova_enrichment"] = r.enrichment;
    p["metallicity_feh"] = std::log10(1.0 + r.enrichment);
    p["trace_species"] = traceSpecies(r.temperatureK, r.dustToGas);
    p["total_mass_kg"] = massKg;
    p["extraction_profile"] = extractionProfile(meanDensity, r.dustToGas, r.ice, r.enrichment);
    p["gameplay_gravity"] = false; // diffuse: negligible on ship scales
    if (r.authored)
        p["provenance"] = {{"class", "observed"}, {"source", r.source}, {"geometry", "approximate"}};
    else
    {
        p["provenance"] = provenance(r.seed, v, "structure formed by the local medium state (CloudFormationPotential)");
        p["provenance"]["class"] = r.provenanceClass;
        if (r.provenanceClass == "procedural_inferred")
            p["provenance"]["inference"] =
                "uncatalogued member of the observed local cloud population (Redfield & Linsky 2008: "
                "~15 clouds within 15 pc); placed by the medium model";
        p["formation"] = r.formation;
    }
    o.properties = p;
    return o;
}


/*
    Icy debris field: a rare interstellar reservoir of SOLID bodies (not gas)
    - comets, icy fragments, dust and occasional larger frozen bodies - left
    by a stripped Oort cloud, a disrupted planetary system, a strong stellar
    encounter, ejected young-system debris or around a rogue system. On light-
    year scales it is very sparse; it is stored as a statistical population
    (number densities per size class, cumulative power law N(>D) ~ D^-q), and
    individual bodies are materialised deterministically only where a local
    simulation needs them (world::interstellar::materializeIcyBodies). At high
    interstellar speed even this sparse population is a hazard; that judgement
    belongs to the planner and the ship (InterstellarNavigationEnvironment).
*/
InterstellarObject icyDebrisField(std::uint64_t seed, const std::string& id, const glm::dvec3& centre,
                                  const std::string& origin, const UniverseGenerationVersions& v)
{
    RandomStream rs(seed, "icy_field");
    glm::dvec4 quat;
    (void)randomRotation(rs, quat);
    const bool oort = origin == "stripped_oort_cloud";
    const bool young = origin == "ejected_young_system_debris";
    const double a = rs.logUniform(0.1, oort ? 2.0 : 1.2);
    const glm::dvec3 radii(a, a * rs.uniform(0.4, 1.0), a * rs.uniform(0.15, 0.5));
    // Bodies above 1 km: a stripped Oort cloud keeps a few % of ~1e11-1e12
    // comets; system break-ups and encounters release fewer.
    const double bodiesAbove1Km = oort ? rs.logUniform(1.0e9, 3.0e10) : young ? rs.logUniform(1.0e8, 3.0e9)
                                                                             : rs.logUniform(1.0e7, 1.0e9);
    const double q = young ? rs.uniform(2.6, 3.0) : rs.uniform(2.3, 2.7); // collisional cascade ~2.5
    constexpr double AuPerLy = 63241.077;
    const double volumeAu3 = 4.0 / 3.0 * Pi * radii.x * radii.y * radii.z * AuPerLy * AuPerLy * AuPerLy;
    const double n1km = bodiesAbove1Km / volumeAu3;
    const auto above = [&](double diameterM) { return n1km * std::pow(diameterM / 1000.0, -q); };
    // Mass: a 1-km icy body ~ 5e11 kg (600 kg/m^3); for q < 3 the largest
    // bodies dominate (cut at 50 km).
    const double massKg = bodiesAbove1Km * 5.2e11 * (q / (3.0 - q)) * (std::pow(50.0, 3.0 - q) - 1.0);
    const double dustMassDensityKgM3 = // grains 1 um - 1 mm carry ~1 % of the solid mass, spread over the field
        0.01 * massKg / (volumeAu3 * std::pow(AuMeters, 3.0));
    const double water = oort ? rs.uniform(0.45, 0.6) : rs.uniform(0.3, 0.5);

    InterstellarObject o;
    o.id = id;
    o.name = (oort ? "Frost stream I-" : "Ice shoal I-") + shortCode(seed);
    o.objectType = "icy_debris_field";
    o.positionLy = centre;
    o.description = "Sparse field of interstellar comets, icy fragments and dust.";
    o.tags = {"debris", "icy", "solid_bodies", "hazard", "volatile_rich"};
    ojson p;
    p["origin"] = origin;
    p["geometry"] = {{"shape", "ellipsoid"},
                     {"radii_ly", {radii.x, radii.y, radii.z}},
                     {"orientation_quat_wxyz", {quat.x, quat.y, quat.z, quat.w}},
                     {"edge", "soft (number density ~ 1 - r^2)"}};
    p["solid_population"] = {
        {"cumulative_size_slope", q},
        {"bodies_above_1km", bodiesAbove1Km},
        {"number_density_per_au3",
         {{"above_1km", n1km}, {"above_10m", above(10.0)}, {"above_1cm", above(0.01)}, {"above_100um", above(1.0e-4)}}},
        {"largest_body_km", rs.uniform(10.0, 50.0)},
        {"dust_mass_density_kg_m3", dustMassDensityKgM3},
        {"materialisation", "deterministic per 1000-AU cube on demand (world::interstellar::materializeIcyBodies)"}};
    p["total_mass_kg"] = massKg;
    p["composition"] = {{"water_ice", water}, {"co_co2_ices", oort ? 0.2 : 0.12}, {"silicate_dust", 0.25},
                        {"organics", 0.08}};
    p["resource_profile"] = {{"water_ice_fraction", water},
                             {"volatile_index", oort ? rs.uniform(0.8, 1.0) : rs.uniform(0.6, 0.85)},
                             {"heavy_element_index", rs.uniform(0.1, 0.3)},
                             {"access", "dispersed: bodies must be found and reached individually"}};
    p["gameplay_gravity"] = false;
    p["provenance"] = provenance(seed, v, "icy debris field population");
    p["provenance"]["class"] = "procedural_generated";
    o.properties = p;
    return o;
}

} // namespace

std::vector<GridCell3> mediumStructureCells(const std::vector<GridCell3>& domainCells, double cell)
{
    std::set<GridCell3> all(domainCells.begin(), domainCells.end());
    const long long reach = static_cast<long long>(std::ceil(LocalMediumContextLy / cell));
    for (long long z = -reach; z < reach; ++z)
        for (long long y = -reach; y < reach; ++y)
            for (long long x = -reach; x < reach; ++x)
            {
                const glm::dvec3 nearest = glm::clamp(glm::dvec3(0.0), glm::dvec3(x, y, z) * cell,
                                                      glm::dvec3(x + 1, y + 1, z + 1) * cell);
                if (glm::length(nearest) <= LocalMediumContextLy)
                    all.insert({x, y, z});
            }
    return {all.begin(), all.end()};
}

std::string interstellarCellTag(const GridCell3& c)
{
    std::ostringstream s;
    s << (c[0] < 0 ? "m" : "p") << std::llabs(c[0]) << (c[1] < 0 ? "m" : "p") << std::llabs(c[1])
      << (c[2] < 0 ? "m" : "p") << std::llabs(c[2]);
    return s.str();
}

InterstellarCell generateInterstellarCell(
    std::uint64_t universeSeed,
    const GridCell3& cell,
    const std::vector<glm::dvec3>& systems,
    const InterstellarSettings& s,
    const UniverseGenerationVersions& v)
{
    InterstellarCell result;
    result.cell = cell;
    const long long cx = cell[0], cy = cell[1], cz = cell[2];
    const std::uint64_t cellSeed = galacticCellSeed(universeSeed, cx, cy, cz);
    result.seed = cellSeed;
    const glm::dvec3 origin(cx * s.cellSizeLy, cy * s.cellSizeLy, cz * s.cellSizeLy);
    const std::string tag = interstellarCellTag(cell);

    // Background medium lattice of the cell (5-ly voxels, log10 density).
    {
        DensityLattice L;
        const int n = std::max(1, s.backgroundLatticeResolution);
        L.voxelLy = s.cellSizeLy / n;
        L.dims = {n, n, n};
        L.originLy = origin;
        L.encoding = "log10";
        L.values.resize(static_cast<std::size_t>(n) * n * n);
        L.phases.resize(L.values.size());
        double sumN = 0.0, sumNT = 0.0, sumIon = 0.0, sumDust = 0.0, sumMol = 0.0, sumIce = 0.0, sumEnr = 0.0;
        int phaseCount[128] = {};
        for (int k = 0; k < n; ++k)
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i)
                {
                    const glm::dvec3 x = origin + (glm::dvec3(i, j, k) + 0.5) * L.voxelLy;
                    const MediumSample m = backgroundMediumAt(universeSeed, x);
                    const std::size_t idx = static_cast<std::size_t>(i + n * (j + n * k));
                    const double u = (std::log10(std::max(m.densityCm3, 1.0e-12)) - L.log10Min) /
                                     (L.log10Max - L.log10Min);
                    L.values[idx] = static_cast<std::uint8_t>(std::lround(255.0 * std::clamp(u, 0.0, 1.0)));
                    L.phases[idx] = m.phase;
                    ++phaseCount[static_cast<unsigned char>(m.phase)];
                    sumN += m.densityCm3;
                    sumNT += m.densityCm3 * m.temperatureK;
                    sumIon += m.densityCm3 * m.ionizationFraction;
                    sumDust += m.densityCm3 * m.dustToGas;
                    sumMol += m.densityCm3 * m.molecularFraction;
                    sumIce += m.densityCm3 * m.iceMantleFraction;
                    sumEnr += m.densityCm3 * m.supernovaEnrichment;
                }
        char dominant = 'h';
        for (const char c : std::string("hpwcms"))
            if (phaseCount[static_cast<unsigned char>(c)] > phaseCount[static_cast<unsigned char>(dominant)])
                dominant = c;
        const double count = static_cast<double>(L.values.size());
        const double meanN = sumN / count;
        const double cellVolumeLy3 = s.cellSizeLy * s.cellSizeLy * s.cellSizeLy;

        InterstellarObject bg;
        bg.id = "ism_bg_" + tag;
        bg.name = "Interstellar medium " + tag;
        bg.objectType = "interstellar_medium_background";
        bg.positionLy = origin + 0.5 * s.cellSizeLy;
        bg.description = std::string("Background interstellar medium (") + mediumPhaseName(dominant) + ").";
        bg.tags = {"ism", "background", mediumPhaseName(dominant)};
        ojson p;
        p["cell"] = {cx, cy, cz};
        p["cell_size_ly"] = s.cellSizeLy;
        p["dominant_phase"] = mediumPhaseName(dominant);
        p["mean_density_cm3"] = meanN;
        p["temperature_k"] = sumN > 0.0 ? sumNT / sumN : 0.0;
        p["ionization_fraction"] = sumN > 0.0 ? sumIon / sumN : 0.0;
        p["molecular_fraction"] = sumN > 0.0 ? sumMol / sumN : 0.0;
        p["dust_to_gas"] = sumN > 0.0 ? sumDust / sumN : 0.0;
        p["ice_mantle_fraction"] = sumN > 0.0 ? sumIce / sumN : 0.0;
        p["supernova_enrichment"] = sumN > 0.0 ? sumEnr / sumN : 0.0;
        p["mass_fraction"] = {{"H", 0.71}, {"He", 0.275}, {"metals", 0.015}};
        p["total_mass_kg"] = meanN * cellVolumeLy3 * CubicCmPerCubicLy * MeanParticleMassKg;
        p["lattice"] = latticeJson(L);
        p["extraction_profile"] = extractionProfile(
            meanN, sumN > 0.0 ? sumDust / sumN : 0.0, sumN > 0.0 ? sumIce / sumN : 0.0,
            sumN > 0.0 ? sumEnr / sumN : 0.0);
        p["gameplay_gravity"] = false;
        p["provenance"] = provenance(cellSeed, v, "background medium field (Local Bubble cavity / wall / warm neutral)");
        bg.properties = p;
        result.objects.push_back(std::move(bg));
    }

    // Compact bodies: notable members of the cell population.
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
            if (dSol < 2.0)
                continue;
            bool nearSystem = false;
            for (const auto& sp : systems)
                nearSystem = nearSystem || glm::length(p - sp) < s.minDistanceToSystemLy;
            if (nearSystem)
                continue;
            const std::string code = shortCode(seed);
            auto& out = result.objects;
            const std::string type = rate.type;
            if (type == "brown_dwarf" && dSol < s.surveyCompleteBrownDwarfLy)
                continue; // WISE would have found it
            InterstellarObject o;
            o.id = "ism_obj_" + tag + "_" + type + "_" + std::to_string(k);
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
                name << "Nomad " << (type == "binary_rogue_planet" ? "binary " : "") << "J-" << code;
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
                name << "Nomad T-" << code;
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
                name << "Drifter M-" << code;
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
                name << "Ember " << props["spectral_type"].get<std::string>() << "-" << code;
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
                name << (ns ? "Pulsar remnant R-" : "Cinder W-") << code;
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
                name << "Wanderer C-" << code;
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
                name << "Shoal D-" << code;
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

    // Icy debris fields (rare). Causes in this cell: debris around its rogue
    // systems, stripped Oort clouds trailing its catalog stars (never Sol's:
    // ours is observed intact), and free fields from break-ups, encounters or
    // young-system ejecta.
    {
        std::vector<std::pair<glm::dvec3, std::string>> causes;
        const std::size_t compact = result.objects.size();
        for (std::size_t i = 1; i < compact; ++i)
        {
            const auto& o = result.objects[i];
            if (o.objectType != "rogue_planet" && o.objectType != "binary_rogue_planet" &&
                o.objectType != "brown_dwarf")
                continue;
            RandomStream as(deriveSeed(cellSeed, "icy_rogue_" + o.id), "cause");
            if (as.chance(0.04))
                causes.push_back({o.positionLy + glm::dvec3(as.normal(0.0, 0.3), as.normal(0.0, 0.3), as.normal(0.0, 0.3)),
                                  "rogue_system_debris"});
        }
        for (std::size_t i = 0; i < systems.size(); ++i)
        {
            const glm::dvec3& sp = systems[i];
            if (glm::length(sp) < 1.0e-6 || glm::any(glm::lessThan(sp, origin)) ||
                glm::any(glm::greaterThanEqual(sp, origin + glm::dvec3(s.cellSizeLy))))
                continue;
            RandomStream ss(deriveSeed(cellSeed, "icy_oort", static_cast<std::uint64_t>(i)), "cause");
            if (!ss.chance(0.06))
                continue;
            const glm::dvec3 dir = glm::normalize(glm::dvec3(ss.normal(0.0, 1.0), ss.normal(0.0, 1.0), ss.normal(0.0, 1.0)));
            causes.push_back({sp + dir * ss.uniform(0.8, 3.0), "stripped_oort_cloud"});
        }
        RandomStream fs(cellSeed, "interstellar_icy_debris_field");
        const int free = fs.poisson(0.006);
        static const char* freeOrigins[] = {"disrupted_planetary_system", "stellar_encounter_debris",
                                            "ejected_young_system_debris"};
        for (int k = 0; k < free; ++k)
            causes.push_back({origin + glm::dvec3(fs.uniform(), fs.uniform(), fs.uniform()) * s.cellSizeLy,
                              freeOrigins[std::min(2, static_cast<int>(fs.uniform() * 3.0))]});
        for (std::size_t k = 0; k < causes.size(); ++k)
        {
            if (glm::length(causes[k].first) < 2.0)
                continue;
            const std::uint64_t seed = deriveSeed(cellSeed, "icy_debris_field", static_cast<std::uint64_t>(k));
            result.objects.push_back(icyDebrisField(
                seed, "ism_icy_" + tag + "_" + std::to_string(k), causes[k].first, causes[k].second, v));
        }
    }
    return result;
}

std::vector<InterstellarObject> generateInterstellarMedium(
    std::uint64_t universeSeed,
    const std::vector<GridCell3>& cells,
    const InterstellarSettings& s,
    const UniverseGenerationVersions& v)
{
    std::vector<InterstellarObject> out;
    struct Authored
    {
        const char* id;
        const char* name;
        char phase;
        double l, b, distanceLy;
        glm::dvec3 radiiLy;
        double densityCm3, temperatureK, ionization, molecular, dustToGas, ice;
        const char* source;
    };
    // Authored local clouds (approximate geometry from the cited work). The
    // Local Bubble itself is the background field (backgroundMediumAt).
    const Authored authored[] = {
        {"ism_local_interstellar_cloud", "Local Interstellar Cloud", 'p', 170.0, -10.0, 5.0,
         {9.0, 6.0, 5.0}, 0.25, 7500.0, 0.3, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (LIC; Sol at its edge)"},
        {"ism_g_cloud", "G Cloud", 'p', 315.0, 0.0, 4.0, {5.0, 4.0, 3.0},
         0.2, 5500.0, 0.3, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (towards alpha Centauri)"},
        {"ism_blue_cloud", "Blue Cloud", 'w', 230.0, -15.0, 6.0, {3.0, 2.0, 2.0},
         0.15, 3000.0, 0.1, 0.0, 0.006, 0.0, "Redfield & Linsky 2008 (towards Sirius)"},
        {"ism_leo_cold_cloud", "Leo Cold Cloud", 'c', 225.0, 44.0, 60.0, {6.0, 2.0, 0.8},
         3000.0, 20.0, 1.0e-4, 0.1, 0.01, 0.3, "Peek et al. 2011; Meyer et al. 2006 (11-24 pc)"},
    };
    for (const auto& a : authored)
    {
        RegionSpec r;
        r.id = a.id;
        r.name = a.name;
        r.phase = a.phase;
        r.centre = galacticToIcrsLy(a.l, a.b, a.distanceLy);
        r.radiiLy = a.radiiLy;
        r.peakDensityCm3 = a.densityCm3;
        r.temperatureK = a.temperatureK;
        r.ionization = a.ionization;
        r.molecular = a.molecular;
        r.dustToGas = a.dustToGas;
        r.ice = a.ice;
        r.source = a.source;
        r.seed = deriveSeed(universeSeed, a.id);
        r.authored = true;
        out.push_back(regionObject(r, v));
    }

    /*
        Procedural structures per generation cell, CAUSED by the medium: the
        cell's 5-ly lattice of physical states gives a CloudFormationPotential
        per voxel; the expected number of structures of a kind is its rate x
        the potential integrated over the cell volume; each structure sits in
        a voxel drawn by potential and takes its density / temperature from
        the local pressure and state. The observed clouds are members of this
        population, not exceptions: near Sol the same model produces the
        uncatalogued rest of the local cloud complex (procedural_inferred,
        never overlapping an observed cloud); elsewhere procedural_generated.
    */
    struct Kind
    {
        const char* role;
        const char* idPrefix;
        const char* namePrefix;
        char phase;
        double perLy3; // structures per ly^3 at potential 1
    };
    static constexpr Kind Kinds[] = {
        {"ism_warm_cloudlets", "ism_cloudlet_", "Cloudlet ", 'p', WarmCloudletsPerLy3},
        {"ism_cold_clouds", "ism_cold_cloud_", "Cold cloud ", 'c', ColdCloudsPerLy3},
        {"ism_molecular_clumps", "ism_molecular_clump_", "Molecular clump ", 'm', MolecularClumpsPerLy3},
    };
    std::vector<std::pair<glm::dvec3, double>> observedExtents;
    for (const auto& o : out)
    {
        const auto& r = o.properties["geometry"]["bounding_ellipsoid"]["radii_ly"];
        observedExtents.push_back({o.positionLy, std::max({r[0].get<double>(), r[1].get<double>(), r[2].get<double>()})});
    }
    const int n = std::max(1, s.backgroundLatticeResolution);
    const double voxel = s.cellSizeLy / n;
    for (const auto& cell : cells)
    {
        const std::uint64_t cellSeed = galacticCellSeed(universeSeed, cell[0], cell[1], cell[2]);
        const glm::dvec3 origin = glm::dvec3(cell[0], cell[1], cell[2]) * s.cellSizeLy;
        const std::string tag = interstellarCellTag(cell);
        std::vector<CloudFormationPotential> potential(static_cast<std::size_t>(n) * n * n);
        for (int k = 0; k < n; ++k)
            for (int j = 0; j < n; ++j)
                for (int i = 0; i < n; ++i)
                    potential[static_cast<std::size_t>(i + n * (j + n * k))] = cloudFormationPotential(
                        backgroundMediumAt(universeSeed, origin + (glm::dvec3(i, j, k) + 0.5) * voxel));
        for (const auto& kind : Kinds)
        {
            const auto weight = [&](const CloudFormationPotential& p) {
                return kind.phase == 'p' ? p.warm : kind.phase == 'c' ? p.cold : p.molecular;
            };
            double total = 0.0;
            for (const auto& p : potential)
                total += weight(p);
            if (total <= 0.0)
                continue;
            RandomStream rs(cellSeed, kind.role);
            const int count = rs.poisson(kind.perLy3 * total * voxel * voxel * voxel);
            for (int k = 0; k < count; ++k)
            {
                const std::uint64_t seed = deriveSeed(cellSeed, kind.role, static_cast<std::uint64_t>(k));
                RandomStream cs(seed, "structure");
                double pick = cs.uniform() * total;
                std::size_t chosen = 0;
                for (; chosen + 1 < potential.size(); ++chosen)
                {
                    pick -= weight(potential[chosen]);
                    if (pick <= 0.0)
                        break;
                }
                const int ci = static_cast<int>(chosen % n), cj = static_cast<int>((chosen / n) % n),
                          ck = static_cast<int>(chosen / (static_cast<std::size_t>(n) * n));
                RegionSpec r;
                r.centre = origin + (glm::dvec3(ci, cj, ck) + glm::dvec3(cs.uniform(), cs.uniform(), cs.uniform())) * voxel;
                const MediumSample here = backgroundMediumAt(universeSeed, r.centre);
                const CloudFormationPotential hereP = cloudFormationPotential(here);
                r.id = std::string(kind.idPrefix) + tag + "_" + std::to_string(k);
                r.name = std::string(kind.namePrefix) + shortCode(seed);
                r.phase = kind.phase;
                r.seed = seed;
                r.enrichment = here.supernovaEnrichment;
                if (kind.phase == 'p')
                {
                    r.radiiLy = {cs.uniform(1.0, 5.0), cs.uniform(0.7, 3.0), cs.uniform(0.5, 2.0)};
                    r.peakDensityCm3 = std::clamp(here.densityCm3 * cs.uniform(1.5, 3.0), 0.05, 0.5);
                    r.temperatureK = here.phase == 'p' ? here.temperatureK : cs.uniform(5000.0, 9000.0);
                    r.ionization = std::clamp(here.ionizationFraction, 0.2, 0.5);
                }
                else if (kind.phase == 'c')
                {
                    r.radiiLy = {cs.uniform(2.0, 8.0), cs.uniform(1.5, 5.0), cs.uniform(1.0, 3.0)};
                    r.temperatureK = cs.uniform(30.0, 120.0);
                    // Pressure equilibrium with the surrounding medium.
                    r.peakDensityCm3 =
                        std::clamp(here.pressureKcm3 / r.temperatureK * cs.uniform(0.6, 1.4), 10.0, 1000.0);
                    r.ionization = 1.0e-3;
                    r.molecular = std::clamp(here.molecularFraction + cs.uniform(0.0, 0.1), 0.01, 0.3);
                    r.dustToGas = 0.01;
                    r.ice = cs.uniform(0.05, 0.3);
                }
                else
                {
                    r.radiiLy = {cs.uniform(1.0, 4.0), cs.uniform(0.8, 3.0), cs.uniform(0.6, 2.0)};
                    r.temperatureK = cs.uniform(10.0, 20.0);
                    r.peakDensityCm3 =
                        std::clamp(here.pressureKcm3 / r.temperatureK * cs.uniform(1.0, 3.0), 300.0, 5000.0);
                    r.ionization = 1.0e-5;
                    r.molecular = cs.uniform(0.5, 0.9);
                    r.dustToGas = 0.012;
                    r.ice = cs.uniform(0.3, 0.6);
                }
                bool overlapsObserved = false;
                const double reach = std::max({r.radiiLy.x, r.radiiLy.y, r.radiiLy.z});
                for (const auto& [centre, radius] : observedExtents)
                    overlapsObserved = overlapsObserved || glm::length(r.centre - centre) < 1.2 * (radius + reach);
                if (overlapsObserved)
                    continue; // the observed cloud is this member of the population
                r.provenanceClass = glm::length(r.centre) < ObservedCloudCompletenessLy ? "procedural_inferred"
                                                                                           : "procedural_generated";
                const char* environment = here.wallCount >= 2   ? "wall_intersection"
                                          : here.wallCount == 1 ? "compressed_wall"
                                          : here.immersedWallCount >= 2 ? "immersed_wall_interaction"
                                          : here.immersedWall   ? "wall_immersed_in_cavity"
                                          : here.cavityCount > 0 ? "cavity_interior"
                                                                 : "warm_neutral_medium";
                r.formation = {{"environment", environment},
                               {"potential", {{"warm", hereP.warm}, {"cold", hereP.cold}, {"molecular", hereP.molecular}}},
                               {"local_density_cm3", here.densityCm3},
                               {"local_temperature_k", here.temperatureK},
                               {"local_pressure_k_cm3", here.pressureKcm3},
                               {"local_turbulence_kms", here.turbulenceKms},
                               {"local_shielding_av", here.shieldingAv}};
                out.push_back(regionObject(r, v));
            }
        }
    }

    // The cavities / shells of the substrate (observed + procedural) that
    // reach the domain, as descriptive objects (not drawn as bodies).
    if (!cells.empty())
    {
        glm::dvec3 lo(1.0e30), hi(-1.0e30);
        for (const auto& cell : cells)
        {
            const glm::dvec3 o = glm::dvec3(cell[0], cell[1], cell[2]) * s.cellSizeLy;
            lo = glm::min(lo, o);
            hi = glm::max(hi, o + glm::dvec3(s.cellSizeLy));
        }
        for (const auto& c : cavitiesInBox(universeSeed, lo, hi))
        {
            InterstellarObject o;
            o.id = c.id;
            o.name = c.name;
            o.objectType = "interstellar_cavity";
            o.positionLy = c.centreLy;
            o.description = "Hot supernova-blown cavity bounded by a swept-up shell.";
            o.tags = {"ism", "cavity", "hot_ionized"};
            ojson p;
            p["radius_ly"] = c.radiusLy;
            p["wall_thickness_ly"] = c.wallLy;
            p["age_myr"] = c.ageMyr;
            p["supernovae"] = c.supernovae;
            p["wall_compression"] = c.wallCompression;
            p["shape"] = c.localBubble ? "radius per galactic direction (observed table), polar chimneys"
                                       : "near-spherical, elongated along the disc normal, seeded roughness";
            p["gameplay_gravity"] = false;
            p["provenance"] = c.provenance == "observed"
                ? ojson {{"class", "observed"}, {"source", c.source}, {"geometry", "approximate"}}
                : provenance(c.seed, v, "superbubble cell population");
            if (c.provenance != "observed")
                p["provenance"]["class"] = c.provenance;
            o.properties = p;
            out.push_back(std::move(o));
        }
    }
    return out;
}

} // namespace world::generation
