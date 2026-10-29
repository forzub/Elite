#include "src/world/generation/InterstellarMediumField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <mutex>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/InterstellarNoise.h"
#include "src/world/generation/StellarSystemGenerator.h"

namespace world::generation
{

namespace
{

using namespace astro;
using namespace interstellar_noise;

constexpr double LyPerPc = 3.261563777;
constexpr double SuperbubbleCellLy = 500.0;
// The volume the observed anchors (Local Bubble, Loop I) describe: no
// procedural cavity reaches into it.
constexpr double ObservedCavityVolumeLy = 300.0;
constexpr double AmbientPressureKcm3 = 0.3 * 8000.0; // warm neutral medium

// Local Bubble cavity radius in the galactic plane by longitude (pc), after
// the 3D dust maps of Lallement et al. 2014 and Pelgrims et al. 2020
// (approximate): closest toward the Galactic centre / Sco-Cen side, the
// beta CMa tunnel at l ~ 240.
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

const glm::dvec3& galacticNorthIcrs()
{
    static const glm::dvec3 north = galacticToIcrsLy(0.0, 90.0, 1.0);
    return north;
}

std::vector<CavityShell> observedCavities(std::uint64_t universeSeed)
{
    std::vector<CavityShell> out;
    CavityShell lb;
    lb.id = "ism_cavity_local_bubble";
    lb.name = "Local Bubble";
    lb.localBubble = true;
    lb.radiusLy = 80.0 * LyPerPc;
    lb.wallLy = 40.0;
    lb.ageMyr = 14.0;
    lb.supernovae = 15;
    lb.wallCompression = 15.0;
    lb.provenance = "observed";
    lb.source = "Lallement et al. 2014; Pelgrims et al. 2020; Zucker et al. 2022 (age, ~15 SNe)";
    lb.seed = deriveSeed(universeSeed, "ism_bubble_wall");
    out.push_back(lb);

    // Loop I shells (Wolleben 2007, polarised synchrotron): S1 (l 346, b 3,
    // 78 pc, R 72 pc) and S2 (l 332, b 37, 95 pc, R 81 pc). Geometry
    // approximate; the Sun lies near the S1 rim.
    const struct
    {
        const char* id;
        const char* name;
        double l, b, dPc, rPc;
    } loop[] = {{"ism_cavity_loop1_s1", "Loop I shell S1", 346.0, 3.0, 78.0, 72.0},
                {"ism_cavity_loop1_s2", "Loop I shell S2", 332.0, 37.0, 95.0, 81.0}};
    for (const auto& s : loop)
    {
        CavityShell c;
        c.id = s.id;
        c.name = s.name;
        c.centreLy = galacticToIcrsLy(s.l, s.b, s.dPc * LyPerPc);
        c.radiusLy = s.rPc * LyPerPc;
        c.wallLy = 0.15 * c.radiusLy;
        c.ageMyr = 10.0;
        c.supernovae = 20;
        c.wallCompression = 10.0;
        c.provenance = "observed";
        c.source = "Wolleben 2007 (Loop I, Sco-Cen superbubble); geometry approximate";
        c.seed = deriveSeed(universeSeed, s.id);
        out.push_back(c);
    }
    return out;
}

std::vector<CavityShell> superbubbleCell(std::uint64_t universeSeed, long long x, long long y, long long z)
{
    std::vector<CavityShell> out;
    const std::uint64_t cellSeed = deriveSeed(galacticCellSeed(universeSeed, x, y, z), "superbubble_cell");
    RandomStream rs(cellSeed, "cavities");
    const int count = rs.poisson(0.35);
    for (int k = 0; k < count; ++k)
    {
        const std::uint64_t seed = deriveSeed(cellSeed, "cavity", static_cast<std::uint64_t>(k));
        RandomStream cs(seed, "shape");
        CavityShell c;
        c.centreLy = (glm::dvec3(x, y, z) + glm::dvec3(cs.uniform(), cs.uniform(), cs.uniform())) * SuperbubbleCellLy;
        c.radiusLy = cs.logUniform(60.0, 300.0);
        c.wallLy = c.radiusLy * cs.uniform(0.12, 0.25);
        c.ageMyr = cs.uniform(1.0, 20.0);
        c.supernovae = 1 + cs.poisson(c.radiusLy / 25.0);
        c.wallCompression = std::min(20.0, 6.0 + 0.5 * c.supernovae);
        c.seed = seed;
        // Observed volume: the anchors describe it; disc thickness: OB
        // associations (and their bubbles) stay within ~800 ly of the plane.
        if (glm::length(c.centreLy) - c.radiusLy < ObservedCavityVolumeLy)
            continue;
        if (std::abs(glm::dot(c.centreLy, galacticNorthIcrs())) > 800.0)
            continue;
        c.provenance = "procedural_generated";
        c.source = "superbubble cell population";
        static const char* digits = "0123456789ABCDEFGHJKLMNPQRSTUVWXYZ";
        std::string code;
        std::uint64_t h = splitMix64(seed);
        for (int i = 0; i < 4; ++i, h /= 34)
            code += digits[h % 34];
        c.id = "ism_cavity_" + std::to_string(x) + "_" + std::to_string(y) + "_" + std::to_string(z) + "_" +
               std::to_string(k);
        c.name = "Superbubble " + code;
        out.push_back(c);
    }
    return out;
}

const std::vector<CavityShell>& cachedSuperbubbleCell(std::uint64_t universeSeed, long long x, long long y, long long z)
{
    // Per thread: the field is sampled from worker threads too.
    thread_local std::map<std::array<long long, 4>, std::vector<CavityShell>> cache;
    const std::array<long long, 4> key {static_cast<long long>(universeSeed), x, y, z};
    auto it = cache.find(key);
    if (it == cache.end())
        it = cache.emplace(key, superbubbleCell(universeSeed, x, y, z)).first;
    return it->second;
}

const std::vector<CavityShell>& cachedObserved(std::uint64_t universeSeed)
{
    thread_local std::map<std::uint64_t, std::vector<CavityShell>> cache;
    auto it = cache.find(universeSeed);
    if (it == cache.end())
        it = cache.emplace(universeSeed, observedCavities(universeSeed)).first;
    return it->second;
}

// Radius and wall thickness of a cavity toward a direction.
void cavityExtent(const CavityShell& c, const glm::dvec3& dir, double lDeg, double bDeg, double& R, double& W)
{
    if (c.localBubble)
    {
        const double sb = std::sin(bDeg * DegToRad), cb = std::cos(bDeg * DegToRad);
        const double chimney = 1.0 / std::sqrt(cb * cb + (sb / 2.2) * (sb / 2.2)); // open toward the poles
        const double rough = 1.0 + 0.15 * (2.0 * fbm(c.seed, dir * 2.5, 3) - 1.0);
        R = bubbleRadiusPc(lDeg) * LyPerPc * chimney * rough;
        W = 40.0 * (0.7 + 0.6 * fbm(deriveSeed(c.seed, "thickness"), dir * 4.0, 2));
        return;
    }
    // Superbubbles blow out along the disc normal.
    const double vertical = 1.0 + 0.25 * std::abs(glm::dot(dir, galacticNorthIcrs()));
    const double rough = 1.0 + 0.15 * (2.0 * fbm(c.seed, dir * 2.0, 3) - 1.0);
    R = c.radiusLy * vertical * rough;
    W = c.wallLy * (0.7 + 0.6 * fbm(deriveSeed(c.seed, "thickness"), dir * 3.0, 2));
}

// Relative radiative cooling function Lambda(T) (1 = warm 8000 K gas).
double coolingFunction(double temperatureK)
{
    if (temperatureK < 50.0)
        return 0.05;
    if (temperatureK < 300.0)
        return 0.3;
    if (temperatureK < 1.2e4)
        return 1.0;
    if (temperatureK < 2.0e5)
        return 30.0;
    if (temperatureK < 1.0e6)
        return 8.0;
    return 3.0;
}

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

std::vector<CavityShell> cavitiesNear(std::uint64_t universeSeed, const glm::dvec3& p)
{
    std::vector<CavityShell> out = cachedObserved(universeSeed);
    const long long cx = static_cast<long long>(std::floor(p.x / SuperbubbleCellLy));
    const long long cy = static_cast<long long>(std::floor(p.y / SuperbubbleCellLy));
    const long long cz = static_cast<long long>(std::floor(p.z / SuperbubbleCellLy));
    for (long long z = cz - 1; z <= cz + 1; ++z)
        for (long long y = cy - 1; y <= cy + 1; ++y)
            for (long long x = cx - 1; x <= cx + 1; ++x)
                for (const auto& c : cachedSuperbubbleCell(universeSeed, x, y, z))
                    out.push_back(c);
    return out;
}

std::vector<CavityShell> cavitiesInBox(std::uint64_t universeSeed, const glm::dvec3& lo, const glm::dvec3& hi)
{
    std::vector<CavityShell> out = cachedObserved(universeSeed);
    const auto cell = [](double v) { return static_cast<long long>(std::floor(v / SuperbubbleCellLy)); };
    for (long long z = cell(lo.z) - 1; z <= cell(hi.z) + 1; ++z)
        for (long long y = cell(lo.y) - 1; y <= cell(hi.y) + 1; ++y)
            for (long long x = cell(lo.x) - 1; x <= cell(hi.x) + 1; ++x)
                for (const auto& c : cachedSuperbubbleCell(universeSeed, x, y, z))
                {
                    const double reach = 1.6 * c.radiusLy + c.wallLy;
                    const glm::dvec3 nearest = glm::clamp(c.centreLy, lo, hi);
                    if (glm::length(nearest - c.centreLy) <= reach)
                        out.push_back(c);
                }
    return out;
}

MediumSample backgroundMediumAt(std::uint64_t universeSeed, const glm::dvec3& p)
{
    const double local = fbm(deriveSeed(universeSeed, "ism_fine_structure"), p / 12.0, 3);

    struct Wall
    {
        double profile;
        double slopePerLy;
        double compression;
        bool immersed;
    };
    int inside = 0;
    std::array<Wall, 8> walls {};
    int wallCount = 0;
    std::array<int, 8> wallOwner {};
    std::vector<int> insideOwners;
    thread_local std::vector<CavityShell> cavities;
    cavities = cavitiesNear(universeSeed, p);
    for (int index = 0; index < static_cast<int>(cavities.size()); ++index)
    {
        const auto& c = cavities[static_cast<std::size_t>(index)];
        const glm::dvec3 rel = p - c.centreLy;
        const double d = glm::length(rel);
        const glm::dvec3 dir = d > 1.0e-9 ? rel / d : glm::dvec3(1.0, 0.0, 0.0);
        double l = 0.0, b = 0.0, dist = 0.0;
        if (c.localBubble && d > 1.0e-9)
            icrsToGalactic(rel, l, b, dist);
        double R = 0.0, W = 0.0;
        cavityExtent(c, dir, l, b, R, W);
        if (d < R)
        {
            ++inside;
            insideOwners.push_back(index);
        }
        else if (d < R + W && wallCount < static_cast<int>(walls.size()))
        {
            const double x = (d - R) / W;
            const double s = std::sin(Pi * x);
            walls[static_cast<std::size_t>(wallCount)] = {
                std::pow(s, 0.7), 0.7 * std::pow(std::max(s, 1.0e-3), -0.3) * std::abs(std::cos(Pi * x)) * Pi / W,
                c.wallCompression, false};
            wallOwner[static_cast<std::size_t>(wallCount)] = index;
            ++wallCount;
        }
    }
    // A wall inside ANOTHER cavity is immersed in its hot gas.
    double compression = 0.0, immersedProfile = 0.0, peak = 0.0, slope = 0.0;
    int neutralWalls = 0, immersedWalls = 0;
    for (int w = 0; w < wallCount; ++w)
    {
        auto& wall = walls[static_cast<std::size_t>(w)];
        wall.immersed = std::any_of(insideOwners.begin(), insideOwners.end(),
                                    [&](int owner) { return owner != wallOwner[static_cast<std::size_t>(w)]; });
        if (wall.immersed)
        {
            immersedProfile += wall.profile;
            ++immersedWalls;
        }
        else
        {
            ++neutralWalls;
            compression += wall.profile * wall.compression;
            peak = std::max(peak, wall.profile);
            slope = std::max(slope, wall.slopePerLy * wall.compression);
        }
    }

    MediumSample m;
    m.cavityCount = inside;
    m.wallCount = neutralWalls;
    if (neutralWalls > 0)
    {
        // Swept-up wall: neutral, dusty, carrying the ejecta of the
        // supernovae that blew the cavity (60Fe, Wallner et al. 2016).
        // Overlapping walls of different cavities are compressed again.
        const double overlap = 1.0 + 1.2 * (neutralWalls - 1);
        m.densityCm3 = (0.5 + compression * (0.4 + 1.2 * local)) * overlap;
        m.temperatureK = neutralWalls >= 2 ? 50.0 + 3000.0 * (1.0 - peak) : 8000.0 * (1.0 - peak) + 80.0 * peak;
        m.phase = neutralWalls >= 2 && peak > 0.5 && local > 0.55 ? 'm' : peak > 0.6 && local > 0.5 ? 'c' : 's';
        m.ionizationFraction = 0.05;
        m.molecularFraction = std::min(0.9, 0.1 * peak + (neutralWalls >= 2 ? 0.3 * peak : 0.0));
        m.dustToGas = 0.01;
        m.iceMantleFraction = 0.2 * peak + (neutralWalls >= 2 ? 0.2 : 0.0);
        m.supernovaEnrichment = 1.0;
        m.turbulenceKms = 1.5 + 3.0 * peak;
        m.densityGradientPerLy = slope / std::max(m.densityCm3, 1.0e-6);
        // Column through half a wall.
        m.shieldingAv = m.densityCm3 * 20.0 * LightYearMeters * 100.0 * m.dustToGas / 0.01 / 1.9e21;
    }
    else if (immersedProfile > 0.0)
    {
        // A wall bathed in a neighbouring cavity's hot gas and UV: a thin
        // warm, partly ionised sheet (the LIC / G-cloud environment).
        const double s = std::min(immersedProfile, 1.0);
        m.immersedWall = true;
        m.immersedWallCount = immersedWalls;
        m.phase = 'p';
        m.densityCm3 = 0.08 + 0.35 * s * (0.5 + local);
        m.temperatureK = 7500.0 - 1500.0 * s;
        m.ionizationFraction = 0.3;
        m.dustToGas = 0.006;
        m.supernovaEnrichment = 0.6;
        m.turbulenceKms = 2.5;
        m.densityGradientPerLy = 0.05 * s;
        m.shieldingAv = m.densityCm3 * 10.0 * LightYearMeters * 100.0 * 0.6 / 1.9e21;
    }
    else if (inside > 0)
    {
        // Hot, tenuous cavity gas (soft X-ray bubble), dust largely destroyed.
        m.phase = 'h';
        m.densityCm3 = 0.004 * (0.6 + 0.8 * local);
        m.temperatureK = 1.0e6;
        m.ionizationFraction = 1.0;
        m.dustToGas = 0.0005;
        m.supernovaEnrichment = 0.2;
        m.turbulenceKms = 15.0;
    }
    else
    {
        m.phase = 'w';
        m.densityCm3 = 0.3 * (0.5 + local);
        m.temperatureK = 8000.0;
        m.ionizationFraction = 0.1;
        m.dustToGas = 0.006;
        m.turbulenceKms = 1.5 + 1.5 * local;
        m.densityGradientPerLy = 0.01;
        m.shieldingAv = m.densityCm3 * 10.0 * LightYearMeters * 100.0 * 0.6 / 1.9e21;
    }
    m.pressureKcm3 = m.densityCm3 * m.temperatureK;
    return m;
}

CloudFormationPotential cloudFormationPotential(const MediumSample& m)
{
    // Cooling efficiency ~ t_dyn / t_cool ~ n Lambda(T) (1 + metals) / T.
    const double coolingRate =
        m.densityCm3 * coolingFunction(m.temperatureK) * (1.0 + m.supernovaEnrichment) / std::max(m.temperatureK, 1.0);
    const double cooling = smoothstep(-5.0, -2.5, std::log10(std::max(coolingRate, 1.0e-30)));
    const double compression = smoothstep(1.5, 8.0, m.pressureKcm3 / AmbientPressureKcm3);
    const double turbulence = 1.0 / (1.0 + (m.turbulenceKms / 6.0) * (m.turbulenceKms / 6.0));
    const double shielding = smoothstep(0.03, 0.5, m.shieldingAv);
    const double gradient = 0.7 + 0.3 * smoothstep(0.01, 0.2, m.densityGradientPerLy);
    const double intersection = m.wallCount >= 2 ? 1.6 : 1.0;

    CloudFormationPotential p;
    if (m.immersedWall)
        p.warm = 0.4 + 0.6 * smoothstep(0.05, 0.3, m.densityCm3);
    else if (m.phase == 'h')
        p.warm = 0.02;
    else
        p.warm = 0.05 * cooling;
    p.warm = std::clamp(p.warm * gradient, 0.0, 1.0);

    if (m.phase != 'h' && m.phase != 'p')
        p.cold = std::clamp(cooling * (0.25 + 0.75 * compression) * turbulence * gradient * intersection, 0.0, 1.0);
    else if (m.immersedWallCount >= 2)
        // Interaction zone: two immersed walls collide (converging flows
        // inside the cavities); rare cold clouds form there (the Local Leo
        // Cold Cloud, Peek et al. 2011).
        p.cold = 0.3 * smoothstep(0.15, 0.45, m.densityCm3);
    p.molecular = std::clamp(p.cold * shielding * (m.wallCount >= 2 ? 1.0 : 0.25), 0.0, 1.0);
    return p;
}

} // namespace world::generation
