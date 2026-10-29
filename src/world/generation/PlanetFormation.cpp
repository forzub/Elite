#include "src/world/generation/PlanetFormation.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/StellarPhysics.h"

namespace world::generation
{

namespace
{

constexpr double MePerSun = astro::EarthMassesPerSunMass;
constexpr double SigmaMmsnAt1Au = 0.262;  // rock, Earth masses per AU^2 (~7 g/cm^2)
constexpr double EmbryoSpacingHill = 10.0;
constexpr double MinPlanetMassEarth = 0.05;

double clampd(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

struct DiskModel
{
    DiskDescriptor d;
    double sigma0 = 0.0;      // rock surface density scale, Earth masses / AU
    double sigmaDrift = 0.0;  // drifted solids, sigma = s / r^2 inside driftOuterAu
    double driftOuterAu = 0.0;
    double iceFactor = 2.2;   // solids enhancement beyond the snow line
    double hostMassSolar = 1.0;
    double outerAu = 100.0;

    double sigma(double r) const
    {
        const double s = sigma0 / r * std::exp(-r / d.characteristicRadiusAu);
        if (r > d.snowLineAu)
            return s * iceFactor;
        return r < driftOuterAu ? s + sigmaDrift / (r * r) : s;
    }

    double isolationMass(double r) const
    {
        const double x = astro::TwoPi * EmbryoSpacingHill * r * r * sigma(r);
        return std::pow(x, 1.5) / std::sqrt(3.0 * hostMassSolar * MePerSun);
    }

    // Time to reach isolation (Myr): faster in denser disks, slower outside.
    double growthTimeMyr(double r) const
    {
        const double ref = SigmaMmsnAt1Au / r;
        return 0.2 * r * r * (ref / std::max(sigma(r), 1e-12)) / std::sqrt(hostMassSolar);
    }

    // Rock mass between two radii (with the drifted inner solids).
    double rockMass(double a, double b) const
    {
        const double rc = d.characteristicRadiusAu;
        const double lo = std::max(a, d.innerEdgeAu), hi = std::min(b, driftOuterAu);
        const double drift = hi > lo ? astro::TwoPi * sigmaDrift * std::log(hi / lo) : 0.0;
        return astro::TwoPi * sigma0 * rc * (std::exp(-a / rc) - std::exp(-b / rc)) + drift;
    }

    double solidMass(double a, double b) const
    {
        if (b <= a) return 0.0;
        const double s = d.snowLineAu;
        if (b <= s) return rockMass(a, b);
        if (a >= s) return iceFactor * rockMass(a, b);
        return rockMass(a, s) + iceFactor * rockMass(s, b);
    }
};

DiskModel buildDisk(const PlanetHost& host, const GalacticContextDescriptor& ctx, RandomStream& rs)
{
    DiskModel m;
    DiskDescriptor& d = m.d;
    d.hostNode = host.nodeId;
    const double mStar = host.initialMassSolar;
    m.hostMassSolar = host.massSolar;

    // Disk-to-star mass ratio: ~1 % median, 0.45 dex scatter (Andrews 2013, Pascucci 2016).
    const double ratio = clampd(rs.logNormal10(0.01 * std::pow(mStar, 0.5), 0.45), 1.0e-4, 0.12);
    d.gasMassSolar = ratio * mStar;
    const double z = 0.0134 * std::pow(10.0, host.metallicityFeH);
    d.dustToGas = z;
    const double rockTotalEarth = d.gasMassSolar * MePerSun * z * 0.45;

    d.characteristicRadiusAu = clampd(rs.logNormal10(40.0 * std::pow(mStar, 0.5), 0.25), 5.0, 200.0);
    // External photoevaporation in a dense birth cluster truncates the disk.
    const double photoEdge = 250.0 / std::sqrt(std::max(ctx.formationEnvironmentDensity, 0.05));
    d.outerTruncationAu = std::min(photoEdge, host.outerLimitAu > 0.0 ? host.outerLimitAu : 1e9);
    m.outerAu = std::min(d.outerTruncationAu, 4.0 * d.characteristicRadiusAu);

    d.innerEdgeAu = std::max(0.034 * std::sqrt(host.zamsLuminositySolar),
                             std::max(host.innerLimitAu, 0.006));
    d.snowLineAu = std::max(2.7 * mStar * mStar, 0.3 * std::sqrt(mStar));
    if (host.circumbinary)
        d.snowLineAu = std::max(d.snowLineAu, 0.8 * std::sqrt(host.zamsLuminositySolar) * 2.7);
    d.lifetimeMyr = clampd(
        rs.logNormal10(3.0 * std::pow(mStar, -0.25) *
                           std::pow(std::max(ctx.formationEnvironmentDensity, 0.05), -0.3), 0.25),
        0.4, 15.0);
    d.turbulenceAlpha = rs.logUniform(1.0e-4, 1.0e-2);
    m.iceFactor = rs.uniform(1.8, 2.6);

    // Normalise the rock surface density to the rock budget inside the disk.
    const double rc = d.characteristicRadiusAu;
    const double unit = astro::TwoPi * rc * (std::exp(-d.innerEdgeAu / rc) - std::exp(-m.outerAu / rc));
    m.sigma0 = rockTotalEarth / std::max(unit, 1e-12);
    // Radial drift of pebbles enriches the inner disk (stronger in the compact
    // disks of low-mass stars); the drifted mass leaves the outer disk.
    const double driftShare = clampd(rs.uniform(0.01, 0.07) / mStar, 0.0, 0.5);
    // Icy pebbles sublimate at the snow line: only their rock arrives inside.
    // Plus the early (class 0/I) inner budget: Kepler planets carry ~2-5 M_earth
    // inside 1 AU nearly independent of stellar mass, more than the dust of
    // observed class II disks (Mulders et al. 2015, 2021): PopulationModel.
    const double earlyInnerBudget = rs.logNormal10(3.0 * std::pow(mStar, 0.1), 0.4) *
        std::pow(10.0, 0.5 * host.metallicityFeH);
    const double driftMass = driftShare * rockTotalEarth + earlyInnerBudget;
    if (d.snowLineAu > d.innerEdgeAu * 1.5)
    {
        m.sigma0 *= 1.0 - driftShare;
        // Drifted solids pile up in the inner disk (pressure trap near the
        // inner edge): equal mass per log radius over about one decade.
        m.driftOuterAu = std::min(d.snowLineAu, 12.0 * d.innerEdgeAu);
        m.sigmaDrift = driftMass / (astro::TwoPi * std::log(m.driftOuterAu / d.innerEdgeAu));
    }
    d.solidMassEarth = m.solidMass(d.innerEdgeAu, m.outerAu);

    const double score = std::log10(ratio / 0.01) + 0.5 * std::log10(d.lifetimeMyr / 3.0) +
        0.3 * std::log10(d.turbulenceAlpha / 1.0e-3) + (mStar < 0.5 ? 0.2 : 0.0) +
        rs.normal(0.0, 0.4);
    d.migrationRegime = score > 0.35 ? "strong" : score > -0.35 ? "moderate" : "weak";
    return m;
}

double hillRadiusAu(double aAu, double mEarth, double hostSolar)
{
    return aAu * std::cbrt(mEarth / (3.0 * hostSolar * MePerSun));
}

double periodDaysOf(double aAu, double hostSolar)
{
    return periodYearsFromAu(aAu, hostSolar) * 365.25;
}

// Detectability bound used to condition procedural planets on surveys.
struct DetectionLimit
{
    bool active = false;
    double kMs = 1.5;
    double maxPeriodDays = 7300.0;
    double sinSky = 1.0;
};

} // namespace

double mutualHillRadiusAu(double a1, double m1, double a2, double m2, double host)
{
    return 0.5 * (a1 + a2) * std::cbrt((m1 + m2) / (3.0 * host * MePerSun));
}

double radialVelocityAmplitudeMs(double massEarth, double periodDays, double hostSolar, double e)
{
    const double mj = massEarth / astro::EarthMassesPerJupiterMass;
    return 28.4329 * mj * std::pow(periodDays / 365.25, -1.0 / 3.0) *
        std::pow(hostSolar, -2.0 / 3.0) / std::sqrt(std::max(1.0 - e * e, 1e-6));
}

FormationResult formPlanets(
    const PlanetHost& host,
    const GalacticContextDescriptor& ctx,
    const std::vector<const ObservedBodyInput*>& anchors,
    bool observedComplete,
    std::vector<StabilityCheck>& checks,
    std::vector<std::string>& warnings)
{
    FormationResult best;
    const double M = host.massSolar;

    // Sky inclination shared by the host's planets (coplanar system).
    RandomStream skyRs(host.seed, "sky_inclination");
    const double cosSky = skyRs.uniform(0.0, 0.9);
    DetectionLimit det;
    det.sinSky = std::sqrt(1.0 - cosSky * cosSky);
    det.active = host.rvDetectionLimitMs > 0.0 && !observedComplete;
    det.kMs = host.rvDetectionLimitMs;
    det.maxPeriodDays = host.rvBaselineDays;

    // Observational anchors as fixed planets.
    std::vector<PlanetCandidate> anchorPlanets;
    for (std::size_t k = 0; k < anchors.size(); ++k)
    {
        const ObservedBodyInput& in = *anchors[k];
        if (in.kind == "moon")
            continue;
        PlanetCandidate p;
        p.anchor = true;
        p.anchorIndex = static_cast<int>(k);
        p.formationOrigin = "observed";
        RandomStream ars(host.seed, "anchor_" + in.designation);
        if (in.semiMajorAxisAu)
        {
            p.aAu = *in.semiMajorAxisAu;
            p.provenance["semi_major_axis_au"] = Provenance::Observed;
        }
        else if (in.periodDays)
        {
            p.aAu = semiMajorAxisAuFromPeriod(*in.periodDays / 365.25, M);
            p.provenance["semi_major_axis_au"] = Provenance::Derived;
        }
        if (in.massEarth)
        {
            p.massEarth = *in.massEarth;
            p.provenance["mass_earth"] = Provenance::Observed;
        }
        else if (in.minimumMassEarth)
        {
            p.massEarth = *in.minimumMassEarth / det.sinSky;
            p.provenance["mass_earth"] = Provenance::Derived; // m sin i / sampled sin i
        }
        else if (in.radiusEarth)
        {
            // Chen & Kipping 2017 inverse for small planets.
            const double r = *in.radiusEarth;
            p.massEarth = r < 1.23 ? std::pow(r, 1.0 / 0.279) : std::pow(r / 1.008, 1.0 / 0.589);
            p.provenance["mass_earth"] = Provenance::Derived;
        }
        p.e = in.eccentricity.value_or(std::min(ars.rayleigh(0.03), 0.3));
        p.provenance["eccentricity"] = in.eccentricity ? Provenance::Observed : Provenance::Generated;
        p.iDeg = in.inclinationDeg.value_or(ars.rayleigh(1.0));
        p.nodeDeg = in.longitudeAscendingNodeDeg.value_or(ars.uniform(0.0, 360.0));
        p.periDeg = in.argumentOfPeriapsisDeg.value_or(ars.uniform(0.0, 360.0));
        p.meanAnomalyDeg = in.meanAnomalyAtEpochDeg.value_or(ars.uniform(0.0, 360.0));
        p.giant = p.massEarth > 50.0;
        p.formationAu = p.aAu;
        p.coreMassEarth = p.massEarth;
        if (p.aAu <= 0.0 || p.massEarth <= 0.0)
        {
            warnings.push_back("anchor " + in.designation + " lacks orbit or mass; skipped");
            continue;
        }
        anchorPlanets.push_back(p);
    }

    const int maxAttempts = observedComplete ? 1 : 24;
    for (int attempt = 0; attempt < maxAttempts; ++attempt)
    {
        FormationResult r;
        r.attempts = attempt + 1;
        RandomStream rs(host.seed, "disk", static_cast<std::uint64_t>(attempt));
        DiskModel disk = buildDisk(host, ctx, rs);
        r.disk = disk.d;
        const DiskDescriptor& d = disk.d;
        std::vector<PlanetCandidate> planets;

        if (!observedComplete)
        {
            RandomStream fs(host.seed, "embryos", static_cast<std::uint64_t>(attempt));
            const double criticalCore = fs.logNormal10(10.0 * std::pow(10.0, 0.2 * -host.metallicityFeH), 0.15);
            const double rMax = std::min(disk.outerAu, 0.95 * host.outerLimitAu);
            double rr = std::max(d.innerEdgeAu, host.innerLimitAu * 1.05);
            int count = 0;
            double lastPebbleCoreAu = 0.0;
            const double gasRatio = d.gasMassSolar / host.initialMassSolar;
            while (rr < rMax && count < 600)
            {
                ++count;
                const double miso = disk.isolationMass(rr);
                const double tau = disk.growthTimeMyr(rr);
                const double g = clampd(d.lifetimeMyr / tau, 0.0, 1.0);
                double mass = miso * std::pow(g, 1.5) * fs.logNormal10(1.0, 0.1);
                double coreTime = g >= 1.0 ? tau : d.lifetimeMyr;
                // Pebble accretion beyond the snow line (Lambrechts & Johansen 2014):
                // a core filters the inward pebble flux up to the pebble isolation
                // mass; the next core needs room outside it. Faster with more metals.
                if (rr > 0.7 * d.snowLineAu && rr > 1.6 * lastPebbleCoreAu)
                {
                    const double tauPeb = 16.0 * (rr / 5.0) * (0.0134 / d.dustToGas) *
                        std::sqrt(0.01 / std::max(gasRatio, 1e-5)) / std::sqrt(host.initialMassSolar) *
                        fs.logNormal10(1.0, 0.3);
                    if (tauPeb < d.lifetimeMyr)
                    {
                        const double pebbleIso = 20.0 * std::pow(rr / 5.0, 0.75) * host.initialMassSolar;
                        const double grow = clampd((d.lifetimeMyr - tauPeb) / 0.5, 0.0, 1.0);
                        if (pebbleIso * grow > mass)
                        {
                            mass = pebbleIso * grow;
                            coreTime = tauPeb + 0.5 * grow;
                            lastPebbleCoreAu = rr;
                        }
                    }
                }
                const double step = std::max(EmbryoSpacingHill * hillRadiusAu(rr, std::max(miso, 1e-6), M), 0.002 * rr);
                if (mass >= 1.0e-3)
                {
                    PlanetCandidate p;
                    p.aAu = rr;
                    p.formationAu = rr;
                    p.coreMassEarth = mass;
                    p.massEarth = mass;
                    p.waterFraction = rr > d.snowLineAu ? fs.uniform(0.25, 0.5)
                        : rr > 0.7 * d.snowLineAu ? fs.uniform(0.01, 0.1) : fs.uniform(0.0, 0.005);
                    const double tLeft = d.lifetimeMyr - coreTime;
                    const double tKh = 100.0 * std::pow(std::max(mass, 0.1), -3.5); // Myr
                    if (mass >= criticalCore && tLeft > tKh)
                    {
                        p.giant = true;
                        const double median = 318.0 * std::pow(host.initialMassSolar, 1.0) *
                            std::sqrt((d.gasMassSolar / host.initialMassSolar) / 0.01) *
                            std::pow(std::max(tLeft, 0.1) / 2.0, 0.3);
                        p.massEarth = clampd(fs.logNormal10(median, 0.35), 30.0, 4500.0);
                        p.envelopeFraction = 1.0 - std::min(0.9, 57.9 * std::pow(p.massEarth / 318.0, 0.61) / p.massEarth);
                        p.provenance["formation"] = Provenance::Generated;
                    }
                    else if (mass > 0.5 && tLeft > 0.0)
                    {
                        const double coldBoost = rr > d.snowLineAu ? 1.5 : 1.0;
                        p.envelopeFraction = clampd(0.015 * std::pow(mass / 4.0, 1.2) *
                                                        std::pow(tLeft, 0.4) * coldBoost, 0.0, 0.25);
                        p.massEarth = mass / (1.0 - p.envelopeFraction);
                    }
                    planets.push_back(p);
                }
                rr += step;
            }
            r.outerPlanetesimalEdgeAu = rMax;

            // Giants clear their feeding zones.
            for (const auto& gp : std::vector<PlanetCandidate>(planets))
            {
                if (!gp.giant) continue;
                const double rh = hillRadiusAu(gp.aAu, gp.massEarth, M);
                planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
                    return !p.giant && std::abs(p.aAu - gp.aAu) < 5.0 * rh;
                }), planets.end());
            }
            // Adjacent giants inside each other's zones merge into the heavier one.
            std::sort(planets.begin(), planets.end(), [](auto& a, auto& b) { return a.aAu < b.aAu; });

            // Type I migration of cores while the gas is present.
            RandomStream ms(host.seed, "migration", static_cast<std::uint64_t>(attempt));
            const std::string& regime = d.migrationRegime;
            if (regime == "strong")
            {
                std::vector<PlanetCandidate*> migrators;
                for (auto& p : planets)
                    if (!p.giant && p.coreMassEarth > 0.5 && p.aAu < 3.0 * d.snowLineAu)
                        migrators.push_back(&p);
                double aPrev = d.innerEdgeAu * ms.uniform(1.0, 1.5);
                for (auto* p : migrators)
                {
                    p->aAu = aPrev;
                    p->resonantChain = true;
                    p->formationOrigin = "migrated";
                    const double u = ms.uniform();
                    const double ratio = (u < 0.3 ? 4.0 / 3.0 : u < 0.7 ? 1.5 : u < 0.85 ? 5.0 / 3.0 : 2.0) *
                        (1.0 + ms.normal(0.0, 0.01));
                    aPrev = aPrev * std::pow(ratio, 2.0 / 3.0);
                }
            }
            else
            {
                for (auto& p : planets)
                {
                    if (p.giant || p.coreMassEarth < 0.5)
                        continue;
                    const double f = regime == "moderate" ? ms.uniform(0.15, 0.7) : ms.uniform(0.7, 1.0);
                    p.aAu = std::max(p.aAu * f, d.innerEdgeAu * ms.uniform(1.0, 1.3));
                    if (f < 0.8)
                        p.formationOrigin = "migrated";
                }
            }
            // Type II migration of giants (hot and warm giants).
            const double pGiantMig = regime == "strong" ? 0.12 : regime == "moderate" ? 0.05 : 0.01;
            for (auto& g : planets)
            {
                if (!g.giant || !ms.chance(pGiantMig))
                    continue;
                const double a0 = g.aAu;
                g.aAu = std::max(d.innerEdgeAu * 1.2, a0 * ms.logUniform(0.005, 0.6));
                g.formationOrigin = "migrated";
                planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
                    return !p.giant && p.aAu > g.aAu * 0.75 && p.aAu < a0;
                }), planets.end());
            }
            std::sort(planets.begin(), planets.end(), [](auto& a, auto& b) { return a.aAu < b.aAu; });

            // Post-gas giant scattering.
            RandomStream ss(host.seed, "scattering", static_cast<std::uint64_t>(attempt));
            for (bool changed = true; changed;)
            {
                changed = false;
                std::vector<std::size_t> giants;
                for (std::size_t k = 0; k < planets.size(); ++k)
                    if (planets[k].giant) giants.push_back(k);
                for (std::size_t k = 0; k + 1 < giants.size(); ++k)
                {
                    auto& g1 = planets[giants[k]];
                    auto& g2 = planets[giants[k + 1]];
                    const double rh = mutualHillRadiusAu(g1.aAu, g1.massEarth, g2.aAu, g2.massEarth, M);
                    if ((g2.aAu - g1.aAu) / rh > 2.0 * std::sqrt(3.0) * 1.6)
                        continue;
                    if (ss.chance(0.5))
                    {
                        const std::size_t lost = g1.massEarth < g2.massEarth ? giants[k] : giants[k + 1];
                        const std::size_t kept = lost == giants[k] ? giants[k + 1] : giants[k];
                        planets[kept].e = clampd(ss.rayleigh(0.3), 0.05, 0.85);
                        planets[kept].aAu *= 0.8;
                        planets[kept].iDeg = ss.rayleigh(8.0);
                        planets[kept].formationOrigin = "scattered";
                        PlanetCandidate ej = planets[lost];
                        ej.formationOrigin = "ejected";
                        r.ejected.push_back(ej);
                        planets.erase(planets.begin() + static_cast<std::ptrdiff_t>(lost));
                    }
                    else
                    {
                        g1.e = clampd(ss.rayleigh(0.2), 0.02, 0.7);
                        g2.e = clampd(ss.rayleigh(0.2), 0.02, 0.7);
                        g1.aAu *= 0.9;
                        g2.aAu *= 1.15;
                        g1.iDeg = ss.rayleigh(6.0);
                        g2.iDeg = ss.rayleigh(6.0);
                        g1.formationOrigin = g2.formationOrigin = "scattered";
                    }
                    changed = true;
                    break;
                }
            }

            // Anchors dominate their feeding zones.
            for (const auto& ap : anchorPlanets)
            {
                const double rh = hillRadiusAu(ap.aAu, ap.massEarth, M);
                const double clear = (ap.giant ? 10.0 : 8.0) * rh;
                planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
                    return std::abs(p.aAu - ap.aAu) < std::max(clear, 0.15 * ap.aAu) ||
                        (p.giant && std::abs(p.aAu - ap.aAu) < 0.5 * ap.aAu);
                }), planets.end());
            }
        }
        for (const auto& ap : anchorPlanets)
            planets.push_back(ap);
        std::sort(planets.begin(), planets.end(), [](auto& a, auto& b) { return a.aAu < b.aAu; });

        // Giant-impact consolidation of small bodies after the gas is gone.
        RandomStream cs(host.seed, "consolidation", static_cast<std::uint64_t>(attempt));
        // Final spacing after giant impacts: Kepler multis ~20 mutual Hill radii,
        // the inner Solar System ~25-40.
        const double deltaCrit = cs.truncatedNormal(20.0, 4.0, 12.0, 32.0);
        const bool chainSurvives = cs.chance(0.3);
        const double consolidationEdge = 3.0 * d.snowLineAu + 2.0;
        for (bool merged = true; merged;)
        {
            merged = false;
            double bestDelta = 1.0;
            std::size_t bestK = planets.size();
            for (std::size_t k = 0; k + 1 < planets.size(); ++k)
            {
                const auto& p1 = planets[k];
                const auto& p2 = planets[k + 1];
                if (p1.anchor || p2.anchor || p1.giant || p2.giant)
                    continue;
                // Cores that stayed gas-poor do not merge into gas-free mega-Earths.
                if (p1.massEarth + p2.massEarth > 25.0)
                    continue;
                if (p2.aAu > consolidationEdge)
                    continue;
                if (chainSurvives && p1.resonantChain && p2.resonantChain)
                    continue;
                const double delta = (p2.aAu - p1.aAu) /
                    mutualHillRadiusAu(p1.aAu, p1.massEarth, p2.aAu, p2.massEarth, M);
                // Small embryos keep colliding to wider spacings (N-body terrestrial
                // outcomes ~30-40 mutual Hill radii, Chambers 2001).
                const double meanMass = 0.5 * (p1.massEarth + p2.massEarth);
                const double need = deltaCrit * (1.0 + 1.5 * clampd(std::log10(1.0 / meanMass), 0.0, 2.0));
                if (delta < need && delta / need < bestDelta)
                {
                    bestDelta = delta / need;
                    bestK = k;
                }
            }
            if (bestK < planets.size())
            {
                auto& p1 = planets[bestK];
                const auto& p2 = planets[bestK + 1];
                const double m = p1.massEarth + p2.massEarth;
                p1.aAu = (p1.aAu * p1.massEarth + p2.aAu * p2.massEarth) / m;
                p1.formationAu = (p1.formationAu * p1.massEarth + p2.formationAu * p2.massEarth) / m;
                p1.waterFraction = (p1.waterFraction * p1.massEarth + p2.waterFraction * p2.massEarth) / m;
                p1.envelopeFraction = 0.5 * (p1.envelopeFraction * p1.massEarth +
                                             p2.envelopeFraction * p2.massEarth) / m;
                p1.coreMassEarth += p2.coreMassEarth;
                p1.massEarth = m;
                p1.resonantChain = p1.resonantChain && p2.resonantChain;
                planets.erase(planets.begin() + static_cast<std::ptrdiff_t>(bestK + 1));
                merged = true;
            }
        }

        // Leftover solids and sub-planetary bodies become reservoirs.
        double innerLeft = 0.0, outerLeft = 0.0;
        double outermostPlanet = 0.0;
        for (const auto& p : planets)
            if (p.massEarth >= MinPlanetMassEarth)
                outermostPlanet = std::max(outermostPlanet, p.aAu);
        // Beyond the consolidation zone sub-Earth embryos never finish growing:
        // they stay a planetesimal population (outer belt), not planets.
        planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
            if (p.anchor || p.giant)
                return false;
            if (p.aAu > consolidationEdge && p.massEarth < 1.0)
            {
                outerLeft += p.massEarth;
                return true;
            }
            return false;
        }), planets.end());
        outermostPlanet = 0.0;
        for (const auto& p : planets)
            if (p.massEarth >= MinPlanetMassEarth)
                outermostPlanet = std::max(outermostPlanet, p.aAu);
        planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
            if (p.anchor || p.massEarth >= MinPlanetMassEarth)
                return false;
            (p.aAu > outermostPlanet ? outerLeft : innerLeft) += p.massEarth;
            return true;
        }), planets.end());
        r.leftoverInnerSolidsEarth = innerLeft;
        r.leftoverOuterSolidsEarth = outerLeft +
            (observedComplete ? 0.0 : disk.solidMass(std::max(outermostPlanet * 1.5, d.innerEdgeAu), disk.outerAu) * 0.3);

        // Final eccentricities and inclinations of the quiet planets.
        RandomStream es(host.seed, "orbits", static_cast<std::uint64_t>(attempt));
        const std::size_t n = planets.size();
        const double sigmaE = n >= 4 ? 0.02 : n >= 2 ? 0.04 : 0.08;
        const double sigmaI = n >= 4 ? 1.2 : 2.5;
        for (auto& p : planets)
        {
            if (p.anchor)
                continue;
            if (p.formationOrigin != "scattered")
            {
                p.e = p.resonantChain ? es.rayleigh(0.01) : p.giant ? es.rayleigh(0.06) : es.rayleigh(sigmaE);
                p.iDeg = p.resonantChain ? es.rayleigh(0.3) : es.rayleigh(sigmaI);
            }
            if (p.aAu < 0.05)
                p.e *= 0.1; // tidal circularisation
            p.e = clampd(p.e, 0.0, 0.9);
            p.nodeDeg = es.uniform(0.0, 360.0);
            p.periDeg = es.uniform(0.0, 360.0);
            p.meanAnomalyDeg = es.uniform(0.0, 360.0);
            p.provenance["orbit"] = Provenance::Generated;
            p.provenance["mass_earth"] = Provenance::Generated;
        }

        // Stellar evolution after formation: engulfment and orbital expansion.
        if (host.stage == StellarStage::WhiteDwarf || host.stage == StellarStage::RedGiant)
        {
            const bool wd = host.stage == StellarStage::WhiteDwarf;
            const double engulf = wd ? 1.5 * std::pow(host.initialMassSolar, 0.8)
                                     : 3.0 * host.radiusSolar * astro::SunRadiusM / astro::AuMeters;
            const double expand = host.initialMassSolar / host.massSolar;
            planets.erase(std::remove_if(planets.begin(), planets.end(), [&](const PlanetCandidate& p) {
                return !p.anchor && p.aAu < engulf;
            }), planets.end());
            for (auto& p : planets)
                if (!p.anchor && wd)
                    p.aAu *= expand;
        }

        // Stability gates with repair (the lighter procedural body is lost).
        std::vector<StabilityCheck> local;
        const double surfaceAu = std::max(3.0 * host.radiusSolar * astro::SunRadiusM / astro::AuMeters, 0.004);
        for (bool repaired = true; repaired;)
        {
            repaired = false;
            std::sort(planets.begin(), planets.end(), [](auto& a, auto& b) { return a.aAu < b.aAu; });
            for (std::size_t k = 0; k < planets.size() && !repaired; ++k)
            {
                auto& p = planets[k];
                const double peri = p.aAu * (1.0 - p.e);
                const double apo = p.aAu * (1.0 + p.e);
                const bool inZone = apo < 0.95 * host.outerLimitAu && peri > std::max(surfaceAu, host.innerLimitAu);
                if (!inZone)
                {
                    if (!p.anchor)
                    {
                        planets.erase(planets.begin() + static_cast<std::ptrdiff_t>(k));
                        repaired = true;
                    }
                    else
                    {
                        std::ostringstream s;
                        s << "anchor at a=" << p.aAu << " AU outside the stable zone (" << host.innerLimitAu
                          << ".." << host.outerLimitAu << " AU)";
                        local.push_back({"host_zone_anchor", false, s.str()});
                    }
                    continue;
                }
                if (k + 1 >= planets.size())
                    continue;
                auto& q = planets[k + 1];
                const double rh = mutualHillRadiusAu(p.aAu, p.massEarth, q.aAu, q.massEarth, M);
                const double delta = (q.aAu - p.aAu) / rh;
                const double pr = std::pow(q.aAu / p.aAu, 1.5);
                const bool nearResonance = std::abs(pr - 1.5) < 0.03 || std::abs(pr - 4.0 / 3.0) < 0.03 ||
                    std::abs(pr - 2.0) < 0.04 || std::abs(pr - 5.0 / 3.0) < 0.03;
                const double need = planets.size() >= 3 ? (nearResonance ? 5.0 : 8.0) : 2.0 * std::sqrt(3.0);
                const bool crossing = apo + 2.0 * rh > q.aAu * (1.0 - q.e);
                if (delta >= need && !crossing)
                    continue;
                if (p.anchor && q.anchor)
                {
                    std::ostringstream s;
                    s << "observed pair at " << p.aAu << " / " << q.aAu << " AU: delta=" << delta;
                    local.push_back({"observed_pair_spacing", delta >= 2.0 * std::sqrt(3.0), s.str()});
                    continue;
                }
                const std::size_t lose = p.anchor ? k + 1 : q.anchor ? k : (p.massEarth < q.massEarth ? k : k + 1);
                planets.erase(planets.begin() + static_cast<std::ptrdiff_t>(lose));
                repaired = true;
            }
        }

        // Observational conditioning: no procedural planet a survey would have found.
        bool detectable = false;
        if (det.active)
        {
            for (const auto& p : planets)
            {
                if (p.anchor)
                    continue;
                const double period = periodDaysOf(p.aAu, M);
                if (period < det.maxPeriodDays &&
                    radialVelocityAmplitudeMs(p.massEarth, period, M, p.e) * det.sinSky > det.kMs)
                    detectable = true;
            }
        }

        r.planets = planets;
        if (!detectable || attempt + 1 == maxAttempts)
        {
            if (detectable)
            {
                const double kMs = det.kMs;
                r.planets.erase(std::remove_if(r.planets.begin(), r.planets.end(), [&](const PlanetCandidate& p) {
                    const double period = periodDaysOf(p.aAu, M);
                    return !p.anchor && period < det.maxPeriodDays &&
                        radialVelocityAmplitudeMs(p.massEarth, period, M, p.e) * det.sinSky > kMs;
                }), r.planets.end());
                warnings.push_back("host " + host.nodeId + ": detectable procedural planets removed after " +
                                   std::to_string(maxAttempts) + " attempts");
            }
            std::ostringstream s;
            s << "planets=" << r.planets.size() << " attempts=" << r.attempts
              << " rv_limit=" << (det.active ? det.kMs : 0.0) << "m/s";
            local.push_back({"architecture_" + host.nodeId, true, s.str()});
            checks.insert(checks.end(), local.begin(), local.end());
            best = r;
            break;
        }
    }

    // Architecture mode label, derived from the result.
    const auto& ps = best.planets;
    int small = 0, smallInner = 0, envelopes = 0;
    bool hotGiant = false, coldGiant = false, eccentricGiant = false;
    for (const auto& p : ps)
    {
        if (p.giant || p.massEarth > 50.0)
        {
            if (p.aAu < 1.0) hotGiant = true; else coldGiant = true;
            if (p.e > 0.3) eccentricGiant = true;
        }
        else
        {
            ++small;
            if (p.aAu < 1.0) ++smallInner;
            if (p.envelopeFraction > 0.005) ++envelopes;
        }
    }
    std::string mode;
    if (host.circumbinary) mode = "circumbinary";
    else if (host.outerLimitAu < 50.0) mode = "binary_truncated";
    else if (ps.empty()) mode = "sparse";
    else if (hotGiant) mode = "hot_warm_giant_disturbed";
    else if (coldGiant && eccentricGiant) mode = "eccentric_cold_giants";
    else if (coldGiant && smallInner > 0) mode = "cold_giant_inner_small";
    else if (coldGiant) mode = "wide_giant";
    else if (host.massSolar < 0.6 && smallInner >= 3) mode = "m_dwarf_compact";
    else if (smallInner >= 3) mode = envelopes > 0 ? "compact_mixed" : "compact_rocky";
    else mode = "sparse";
    best.architectureMode = mode;
    return best;
}

} // namespace world::generation
