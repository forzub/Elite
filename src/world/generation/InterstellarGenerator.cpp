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

// --- deterministic value noise -------------------------------------------

double latticeHash01(std::uint64_t seed, long long x, long long y, long long z)
{
    const std::uint64_t h = splitMix64(
        seed ^ splitMix64(static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull ^
                          splitMix64(static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full ^
                                     splitMix64(static_cast<std::uint64_t>(z)))));
    return static_cast<double>(h >> 11) * 0x1.0p-53;
}

double valueNoise(std::uint64_t seed, const glm::dvec3& p)
{
    const glm::dvec3 f = glm::floor(p);
    const glm::dvec3 t = p - f;
    const glm::dvec3 w = t * t * (3.0 - 2.0 * t);
    const long long x0 = static_cast<long long>(f.x), y0 = static_cast<long long>(f.y),
                    z0 = static_cast<long long>(f.z);
    double c[2][2][2];
    for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
            for (int k = 0; k < 2; ++k)
                c[i][j][k] = latticeHash01(seed, x0 + i, y0 + j, z0 + k);
    const auto lerp = [](double a, double b, double s) { return a + (b - a) * s; };
    const double x00 = lerp(c[0][0][0], c[1][0][0], w.x), x10 = lerp(c[0][1][0], c[1][1][0], w.x);
    const double x01 = lerp(c[0][0][1], c[1][0][1], w.x), x11 = lerp(c[0][1][1], c[1][1][1], w.x);
    return lerp(lerp(x00, x10, w.y), lerp(x01, x11, w.y), w.z);
}

// Fractal value noise in [0, 1].
double fbm(std::uint64_t seed, const glm::dvec3& p, int octaves)
{
    double sum = 0.0, norm = 0.0, amp = 1.0, freq = 1.0;
    for (int o = 0; o < octaves; ++o)
    {
        sum += amp * valueNoise(seed + static_cast<std::uint64_t>(o) * 0x51ED27ull, p * freq);
        norm += amp;
        amp *= 0.5;
        freq *= 2.03;
    }
    return sum / norm;
}

double smoothstep(double e0, double e1, double x)
{
    const double t = std::clamp((x - e0) / (e1 - e0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// --- Local Bubble cavity --------------------------------------------------

// Cavity radius in the galactic plane by longitude (pc), after the 3D dust
// maps of Lallement et al. 2014 and Pelgrims et al. 2020 (approximate):
// closest toward the Galactic centre / Sco-Cen side, the beta CMa tunnel at
// l ~ 240.
double bubbleRadiusPc(double lDeg)
{
    static const double table[][2] = {{0.0, 60.0},    {30.0, 70.0},   {60.0, 85.0},   {90.0, 90.0},
                                      {120.0, 95.0},  {150.0, 95.0},  {180.0, 100.0}, {210.0, 130.0},
                                      {240.0, 180.0}, {270.0, 120.0}, {300.0, 80.0},  {330.0, 65.0},
                                      {360.0, 60.0}};
    double l = std::fmod(lDeg, 360.0);
    if (l < 0.0)
        l += 360.0;
    for (int i = 0; i < 12; ++i)
        if (l <= table[i + 1][0])
        {
            const double s = (l - table[i][0]) / (table[i + 1][0] - table[i][0]);
            return table[i][1] + (table[i + 1][1] - table[i][1]) * s;
        }
    return table[0][1];
}

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
    const double rhoKgM3 = densityCm3 * 1.0e6 * MeanParticleMassKg;
    const double perKm3 = rhoKgM3 * 1.0e9;
    const double collectorKgDay = rhoKgM3 * 1.0e6 * 1.0e5 * 86400.0;
    const auto index = [](double kgKm3, double referenceKgKm3) {
        return std::clamp((std::log10(std::max(kgKm3, 1.0e-30)) - std::log10(referenceKgKm3) + 6.0) / 6.0, 0.0, 1.0);
    };
    const double reference = 1.0e3 * 1.0e6 * MeanParticleMassKg * 1.0e9; // molecular clump, kg/km^3
    const double dust = perKm3 * dustToGas;
    const double ices = dust * iceMantle;
    return {{"gas_kg_per_km3", perKm3},
            {"hydrogen_kg_per_km3", perKm3 * 0.71},
            {"dust_kg_per_km3", dust},
            {"ice_kg_per_km3", ices},
            {"heavy_elements_kg_per_km3", dust * (1.0 + 2.0 * enrichment)},
            {"collector_kg_per_day_1km2_100kms", collectorKgDay},
            {"hydrogen_fuel_index", index(perKm3 * 0.71, reference * 0.71)},
            {"volatile_index", index(ices, reference * 0.012 * 0.5)},
            {"heavy_element_index", index(dust * (1.0 + 2.0 * enrichment), reference * 0.012 * 3.0)}};
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
                               const glm::dmat3& rotation, double peakDensityCm3)
{
    DensityLattice L;
    const double rmax = std::max({radii.x, radii.y, radii.z});
    const double rmin = std::min({radii.x, radii.y, radii.z});
    const double extent = rmax * 1.35;
    const int n = std::clamp(static_cast<int>(std::ceil(2.0 * extent / (rmax / 7.0))), 8, 20);
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
};

InterstellarObject regionObject(const RegionSpec& r, const UniverseGenerationVersions& v)
{
    RandomStream rs(r.seed, "ism_shape");
    glm::dvec4 quat;
    const glm::dmat3 rotation = randomRotation(rs, quat);
    const DensityLattice L = irregularVolume(r.seed, r.centre, r.radiiLy, rotation, r.peakDensityCm3);
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
    p["provenance"] = r.authored ? ojson {{"source", r.source}, {"geometry", "approximate"}}
                                 : provenance(r.seed, v, "procedural medium structure");
    o.properties = p;
    return o;
}

} // namespace

std::string interstellarCellTag(const GridCell3& c)
{
    std::ostringstream s;
    s << (c[0] < 0 ? "m" : "p") << std::llabs(c[0]) << (c[1] < 0 ? "m" : "p") << std::llabs(c[1])
      << (c[2] < 0 ? "m" : "p") << std::llabs(c[2]);
    return s.str();
}

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

void icrsToGalactic(const glm::dvec3& p, double& lDeg, double& bDeg, double& d)
{
    const double r[3][3] = {
        {-0.0548755604, -0.8734370902, -0.4838350155},
        {0.4941094279, -0.4448296300, 0.7469822445},
        {-0.8676661490, -0.1980763734, 0.4559837762}};
    const glm::dvec3 g(r[0][0] * p.x + r[0][1] * p.y + r[0][2] * p.z,
                       r[1][0] * p.x + r[1][1] * p.y + r[1][2] * p.z,
                       r[2][0] * p.x + r[2][1] * p.y + r[2][2] * p.z);
    d = glm::length(g);
    lDeg = std::atan2(g.y, g.x) / DegToRad;
    if (lDeg < 0.0)
        lDeg += 360.0;
    bDeg = d > 0.0 ? std::asin(std::clamp(g.z / d, -1.0, 1.0)) / DegToRad : 0.0;
}

MediumSample backgroundMediumAt(std::uint64_t universeSeed, const glm::dvec3& p)
{
    const double d = glm::length(p);
    double l = 0.0, b = 0.0, dist = 0.0;
    if (d > 1.0e-9)
        icrsToGalactic(p, l, b, dist);
    const glm::dvec3 dir = d > 1.0e-9 ? p / d : glm::dvec3(1.0, 0.0, 0.0);
    const double sb = std::sin(b * DegToRad), cb = std::cos(b * DegToRad);
    const double chimney = 1.0 / std::sqrt(cb * cb + (sb / 2.2) * (sb / 2.2)); // open toward the poles
    const double rough = 1.0 + 0.15 * (2.0 * fbm(deriveSeed(universeSeed, "ism_bubble_wall"), dir * 2.5, 3) - 1.0);
    const double R = bubbleRadiusPc(l) * LyPerPc * chimney * rough;
    const double W = 40.0 * (0.7 + 0.6 * fbm(deriveSeed(universeSeed, "ism_wall_thickness"), dir * 4.0, 2));
    const double local = fbm(deriveSeed(universeSeed, "ism_fine_structure"), p / 12.0, 3);

    MediumSample m;
    if (d < R)
    {
        // Hot, tenuous cavity gas (soft X-ray bubble), dust largely destroyed.
        m.phase = 'h';
        m.densityCm3 = 0.004 * (0.6 + 0.8 * local);
        m.temperatureK = 1.0e6;
        m.ionizationFraction = 1.0;
        m.dustToGas = 0.0005;
        m.supernovaEnrichment = 0.2;
    }
    else if (d < R + W)
    {
        // Swept-up wall: neutral, dusty, carrying the ejecta of the
        // supernovae that blew the bubble (60Fe, Wallner et al. 2016).
        const double x = (d - R) / W;
        const double profile = std::pow(std::sin(Pi * x), 0.7);
        m.densityCm3 = 0.5 + 15.0 * profile * (0.4 + 1.2 * local);
        m.temperatureK = 8000.0 * (1.0 - profile) + 80.0 * profile;
        m.phase = profile > 0.6 && local > 0.5 ? 'c' : 's';
        m.ionizationFraction = 0.05;
        m.molecularFraction = 0.1 * profile;
        m.dustToGas = 0.01;
        m.iceMantleFraction = 0.2 * profile;
        m.supernovaEnrichment = 1.0;
    }
    else
    {
        m.phase = 'w';
        m.densityCm3 = 0.3 * (0.5 + local);
        m.temperatureK = 8000.0;
        m.ionizationFraction = 0.1;
        m.dustToGas = 0.006;
    }
    return m;
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

    // Procedural structures, seeded per generation cell and by the
    // background phase at the cell centre.
    for (const auto& cell : cells)
    {
        const std::uint64_t cellSeed = galacticCellSeed(universeSeed, cell[0], cell[1], cell[2]);
        const glm::dvec3 origin = glm::dvec3(cell[0], cell[1], cell[2]) * s.cellSizeLy;
        const char phase = backgroundMediumAt(universeSeed, origin + 0.5 * s.cellSizeLy).phase;
        const std::string tag = interstellarCellTag(cell);
        struct Kind
        {
            const char* role;
            const char* idPrefix;
            const char* namePrefix;
            char phase;
            double rate;
        };
        std::vector<Kind> kinds;
        if (phase == 'h')
            kinds.push_back({"ism_cloudlets", "ism_cloudlet_", "Cloudlet ", 'p', 0.03});
        else if (phase == 's' || phase == 'c')
        {
            kinds.push_back({"ism_cold_clouds", "ism_cold_cloud_", "Cold cloud ", 'c', 0.25});
            kinds.push_back({"ism_molecular_clumps", "ism_molecular_clump_", "Molecular clump ", 'm', 0.05});
        }
        else
            kinds.push_back({"ism_cold_clouds", "ism_cold_cloud_", "Cold cloud ", 'c', 0.08});
        for (const auto& kind : kinds)
        {
            RandomStream rs(cellSeed, kind.role);
            const int count = rs.poisson(kind.rate);
            for (int k = 0; k < count; ++k)
            {
                const std::uint64_t seed = deriveSeed(cellSeed, kind.role, static_cast<std::uint64_t>(k));
                RandomStream cs(seed, "structure");
                RegionSpec r;
                r.centre = origin + glm::dvec3(cs.uniform(), cs.uniform(), cs.uniform()) * s.cellSizeLy;
                if (kind.phase == 'p' && glm::length(r.centre) < 15.0)
                    continue; // the authored clouds own the immediate solar neighbourhood
                r.id = std::string(kind.idPrefix) + tag + "_" + std::to_string(k);
                r.name = std::string(kind.namePrefix) + shortCode(seed);
                r.phase = kind.phase;
                r.seed = seed;
                if (kind.phase == 'p')
                {
                    r.radiiLy = {cs.uniform(1.0, 5.0), cs.uniform(0.7, 3.0), cs.uniform(0.5, 2.0)};
                    r.peakDensityCm3 = cs.uniform(0.05, 0.3);
                    r.temperatureK = cs.uniform(4000.0, 9000.0);
                    r.ionization = cs.uniform(0.2, 0.5);
                }
                else if (kind.phase == 'c')
                {
                    r.radiiLy = {cs.uniform(2.0, 8.0), cs.uniform(1.5, 5.0), cs.uniform(1.0, 3.0)};
                    r.peakDensityCm3 = cs.logUniform(10.0, 300.0);
                    r.temperatureK = cs.uniform(30.0, 120.0);
                    r.ionization = 1.0e-3;
                    r.molecular = cs.uniform(0.01, 0.2);
                    r.dustToGas = 0.01;
                    r.ice = cs.uniform(0.05, 0.3);
                    r.enrichment = phase == 'w' ? 0.0 : 1.0;
                }
                else
                {
                    r.radiiLy = {cs.uniform(1.0, 4.0), cs.uniform(0.8, 3.0), cs.uniform(0.6, 2.0)};
                    r.peakDensityCm3 = cs.logUniform(300.0, 3000.0);
                    r.temperatureK = cs.uniform(10.0, 20.0);
                    r.ionization = 1.0e-5;
                    r.molecular = cs.uniform(0.5, 0.9);
                    r.dustToGas = 0.012;
                    r.ice = cs.uniform(0.3, 0.6);
                    r.enrichment = 1.0;
                }
                out.push_back(regionObject(r, v));
            }
        }
    }
    return out;
}

} // namespace world::generation
