#include "src/world/generation/StellarPhysics.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <sstream>

#include "src/world/generation/AstroUnits.h"

namespace world::generation
{

namespace
{

constexpr double SolGalactocentricKpc = 8.18;
constexpr double SolHeightKpc = 0.0208;
constexpr double LyPerKpc = 3261.563777;

double clampd(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

double gaussian(double x, double mean, double sigma)
{
    const double z = (x - mean) / sigma;
    return std::exp(-0.5 * z * z) / sigma;
}

std::string lower(std::string s)
{
    for (auto& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

StellarStage inferStage(const StarInput& in)
{
    const std::string hint = lower(in.stageHint);
    if (hint == "brown_dwarf") return StellarStage::BrownDwarf;
    if (hint == "white_dwarf") return StellarStage::WhiteDwarf;
    if (hint == "giant") return StellarStage::RedGiant;
    if (hint == "subgiant") return StellarStage::Subgiant;
    if (hint == "main_sequence") return StellarStage::MainSequence;

    if (in.massSolar && *in.massSolar < 0.075)
        return StellarStage::BrownDwarf;
    const std::string sp = in.spectralType;
    if (!sp.empty() && sp[0] == 'D')
        return StellarStage::WhiteDwarf;
    if (!sp.empty() && (sp[0] == 'L' || sp[0] == 'T' || sp[0] == 'Y'))
        return StellarStage::BrownDwarf;
    if (sp.find("III") != std::string::npos || sp.find("II") != std::string::npos)
        return StellarStage::RedGiant;
    if (sp.find("IV") != std::string::npos && sp.find("IV-V") == std::string::npos &&
        sp.find("IV/V") == std::string::npos)
        return StellarStage::Subgiant;
    return StellarStage::MainSequence;
}

double zeroAgeRadiusSolar(double m)
{
    return m < 1.0 ? 0.89 * std::pow(m, 0.8) : 0.89 * std::pow(m, 0.6);
}

double teffFrom(double lSolar, double rSolar)
{
    return astro::SunTeffK * std::pow(lSolar / (rSolar * rSolar), 0.25);
}

double radiusFrom(double lSolar, double teffK)
{
    const double t = teffK / astro::SunTeffK;
    return std::sqrt(lSolar) / (t * t);
}

std::string spectralTypeFromTeff(double t, StellarStage stage)
{
    if (stage == StellarStage::WhiteDwarf)
        return "DA";
    struct Row { double teff; const char* type; };
    static const Row rows[] = {
        {30000, "B0"}, {15000, "B5"}, {9700, "A0"}, {8100, "A5"}, {7200, "F0"},
        {6500, "F5"}, {5940, "G0"}, {5650, "G5"}, {5270, "K0"}, {4400, "K5"},
        {3850, "M0"}, {3200, "M4"}, {2650, "M8"}, {2200, "L0"}, {1300, "T0"},
        {0, "Y0"}};
    const char* type = "Y0";
    for (const auto& r : rows)
        if (t >= r.teff) { type = r.type; break; }
    const char* cls = stage == StellarStage::RedGiant ? " III"
        : stage == StellarStage::Subgiant ? " IV"
        : stage == StellarStage::BrownDwarf ? "" : " V";
    return std::string(type) + cls;
}

} // namespace

const char* toString(Provenance p)
{
    switch (p)
    {
        case Provenance::Observed: return "observed";
        case Provenance::Literature: return "literature";
        case Provenance::Authored: return "authored";
        case Provenance::Derived: return "derived";
        case Provenance::Generated: return "generated";
    }
    return "generated";
}

Provenance provenanceFromString(const std::string& s)
{
    if (s == "observed") return Provenance::Observed;
    if (s == "literature") return Provenance::Literature;
    if (s == "authored") return Provenance::Authored;
    if (s == "derived") return Provenance::Derived;
    return Provenance::Generated;
}

const char* toString(GalacticPopulation p)
{
    switch (p)
    {
        case GalacticPopulation::ThinDisk: return "thin_disk";
        case GalacticPopulation::ThickDisk: return "thick_disk";
        case GalacticPopulation::Halo: return "halo";
    }
    return "thin_disk";
}

const char* toString(StellarStage s)
{
    switch (s)
    {
        case StellarStage::PreMainSequence: return "pre_main_sequence";
        case StellarStage::MainSequence: return "main_sequence";
        case StellarStage::Subgiant: return "subgiant";
        case StellarStage::RedGiant: return "red_giant";
        case StellarStage::WhiteDwarf: return "white_dwarf";
        case StellarStage::BrownDwarf: return "brown_dwarf";
    }
    return "main_sequence";
}

double periodYearsFromAu(double aAu, double totalMassSolar)
{
    return std::sqrt(aAu * aAu * aAu / std::max(totalMassSolar, 1.0e-6));
}

double semiMajorAxisAuFromPeriod(double periodYears, double totalMassSolar)
{
    return std::cbrt(periodYears * periodYears * std::max(totalMassSolar, 1.0e-6));
}

double mainSequenceLifetimeGyr(double massSolar, double feh)
{
    if (massSolar < 0.08)
        return std::numeric_limits<double>::infinity();
    // Metal-poor stars are hotter and burn slightly faster.
    return 10.0 * std::pow(massSolar, -2.5) * (1.0 + 0.1 * clampd(feh, -2.0, 0.5));
}

double zeroAgeLuminositySolar(double m)
{
    if (m < 0.43) return 0.23 * std::pow(m, 2.3);
    if (m < 2.0) return 0.72 * std::pow(m, 4.0); // Sun ZAMS ~0.72 L_sun
    return 1.4 * 0.72 * std::pow(m, 3.5);
}

double holmanWiegertSTypeCritical(double mu, double e)
{
    const double v = 0.464 - 0.380 * mu - 0.631 * e + 0.586 * mu * e +
        0.150 * e * e - 0.198 * mu * e * e;
    return std::max(0.0, v);
}

double holmanWiegertPTypeCritical(double mu, double e)
{
    return 1.60 + 5.10 * e - 2.22 * e * e + 4.12 * mu - 4.27 * e * mu -
        5.09 * mu * mu + 4.61 * e * e * mu * mu;
}

double mardlingAarsethCriticalRatio(double q, double e, double iDeg)
{
    const double ee = clampd(e, 0.0, 0.99);
    return 2.8 * std::pow(1.0 + q, 0.4) * std::pow(1.0 + ee, 0.4) /
        std::pow(1.0 - ee, 1.2) * (1.0 - 0.3 * clampd(iDeg, 0.0, 180.0) / 180.0);
}

GalacticContextDescriptor buildGalacticContext(
    const glm::dvec3& p, const std::vector<StarInput>& stars, std::uint64_t systemSeed)
{
    GalacticContextDescriptor c;
    // ICRS -> Galactic (Hipparcos convention), heliocentric.
    const double r[3][3] = {
        {-0.0548755604, -0.8734370902, -0.4838350155},
        {0.4941094279, -0.4448296300, 0.7469822445},
        {-0.8676661490, -0.1980763734, 0.4559837762}};
    const glm::dvec3 g(
        r[0][0] * p.x + r[0][1] * p.y + r[0][2] * p.z,
        r[1][0] * p.x + r[1][1] * p.y + r[1][2] * p.z,
        r[2][0] * p.x + r[2][1] * p.y + r[2][2] * p.z);
    c.galactocentricKpc = g / LyPerKpc;
    const double x = SolGalactocentricKpc - c.galactocentricKpc.x;
    const double y = c.galactocentricKpc.y;
    c.galactocentricRadiusKpc = std::sqrt(x * x + y * y);
    c.heightAbovePlaneKpc = c.galactocentricKpc.z + SolHeightKpc;

    const double R = c.galactocentricRadiusKpc;
    const double z = std::abs(c.heightAbovePlaneKpc);
    const double dR = R - SolGalactocentricKpc;
    const double thin = std::exp(-dR / 2.6) * std::exp(-z / 0.30);
    const double thick = 0.12 * std::exp(-dR / 3.6) * std::exp(-z / 0.90);
    const double rh = std::sqrt(R * R + (z / 0.64) * (z / 0.64));
    const double halo = 0.005 * std::pow(SolGalactocentricKpc / std::max(rh, 0.5), 2.77);

    const double thinMean = -0.05 - 0.06 * dR;
    double wThin = thin, wThick = thick, wHalo = halo;

    // Posterior with the observed metallicity / age of the first star that has one.
    for (const auto& s : stars)
    {
        if (s.metallicityFeH)
        {
            const double f = *s.metallicityFeH;
            wThin *= gaussian(f, thinMean, 0.22);
            wThick *= gaussian(f, -0.6, 0.30);
            wHalo *= gaussian(f, -1.5, 0.50);
            break;
        }
    }
    for (const auto& s : stars)
    {
        if (s.ageGyr)
        {
            const double a = *s.ageGyr;
            wThin *= a <= 10.5 ? 1.0 : std::exp(-(a - 10.5) / 0.7);
            wThick *= gaussian(a, 10.5, 1.5);
            wHalo *= gaussian(a, 12.0, 1.0);
            break;
        }
    }

    const bool observed = std::any_of(stars.begin(), stars.end(),
        [](const StarInput& s) { return s.metallicityFeH.has_value(); });
    const double total = wThin + wThick + wHalo;
    if (observed)
    {
        c.population = wThin >= wThick && wThin >= wHalo ? GalacticPopulation::ThinDisk
            : wThick >= wHalo ? GalacticPopulation::ThickDisk : GalacticPopulation::Halo;
    }
    else
    {
        RandomStream rs(systemSeed, "galactic_population");
        const double u = rs.uniform() * total;
        c.population = u < wThin ? GalacticPopulation::ThinDisk
            : u < wThin + wThick ? GalacticPopulation::ThickDisk : GalacticPopulation::Halo;
    }

    switch (c.population)
    {
        case GalacticPopulation::ThinDisk:
            c.populationMetallicityMeanFeH = thinMean;
            c.populationMetallicitySigmaDex = 0.22;
            c.alphaEnhancement = 0.05;
            break;
        case GalacticPopulation::ThickDisk:
            c.populationMetallicityMeanFeH = -0.6;
            c.populationMetallicitySigmaDex = 0.30;
            c.alphaEnhancement = 0.25;
            break;
        case GalacticPopulation::Halo:
            c.populationMetallicityMeanFeH = -1.5;
            c.populationMetallicitySigmaDex = 0.50;
            c.alphaEnhancement = 0.35;
            break;
    }

    c.localStellarDensityPerPc3 = 0.1 * (thin + thick + halo);
    RandomStream env(systemSeed, "birth_environment");
    c.formationEnvironmentDensity = env.logNormal10(1.0, 0.5);
    // Sol sits in the Local Bubble (~100 pc of hot, rarefied gas).
    const double distLy = glm::length(p);
    c.interstellarDensityPerCm3 = distLy < 300.0 ? 0.05 : 0.5 * std::exp(-z / 0.15);
    return c;
}

void sampleSystemAgeMetallicity(
    const std::vector<StarInput>& stars,
    const GalacticContextDescriptor& c,
    std::uint64_t systemSeed,
    double& age,
    double& feh,
    ProvenanceMap& prov)
{
    RandomStream rs(systemSeed, "system_age_metallicity");
    const StarInput* withAge = nullptr;
    const StarInput* withFeH = nullptr;
    for (const auto& s : stars)
    {
        if (!withAge && s.ageGyr) withAge = &s;
        if (!withFeH && s.metallicityFeH) withFeH = &s;
    }

    if (withAge)
    {
        age = *withAge->ageGyr;
        prov["age_gyr"] = Provenance::Literature;
    }
    else
    {
        switch (c.population)
        {
            case GalacticPopulation::ThinDisk: age = rs.uniform(0.3, 10.0); break;
            case GalacticPopulation::ThickDisk: age = rs.truncatedNormal(10.5, 1.0, 8.0, 12.5); break;
            case GalacticPopulation::Halo: age = rs.truncatedNormal(12.0, 0.8, 10.0, 13.5); break;
        }
        // A massive component bounds the age by its main-sequence lifetime.
        for (const auto& s : stars)
        {
            if (s.massSolar && inferStage(s) == StellarStage::MainSequence)
                age = std::min(age, 0.9 * mainSequenceLifetimeGyr(*s.massSolar, 0.0));
        }
        prov["age_gyr"] = Provenance::Generated;
    }

    if (withFeH)
    {
        feh = *withFeH->metallicityFeH;
        prov["metallicity_feh"] = Provenance::Literature;
    }
    else
    {
        // Thin-disk age-metallicity relation: older stars are slightly poorer.
        const double mean = c.populationMetallicityMeanFeH -
            (c.population == GalacticPopulation::ThinDisk ? 0.03 * (age - 4.6) : 0.0);
        feh = rs.truncatedNormal(mean, c.populationMetallicitySigmaDex, -2.5, 0.5);
        prov["metallicity_feh"] = Provenance::Generated;
    }
}

StarDescriptor buildStar(
    const StarInput& in,
    const GalacticContextDescriptor& context,
    double systemAge,
    double systemFeH,
    std::uint64_t seed,
    std::vector<std::string>& warnings)
{
    (void)context;
    StarDescriptor s;
    s.key = in.key;
    s.name = in.name;
    s.alternativeNames = in.alternativeNames;
    s.seed = seed;
    s.sources = in.sources;
    s.extra = in.extra;
    s.stage = inferStage(in);
    RandomStream rs(seed, "star_state");

    auto prov = [&](const char* field, bool fromInput) {
        if (fromInput)
        {
            const auto it = in.sources.find(field);
            const bool observed = it != in.sources.end() &&
                it->second.find("NASA Exoplanet Archive") != std::string::npos;
            s.provenance[field] = observed ? Provenance::Observed : Provenance::Literature;
        }
        else
        {
            s.provenance[field] = Provenance::Derived;
        }
    };

    s.ageGyr = in.ageGyr.value_or(systemAge);
    s.metallicityFeH = in.metallicityFeH.value_or(systemFeH);
    prov("age_gyr", in.ageGyr.has_value());
    prov("metallicity_feh", in.metallicityFeH.has_value());

    if (in.massSolar)
    {
        s.massSolar = *in.massSolar;
        prov("mass_solar", true);
    }
    else
    {
        // Kroupa-like IMF above the hydrogen-burning limit.
        s.massSolar = rs.chance(0.8) ? rs.powerLaw(-1.3, 0.08, 0.5) : rs.powerLaw(-2.3, 0.5, 3.0);
        s.provenance["mass_solar"] = Provenance::Generated;
    }

    // Model state.
    double mi = s.massSolar;
    double lModel = 0.0, rModel = 0.0;
    switch (s.stage)
    {
        case StellarStage::BrownDwarf:
        {
            const double age = std::max(s.ageGyr, 0.01);
            lModel = 1.0e-4 * std::pow(s.massSolar / 0.05, 2.64) * std::pow(age, -1.3);
            rModel = clampd(0.1 * std::pow(age, -0.1), 0.08, 0.16);
            break;
        }
        case StellarStage::WhiteDwarf:
        {
            mi = clampd((s.massSolar - 0.394) / 0.109, 0.8, 8.0);
            const double tCool = std::max(0.01, s.ageGyr - 1.15 * mainSequenceLifetimeGyr(mi, s.metallicityFeH));
            lModel = 3.5e-3 * (s.massSolar / 0.6) * std::pow(tCool, -1.4);
            rModel = 0.0127 * std::pow(s.massSolar / 0.6, -1.0 / 3.0);
            break;
        }
        case StellarStage::RedGiant:
            lModel = 100.0 * std::pow(s.massSolar, 1.5);
            rModel = radiusFrom(lModel, 4300.0);
            break;
        case StellarStage::Subgiant:
        {
            const double lz = zeroAgeLuminositySolar(s.massSolar);
            lModel = lz * 1.85 * rs.uniform(1.0, 1.6);
            rModel = zeroAgeRadiusSolar(s.massSolar) * 1.34 * rs.uniform(1.2, 2.2);
            break;
        }
        default:
        {
            const double tms = mainSequenceLifetimeGyr(s.massSolar, s.metallicityFeH);
            const double tau = clampd(s.ageGyr / tms, 0.0, 1.0);
            lModel = zeroAgeLuminositySolar(s.massSolar) * (1.0 + 0.85 * tau);
            rModel = zeroAgeRadiusSolar(s.massSolar) * (1.0 + 0.5 * tau * tau);
            if (s.ageGyr > tms * 1.1)
            {
                std::ostringstream w;
                w << s.name << ": age " << s.ageGyr << " Gyr exceeds the main-sequence lifetime "
                  << tms << " Gyr of a " << s.massSolar << " Msun star";
                warnings.push_back(w.str());
            }
            break;
        }
    }
    s.initialMassSolar = mi;
    s.mainSequenceLifetimeGyr = mainSequenceLifetimeGyr(mi, s.metallicityFeH);
    s.provenance["initial_mass_solar"] = Provenance::Derived;

    // Observations override the model; the missing third of (L, R, Teff) is derived.
    const bool hasL = in.luminositySolar.has_value();
    const bool hasR = in.radiusSolar.has_value();
    const bool hasT = in.teffK.has_value();
    s.luminositySolar = hasL ? *in.luminositySolar : lModel;
    s.radiusSolar = hasR ? *in.radiusSolar : rModel;
    if (hasT)
        s.teffK = *in.teffK;
    else
        s.teffK = teffFrom(s.luminositySolar, s.radiusSolar);
    if (!hasR && hasL && hasT)
        s.radiusSolar = radiusFrom(s.luminositySolar, s.teffK);
    if (!hasL && hasR && hasT)
        s.luminositySolar = s.radiusSolar * s.radiusSolar *
            std::pow(s.teffK / astro::SunTeffK, 4.0);
    prov("luminosity_solar", hasL);
    prov("radius_solar", hasR);
    prov("teff_k", hasT);
    if (!hasL && !hasR)
        s.provenance["luminosity_solar"] = s.provenance["radius_solar"] = Provenance::Derived;

    if (hasL && hasR && hasT)
    {
        const double lStefan = s.radiusSolar * s.radiusSolar * std::pow(s.teffK / astro::SunTeffK, 4.0);
        if (std::abs(lStefan / s.luminositySolar - 1.0) > 0.35)
        {
            std::ostringstream w;
            w << s.name << ": observed L=" << s.luminositySolar << " disagrees with R^2 T^4 = "
              << lStefan << " (kept the observed values)";
            warnings.push_back(w.str());
        }
    }

    s.spectralType = !in.spectralType.empty() ? in.spectralType : spectralTypeFromTeff(s.teffK, s.stage);
    s.muM3s2 = astro::G * s.massSolar * astro::SunMassKg;
    s.provenance["mu_m3s2"] = Provenance::Derived;

    if (in.rotationPeriodDays)
    {
        s.rotationPeriodDays = *in.rotationPeriodDays;
        prov("rotation_period_days", true);
    }
    else
    {
        switch (s.stage)
        {
            case StellarStage::BrownDwarf: s.rotationPeriodDays = rs.logUniform(0.08, 0.6); break;
            case StellarStage::WhiteDwarf: s.rotationPeriodDays = rs.logUniform(0.05, 3.0); break;
            case StellarStage::RedGiant: s.rotationPeriodDays = rs.logUniform(200.0, 900.0); break;
            default:
                if (s.massSolar > 1.35)
                    s.rotationPeriodDays = rs.logUniform(0.6, 3.0);
                else // gyrochronology, Skumanich-like spin-down
                    s.rotationPeriodDays = clampd(
                        rs.logNormal10(26.0 * std::pow(std::max(s.ageGyr, 0.05) / 4.6, 0.52) *
                                       std::pow(1.0 / s.massSolar, 0.8), 0.1),
                        0.3, 200.0);
                break;
        }
        s.provenance["rotation_period_days"] = Provenance::Generated;
    }
    return s;
}

namespace
{

struct HierarchyBuilder
{
    int systemId = 0;
    std::uint64_t seed = 0;
    std::vector<StarDescriptor>& stars;
    std::vector<BarycenterDescriptor> nodes;
    std::vector<StabilityCheck>& checks;
    std::vector<std::string>& warnings;

    StarDescriptor* star(const std::string& key)
    {
        for (auto& s : stars)
            if (s.key == key)
                return &s;
        return nullptr;
    }

    double mass(const HierarchyNodeInput& n)
    {
        if (!n.starKey.empty())
        {
            const auto* s = star(n.starKey);
            return s ? s->massSolar : 0.0;
        }
        double m = 0.0;
        for (const auto& c : n.children)
            m += mass(c);
        return m;
    }

    std::string nodeBodyId(const std::string& id) const
    {
        return id == "system_barycenter"
            ? "system_" + std::to_string(systemId) + ".barycenter"
            : "system_" + std::to_string(systemId) + ".barycenter_" + id;
    }

    // Relative orbit of a binary node: observed values win, the rest sampled.
    KeplerOrbit relativeOrbit(const HierarchyNodeInput& n, double totalMass, int attempt,
                              ProvenanceMap& prov, bool& sampled)
    {
        RandomStream rs(seed, "binary_orbit_" + n.id, static_cast<std::uint64_t>(attempt));
        KeplerOrbit o;
        sampled = false;
        if (n.semiMajorAxisAu)
        {
            o.semiMajorAxisAu = *n.semiMajorAxisAu;
            prov["semi_major_axis_au"] = Provenance::Literature;
        }
        else if (n.periodYears)
        {
            o.semiMajorAxisAu = semiMajorAxisAuFromPeriod(*n.periodYears, totalMass);
            prov["semi_major_axis_au"] = Provenance::Derived;
        }
        else if (n.projectedSeparationAu)
        {
            // Mean deprojection of a random orbit phase / orientation (~1.26 s).
            o.semiMajorAxisAu = *n.projectedSeparationAu * rs.logNormal10(1.15, 0.12);
            prov["semi_major_axis_au"] = Provenance::Generated;
            sampled = true;
        }
        else
        {
            // Raghavan 2010: log10 P[d] ~ N(5.03, 2.28).
            const double logP = rs.truncatedNormal(5.03, 2.28, 0.0, 8.0);
            o.semiMajorAxisAu = semiMajorAxisAuFromPeriod(std::pow(10.0, logP) / 365.25, totalMass);
            prov["semi_major_axis_au"] = Provenance::Generated;
            sampled = true;
        }
        const double pYears = periodYearsFromAu(o.semiMajorAxisAu, totalMass);
        o.periodDays = pYears * 365.25;
        prov["period_days"] = Provenance::Derived;
        if (n.periodYears && n.semiMajorAxisAu)
        {
            const double ratio = *n.periodYears / pYears;
            if (std::abs(ratio - 1.0) > 0.25)
            {
                std::ostringstream w;
                w << "binary " << n.id << ": period " << *n.periodYears << " yr inconsistent with a="
                  << o.semiMajorAxisAu << " AU and M=" << totalMass << " (Kepler " << pYears
                  << " yr); the period is derived from a";
                warnings.push_back(w.str());
            }
        }

        if (n.eccentricity)
        {
            o.eccentricity = *n.eccentricity;
            prov["eccentricity"] = Provenance::Literature;
        }
        else
        {
            const double pDays = o.periodDays;
            const double eMax = pDays <= 2.0 ? 0.0 : 1.0 - std::pow(pDays / 2.0, -2.0 / 3.0);
            o.eccentricity = pDays < 10.0 ? rs.uniform(0.0, 0.05)
                : clampd(rs.powerLaw(0.4, 0.0001, 1.0) * eMax, 0.0, 0.95);
            prov["eccentricity"] = Provenance::Generated;
            sampled = true;
        }
        if (n.inclinationDeg)
        {
            o.inclinationDeg = *n.inclinationDeg;
            prov["inclination_deg"] = Provenance::Literature;
        }
        else
        {
            // Inclination to the planet plane: close binaries tend to align.
            o.inclinationDeg = o.semiMajorAxisAu < 50.0
                ? std::min(rs.rayleigh(12.0), 60.0)
                : rs.isotropicInclinationRad() * astro::RadToDeg;
            prov["inclination_deg"] = Provenance::Generated;
        }
        o.longitudeAscendingNodeDeg = rs.uniform(0.0, 360.0);
        o.argumentOfPeriapsisDeg = rs.uniform(0.0, 360.0);
        o.meanAnomalyAtEpochDeg = rs.uniform(0.0, 360.0);
        o.direction = o.inclinationDeg > 90.0 ? -1 : 1;
        prov["orientation"] = Provenance::Generated;
        return o;
    }

    // Returns this subtree's node/star id.
    std::string build(const HierarchyNodeInput& n, const std::string& parent)
    {
        if (!n.starKey.empty())
        {
            if (auto* s = star(n.starKey))
                s->parentNode = parent;
            return n.starKey;
        }
        BarycenterDescriptor b;
        b.id = n.id;
        b.bodyId = nodeBodyId(n.id);
        b.parentNode = parent;
        b.massSolar = mass(n);
        const std::size_t index = nodes.size();
        nodes.push_back(b);

        std::vector<std::string> childIds;
        for (const auto& c : n.children)
            childIds.push_back(build(c, n.id));
        nodes[index].children = childIds;

        if (n.children.size() != 2)
        {
            warnings.push_back("hierarchy node " + n.id + " does not have exactly two children");
            return n.id;
        }

        const double m1 = mass(n.children[0]);
        const double m2 = mass(n.children[1]);
        ProvenanceMap prov;
        bool sampled = false;
        KeplerOrbit rel;
        // Hierarchical-triple gate: resample sampled outer orbits until stable.
        for (int attempt = 0; attempt < 48; ++attempt)
        {
            prov.clear();
            rel = relativeOrbit(n, m1 + m2, attempt, prov, sampled);
            const KeplerOrbit* inner = nullptr;
            double innerMass = 0.0, outerMass = 0.0;
            for (int k = 0; k < 2; ++k)
            {
                if (!n.children[k].starKey.empty())
                    continue;
                for (const auto& nd : nodes)
                    if (nd.id == n.children[k].id && nd.relativeOrbit)
                    {
                        inner = &*nd.relativeOrbit;
                        innerMass = mass(n.children[k]);
                        outerMass = mass(n.children[1 - k]);
                    }
            }
            if (!inner)
                break;
            const double crit = mardlingAarsethCriticalRatio(
                outerMass / std::max(innerMass, 1e-6), rel.eccentricity,
                std::abs(rel.inclinationDeg - inner->inclinationDeg));
            const double ratio = rel.semiMajorAxisAu / inner->semiMajorAxisAu;
            const bool ok = ratio > crit;
            if (ok || !sampled || attempt == 47)
            {
                StabilityCheck c;
                c.name = "mardling_aarseth_" + n.id;
                c.passed = ok;
                std::ostringstream d;
                d << "a_out/a_in=" << ratio << " critical=" << crit;
                c.detail = d.str();
                checks.push_back(c);
                if (!ok)
                    warnings.push_back("hierarchy " + n.id + " fails Mardling-Aarseth: " + c.detail);
                break;
            }
        }
        nodes[index].relativeOrbit = rel;
        nodes[index].provenance = prov;

        // Each child orbits the node barycenter on the scaled relative orbit.
        for (int k = 0; k < 2; ++k)
        {
            KeplerOrbit o = rel;
            const double mOther = k == 0 ? m2 : m1;
            o.semiMajorAxisAu = rel.semiMajorAxisAu * mOther / (m1 + m2);
            if (k == 0)
                o.argumentOfPeriapsisDeg = std::fmod(rel.argumentOfPeriapsisDeg + 180.0, 360.0);
            const std::string& id = childIds[static_cast<std::size_t>(k)];
            if (auto* s = star(id))
                s->orbit = o;
            for (auto& nd : nodes)
                if (nd.id == id)
                    nd.orbit = o;
        }
        return n.id;
    }

    const BarycenterDescriptor* node(const std::string& id) const
    {
        for (const auto& n : nodes)
            if (n.id == id)
                return &n;
        return nullptr;
    }

    double subtreeMass(const std::string& id)
    {
        if (const auto* s = star(id))
            return s->massSolar;
        if (const auto* n = node(id))
            return n->massSolar;
        return 0.0;
    }

    // Outer stability limit for a body orbiting `id` (star or node) alone.
    double outerLimit(const std::string& id, std::string parent)
    {
        double limit = 1.0e5; // galactic-tide scale
        std::string child = id;
        while (!parent.empty())
        {
            const auto* p = node(parent);
            if (!p || !p->relativeOrbit)
                break;
            const std::string other = p->children.size() == 2
                ? (p->children[0] == child ? p->children[1] : p->children[0]) : "";
            const double mc = subtreeMass(child);
            const double mo = subtreeMass(other);
            const double mu = mo / std::max(mc + mo, 1e-9);
            limit = std::min(limit, holmanWiegertSTypeCritical(mu, p->relativeOrbit->eccentricity) *
                                        p->relativeOrbit->semiMajorAxisAu);
            child = parent;
            parent = p->parentNode;
        }
        return limit;
    }
};

} // namespace

std::vector<BarycenterDescriptor> buildStellarHierarchy(
    const HierarchyNodeInput& root,
    std::vector<StarDescriptor>& stars,
    int systemId,
    std::uint64_t systemSeed,
    std::vector<StabilityCheck>& checks,
    std::vector<std::string>& warnings)
{
    HierarchyBuilder b {systemId, systemSeed, stars, {}, checks, warnings};
    if (root.starKey.empty())
        b.build(root, "");
    else if (auto* s = b.star(root.starKey))
        s->parentNode.clear();

    for (auto& s : stars)
    {
        s.stableZoneInnerAu = 0.0;
        s.stableZoneOuterAu = b.outerLimit(s.key, s.parentNode);
    }
    for (auto& n : b.nodes)
    {
        if (n.relativeOrbit && n.children.size() == 2)
        {
            const double m1 = b.subtreeMass(n.children[0]);
            const double m2 = b.subtreeMass(n.children[1]);
            const double mu = std::min(m1, m2) / (m1 + m2);
            n.circumbinaryInnerAu = holmanWiegertPTypeCritical(mu, n.relativeOrbit->eccentricity) *
                n.relativeOrbit->semiMajorAxisAu;
        }
    }
    return b.nodes;
}

} // namespace world::generation
