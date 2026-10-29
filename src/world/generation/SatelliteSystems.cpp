#include "src/world/generation/SatelliteSystems.h"

#include <algorithm>
#include <cmath>
#include <sstream>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/GenerationRandom.h"

namespace world::generation
{

namespace
{

using namespace astro;

double clampd(double v, double lo, double hi)
{
    return std::max(lo, std::min(hi, v));
}

bool isGiant(const BodyDescriptor& b)
{
    return b.bodyClass == "gas_giant" || b.bodyClass == "super_jupiter";
}

double planetHillKm(const BodyDescriptor& p, double hostMassKg)
{
    const double a = p.orbit.semiMajorAxisAu * AuMeters * (1.0 - p.orbit.eccentricity);
    return a * std::cbrt(p.massKg / (3.0 * std::max(hostMassKg, 1.0))) / 1000.0;
}

// N(>1 km) for dN/dD ~ D^q given the reservoir mass (rho ~ 2000 kg/m^3).
double bodiesAbove1Km(double massEarth, double q, double dMinKm, double dMaxKm)
{
    const double rho = 2000.0;
    const double massKg = massEarth * EarthMassKg;
    // mass = C * pi/6 rho * int D^(3+q) dD   (D in metres)
    const double p = 4.0 + q;
    const double dMin = dMinKm * 1000.0, dMax = dMaxKm * 1000.0;
    const double massIntegral = std::abs(p) < 1e-9 ? std::log(dMax / dMin)
        : (std::pow(dMax, p) - std::pow(dMin, p)) / p;
    const double c = massKg / (Pi / 6.0 * rho * massIntegral);
    const double n1 = q + 1.0;
    const double d1 = std::max(1000.0, dMin);
    return std::max(0.0, c * (std::pow(dMax, n1) - std::pow(d1, n1)) / n1);
}

} // namespace

std::string romanNumeral(int n)
{
    static const std::pair<int, const char*> table[] = {
        {1000, "M"}, {900, "CM"}, {500, "D"}, {400, "CD"}, {100, "C"}, {90, "XC"},
        {50, "L"}, {40, "XL"}, {10, "X"}, {9, "IX"}, {5, "V"}, {4, "IV"}, {1, "I"}};
    std::string s;
    for (const auto& [v, sym] : table)
        while (n >= v) { s += sym; n -= v; }
    return s;
}

std::vector<MoonCandidate> generateMoons(
    const BodyDescriptor& p,
    double hostMassKg,
    double snowLineAu,
    double ageGyr,
    std::uint64_t seed,
    std::vector<StabilityCheck>& checks)
{
    RandomStream rs(seed, "moons");
    std::vector<MoonCandidate> moons;
    const double rp = p.radiusKm;
    const double mpEarth = p.massKg / EarthMassKg;
    const double hill = planetHillKm(p, hostMassKg);
    const double aMaxPro = 0.4 * hill;
    const double aMaxRetro = 0.6 * hill;
    const double rochePrefactor = 2.44 * rp * std::cbrt(p.densityKgM3);
    auto roche = [&](double rhoMoon) { return rochePrefactor / std::cbrt(rhoMoon); };
    if (aMaxPro < 3.0 * rp)
    {
        checks.push_back({"moons_" + p.designation, true, "Hill sphere too small for moons"});
        return moons;
    }
    const bool cold = p.formationDistanceAu > snowLineAu;

    if (isGiant(p) || p.bodyClass == "ice_giant")
    {
        const bool ice = p.bodyClass == "ice_giant";
        bool captured = false;
        if (ice && rs.chance(0.15))
        {
            MoonCandidate t;
            t.massEarth = rs.logUniform(1.0e-3, 5.0e-3);
            t.aKm = rp * rs.uniform(10.0, 20.0);
            t.e = rs.uniform(0.0, 0.02);
            t.iDeg = rs.uniform(140.0, 165.0);
            t.retrograde = true;
            t.waterFraction = rs.uniform(0.15, 0.3);
            t.origin = "captured";
            moons.push_back(t);
            captured = true;
        }
        const int n = std::clamp(rs.poisson(ice ? 2.5 : 3.0) + 1, 1, ice ? 5 : 6);
        const double total = 1.0e-4 * mpEarth * rs.logNormal10(1.0, 0.3) * (captured ? 0.01 : 1.0);
        std::vector<double> w;
        double ws = 0.0;
        for (int k = 0; k < n; ++k) { w.push_back(rs.uniform(0.2, 1.0)); ws += w.back(); }
        double a = rp * rs.uniform(4.0, 8.0);
        const double regularEdge = std::min(0.05 * hill, rp * (ice ? 60.0 : 40.0));
        for (int k = 0; k < n && a < regularEdge; ++k)
        {
            if (captured && k >= 2) break; // a captured giant moon disrupts the outer regulars
            MoonCandidate m;
            m.massEarth = total * w[static_cast<std::size_t>(k)] / ws;
            m.aKm = a;
            m.e = rs.rayleigh(0.004);
            m.iDeg = rs.rayleigh(0.3);
            m.waterFraction = cold ? clampd(0.05 + 0.45 * (n > 1 ? double(k) / (n - 1) : 0.5), 0.0, 0.5) : 0.02;
            m.origin = "regular";
            moons.push_back(m);
            a *= rs.uniform(1.5, 2.5);
        }
        const int irregular = std::min(rs.poisson(ice ? 1.5 : 2.0), 4);
        for (int k = 0; k < irregular; ++k)
        {
            MoonCandidate m;
            m.massEarth = rs.logUniform(1.0e-10, 1.0e-8) * mpEarth;
            m.retrograde = rs.chance(0.6);
            m.aKm = (m.retrograde ? aMaxRetro : aMaxPro) * rs.uniform(0.25, 0.9);
            m.e = rs.uniform(0.1, 0.5);
            m.iDeg = m.retrograde ? rs.uniform(140.0, 170.0) : rs.uniform(20.0, 50.0);
            m.waterFraction = cold ? rs.uniform(0.0, 0.3) : 0.0;
            m.origin = "captured";
            moons.push_back(m);
        }
    }
    else if (p.bodyClass == "sub_neptune")
    {
        if (rs.chance(0.1))
        {
            MoonCandidate m;
            m.massEarth = 1.0e-5 * mpEarth * rs.logNormal10(1.0, 0.5);
            m.aKm = rp * rs.uniform(5.0, 15.0);
            m.e = rs.rayleigh(0.01);
            m.iDeg = rs.rayleigh(1.0);
            m.waterFraction = cold ? 0.3 : 0.02;
            m.origin = "regular";
            moons.push_back(m);
        }
    }
    else if (mpEarth > 0.005)
    {
        if (mpEarth > 0.3 && mpEarth < 4.0 && rs.chance(0.2) && aMaxPro > 25.0 * rp)
        {
            MoonCandidate m;
            m.massEarth = mpEarth * rs.logUniform(0.002, 0.02);
            const double tidalA = (ageGyr < 1.0 ? rs.uniform(20.0, 40.0) : rs.uniform(30.0, 70.0)) * rp;
            m.aKm = std::min(tidalA, 0.45 * aMaxPro);
            m.e = rs.rayleigh(0.03);
            m.iDeg = rs.rayleigh(5.0);
            m.waterFraction = 0.001;
            m.origin = "giant_impact";
            moons.push_back(m);
        }
        if (mpEarth < 0.6 && rs.chance(0.15))
        {
            const int n = rs.uniformInt(1, 2);
            for (int k = 0; k < n; ++k)
            {
                MoonCandidate m;
                m.massEarth = rs.logUniform(1.0e-9, 1.0e-8);
                m.aKm = rp * rs.uniform(2.5, 7.0);
                m.e = rs.rayleigh(0.01);
                m.iDeg = rs.rayleigh(1.0);
                m.waterFraction = 0.02;
                m.origin = "captured";
                moons.push_back(m);
            }
        }
    }

    // Gates: Roche, Hill, tidal decay, mutual spacing.
    std::sort(moons.begin(), moons.end(), [](auto& a, auto& b) { return a.aKm < b.aKm; });
    std::vector<MoonCandidate> kept;
    int removed = 0;
    for (const auto& m : moons)
    {
        const double rhoMoon = m.waterFraction > 0.1 ? 1500.0 : 3000.0;
        const double aMax = m.retrograde ? aMaxRetro : aMaxPro;
        bool ok = m.aKm > roche(rhoMoon) && m.aKm * (1.0 + m.e) < aMax;
        // A planet locked to its star spins slowly: heavy moons inside the
        // synchronous orbit spiral in within the age.
        if (ok && p.tidalLock != "none" && m.massEarth / mpEarth > 1.0e-6)
            ok = false;
        if (ok && !kept.empty())
        {
            const auto& prev = kept.back();
            const double rh = 0.5 * (m.aKm + prev.aKm) *
                std::cbrt((m.massEarth + prev.massEarth) / (3.0 * mpEarth));
            if ((m.aKm - prev.aKm) / std::max(rh, 1e-9) < 6.0 && !(m.retrograde != prev.retrograde))
                ok = false;
        }
        if (ok) kept.push_back(m); else ++removed;
    }
    if (removed > 0)
    {
        std::ostringstream s;
        s << removed << " moon candidates removed by Roche / Hill / tidal / spacing gates";
        checks.push_back({"moons_" + p.designation, true, s.str()});
    }
    return kept;
}

std::vector<RingDescriptor> generateRings(
    const BodyDescriptor& p,
    const std::vector<const BodyDescriptor*>& moons,
    double snowLineAu,
    std::uint64_t seed)
{
    RandomStream rs(seed, "rings");
    std::vector<RingDescriptor> rings;
    const double rp = p.radiusKm;
    const bool giant = isGiant(p);
    const bool ice = p.bodyClass == "ice_giant";
    const bool cold = p.formationDistanceAu > snowLineAu;
    const double rocheIce = 2.45 * rp * std::cbrt(p.densityKgM3 / 900.0);
    const double rocheRock = 2.45 * rp * std::cbrt(p.densityKgM3 / 3000.0);
    const double rocheLimit = cold ? rocheIce : rocheRock;
    const double hostDistance = p.orbit.semiMajorAxisAu;

    double pMain = giant ? clampd(0.25 * (1.0 + 0.15 * double(moons.size())), 0.0, 0.7)
        : ice ? 0.4 : p.bodyClass == "sub_neptune" ? 0.05 : 0.02;
    if (hostDistance < 0.1) pMain *= 0.1; // stellar heating / drag removes rings close in
    if (rs.chance(pMain) && rocheLimit > 1.25 * rp)
    {
        RingDescriptor r;
        r.id = "main";
        r.name = "main";
        r.innerRadiusKm = rp * rs.uniform(1.1, 1.3);
        const double width = ice ? rp * rs.uniform(0.05, 0.25) : rp * rs.uniform(0.3, 1.2);
        r.outerRadiusKm = std::min(rocheLimit, r.innerRadiusKm + width);
        if (r.outerRadiusKm > r.innerRadiusKm * 1.03)
        {
            r.opticalDepth = giant ? rs.logUniform(0.05, 2.0) : rs.logUniform(0.05, 0.6);
            r.ageMyr = rs.logUniform(10.0, 4000.0);
            r.iceFraction = cold ? rs.uniform(0.85, 0.99) * (r.ageMyr > 1000.0 ? 0.8 : 1.0) : rs.uniform(0.0, 0.2);
            r.dustFraction = rs.uniform(0.0, 0.05);
            r.rockFraction = std::max(0.0, 1.0 - r.iceFraction - r.dustFraction);
            const double area = Pi * (r.outerRadiusKm * r.outerRadiusKm - r.innerRadiusKm * r.innerRadiusKm) * 1.0e6;
            r.massKg = 500.0 * r.opticalDepth * area;
            r.origin = rs.chance(0.6) ? "disrupted_moon" : "collisional";
            r.visibilityClass = r.opticalDepth > 0.3 ? "main" : "secondary";
            r.planeInclinationDeg = 0.0;
            for (const auto* m : moons)
            {
                const double aRes = m->orbit.semiMajorAxisAu * AuMeters / 1000.0 * std::pow(0.5, 2.0 / 3.0);
                if (aRes > r.innerRadiusKm && aRes < r.outerRadiusKm)
                    r.gapsKm.push_back({aRes - 0.01 * rp, aRes + 0.01 * rp});
            }
            rings.push_back(r);
        }
    }
    if ((giant || ice) && rs.chance(0.8))
    {
        RingDescriptor r;
        r.id = "faint_dust";
        r.name = "faint dust";
        r.innerRadiusKm = rp * rs.uniform(1.3, 2.0);
        r.outerRadiusKm = rp * rs.uniform(2.5, 4.0);
        r.opticalDepth = rs.logUniform(1.0e-6, 1.0e-4);
        r.dustFraction = 0.9;
        r.rockFraction = 0.1;
        r.ageMyr = rs.logUniform(1.0, 100.0);
        const double area = Pi * (r.outerRadiusKm * r.outerRadiusKm - r.innerRadiusKm * r.innerRadiusKm) * 1.0e6;
        r.massKg = 500.0 * r.opticalDepth * area;
        r.origin = "dusty";
        r.visibilityClass = "faint";
        if (rings.empty() || r.innerRadiusKm > rings.back().outerRadiusKm)
            rings.push_back(r);
    }
    return rings;
}

void generateReservoirs(
    const ReservoirHostInput& h,
    const std::vector<const BodyDescriptor*>& planets,
    std::vector<ReservoirDescriptor>& out,
    std::vector<DwarfPlanetCandidate>& dwarfs)
{
    RandomStream rs(h.seed, "reservoirs");
    const double hostMassKg = h.hostMassSolar * SunMassKg;
    auto zoneFree = [&](double lo, double hi) {
        for (const auto* p : planets)
        {
            const double a = p->orbit.semiMajorAxisAu;
            if (a > lo && a < hi) return false;
        }
        return true;
    };

    const BodyDescriptor* firstGiant = nullptr;
    double giantMass = 0.0;
    double outermost = 0.0;
    for (const auto* p : planets)
    {
        outermost = std::max(outermost, p->orbit.semiMajorAxisAu * (1.0 + p->orbit.eccentricity));
        if (p->massKg / EarthMassKg > 50.0)
        {
            giantMass += p->massKg / EarthMassKg;
            if (!firstGiant && p->orbit.semiMajorAxisAu > 0.8 * h.snowLineAu)
                firstGiant = p;
        }
    }

    // Inner asteroid belt sculpted by the first cold giant (Kirkwood gaps).
    if (firstGiant)
    {
        const double ag = firstGiant->orbit.semiMajorAxisAu;
        double lo = ag * 0.40, hi = ag * 0.72;
        for (const auto* p : planets)
        {
            const double a = p->orbit.semiMajorAxisAu;
            if (a < ag && a > lo && a < hi) lo = std::max(lo, a * 1.3);
        }
        if (hi > lo * 1.15 && zoneFree(lo, hi))
        {
            ReservoirDescriptor r;
            r.id = "asteroid_belt";
            r.name = h.hostName + " asteroid belt";
            r.kind = "asteroid_belt";
            r.parentId = h.hostBodyId;
            r.seed = deriveSeed(h.seed, "asteroid_belt");
            r.innerAu = lo;
            r.outerAu = hi;
            r.peakAu = 0.5 * (lo + hi);
            r.totalMassEarth = clampd((h.leftoverInnerSolidsEarth + 0.05) * rs.logUniform(0.005, 0.05), 1.0e-5, 0.05);
            r.eccentricityMean = rs.uniform(0.07, 0.2);
            r.inclinationMeanDeg = rs.uniform(4.0, 12.0);
            r.sizeSlope = -3.5;
            r.minBodyKm = 0.001;
            r.maxBodyKm = clampd(900.0 * std::cbrt(r.totalMassEarth / 4.5e-4), 50.0, 1500.0);
            r.clumpiness = rs.uniform(0.05, 0.3);
            const double snowRatio = r.peakAu / h.snowLineAu;
            r.composition = snowRatio < 1.0
                ? std::vector<std::pair<std::string, double>>{{"silicate_s_type", 0.5}, {"carbonaceous_c_type", 0.3}, {"metallic_m_type", 0.15}, {"water_ice", 0.05}}
                : std::vector<std::pair<std::string, double>>{{"carbonaceous_c_type", 0.55}, {"silicate_s_type", 0.2}, {"water_ice", 0.2}, {"metallic_m_type", 0.05}};
            for (const auto& [j, k] : std::vector<std::pair<double, double>>{{3, 1}, {5, 2}, {7, 3}, {2, 1}})
            {
                const double a = ag * std::pow(k / j, 2.0 / 3.0);
                if (a > lo && a < hi) r.gapsAu.push_back({a - 0.01 * ag, a + 0.01 * ag});
            }
            r.resonance = "Kirkwood gaps of " + firstGiant->designation;
            r.formationOrigin = "giant_suppressed_accretion";
            r.estimatedBodiesAbove1Km = bodiesAbove1Km(r.totalMassEarth, r.sizeSlope, r.minBodyKm, r.maxBodyKm);
            r.provenance["population"] = Provenance::Generated;
            out.push_back(r);
            if (r.totalMassEarth > 2.0e-4)
            {
                DwarfPlanetCandidate d;
                d.reservoirId = r.id;
                d.massEarth = r.totalMassEarth * rs.uniform(0.2, 0.35);
                d.aAu = rs.uniform(lo, hi);
                d.e = rs.rayleigh(0.06);
                d.iDeg = rs.rayleigh(6.0);
                d.waterFraction = snowRatio < 1.0 ? 0.05 : 0.25;
                dwarfs.push_back(d);
            }
        }
    }

    // Outer planetesimal belt beyond the outermost planet.
    {
        const double lo = std::max(std::max(outermost * 1.4, 3.0 * h.snowLineAu), 0.5);
        const double hi = std::min(std::min(h.outerPlanetesimalEdgeAu, 0.9 * h.outerLimitAu), lo * rs.uniform(1.4, 2.0));
        if (hi > lo * 1.1)
        {
            ReservoirDescriptor r;
            const bool young = h.ageGyr < 1.0;
            r.id = young ? "debris_disk" : "kuiper_belt";
            r.name = h.hostName + (young ? " debris disk" : " outer belt");
            r.kind = young ? "debris_disk" : "kuiper_belt";
            r.parentId = h.hostBodyId;
            r.seed = deriveSeed(h.seed, r.id);
            r.innerAu = lo;
            r.outerAu = hi;
            r.peakAu = lo * rs.uniform(1.05, 1.3);
            r.totalMassEarth = clampd((h.leftoverOuterSolidsEarth + 0.01) * 0.1 / (1.0 + h.ageGyr / 0.5), 1.0e-4, 30.0);
            r.eccentricityMean = rs.uniform(0.03, 0.15);
            r.inclinationMeanDeg = rs.uniform(3.0, 15.0);
            r.sizeSlope = -3.0;
            r.minBodyKm = 0.01;
            r.maxBodyKm = clampd(2400.0 * std::cbrt(r.totalMassEarth / 0.03), 100.0, 3000.0);
            r.clumpiness = rs.uniform(0.05, 0.4);
            r.composition = {{"water_ice", 0.45}, {"silicate", 0.3}, {"organics", 0.15}, {"co_n2_ices", 0.1}};
            r.formationOrigin = "incomplete_growth";
            r.estimatedBodiesAbove1Km = bodiesAbove1Km(r.totalMassEarth, r.sizeSlope, r.minBodyKm, r.maxBodyKm);
            r.provenance["population"] = Provenance::Generated;
            out.push_back(r);
            const int n = std::min(3, rs.poisson(r.totalMassEarth / 0.01));
            for (int k = 0; k < n; ++k)
            {
                DwarfPlanetCandidate d;
                d.reservoirId = r.id;
                d.massEarth = std::min(r.totalMassEarth * 0.1, 0.0022 * rs.logUniform(0.2, 1.5));
                d.aAu = rs.uniform(lo, hi);
                d.e = rs.rayleigh(0.15);
                d.iDeg = rs.rayleigh(8.0);
                d.waterFraction = rs.uniform(0.3, 0.5);
                dwarfs.push_back(d);
            }
        }
    }

    // Comet cloud from planetesimals scattered by giants, cut by the galactic tide / companions.
    if (giantMass > 15.0)
    {
        const double lo = 2000.0 * std::cbrt(h.hostMassSolar);
        const double hi = std::min(1.0e5 * std::cbrt(h.hostMassSolar), 0.3 * h.outerLimitAu);
        if (hi > 2.0 * lo)
        {
            ReservoirDescriptor r;
            r.id = "comet_cloud";
            r.name = h.hostName + " comet cloud";
            r.kind = "comet_cloud";
            r.parentId = h.hostBodyId;
            r.seed = deriveSeed(h.seed, "comet_cloud");
            r.innerAu = lo;
            r.outerAu = hi;
            r.peakAu = std::sqrt(lo * hi);
            r.totalMassEarth = rs.uniform(1.0, 10.0) * std::sqrt(giantMass / 400.0);
            r.eccentricityMean = 0.7;
            r.inclinationMeanDeg = 90.0; // isotropic
            r.sizeSlope = -3.0;
            r.minBodyKm = 0.1;
            r.maxBodyKm = 100.0;
            r.clumpiness = 0.0;
            r.composition = {{"water_ice", 0.5}, {"co_co2_ices", 0.2}, {"silicate_dust", 0.2}, {"organics", 0.1}};
            r.formationOrigin = "giant_scattering";
            r.estimatedBodiesAbove1Km = bodiesAbove1Km(r.totalMassEarth, r.sizeSlope, r.minBodyKm, r.maxBodyKm);
            r.provenance["population"] = Provenance::Generated;
            out.push_back(r);
        }
    }

    // Trojans of each giant.
    for (const auto* p : planets)
    {
        const double m = p->massKg / EarthMassKg;
        if (m < 50.0 || p->orbit.eccentricity > 0.2)
            continue;
        ReservoirDescriptor r;
        r.id = "trojans_" + p->designation;
        r.name = p->designation + " trojans";
        r.kind = "trojans";
        r.parentId = p->id;
        r.seed = deriveSeed(h.seed, r.id);
        r.innerAu = p->orbit.semiMajorAxisAu * 0.95;
        r.outerAu = p->orbit.semiMajorAxisAu * 1.05;
        r.peakAu = p->orbit.semiMajorAxisAu;
        r.totalMassEarth = rs.logUniform(1.0e-5, 1.0e-4) * m / 318.0;
        r.eccentricityMean = 0.07;
        r.inclinationMeanDeg = rs.uniform(5.0, 15.0);
        r.sizeSlope = -3.0;
        r.minBodyKm = 0.1;
        r.maxBodyKm = clampd(250.0 * std::cbrt(r.totalMassEarth / 1e-5), 20.0, 400.0);
        r.composition = {{"d_type_organics", 0.6}, {"silicate", 0.25}, {"water_ice", 0.15}};
        r.resonance = "1:1 L4/L5 with " + p->designation;
        r.formationOrigin = "captured_at_lagrange_points";
        r.estimatedBodiesAbove1Km = bodiesAbove1Km(r.totalMassEarth, r.sizeSlope, r.minBodyKm, r.maxBodyKm);
        r.provenance["population"] = Provenance::Generated;
        out.push_back(r);
    }
    (void)hostMassKg;
}

} // namespace world::generation
