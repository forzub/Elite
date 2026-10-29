#include "src/world/interstellar/InterstellarNavigationEnvironment.h"

#include <algorithm>
#include <cmath>

#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/InterstellarMediumField.h"
#include "src/world/interstellar/ExtractionIndices.h"

namespace world::interstellar
{

namespace
{
constexpr double LightYearCm = 9.4607304725808e17;
constexpr double AuM = 149597870700.0;
constexpr double AuPerLy = 63241.077;
constexpr double MeanParticleMassKg = 1.4 * 1.67262192369e-27;
// Peak / mean number density of the soft-edged (1 - r^2) ellipsoid.
constexpr double SoftEdgePeak = 2.5;

std::uint64_t parseSeed(const nlohmann::json& props)
{
    if (!props.contains("provenance") || !props["provenance"].contains("seed"))
        return 0;
    try
    {
        return std::stoull(props["provenance"]["seed"].get<std::string>(), nullptr, 16);
    }
    catch (const std::exception&)
    {
        return 0;
    }
}

glm::dmat3 rotationFromQuatWxyz(const nlohmann::json& q)
{
    const double w = q.at(0).get<double>(), x = q.at(1).get<double>(), y = q.at(2).get<double>(),
                 z = q.at(3).get<double>();
    return glm::dmat3(1 - 2 * (y * y + z * z), 2 * (x * y + w * z), 2 * (x * z - w * y),
                      2 * (x * y - w * z), 1 - 2 * (x * x + z * z), 2 * (y * z + w * x),
                      2 * (x * z + w * y), 2 * (y * z - w * x), 1 - 2 * (x * x + y * y));
}

// Ionising-radiation environment index (documented relative scale): the
// hot cavity bath = 1, warm partly ionised gas ~0.6, neutral gas lower,
// shielded cold / molecular gas ~0.1.
double radiationIndex(double ionization, double temperatureK)
{
    return std::clamp(0.05 + 0.95 * std::sqrt(std::clamp(ionization, 0.0, 1.0)) * (temperatureK > 1.0e5 ? 1.0 : 0.7),
                      0.0, 1.0);
}
} // namespace

InterstellarNavigationEnvironment::InterstellarNavigationEnvironment(
    std::uint64_t universeSeed, const std::vector<ObjectView>& objects)
    : m_seed(universeSeed)
{
    for (const auto& o : objects)
    {
        if (!o.properties)
            continue;
        const auto& p = *o.properties;
        if (o.objectType == "interstellar_medium_region" && p.contains("geometry") && p["geometry"].contains("lattice"))
        {
            Structure s;
            if (!latticeFromJson(p["geometry"]["lattice"], s.lattice))
                continue;
            s.id = o.id;
            s.phase = mediumPhaseCode(p.value("phase", std::string()));
            s.lo = s.lattice.originLy;
            s.hi = s.lattice.originLy + glm::dvec3(s.lattice.dims) * s.lattice.voxelLy;
            s.temperatureK = p.value("temperature_k", 0.0);
            s.ionization = p.value("ionization_fraction", 0.0);
            s.molecular = p.value("molecular_fraction", 0.0);
            s.dustToGas = p.value("dust_to_gas", 0.0);
            s.ice = p.value("ice_mantle_fraction", 0.0);
            s.enrichment = p.value("supernova_enrichment", 0.0);
            s.turbulenceKms = p.contains("density_profile") ? p["density_profile"].value("turbulence_kms", 0.0) : 0.0;
            m_structures.push_back(std::move(s));
        }
        else if (o.objectType == "icy_debris_field" && p.contains("solid_population") && p.contains("geometry"))
        {
            DebrisField f;
            f.id = o.id;
            f.centreLy = o.positionLy;
            const auto& g = p["geometry"];
            f.radiiLy = {g["radii_ly"][0].get<double>(), g["radii_ly"][1].get<double>(), g["radii_ly"][2].get<double>()};
            f.toLocal = glm::transpose(rotationFromQuatWxyz(g["orientation_quat_wxyz"]));
            const auto& n = p["solid_population"]["number_density_per_au3"];
            f.perAu3[0] = n.value("above_100um", 0.0);
            f.perAu3[1] = n.value("above_1cm", 0.0);
            f.perAu3[2] = n.value("above_10m", 0.0);
            f.perAu3[3] = n.value("above_1km", 0.0);
            f.dustKgM3 = p["solid_population"].value("dust_mass_density_kg_m3", 0.0);
            f.volatileIndex = p.contains("resource_profile") ? p["resource_profile"].value("volatile_index", 0.0) : 0.0;
            m_fields.push_back(std::move(f));
        }
    }
}

NavigationEnvironmentSample InterstellarNavigationEnvironment::sample(const glm::dvec3& x) const
{
    const auto m = world::generation::backgroundMediumAt(m_seed, x);
    NavigationEnvironmentSample out;
    double n = m.densityCm3, T = m.temperatureK, ion = m.ionizationFraction, mol = m.molecularFraction,
           dg = m.dustToGas, ice = m.iceMantleFraction, enr = m.supernovaEnrichment, turb = m.turbulenceKms;
    char phase = m.phase;
    for (const auto& s : m_structures)
    {
        if (glm::any(glm::lessThan(x, s.lo)) || glm::any(glm::greaterThanEqual(x, s.hi)))
            continue;
        const glm::ivec3 v = glm::ivec3(glm::floor((x - s.lattice.originLy) / s.lattice.voxelLy));
        const glm::ivec3 d = s.lattice.dims;
        if (v.x < 0 || v.y < 0 || v.z < 0 || v.x >= d.x || v.y >= d.y || v.z >= d.z)
            continue;
        const double density = s.lattice.densityCm3(static_cast<std::size_t>(v.x + d.x * (v.y + d.y * v.z)));
        if (density <= n)
            continue;
        n = density;
        T = s.temperatureK;
        ion = s.ionization;
        mol = s.molecular;
        dg = s.dustToGas;
        ice = s.ice;
        enr = s.enrichment;
        turb = s.turbulenceKms > 0.0 ? s.turbulenceKms : turb;
        phase = s.phase ? s.phase : phase;
        out.structureId = s.id;
    }
    out.gasDensityCm3 = n;
    out.gasMassDensityKgM3 = n * 1.0e6 * MeanParticleMassKg;
    out.dustMassDensityKgM3 = out.gasMassDensityKgM3 * dg;
    out.temperatureK = T;
    out.ionizationFraction = ion;
    out.electronDensityCm3 = n * ion;
    out.molecularFraction = mol;
    out.turbulenceKms = turb;
    out.phase = phase;
    out.extinctionAvPerLy = n * LightYearCm * (dg / 0.01) / 1.9e21;
    out.ionizingRadiationIndex = radiationIndex(ion, T);
    const auto e = extractionIndices(n, dg, ice, enr);
    out.hydrogenFuelIndex = e.hydrogenFuel;
    out.volatileIndex = e.volatiles;
    out.heavyElementIndex = e.heavyElements;

    for (const auto& f : m_fields)
    {
        const glm::dvec3 q = (f.toLocal * (x - f.centreLy)) / f.radiiLy;
        const double r2 = glm::dot(q, q);
        if (r2 >= 1.0)
            continue;
        const double profile = SoftEdgePeak * (1.0 - r2);
        const double perM3 = profile / (AuM * AuM * AuM);
        const double classes[4] = {f.perAu3[0] - f.perAu3[1], f.perAu3[1] - f.perAu3[2], f.perAu3[2] - f.perAu3[3],
                                   f.perAu3[3]};
        for (std::size_t c = 0; c < 4; ++c)
            out.solidNumberDensityM3[c] += std::max(0.0, classes[c]) * perM3;
        out.dustMassDensityKgM3 += f.dustKgM3 * profile;
        out.volatileIndex = std::max(out.volatileIndex, f.volatileIndex);
        out.debrisFieldId = f.id;
    }
    return out;
}

NavigationEnvironmentSegment InterstellarNavigationEnvironment::segment(
    const glm::dvec3& a, const glm::dvec3& b, double stepLy) const
{
    NavigationEnvironmentSegment s;
    s.lengthLy = glm::length(b - a);
    if (stepLy <= 0.0)
        stepLy = 0.5;
    const int steps = std::max(1, static_cast<int>(std::ceil(s.lengthLy / stepLy)));
    const double dlLy = s.lengthLy / steps;
    const double dlM = dlLy * LightYearCm / 100.0;
    for (int i = 0; i < steps; ++i)
    {
        const auto p = sample(a + (b - a) * ((i + 0.5) / steps));
        s.gasColumnCm2 += p.gasDensityCm3 * dlLy * LightYearCm;
        s.dustColumnKgM2 += p.dustMassDensityKgM3 * dlM;
        for (std::size_t c = 0; c < s.solidColumnM2.size(); ++c)
            s.solidColumnM2[c] += p.solidNumberDensityM3[c] * dlM;
        s.extinctionAv += p.extinctionAvPerLy * dlLy;
        s.maxGasDensityCm3 = std::max(s.maxGasDensityCm3, p.gasDensityCm3);
        s.maxElectronDensityCm3 = std::max(s.maxElectronDensityCm3, p.electronDensityCm3);
        s.maxIonizingRadiationIndex = std::max(s.maxIonizingRadiationIndex, p.ionizingRadiationIndex);
        s.maxTurbulenceKms = std::max(s.maxTurbulenceKms, p.turbulenceKms);
        s.maxHydrogenFuelIndex = std::max(s.maxHydrogenFuelIndex, p.hydrogenFuelIndex);
        s.maxVolatileIndex = std::max(s.maxVolatileIndex, p.volatileIndex);
        if (!p.structureId.empty() &&
            std::find(s.structuresCrossed.begin(), s.structuresCrossed.end(), p.structureId) == s.structuresCrossed.end())
            s.structuresCrossed.push_back(p.structureId);
        if (!p.debrisFieldId.empty() &&
            std::find(s.debrisFieldsCrossed.begin(), s.debrisFieldsCrossed.end(), p.debrisFieldId) ==
                s.debrisFieldsCrossed.end())
            s.debrisFieldsCrossed.push_back(p.debrisFieldId);
    }
    return s;
}

std::vector<IcyBody> materializeIcyBodies(
    const nlohmann::json& p,
    const glm::dvec3& fieldCentreLy,
    std::uint64_t fieldSeed,
    const glm::dvec3& queryCentreLy,
    double queryRadiusLy,
    double minDiameterM,
    std::size_t maxBodies)
{
    std::vector<IcyBody> out;
    if (!p.contains("solid_population") || !p.contains("geometry") || queryRadiusLy <= 0.0 || minDiameterM <= 0.0 ||
        maxBodies == 0)
        return out;
    if (fieldSeed == 0)
        fieldSeed = parseSeed(p);
    const auto& g = p["geometry"];
    const glm::dvec3 radii(g["radii_ly"][0].get<double>(), g["radii_ly"][1].get<double>(), g["radii_ly"][2].get<double>());
    const glm::dmat3 toLocal = glm::transpose(rotationFromQuatWxyz(g["orientation_quat_wxyz"]));
    const auto& pop = p["solid_population"];
    const double q = pop.value("cumulative_size_slope", 2.5);
    const double n1km = pop["number_density_per_au3"].value("above_1km", 0.0);
    const double largestM = pop.value("largest_body_km", 30.0) * 1000.0;
    const auto above = [&](double d) { return n1km * std::pow(d / 1000.0, -q); }; // per AU^3, field mean
    const double queryVolumeAu3 = 4.0 / 3.0 * 3.14159265358979 * std::pow(queryRadiusLy * AuPerLy, 3.0);

    /*
        Size bands of one decade (lower bound 10^k m), largest first. Each
        band has its own cube edge (a power of two in AU, chosen from the
        field's mean density so a cube holds ~8 bodies of the band) and its
        own seed per cube, so a body's existence never depends on the query.
        Bands are added until maxBodies is reached; a band whose expected
        count in the query exceeds 50 x maxBodies is never expanded.
    */
    for (int k = static_cast<int>(std::floor(std::log10(largestM))); k >= -4; --k)
    {
        const double bandLo = std::pow(10.0, k), bandHi = std::min(largestM, std::pow(10.0, k + 1));
        if (bandHi <= minDiameterM)
            break;
        const double lo = std::max(bandLo, minDiameterM);
        const double nBand = above(lo) - above(bandHi);
        if (nBand <= 0.0)
            continue;
        if (nBand * SoftEdgePeak * queryVolumeAu3 > 50.0 * static_cast<double>(maxBodies))
            break;
        // Cube edge from the whole band (a field property, never the query).
        const double bandFull = above(bandLo) - above(bandHi);
        const double edgeAu = std::exp2(std::round(std::log2(std::cbrt(8.0 / bandFull))));
        const double cubeLy = edgeAu / AuPerLy;
        const glm::dvec3 qlo = queryCentreLy - glm::dvec3(queryRadiusLy);
        const glm::dvec3 qhi = queryCentreLy + glm::dvec3(queryRadiusLy);
        const auto cell = [&](double v) { return static_cast<long long>(std::floor(v / cubeLy)); };
        const std::uint64_t bandSeed = world::generation::deriveSeed(fieldSeed, "icy_band", static_cast<std::uint64_t>(k + 16));
        for (long long z = cell(qlo.z); z <= cell(qhi.z); ++z)
            for (long long y = cell(qlo.y); y <= cell(qhi.y); ++y)
                for (long long x = cell(qlo.x); x <= cell(qhi.x); ++x)
                {
                    const glm::dvec3 cubeCentre = (glm::dvec3(x, y, z) + 0.5) * cubeLy;
                    const glm::dvec3 local = (toLocal * (cubeCentre - fieldCentreLy)) / radii;
                    const double r2 = glm::dot(local, local);
                    if (r2 >= 1.0)
                        continue;
                    const std::uint64_t cubeSeed = world::generation::splitMix64(
                        bandSeed ^ world::generation::splitMix64(static_cast<std::uint64_t>(x) * 0x9E3779B97F4A7C15ull ^
                            world::generation::splitMix64(static_cast<std::uint64_t>(y) * 0xC2B2AE3D27D4EB4Full ^
                                                          world::generation::splitMix64(static_cast<std::uint64_t>(z)))));
                    world::generation::RandomStream rs(cubeSeed, "icy_bodies");
                    const int count = rs.poisson(bandFull * SoftEdgePeak * (1.0 - r2) * edgeAu * edgeAu * edgeAu);
                    for (int i = 0; i < count; ++i)
                    {
                        IcyBody body;
                        body.seed = world::generation::deriveSeed(cubeSeed, "body", static_cast<std::uint64_t>(i));
                        world::generation::RandomStream bs(body.seed, "body");
                        body.positionLy = (glm::dvec3(x, y, z) + glm::dvec3(bs.uniform(), bs.uniform(), bs.uniform())) * cubeLy;
                        // Inverse CDF of N(>D) ~ D^-q within the band.
                        const double u = bs.uniform();
                        const double a = std::pow(bandLo, -q), b = std::pow(bandHi, -q);
                        body.diameterM = std::pow(a - u * (a - b), -1.0 / q);
                        if (body.diameterM < minDiameterM || glm::length(body.positionLy - queryCentreLy) > queryRadiusLy)
                            continue;
                        body.massKg = 600.0 * 3.14159265358979 / 6.0 * body.diameterM * body.diameterM * body.diameterM;
                        out.push_back(body);
                    }
                }
        if (out.size() >= maxBodies)
            break;
    }
    std::sort(out.begin(), out.end(), [](const IcyBody& a, const IcyBody& b) { return a.diameterM > b.diameterM; });
    if (out.size() > maxBodies)
        out.resize(maxBodies);
    return out;
}

} // namespace world::interstellar
