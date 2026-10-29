#include "src/world/generation/NaturalResources.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

#include "src/world/generation/AstroUnits.h"
#include "src/world/generation/GenerationRandom.h"
#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

namespace
{

using namespace astro;
using F = ResourceFamily;

double clamp01(double x)
{
    return std::clamp(x, 0.0, 1.0);
}

double smooth(double e0, double e1, double x)
{
    const double t = clamp01((x - e0) / (e1 - e0));
    return t * t * (3.0 - 2.0 * t);
}

// 0..1 from a level label of the body physics.
double volcanismLevel(const std::string& v)
{
    if (v == "low") return 0.25;
    if (v == "moderate") return 0.5;
    if (v == "high") return 0.75;
    if (v == "extreme") return 1.0;
    if (v == "cryovolcanic") return 0.4;
    return 0.0;
}

double tectonicHydrothermal(const std::string& t)
{
    if (t == "mobile_lid") return 1.0;
    if (t == "episodic") return 0.5;
    if (t == "cryotectonic") return 0.2;
    if (t == "stagnant_lid") return 0.1;
    return 0.0;
}

double gasShare(const AtmosphereDescriptor& a, std::initializer_list<const char*> names)
{
    double s = 0.0;
    for (const auto& [gas, x] : a.composition)
        for (const char* n : names)
            if (gas == n)
                s += x;
    return s;
}

struct HostContext
{
    double metallicityFeH = 0.0;
    double alphaFe = 0.0;
    double ageGyr = 4.6;
    double teffK = 5772.0;
    double snowLineAu = 2.7;
    double massSolar = 1.0;
    double luminositySolar = 1.0;
    double orbitAu = 0.0;        // heliocentric orbit of the body / its parent planet
};

// Hohmann delta-v between circular orbits r1 and r2 (AU) around mass M.
double hohmannKms(double r1Au, double r2Au, double massSolar)
{
    if (r1Au <= 0.0 || r2Au <= 0.0 || massSolar <= 0.0)
        return 0.0;
    const double mu = SunMuM3s2 * massSolar;
    const double r1 = r1Au * AuMeters, r2 = r2Au * AuMeters;
    const double a = 0.5 * (r1 + r2);
    const double dv1 = std::abs(std::sqrt(mu / r1) * (std::sqrt(r2 / a) - 1.0));
    const double dv2 = std::abs(std::sqrt(mu / r2) * (1.0 - std::sqrt(r1 / a)));
    return (dv1 + dv2) / 1000.0;
}

double habitableZoneAu(const HostContext& h)
{
    return std::sqrt(std::max(h.luminositySolar, 1.0e-6));
}

// Present-day actinide abundance relative to the Sun's 4.6 Gyr: 238U decays
// with 4.47 Gyr, 232Th with 14.05 Gyr (Th/U ~ 3.9 by mass).
double actinideDecayFactor(double ageGyr)
{
    const double u = std::pow(2.0, -(ageGyr - 4.6) / 4.47);
    const double th = std::pow(2.0, -(ageGyr - 4.6) / 14.05);
    return (u + 3.9 * th) / 4.9;
}

ResourceDeposit& deposit(NaturalResourceProfile& p, F f)
{
    for (auto& d : p.deposits)
        if (d.family == f)
            return d;
    p.deposits.push_back({});
    p.deposits.back().family = f;
    return p.deposits.back();
}

void finishDeposits(NaturalResourceProfile& p)
{
    p.deposits.erase(std::remove_if(p.deposits.begin(), p.deposits.end(),
                                    [](const ResourceDeposit& d) { return d.bulkMassFraction <= 0.0; }),
                     p.deposits.end());
    for (auto& d : p.deposits)
    {
        d.accessibleMassFraction = std::max(0.0, d.accessibleMassFraction);
        // Grade: log concentration of the best deposits, 1 = ~1000x enrichment.
        d.oreGrade = clamp01(std::log10(std::max(d.concentrationFactor, 1.0)) / 3.0);
    }
    std::sort(p.deposits.begin(), p.deposits.end(),
              [](const ResourceDeposit& a, const ResourceDeposit& b) { return a.family < b.family; });
}

bool isEnvelopeWorld(const BodyDescriptor& b)
{
    return b.bodyClass == "gas_giant" || b.bodyClass == "super_jupiter" || b.bodyClass == "ice_giant" ||
           b.bodyClass == "sub_neptune";
}

NaturalResourceProfile naturalProfile(const BodyDescriptor& b, const HostContext& h)
{
    NaturalResourceProfile p;
    p.valid = true;
    p.fieldSeed = deriveSeed(b.seed, "resource_field");
    const auto& c = b.composition;
    const double Z = std::pow(10.0, h.metallicityFeH);
    const double alpha = std::pow(10.0, h.alphaFe);
    const double core = c.ironCore, mantle = c.silicateMantle, water = c.waterIce, vol = c.otherVolatiles,
                 env = c.hydrogenHeliumEnvelope;
    // Condensation temperature sequence: bodies formed well inside the snow
    // line are enriched in refractory Ca-Al-Ti phases, those beyond it in ices.
    const double snowRatio = h.snowLineAu > 0.0 ? b.formationDistanceAu / h.snowLineAu : 1.0;
    const double refractoryBoost = 1.0 + 0.6 * clamp01(1.0 - snowRatio);
    const double outerCarbon = snowRatio > 1.0 ? 3.0 : 1.0;
    const double metalScale = std::pow(Z, 0.4);

    deposit(p, F::Iron).bulkMassFraction = (core * 0.86 + mantle * 0.065) * metalScale;
    deposit(p, F::Nickel).bulkMassFraction = (core * 0.055 + mantle * 0.0015) * metalScale;
    deposit(p, F::Silicates).bulkMassFraction = mantle * 0.47 * std::pow(alpha, 0.3);
    deposit(p, F::Magnesium).bulkMassFraction = mantle * 0.23 * std::pow(alpha, 0.5);
    deposit(p, F::OxygenMinerals).bulkMassFraction = mantle * 0.44 * std::pow(alpha, 0.5);
    deposit(p, F::Aluminium).bulkMassFraction = mantle * 0.022 * refractoryBoost;
    deposit(p, F::Titanium).bulkMassFraction = mantle * 0.0012 * refractoryBoost * std::pow(alpha, 0.3);
    deposit(p, F::CopperGroup).bulkMassFraction = (mantle * 1.2e-4 + core * 1.5e-4) * metalScale;
    deposit(p, F::RefractoryMetals).bulkMassFraction = (core * 3.0e-5 + mantle * 5.0e-7 * refractoryBoost) * metalScale;
    deposit(p, F::PlatinumGroup).bulkMassFraction = (core * 6.0e-6 + mantle * 5.0e-9) * metalScale;
    deposit(p, F::Actinides).bulkMassFraction = mantle * 8.0e-8 * metalScale * actinideDecayFactor(h.ageGyr);
    deposit(p, F::WaterIce).bulkMassFraction = water;
    deposit(p, F::CarbonOrganics).bulkMassFraction = vol * 0.25 + mantle * 1.0e-3 * outerCarbon;
    deposit(p, F::NitrogenAmmonia).bulkMassFraction = vol * 0.15 + env * 1.0e-3;
    deposit(p, F::CarbonGases).bulkMassFraction = vol * 0.45 + env * 3.0e-3;
    deposit(p, F::Hydrogen).bulkMassFraction = env * 0.73 + water * 0.112;
    deposit(p, F::NobleGases).bulkMassFraction = env * 0.26 + vol * 0.01;
    deposit(p, F::Helium3).bulkMassFraction = env * 0.26 * 1.0e-4;

    // Atmosphere mass fraction of the body (accessible gases).
    const double area = 4.0 * Pi * std::pow(b.radiusKm * 1000.0, 2.0);
    const double atmMass = b.surfaceGravityMs2 > 0.0 ? b.atmosphere.surfacePressurePa * area / b.surfaceGravityMs2 : 0.0;
    const double atmFraction = b.massKg > 0.0 ? atmMass / b.massKg : 0.0;

    if (isEnvelopeWorld(b))
    {
        // No reachable solid surface: only the envelope / upper atmosphere.
        p.solidSurface = false;
        p.differentiation = 1.0;
        for (auto& d : p.deposits)
        {
            const bool gas = d.family == F::Hydrogen || d.family == F::NobleGases || d.family == F::Helium3 ||
                             d.family == F::CarbonGases || d.family == F::NitrogenAmmonia ||
                             (d.family == F::WaterIce && b.bodyClass == "ice_giant");
            d.accessibleMassFraction = gas ? d.bulkMassFraction : 0.0;
            d.depthClass = gas ? "atmosphere" : "core";
            if (gas)
                d.mechanisms.push_back("envelope_gas");
        }
        finishDeposits(p);
        return p;
    }

    // Differentiation: bodies above a few hundred km melted and formed a core.
    const double diff = smooth(200.0, 900.0, b.radiusKm);
    p.differentiation = diff;
    const double crater = b.surface.craterDensity;
    const bool giantImpact = b.formationOrigin == "giant_impact";
    const double impact = clamp01(crater + (giantImpact ? 0.5 : 0.0));
    const double volc = volcanismLevel(b.volcanism);
    const double hydro = tectonicHydrothermal(b.tectonicRegime);
    const bool ocean = b.hydrosphere == "surface_ocean" || b.hydrosphere == "global_ocean";
    const bool oxygenated = gasShare(b.atmosphere, {"O2"}) > 0.01;
    const bool airless = b.atmosphere.surfacePressurePa < 100.0;
    const double minT = b.minSurfaceTemperatureK > 0.0 ? b.minSurfaceTemperatureK : b.meanSurfaceTemperatureK;

    const auto siderophile = [&](ResourceDeposit& d, double corePart) {
        // Differentiation removes core-partitioned metal from the crust;
        // impacts excavate deep material and deliver asteroidal metal.
        const double mantlePart = std::max(d.bulkMassFraction - corePart, 0.0);
        d.accessibleMassFraction = mantlePart + corePart * (1.0 - diff) + corePart * diff * 0.02 * impact;
        d.concentrationFactor = 1.0 + 30.0 * impact * diff + (diff < 0.3 ? 5.0 : 0.0);
        d.depthClass = diff > 0.5 ? (impact > 0.3 ? "shallow_crust" : "deep_crust") : "surface";
        if (diff > 0.5)
            d.mechanisms.push_back("differentiation_depleted");
        if (impact > 0.3)
            d.mechanisms.push_back("impact_excavated");
        if (diff < 0.3)
            d.mechanisms.push_back("undifferentiated_metal_grains");
    };
    {
        auto& fe = deposit(p, F::Iron);
        siderophile(fe, core * 0.86 * metalScale);
        fe.concentrationFactor *= 1.0 + 2.0 * volc + (ocean && oxygenated ? 4.0 : 0.0);
        if (ocean && oxygenated)
            fe.mechanisms.push_back("banded_iron_sediments");
        if (volc > 0.0)
            fe.mechanisms.push_back("volcanic_basalt");
    }
    siderophile(deposit(p, F::Nickel), core * 0.055 * metalScale);
    {
        auto& pgm = deposit(p, F::PlatinumGroup);
        siderophile(pgm, core * 6.0e-6 * metalScale);
        pgm.concentrationFactor *= 1.0 + 3.0 * hydro + 2.0 * volc; // layered intrusions, hydrothermal gold
    }
    siderophile(deposit(p, F::RefractoryMetals), core * 3.0e-5 * metalScale);
    {
        auto& cu = deposit(p, F::CopperGroup);
        cu.accessibleMassFraction = cu.bulkMassFraction * (1.0 - 0.6 * diff);
        cu.concentrationFactor = 1.0 + 300.0 * hydro + 20.0 * volc + 5.0 * impact;
        cu.depthClass = hydro > 0.3 ? "shallow_crust" : "deep_crust";
        if (hydro > 0.3)
            cu.mechanisms.push_back("hydrothermal_porphyry");
        if (volc > 0.0)
            cu.mechanisms.push_back("volcanic_sulfides");
    }
    {
        auto& u = deposit(p, F::Actinides);
        // Incompatible elements concentrate in evolved (granitic) crust.
        u.accessibleMassFraction = u.bulkMassFraction * (1.0 + 30.0 * hydro * diff);
        u.concentrationFactor = 1.0 + 200.0 * hydro + 10.0 * volc;
        u.depthClass = "shallow_crust";
        if (hydro > 0.3)
            u.mechanisms.push_back("granitic_crust");
    }
    for (F f : {F::Silicates, F::Magnesium, F::OxygenMinerals})
    {
        auto& d = deposit(p, f);
        d.accessibleMassFraction = d.bulkMassFraction;
        d.concentrationFactor = 1.0 + 2.0 * volc;
        d.depthClass = "surface";
        d.mechanisms.push_back(airless ? "regolith" : "crust");
    }
    {
        auto& al = deposit(p, F::Aluminium);
        al.accessibleMassFraction = al.bulkMassFraction * (1.0 + 0.5 * hydro);
        al.concentrationFactor = 1.0 + 10.0 * hydro + 5.0 * (ocean ? 1.0 : 0.0); // feldspar, laterite
        al.depthClass = "surface";
        al.mechanisms.push_back(hydro > 0.3 ? "feldspathic_crust" : "anorthositic_regolith");
    }
    {
        auto& ti = deposit(p, F::Titanium);
        ti.accessibleMassFraction = ti.bulkMassFraction;
        ti.concentrationFactor = 1.0 + 40.0 * volc; // ilmenite-rich mare basalts
        ti.depthClass = "surface";
        if (volc > 0.0)
            ti.mechanisms.push_back("ilmenite_basalt");
    }
    {
        auto& w = deposit(p, F::WaterIce);
        const double cold = smooth(260.0, 160.0, minT); // permanently shadowed / polar ice
        const double surfaceWater = ocean ? 1.0 : clamp01(b.surface.iceCoverage + 0.2 * cold);
        w.accessibleMassFraction = w.bulkMassFraction * (0.02 + 0.98 * surfaceWater);
        // Exogenous water (comets, solar-wind hydroxyl) cold-trapped in the
        // permanently shadowed polar craters of airless low-obliquity bodies
        // (lunar and Mercurian polar ice).
        const double coldTrap = airless && b.radiusKm > 100.0 ? smooth(12.0, 2.0, b.obliquityDeg) * crater : 0.0;
        if (coldTrap > 0.0)
        {
            w.bulkMassFraction += 1.0e-6 * coldTrap;
            w.accessibleMassFraction += 1.0e-6 * coldTrap;
            w.mechanisms.push_back("polar_cold_trap");
        }
        w.concentrationFactor = 1.0 + 50.0 * surfaceWater;
        w.depthClass = surfaceWater > 0.3 ? "surface" : "shallow_crust";
        if (ocean) w.mechanisms.push_back("surface_ocean");
        if (b.surface.iceCoverage > 0.1) w.mechanisms.push_back("ice_cap");
        if (cold > 0.3 && !ocean) w.mechanisms.push_back("cold_trap");
        if (b.hydrosphere == "subsurface_ocean" || b.hydrosphere == "ice_shell") w.mechanisms.push_back("ice_shell");
    }
    // Volatiles: atmosphere (immediately accessible) and ices.
    const double co2 = gasShare(b.atmosphere, {"CO2", "CO", "CH4"});
    const double n2 = gasShare(b.atmosphere, {"N2", "NH3"});
    const double noble = gasShare(b.atmosphere, {"Ar", "Ne", "He"});
    const double h2 = gasShare(b.atmosphere, {"H2"});
    const auto volatile_ = [&](F f, double atmShare) {
        auto& d = deposit(p, f);
        d.bulkMassFraction += atmFraction * atmShare;
        d.accessibleMassFraction = atmFraction * atmShare + d.bulkMassFraction * (minT < 120.0 ? 0.3 : 0.02);
        d.concentrationFactor = 1.0 + (atmShare > 0.1 ? 20.0 : 0.0) + (minT < 90.0 ? 30.0 : 0.0);
        d.depthClass = atmShare > 0.0 ? "atmosphere" : "surface";
        if (atmShare > 0.0) d.mechanisms.push_back("atmosphere");
        if (minT < 120.0) d.mechanisms.push_back("surface_frost");
    };
    volatile_(F::CarbonGases, co2);
    volatile_(F::NitrogenAmmonia, n2);
    volatile_(F::NobleGases, noble);
    volatile_(F::Hydrogen, h2);
    {
        auto& corg = deposit(p, F::CarbonOrganics);
        corg.accessibleMassFraction = corg.bulkMassFraction * (ocean ? 0.5 : 0.2);
        corg.concentrationFactor = 1.0 + (ocean ? 20.0 : 0.0) + (snowRatio > 1.0 ? 5.0 : 0.0);
        corg.depthClass = "surface";
        corg.mechanisms.push_back(ocean ? "sedimentary_carbon" : "carbonaceous_regolith");
    }
    {
        // Solar-wind implanted 3He in the regolith of airless bodies,
        // ~ flux (1/a^2) x exposure (age), larger for old cratered regolith.
        auto& he3 = deposit(p, F::Helium3);
        if (airless)
        {
            const double implanted = 1.0e-8 * clamp01(b.instellationEarth) * clamp01(h.ageGyr / 4.0) *
                                     (0.5 + 0.5 * crater);
            he3.bulkMassFraction += implanted;
            he3.accessibleMassFraction = implanted;
            he3.concentrationFactor = 1.0 + 4.0 * crater;
            he3.depthClass = "surface";
            he3.mechanisms.push_back("solar_wind_regolith");
        }
    }
    finishDeposits(p);
    return p;
}

double radiationIndex(const BodyDescriptor& b, const HostContext& h)
{
    // Stellar particle / UV environment (M dwarfs flare) against magnetic and
    // atmospheric shielding (column of ~1 bar stops essentially all of it).
    const double stellar = clamp01(std::sqrt(std::max(b.instellationEarth, 0.0)) * (h.teffK < 3900.0 ? 2.5 : 1.0) * 0.5 +
                                   0.25); // galactic cosmic rays everywhere
    const double magnetic = 1.0 - std::exp(-b.magneticFieldEarth / 0.3);
    const double atmosphere = 1.0 - std::exp(-b.atmosphere.surfacePressurePa / 2.0e4);
    return clamp01(stellar * (1.0 - 0.5 * magnetic) * (1.0 - 0.95 * atmosphere));
}

IndustrialSuitabilityProfile industrialProfile(const BodyDescriptor& b, const NaturalResourceProfile& n,
                                               const HostContext& h)
{
    IndustrialSuitabilityProfile s;
    s.valid = true;
    s.surfaceGravityG = b.surfaceGravityMs2 / EarthSurfaceGravity;
    s.escapeVelocityKms = b.escapeVelocityKms;
    s.surfacePressureBar = b.atmosphere.surfacePressurePa / 1.0e5;
    const double vc = b.radiusKm > 0.0 ? std::sqrt(b.muM3s2 / (b.radiusKm * 1000.0)) / 1000.0 : 0.0;
    const double drag = s.surfacePressureBar > 0.01 ? 1.2 * std::log10(1.0 + 10.0 * s.surfacePressureBar) : 0.0;
    s.deltaVToLowOrbitKms = vc * 1.1 + drag;
    s.minTemperatureK = b.minSurfaceTemperatureK;
    s.maxTemperatureK = b.maxSurfaceTemperatureK;
    s.surfaceRadiationIndex = radiationIndex(b, h);
    s.terrainRoughness = clamp01(b.surface.roughness);
    s.solarPowerWm2 = 1361.0 * b.instellationEarth * (1.0 - 0.7 * clamp01(b.atmosphere.cloudPotential) *
                                                               (s.surfacePressureBar > 0.01 ? 1.0 : 0.0));
    s.geothermalPowerWm2 = b.radiogenicHeatFluxWm2 + b.tidalHeatFluxWm2;
    s.tidalVolcanicHazard = clamp01(0.5 * smooth(0.2, 3.0, b.tidalHeatFluxWm2) + 0.5 * volcanismLevel(b.volcanism) *
                                                                                    (b.volcanism == "extreme" ? 1.0 : 0.6));
    s.orbitalAccessIndex = 1.0 / (1.0 + s.deltaVToLowOrbitKms / 3.0);
    s.heliocentricDistanceAu = h.orbitAu;
    s.deltaVToHabitableZoneKms = hohmannKms(h.orbitAu, habitableZoneAu(h), h.massSolar);
    const double thermal = clamp01((std::max(0.0, s.maxTemperatureK - 330.0) + std::max(0.0, 200.0 - s.minTemperatureK)) / 400.0);
    const double pressure = clamp01(std::log10(1.0 + s.surfacePressureBar) / 2.0);
    s.operatingCostIndex = n.solidSurface
        ? clamp01(0.30 * thermal + 0.25 * pressure + 0.15 * s.surfaceRadiationIndex + 0.15 * s.tidalVolcanicHazard +
                  0.15 * s.terrainRoughness)
        : 1.0;
    const double well = smooth(0.5, 12.0, s.deltaVToLowOrbitKms);
    for (const auto& d : n.deposits)
    {
        if (d.accessibleMassFraction <= 0.0)
            continue;
        double depth = 1.0;
        if (d.depthClass == "shallow_crust") depth = 2.0;
        else if (d.depthClass == "deep_crust") depth = 5.0;
        else if (d.depthClass == "atmosphere") depth = n.solidSurface ? 0.7 : 3.0; // envelope: skimming
        const double scarcity = 1.0 / (0.2 + d.oreGrade);
        s.extractionCost.push_back({d.family, depth * scarcity * (1.0 + 4.0 * well) * (1.0 + 2.0 * s.operatingCostIndex)});
    }
    return s;
}

HabitabilityProfile habitabilityProfile(const BodyDescriptor& b, const NaturalResourceProfile& n,
                                        const IndustrialSuitabilityProfile& s)
{
    HabitabilityProfile h;
    h.valid = true;
    const double pBar = s.surfacePressureBar;
    h.oxygenPartialPressureBar = pBar * gasShare(b.atmosphere, {"O2"});
    h.pressureSuitability = pBar <= 0.0 ? 0.0 : clamp01(1.0 - std::abs(std::log10(pBar)) / 1.0);
    h.oxygenRequirement = h.oxygenPartialPressureBar > 0.16 && h.oxygenPartialPressureBar < 0.5 ? "none"
                        : h.oxygenPartialPressureBar > 0.08 ? "supplemental" : "full_supply";
    const double T = b.meanSurfaceTemperatureK;
    const double swing = std::max(0.0, b.maxSurfaceTemperatureK - b.minSurfaceTemperatureK);
    h.temperatureBurden = clamp01(std::abs(T - 288.0) / 120.0 + swing / 400.0);
    // Settlement water depends on the amount in reach, not on its share of
    // the planet's mass: 1e15 kg (a large lake / polar deposit) .. 1e21 kg.
    double water = 0.0;
    for (const auto& d : n.deposits)
        if (d.family == ResourceFamily::WaterIce)
            water = clamp01(std::log10(1.0 + d.accessibleMassFraction * b.massKg / 1.0e15) / 6.0);
    h.waterAccessibility = water;
    h.gravityComfort = s.surfaceGravityG <= 0.0 ? 0.0
                     : clamp01(1.0 - std::abs(std::log(s.surfaceGravityG / 0.9)) / std::log(3.0));
    const double toxic = gasShare(b.atmosphere, {"SO2", "H2S", "NH3", "CH4", "CO", "HCl"}) * 20.0 +
                         smooth(0.02, 0.5, pBar * gasShare(b.atmosphere, {"CO2"}));
    h.atmosphericToxicity = pBar > 0.01 ? clamp01(toxic) : 0.0;
    h.radiationProtectionRequirement = s.surfaceRadiationIndex;
    const double liquidWaterT = smooth(250.0, 275.0, T) * (1.0 - smooth(320.0, 360.0, T));
    h.agriculturePotential = n.solidSurface
        ? clamp01(liquidWaterT * water * smooth(0.05, 0.5, b.instellationEarth) *
                  (pBar > 0.2 ? 1.0 : 0.35) * (1.0 - 0.7 * h.atmosphericToxicity))
        : 0.0;
    h.climateStability = clamp01(1.0 - b.orbit.eccentricity * 2.0 - (b.tidalLock == "synchronous" ? 0.25 : 0.0) -
                                 s.tidalVolcanicHazard * 0.5 - std::abs(b.obliquityDeg - 23.0) / 180.0);
    h.exteriorSurvivability = n.solidSurface
        ? clamp01(h.pressureSuitability * (h.oxygenRequirement == "none" ? 1.0 : 0.0) * (1.0 - h.temperatureBurden) *
                  (1.0 - h.atmosphericToxicity) * (1.0 - h.radiationProtectionRequirement))
        : 0.0;
    h.constructionDifficulty = n.solidSurface
        ? clamp01(0.35 * (1.0 - h.gravityComfort) + 0.25 * clamp01(std::log10(1.0 + pBar) / 2.0) +
                  0.2 * h.temperatureBurden + 0.2 * s.terrainRoughness)
        : 1.0;
    h.lifeSupportCost = n.solidSurface
        ? clamp01(0.3 * (1.0 - h.pressureSuitability) + 0.2 * (h.oxygenRequirement == "none" ? 0.0 : 1.0) +
                  0.2 * h.temperatureBurden + 0.15 * h.radiationProtectionRequirement + 0.15 * (1.0 - water))
        : 1.0;
    h.industrialOperatingCost = s.operatingCostIndex;
    h.settlementClass = !n.solidSurface || s.surfacePressureBar > 50.0 || b.maxSurfaceTemperatureK > 600.0
        ? "orbital_only"
        : h.exteriorSurvivability > 0.5 ? "open_air"
        : h.pressureSuitability > 0.3 && h.temperatureBurden < 0.6 && h.atmosphericToxicity < 0.3 ? "pressure_suit"
                                                                                                : "sealed_habitat";
    return h;
}

FuelPotential fuelPotential(const BodyDescriptor& b, const NaturalResourceProfile& n,
                            const IndustrialSuitabilityProfile& s)
{
    FuelPotential f;
    f.valid = true;
    const auto abundance = [](double fraction) { return clamp01(std::log10(1.0 + fraction * 1.0e4) / 4.0); };
    const auto accessible = [&](ResourceFamily fam) {
        for (const auto& d : n.deposits)
            if (d.family == fam)
                return d;
        return ResourceDeposit {};
    };
    // Access: the body's gravity well and the transfer to the system's hub.
    const double transfer = smooth(1.0, 25.0, s.deltaVToHabitableZoneKms);
    const double access = 1.0 - (1.0 - (n.solidSurface ? smooth(0.3, 15.0, s.deltaVToLowOrbitKms) : 0.0)) *
                                     (1.0 - transfer);
    const auto add = [&](const char* kind, const char* mode, double ab, double through, double cost) {
        if (ab <= 0.0)
            return;
        FuelSource src;
        src.kind = kind;
        src.mode = mode;
        src.abundanceIndex = ab;
        src.throughputIndex = through;
        src.accessCostIndex = cost;
        src.netIndex = ab * through * (1.0 - cost);
        f.sources.push_back(src);
    };
    if (n.solidSurface)
    {
        const auto w = accessible(ResourceFamily::WaterIce);
        add("water_ice", "ice_mining", abundance(w.accessibleMassFraction), clamp01(0.3 + 0.7 * w.oreGrade), access);
        const auto c = accessible(ResourceFamily::CarbonGases);
        add(s.surfacePressureBar > 0.01 ? "carbon_dioxide" : "methane",
            s.surfacePressureBar > 0.01 ? "atmospheric_extraction" : "surface_mining",
            abundance(c.accessibleMassFraction), clamp01(0.2 + 0.6 * c.oreGrade), access);
        const auto nh = accessible(ResourceFamily::NitrogenAmmonia);
        add("ammonia", "atmospheric_extraction", abundance(nh.accessibleMassFraction) * 0.5, 0.3, access);
        const auto he3 = accessible(ResourceFamily::Helium3);
        add("helium3", "surface_mining", clamp01(he3.accessibleMassFraction / 2.0e-8), 0.05, access);
    }
    else
    {
        // Envelope worlds: hydrogen by atmospheric skimming from orbit. The
        // gravity well and the heat / radiation of deep passes set the cost.
        const double skimCost = 1.0 - (1.0 - smooth(5.0, 60.0, b.escapeVelocityKms * 0.7)) * (1.0 - transfer);
        const auto hh = accessible(ResourceFamily::Hydrogen);
        add("hydrogen", "atmospheric_skimming", abundance(hh.accessibleMassFraction), 0.9, skimCost);
        const auto he3 = accessible(ResourceFamily::Helium3);
        add("helium3", "atmospheric_skimming", clamp01(he3.accessibleMassFraction / 5.0e-6), 0.2, skimCost);
        const auto ch4 = accessible(ResourceFamily::CarbonGases);
        add("methane", "atmospheric_skimming", abundance(ch4.accessibleMassFraction), 0.4, skimCost);
    }
    for (const auto& ring : b.rings)
    {
        // Ring particles orbit far up the well: harvesting costs the orbital
        // speed at the ring, not the surface gravity.
        const double r = 0.5 * (ring.innerRadiusKm + ring.outerRadiusKm) * 1000.0;
        const double vRing = r > 0.0 ? std::sqrt(b.muM3s2 / r) / 1000.0 : 0.0;
        const double iceMass = ring.massKg * ring.iceFraction;
        add("ring_ice", "ring_harvesting", clamp01(std::log10(1.0 + iceMass / 1.0e15) / 6.0),
            clamp01(0.2 + ring.opticalDepth), 1.0 - (1.0 - smooth(1.0, 30.0, vRing)) * (1.0 - transfer));
    }
    for (const auto& src : f.sources)
        if (src.netIndex > f.bestNetIndex)
        {
            f.bestNetIndex = src.netIndex;
            f.bestSource = src.kind;
        }
    return f;
}

// Small-body population: composition template x host chemistry; no gravity
// well to speak of, extraction is a matter of finding and reaching bodies.
ResourceProfiles reservoirProfiles(const ReservoirDescriptor& r, const HostContext& h)
{
    ResourceProfiles out;
    auto& n = out.natural;
    n.valid = true;
    n.fieldSeed = deriveSeed(r.seed, "resource_field");
    n.differentiation = 0.1;
    const double Z = std::pow(10.0, h.metallicityFeH);
    const double snowRatio = h.snowLineAu > 0.0 ? r.peakAu / h.snowLineAu : 1.0;
    std::map<std::string, double> c(r.composition.begin(), r.composition.end());
    const auto get = [&](const char* k) { return c.count(k) ? c.at(k) : 0.0; };
    const double metal = get("metallic_m_type") * std::pow(Z, 0.5);
    const double sType = get("silicate_s_type") + get("silicate") + get("silicate_dust");
    const double cType = get("carbonaceous_c_type") + get("d_type_organics") + get("organics");
    // Ices sublimate inside the snow line (thermal history of the belt).
    const double iceSurvival = smooth(0.6, 1.2, snowRatio);
    const double ice = (get("water_ice")) * (0.2 + 0.8 * iceSurvival);
    const double otherIce = (get("co_co2_ices") + get("co_n2_ices")) * iceSurvival;
    const auto set = [&](F f, double bulk, double conc, const char* mechanism) {
        if (bulk <= 0.0)
            return;
        auto& d = deposit(n, f);
        d.bulkMassFraction = bulk;
        d.accessibleMassFraction = bulk; // small bodies: everything is near the surface
        d.concentrationFactor = conc;
        d.depthClass = "population";
        d.mechanisms.push_back(mechanism);
    };
    set(F::Iron, metal * 0.88 + sType * 0.2 + cType * 0.18, 1.0 + 200.0 * metal, "metallic_asteroids");
    set(F::Nickel, metal * 0.09 + sType * 0.012 + cType * 0.011, 1.0 + 200.0 * metal, "metallic_asteroids");
    set(F::PlatinumGroup, metal * 3.0e-5 + (sType + cType) * 1.0e-6, 1.0 + 500.0 * metal, "undifferentiated_metal");
    set(F::RefractoryMetals, metal * 1.0e-4 + sType * 1.0e-6, 1.0 + 100.0 * metal, "metallic_asteroids");
    set(F::CopperGroup, (metal + sType + cType) * 1.2e-4, 3.0, "chondritic");
    set(F::Silicates, sType * 0.45 + cType * 0.35, 2.0, "chondritic");
    set(F::Magnesium, sType * 0.15 + cType * 0.10, 2.0, "chondritic");
    set(F::OxygenMinerals, sType * 0.40 + cType * 0.35, 2.0, "chondritic");
    set(F::Aluminium, (sType + cType) * 0.012, 1.5, "chondritic");
    set(F::Titanium, (sType + cType) * 6.0e-4, 1.5, "chondritic");
    set(F::WaterIce, ice + cType * 0.1 * iceSurvival, 1.0 + 30.0 * iceSurvival, iceSurvival > 0.5 ? "icy_bodies" : "hydrated_minerals");
    set(F::CarbonOrganics, cType * 0.2, 5.0, "carbonaceous_bodies");
    set(F::CarbonGases, otherIce * 0.7, 10.0, "volatile_ices");
    set(F::NitrogenAmmonia, otherIce * 0.3 + cType * 0.002, 5.0, "volatile_ices");
    set(F::Hydrogen, (ice + cType * 0.1 * iceSurvival) * 0.112, 1.0, "water_hydrogen");
    finishDeposits(n);

    auto& s = out.industrial;
    s.valid = true;
    s.surfaceGravityG = 0.0;
    s.deltaVToLowOrbitKms = 0.0;
    s.solarPowerWm2 = 0.0;
    s.terrainRoughness = 0.0;
    // Finding and matching orbits across a wide, inclined, eccentric
    // population, and reaching it from the system's hub.
    s.heliocentricDistanceAu = r.peakAu;
    s.deltaVToHabitableZoneKms = hohmannKms(r.peakAu, habitableZoneAu(h), h.massSolar);
    s.orbitalAccessIndex = clamp01((1.0 - 0.6 * smooth(0.0, 0.3, r.eccentricityMean) -
                                    0.3 * smooth(0.0, 20.0, r.inclinationMeanDeg)) *
                                   (1.0 - smooth(1.0, 25.0, s.deltaVToHabitableZoneKms)));
    s.operatingCostIndex = clamp01(0.3 * (1.0 - r.clumpiness) + 0.4 * smooth(5.0, 100.0, r.peakAu));
    for (const auto& d : n.deposits)
        s.extractionCost.push_back(
            {d.family, (1.0 / (0.2 + d.oreGrade)) * (1.0 + 2.0 * (1.0 - s.orbitalAccessIndex)) * (1.0 + s.operatingCostIndex)});

    auto& hb = out.habitability;
    hb.valid = true;
    hb.settlementClass = "sealed_habitat";
    hb.oxygenRequirement = "full_supply";
    hb.lifeSupportCost = 0.8;
    hb.constructionDifficulty = 0.4;
    hb.industrialOperatingCost = s.operatingCostIndex;
    hb.radiationProtectionRequirement = 1.0;

    auto& f = out.fuel;
    f.valid = true;
    for (const auto& d : n.deposits)
        if (d.family == F::WaterIce)
        {
            FuelSource src;
            src.kind = "water_ice";
            src.mode = "population_mining";
            // Amount matters for a population (the ice mass in the belt).
            const double iceKg = d.bulkMassFraction * r.totalMassEarth * EarthMassKg;
            src.abundanceIndex = clamp01((std::log10(std::max(iceKg, 1.0)) - 15.0) / 9.0);
            src.throughputIndex = clamp01(0.2 + 0.8 * d.oreGrade);
            src.accessCostIndex = 1.0 - s.orbitalAccessIndex;
            src.netIndex = src.abundanceIndex * src.throughputIndex * (1.0 - src.accessCostIndex);
            f.sources.push_back(src);
            f.bestSource = src.kind;
            f.bestNetIndex = src.netIndex;
        }
    return out;
}

} // namespace

const char* resourceFamilyName(ResourceFamily family)
{
    static const std::array<const char*, ResourceFamilyCount> names = {
        "water_ice", "hydrogen", "carbon_organics", "oxygen_minerals", "silicates", "iron",
        "nickel", "aluminium", "magnesium", "titanium", "copper_group", "refractory_metals",
        "platinum_group", "actinides", "nitrogen_ammonia", "carbon_gases", "noble_gases", "helium3"};
    return names[static_cast<std::size_t>(family)];
}

void deriveResourceProfiles(SystemDescriptor& sd)
{
    const auto contextFor = [&](const std::string& starKey, const std::string& parentId) {
        HostContext h;
        h.alphaFe = sd.context.alphaEnhancement;
        const StarDescriptor* star = nullptr;
        for (const auto& s : sd.stars)
            if (s.key == starKey || s.id == parentId)
                star = &s;
        if (!star && !sd.stars.empty())
            star = &sd.stars.front();
        if (star)
        {
            h.metallicityFeH = star->metallicityFeH;
            h.ageGyr = star->ageGyr;
            h.teffK = star->teffK;
            h.massSolar = star->massSolar;
            h.luminositySolar = star->luminositySolar;
        }
        for (const auto& d : sd.disks)
            if (d.hostNode == starKey)
                h.snowLineAu = d.snowLineAu;
        return h;
    };
    std::map<std::string, std::string> hostOfBody;
    std::map<std::string, const BodyDescriptor*> byId;
    for (const auto& b : sd.bodies)
    {
        hostOfBody[b.id] = b.hostStarKey;
        byId[b.id] = &b;
    }
    // Heliocentric orbit: a moon travels with its planet.
    const auto orbitAu = [&](const BodyDescriptor& b) {
        const BodyDescriptor* x = &b;
        for (int depth = 0; depth < 4 && x->kind == "moon"; ++depth)
        {
            const auto it = byId.find(x->parentId);
            if (it == byId.end())
                break;
            x = it->second;
        }
        return x->orbit.semiMajorAxisAu;
    };
    for (auto& b : sd.bodies)
    {
        HostContext h = contextFor(b.hostStarKey, b.parentId);
        h.orbitAu = orbitAu(b);
        b.resources.natural = naturalProfile(b, h);
        b.resources.industrial = industrialProfile(b, b.resources.natural, h);
        b.resources.habitability = habitabilityProfile(b, b.resources.natural, b.resources.industrial);
        b.resources.fuel = fuelPotential(b, b.resources.natural, b.resources.industrial);
    }
    for (auto& r : sd.reservoirs)
    {
        std::string starKey;
        if (const auto it = hostOfBody.find(r.parentId); it != hostOfBody.end())
            starKey = it->second;
        for (const auto& s : sd.stars)
            if (s.id == r.parentId)
                starKey = s.key;
        r.resources = reservoirProfiles(r, contextFor(starKey, r.parentId));
    }
}

} // namespace world::generation
