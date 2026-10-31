#include "src/world/generation/BodyPhysics.h"

#include <algorithm>
#include <cmath>

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

double kopparapuFlux(double s0, double a, double b, double c, double d, double teff)
{
    const double t = clampd(teff, 2600.0, 7200.0) - 5780.0;
    return s0 + a * t + b * t * t + c * t * t * t + d * t * t * t * t;
}

double boilingPointK(double pressureBar)
{
    if (pressureBar <= 0.006)
        return 273.16;
    return 1.0 / (1.0 / 373.15 - std::log(pressureBar) / 4890.0);
}

double meanMolecularWeight(const std::vector<std::pair<std::string, double>>& comp)
{
    static const std::pair<const char*, double> weights[] = {
        {"H2", 2.016}, {"He", 4.003}, {"N2", 28.014}, {"O2", 31.998}, {"CO2", 44.01},
        {"H2O", 18.015}, {"CH4", 16.04}, {"Ar", 39.95}, {"CO", 28.01}, {"SO2", 64.07},
        {"NH3", 17.03}, {"Na", 22.99}, {"SiO", 44.08}, {"O", 16.0}, {"Ne", 20.18}};
    double sum = 0.0, w = 0.0;
    for (const auto& [gas, x] : comp)
    {
        double m = 28.0;
        for (const auto& [name, mm] : weights)
            if (gas == name) m = mm;
        sum += x * m;
        w += x;
    }
    return w > 0.0 ? sum / w : 0.0;
}

void normalise(std::vector<std::pair<std::string, double>>& comp)
{
    double total = 0.0;
    for (const auto& c : comp) total += c.second;
    if (total <= 0.0) return;
    for (auto& c : comp) c.second /= total;
    std::sort(comp.begin(), comp.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
}

double fraction(const std::vector<std::pair<std::string, double>>& comp, const char* gas)
{
    for (const auto& c : comp)
        if (c.first == gas) return c.second;
    return 0.0;
}

} // namespace

void habitableZoneAu(double l, double teff, double& inner, double& outer)
{
    const double sIn = kopparapuFlux(1.107, 1.332e-4, 1.580e-8, -8.308e-12, -1.931e-15, teff);
    const double sOut = kopparapuFlux(0.356, 6.171e-5, 1.698e-9, -3.198e-12, -5.575e-16, teff);
    inner = std::sqrt(l / sIn);
    outer = std::sqrt(l / sOut);
}

void deriveBodyPhysics(
    BodyDescriptor& b,
    const BodySeedProperties& sp,
    const BodyEnvironment& env,
    const ObservedBodyInput* obs,
    std::uint64_t seed)
{
    RandomStream bulk(seed, "bulk");
    RandomStream spin(seed, "spin");
    RandomStream atmo(seed, "atmosphere");
    RandomStream surf(seed, "surface");
    auto mark = [&](const char* field, bool observed, Provenance otherwise) {
        b.provenance[field] = observed ? (b.origin == "authored" ? Provenance::Authored : Provenance::Observed)
                                       : otherwise;
    };

    // ---- bulk composition ------------------------------------------------
    const double massEarth = sp.massEarth;
    const double envFrac = clampd(sp.envelopeFraction, 0.0, 0.99);
    double water = clampd(obs && obs->waterMassFraction ? *obs->waterMassFraction : sp.waterFraction, 0.0, 0.6);
    double iron = sp.ironFraction >= 0.0 ? sp.ironFraction : clampd(bulk.normal(0.32, 0.05), 0.12, 0.7);
    if (obs && obs->ironCoreFraction) iron = *obs->ironCoreFraction;
    if (sp.formationAu < 0.05 && sp.ironFraction < 0.0)
        iron = clampd(iron + bulk.uniform(0.0, 0.15), 0.12, 0.75); // mantle stripping close in
    const double solid = 1.0 - envFrac;
    b.composition.hydrogenHeliumEnvelope = envFrac;
    b.composition.waterIce = solid * water;
    b.composition.otherVolatiles = sp.formationAu > 4.0 * env.snowLineAu ? solid * water * 0.25 : solid * 1.0e-4;
    const double rockMetal = std::max(0.0, solid - b.composition.waterIce - b.composition.otherVolatiles);
    b.composition.ironCore = rockMetal * iron;
    b.composition.silicateMantle = rockMetal * (1.0 - iron);
    b.waterMassFraction = b.composition.waterIce;
    b.formationDistanceAu = sp.formationAu;
    mark("composition", obs && (obs->waterMassFraction || obs->ironCoreFraction), Provenance::Generated);

    // ---- irradiation ------------------------------------------------------
    const double ecc = clampd(env.eccentricity, 0.0, 0.95);
    b.instellationEarth = env.lightLuminositySolar /
        (env.lightDistanceAu * env.lightDistanceAu * std::sqrt(1.0 - ecc * ecc));
    double hzIn = 1.0, hzOut = 1.7;
    habitableZoneAu(env.lightLuminositySolar, env.lightTeffK, hzIn, hzOut);
    b.habitableZonePosition = env.lightDistanceAu / hzIn;

    // ---- radius / mass ---------------------------------------------------
    const double massKg = massEarth * EarthMassKg;
    double radiusEarth = 0.0;
    const double coreEarth = massEarth * solid;
    const double cmf = b.composition.ironCore / std::max(rockMetal, 1e-9);
    double rockyR = (1.07 - 0.21 * clampd(cmf, 0.0, 1.0)) * std::pow(std::max(coreEarth, 1e-6), 1.0 / 3.7);
    if (coreEarth < 0.01)
    {
        // Small bodies: uncompressed density of rock / iron / ice.
        const double rho = 1.0 / (b.composition.ironCore / solid / 7800.0 +
                                  b.composition.silicateMantle / solid / 3300.0 +
                                  (b.composition.waterIce + b.composition.otherVolatiles) / solid / 950.0);
        rockyR = std::cbrt(3.0 * coreEarth * EarthMassKg / (4.0 * Pi * rho)) / EarthRadiusM;
    }
    else
    {
        rockyR *= 1.0 + 0.55 * water;
    }

    const double teqZero = EquilibriumTemperatureAt1AuK * std::pow(b.instellationEarth, 0.25);
    const double mj = massEarth / EarthMassesPerJupiterMass;
    if (envFrac >= 0.3 && massEarth >= 30.0)
    {
        // Giant: nearly constant radius, mild compression above ~4 MJ, inflation when hot.
        // Electron degeneracy keeps the radius near 1 R_J from ~0.3 to ~15 M_J.
        double rj = mj < 0.3 ? 0.75 + 0.85 * (mj - 0.1) : 1.0 * std::pow(mj, -0.04);
        if (teqZero > 1000.0 && mj < 4.0)
            rj += clampd(0.5 * (teqZero - 1000.0) / 1000.0, 0.0, 0.8);
        rj *= std::pow(std::max(env.ageGyr, 0.1) / 4.6, -0.02);
        radiusEarth = rj * JupiterRadiusM / EarthRadiusM;
    }
    else if (envFrac > 0.001)
    {
        const double renv = 2.06 * std::pow(massEarth, -0.21) * std::pow(envFrac / 0.05, 0.59) *
            std::pow(std::max(b.instellationEarth, 1e-3), 0.044) *
            std::pow(std::max(env.ageGyr, 0.1) / 5.0, -0.11);
        radiusEarth = rockyR + renv;
    }
    else
    {
        radiusEarth = rockyR;
    }

    // Photoevaporation strips light envelopes of hot small planets (radius valley).
    const double tSatGyr = clampd(0.1 * std::pow(env.lightMassSolar, -2.0), 0.1, 3.0);
    const double xuvFluenceJm2 = env.lightLuminositySolar * SunLuminosityW * std::pow(10.0, -3.13) *
        tSatGyr * GyrSeconds * 2.0 / (4.0 * Pi * std::pow(env.lightDistanceAu * AuMeters, 2.0));
    const double radiusM0 = radiusEarth * EarthRadiusM;
    const double xuvLossKg = 0.1 * Pi * xuvFluenceJm2 * radiusM0 * radiusM0 * radiusM0 / (G * massKg);
    double envelopeLeft = envFrac;
    if (envFrac > 0.0 && envFrac < 0.3)
    {
        const double envKg = envFrac * massKg;
        if (xuvLossKg >= envKg)
        {
            envelopeLeft = 0.0;
            radiusEarth = rockyR;
            b.formationOrigin = b.formationOrigin == "in_situ" ? "photoevaporated_core" : b.formationOrigin;
        }
        else if (xuvLossKg > 0.0)
        {
            envelopeLeft = (envKg - xuvLossKg) / massKg;
            const double renv = 2.06 * std::pow(massEarth, -0.21) * std::pow(std::max(envelopeLeft, 1e-6) / 0.05, 0.59) *
                std::pow(std::max(b.instellationEarth, 1e-3), 0.044) * std::pow(std::max(env.ageGyr, 0.1) / 5.0, -0.11);
            radiusEarth = envelopeLeft > 0.001 ? rockyR + renv : rockyR;
        }
        // The solid part is unchanged: renormalise the bulk fractions.
        const double solidScale = (1.0 - envelopeLeft) / std::max(1.0 - envFrac, 1e-9);
        b.composition.ironCore *= solidScale;
        b.composition.silicateMantle *= solidScale;
        b.composition.waterIce *= solidScale;
        b.composition.otherVolatiles *= solidScale;
        b.composition.hydrogenHeliumEnvelope = envelopeLeft;
        b.waterMassFraction = b.composition.waterIce;
    }

    if (obs && obs->radiusEarth)
        radiusEarth = *obs->radiusEarth;
    mark("radius_km", obs && obs->radiusEarth, Provenance::Derived);
    b.massKg = massKg;
    b.radiusKm = radiusEarth * EarthRadiusM / 1000.0;
    const double radiusM = b.radiusKm * 1000.0;
    b.densityKgM3 = massKg / (4.0 / 3.0 * Pi * radiusM * radiusM * radiusM);
    b.muM3s2 = G * massKg;
    b.surfaceGravityMs2 = b.muM3s2 / (radiusM * radiusM);
    b.escapeVelocityKms = std::sqrt(2.0 * b.muM3s2 / radiusM) / 1000.0;
    b.provenance["mu_m3s2"] = Provenance::Derived;
    const double aM = env.orbitAu * AuMeters;
    b.hillRadiusKm = aM * (1.0 - ecc) * std::cbrt(massKg / (3.0 * std::max(env.gravitationalHostMassKg, 1.0))) / 1000.0;
    b.sphereOfInfluenceKm = aM * std::pow(massKg / std::max(env.gravitationalHostMassKg, 1.0), 0.4) / 1000.0;

    // ---- class ------------------------------------------------------------
    const double envNow = b.composition.hydrogenHeliumEnvelope;
    if (massEarth >= 50.0 && envNow >= 0.3)
        b.bodyClass = mj >= 3.0 ? "super_jupiter" : "gas_giant";
    else if (massEarth >= 8.0 && envNow >= 0.05)
        b.bodyClass = water > 0.15 ? "ice_giant" : "sub_neptune";
    else if (envNow > 0.001)
        b.bodyClass = "sub_neptune";
    else if (massEarth < 0.005)
        b.bodyClass = water > 0.1 ? "dwarf_icy" : "dwarf_rocky";
    else if (water > 0.1)
        b.bodyClass = "water_rich";
    else if (cmf > 0.5)
        b.bodyClass = "iron_rich";
    else
        b.bodyClass = "rocky";
    const bool giant = b.bodyClass == "gas_giant" || b.bodyClass == "super_jupiter";
    const bool fluid = giant || b.bodyClass == "ice_giant" || b.bodyClass == "sub_neptune";

    // ---- spin and tides ------------------------------------------------
    const double nOrbit = std::sqrt(G * (env.gravitationalHostMassKg + massKg) / (aM * aM * aM));
    const double orbitalPeriodHours = TwoPi / nOrbit / 3600.0;
    const double q = fluid ? 1.0e5 : 100.0;
    const double k2 = fluid ? 0.38 : (water > 0.1 ? 0.1 : 0.3);
    double initialSpinHours = fluid ? spin.uniform(9.0, 18.0)
        : massEarth < 0.01 ? spin.logUniform(4.0, 30.0) : spin.logUniform(8.0, 40.0);
    const double omega0 = TwoPi / (initialSpinHours * 3600.0);
    const double tLock = omega0 * std::pow(aM, 6.0) * 0.33 * massKg * radiusM * radiusM * q /
        (3.0 * G * env.gravitationalHostMassKg * env.gravitationalHostMassKg * k2 * std::pow(radiusM, 5.0));
    const bool locked = tLock < env.ageGyr * GyrSeconds;
    if (locked)
    {
        if (ecc > 0.1 && !env.isMoon)
        {
            b.tidalLock = "spin_orbit_3_2";
            b.rotationPeriodHours = orbitalPeriodHours * 2.0 / 3.0;
        }
        else
        {
            b.tidalLock = "synchronous";
            b.rotationPeriodHours = orbitalPeriodHours;
        }
        b.obliquityDeg = spin.rayleigh(0.5);
    }
    else
    {
        b.tidalLock = "none";
        // Despinning by tides over the age for close, slowly despinning bodies.
        b.rotationPeriodHours = initialSpinHours;
        if (fluid)
            b.obliquityDeg = spin.chance(0.1) ? spin.uniform(25.0, 100.0) : spin.rayleigh(6.0);
        else
            b.obliquityDeg = spin.chance(0.12) ? spin.uniform(0.0, 180.0)
                                               : std::min(std::abs(spin.normal(0.0, 25.0)), 89.0);
    }
    if (obs && obs->rotationPeriodHours) b.rotationPeriodHours = *obs->rotationPeriodHours;
    if (obs && obs->obliquityDeg) b.obliquityDeg = *obs->obliquityDeg;
    if (obs && obs->tidallyLocked) b.tidalLock = *obs->tidallyLocked ? "synchronous" : "none";
    b.rotationDirection = 1;
    b.axisNodeDeg = spin.uniform(0.0, 360.0);
    mark("rotation_period_hours", obs && obs->rotationPeriodHours, Provenance::Generated);
    mark("obliquity_deg", obs && obs->obliquityDeg, Provenance::Generated);

    // ---- internal heat -------------------------------------------------
    const double area = 4.0 * Pi * radiusM * radiusM;
    if (fluid)
    {
        const double tInt = giant ? 100.0 * std::pow(mj, 0.3) * std::pow(std::max(env.ageGyr, 0.1) / 4.6, -0.3)
                                  : 50.0 * std::pow(massEarth / 17.0, 0.3) * std::pow(std::max(env.ageGyr, 0.1) / 4.6, -0.3);
        b.radiogenicHeatFluxWm2 = StefanBoltzmann * std::pow(tInt, 4.0);
    }
    else
    {
        const double silicateKg = massKg * b.composition.silicateMantle;
        b.radiogenicHeatFluxWm2 = 3.4e-12 * std::exp((4.5 - env.ageGyr) / 2.8) * silicateKg / area;
    }
    const double perturber = env.gravitationalHostMassKg;
    const double tidalPower = 10.5 * (k2 / q) * G * perturber * perturber * std::pow(radiusM, 5.0) *
        nOrbit * ecc * ecc / std::pow(aM, 6.0);
    b.tidalHeatFluxWm2 = locked || env.isMoon ? tidalPower / area : 0.0;
    const double heatFlux = b.radiogenicHeatFluxWm2 + b.tidalHeatFluxWm2;

    // ---- albedo and equilibrium temperature -----------------------------
    double albedo = giant ? (teqZero > 1000.0 ? 0.1 : 0.34)
        : b.bodyClass == "ice_giant" ? 0.30 : b.bodyClass == "sub_neptune" ? 0.25
        : water > 0.1 && teqZero < 200.0 ? 0.55 : 0.12;
    b.equilibriumTemperatureK = teqZero * std::pow(1.0 - albedo, 0.25);

    // ---- atmosphere -------------------------------------------------------
    AtmosphereDescriptor& at = b.atmosphere;
    const double sIn = kopparapuFlux(1.107, 1.332e-4, 1.580e-8, -8.308e-12, -1.931e-15, env.lightTeffK);
    const double sOut = kopparapuFlux(0.356, 6.171e-5, 1.698e-9, -3.198e-12, -5.575e-16, env.lightTeffK);
    const double s = b.instellationEarth;
    double pressureBar = 0.0;
    std::vector<std::pair<std::string, double>> comp;
    if (fluid)
    {
        at.origin = "primordial";
        pressureBar = 1.0; // reference 1-bar level
        const double ch4 = b.bodyClass == "ice_giant" || b.bodyClass == "sub_neptune" ? atmo.uniform(0.012, 0.025) : 0.003;
        comp = {{"H2", 0.86 - ch4}, {"He", 0.136}, {"CH4", ch4}};
        if (b.equilibriumTemperatureK < 200.0) comp.push_back({"NH3", 2.0e-4});
        if (b.equilibriumTemperatureK > 300.0) comp.push_back({"H2O", 1.0e-3});
        at.cloudPotential = 0.9;
    }
    else
    {
        // Volatile inventory: outgassed CO2 + N2 (~1e-4 of the planet for Venus/Earth).
        const double inventoryKg = 1.0e-4 * massKg * atmo.logNormal10(1.0, 0.3) *
            (b.composition.otherVolatiles > 1e-3 ? 5.0 : 1.0);
        const double tExo = 1000.0 * std::pow(std::max(s, 1e-4), 0.2);
        const double lambdaN2 = G * massKg * 28.0 * ProtonMassKg / (Boltzmann * tExo * radiusM);
        const double lambdaCO2 = lambdaN2 * 44.0 / 28.0;
        const double heavyLossKg = xuvLossKg * 0.1; // CO2-cooled thermospheres lose less
        double retainedKg = inventoryKg - heavyLossKg;
        if (lambdaCO2 < 20.0) retainedKg = 0.0;
        at.retentionIndex = inventoryKg / std::max(heavyLossKg, 1.0);
        const double toBar = b.surfaceGravityMs2 / area / 1.0e5;
        const bool hasWater = water > 1.0e-5;
        const bool icy = water > 0.1;
        if (b.equilibriumTemperatureK > 1100.0)
        {
            at.origin = "rock_vapour";
            pressureBar = 1.0e-5 * atmo.logUniform(0.1, 10.0);
            comp = {{"Na", 0.4}, {"SiO", 0.3}, {"O", 0.3}};
        }
        else if (retainedKg <= 0.0)
        {
            if (icy && b.equilibriumTemperatureK < 70.0 && lambdaN2 > 10.0)
            {
                at.origin = "sublimated";
                pressureBar = 1.0e-5 * atmo.logUniform(0.3, 3.0);
                comp = {{"N2", 0.99}, {"CH4", 0.005}, {"CO", 0.005}};
            }
            else
            {
                at.origin = "none";
                pressureBar = 0.0;
            }
        }
        else if (s > sIn && hasWater)
        {
            // Runaway greenhouse: water lost, carbon stays in the air.
            at.origin = "outgassed";
            pressureBar = retainedKg * toBar;
            comp = {{"CO2", 0.965}, {"N2", 0.035}, {"SO2", 1.5e-4}, {"H2O", 2.0e-5}};
            at.cloudPotential = 0.95;
        }
        else if (s > sOut && hasWater)
        {
            // Carbonate-silicate cycle draws CO2 down; N2 dominates.
            at.origin = "outgassed";
            pressureBar = retainedKg * toBar * 0.15 * atmo.logNormal10(1.0, 0.25);
            const double co2 = clampd(4.0e-4 * std::pow(sIn / std::max(s, 1e-3), 4.0), 1.0e-4, 0.2);
            comp = {{"N2", 1.0 - co2 - 0.01}, {"Ar", 0.009}, {"CO2", co2}, {"H2O", 0.01}};
            at.cloudPotential = 0.6;
        }
        else if (icy && massEarth > 0.01 && b.equilibriumTemperatureK < 120.0 && lambdaN2 > 25.0)
        {
            at.origin = "outgassed";
            pressureBar = clampd(retainedKg * toBar * 0.3, 0.05, 5.0);
            comp = {{"N2", 0.95}, {"CH4", 0.049}, {"H2", 0.001}};
            at.cloudPotential = 0.7;
        }
        else
        {
            // Cold or dry: CO2 dominates; condensation thins it on small worlds.
            at.origin = "outgassed";
            pressureBar = retainedKg * toBar * (massEarth < 0.3 ? 0.002 : 0.3);
            comp = {{"CO2", 0.95}, {"N2", 0.03}, {"Ar", 0.016}, {"CO", 0.001}};
            at.cloudPotential = 0.2;
        }
    }
    if (obs && obs->surfacePressurePa)
        pressureBar = *obs->surfacePressurePa / 1.0e5;
    if (obs && !obs->atmosphereComposition.empty())
        comp = obs->atmosphereComposition;
    normalise(comp);
    at.composition = comp;
    at.surfacePressurePa = pressureBar * 1.0e5;
    at.meanMolecularWeight = meanMolecularWeight(comp);
    mark("atmosphere", obs && (obs->surfacePressurePa || !obs->atmosphereComposition.empty()), Provenance::Generated);

    // ---- surface temperature --------------------------------------------
    if (!fluid && pressureBar > 0.5 && at.cloudPotential > 0.9)
        albedo = 0.75; // global sulphuric / water clouds
    if (obs && obs->bondAlbedo)
        albedo = *obs->bondAlbedo;
    b.bondAlbedo = albedo;
    b.equilibriumTemperatureK = teqZero * std::pow(1.0 - albedo, 0.25);
    double tau = pressureBar * (1.5 * fraction(comp, "CO2") + 0.7 * fraction(comp, "CH4") + 0.02 * fraction(comp, "N2"));
    const bool liquidPossible = !fluid && water > 1.0e-5 && pressureBar > 0.006;
    if (liquidPossible && b.equilibriumTemperatureK > 200.0 && b.equilibriumTemperatureK < 300.0)
        tau += 0.8 * clampd((b.equilibriumTemperatureK - 200.0) / 60.0, 0.0, 1.2); // water-vapour feedback
    double tSurf = fluid ? b.equilibriumTemperatureK
                         : b.equilibriumTemperatureK * std::pow(1.0 + 0.75 * tau, 0.25);
    tSurf = std::pow(std::pow(tSurf, 4.0) + heatFlux / StefanBoltzmann, 0.25);
    if (obs && obs->meanSurfaceTemperatureK)
        tSurf = *obs->meanSurfaceTemperatureK;
    b.meanSurfaceTemperatureK = tSurf;
    at.greenhouseWarmingK = std::max(0.0, tSurf - b.equilibriumTemperatureK);
    const double swing = (b.tidalLock == "synchronous" ? 0.8 : 0.45) / (1.0 + 5.0 * pressureBar);
    b.maxSurfaceTemperatureK = tSurf * (1.0 + swing);
    b.minSurfaceTemperatureK = tSurf * std::max(0.15, 1.0 - 1.3 * swing);
    b.temperatureRegime = tSurf >= 1000.0 ? "scorched" : tSurf >= 400.0 ? "hot" : tSurf >= 240.0 ? "temperate"
        : tSurf >= 150.0 ? "cold" : tSurf >= 60.0 ? "frozen" : "cryogenic";
    mark("mean_surface_temperature_k", obs && obs->meanSurfaceTemperatureK, Provenance::Derived);
    if (at.meanMolecularWeight > 0.0)
    {
        at.scaleHeightKm = Boltzmann * std::max(tSurf, 20.0) /
            (at.meanMolecularWeight * ProtonMassKg * b.surfaceGravityMs2) / 1000.0;
        at.outerRadiusKm = pressureBar > 0.0 ? b.radiusKm + at.scaleHeightKm * (fluid ? 20.0 : 12.0) : 0.0;
    }

    // ---- hydrosphere ------------------------------------------------------
    const double tBoil = boilingPointK(pressureBar);
    if (fluid)
        b.hydrosphere = "none";
    else if (water <= 1.0e-5)
        b.hydrosphere = "none";
    else if (tSurf > 647.0 && pressureBar > 221.0)
        b.hydrosphere = "supercritical";
    else if (tSurf >= tBoil)
        b.hydrosphere = pressureBar > 0.006 && at.origin != "none" ? "steam" : "none";
    else if (tSurf > 273.0 && pressureBar > 0.006)
        b.hydrosphere = water > 0.02 ? "global_ocean" : "surface_ocean";
    else
    {
        const bool warmInterior = heatFlux > 0.002 && b.radiusKm > 400.0;
        b.hydrosphere = water > 0.05 && warmInterior ? "subsurface_ocean" : "ice_shell";
        if (water < 0.01 && !warmInterior) b.hydrosphere = water > 1e-4 ? "ice_shell" : "none";
    }

    // ---- interior, tectonics, volcanism, magnetism ---------------------
    if (fluid)
    {
        b.tectonicRegime = "fluid";
        b.volcanism = "none";
        b.magneticFieldEarth = giant ? 20000.0 * mj * (10.0 / b.rotationPeriodHours)
                                     : 50.0 * (massEarth / 17.0) * (16.0 / b.rotationPeriodHours);
    }
    else
    {
        const bool icyBody = water > 0.1;
        if (icyBody)
            b.tectonicRegime = heatFlux > 0.01 ? "cryotectonic" : "none";
        else if (massEarth < 0.03)
            b.tectonicRegime = b.tidalHeatFluxWm2 > 0.3 ? "episodic" : heatFlux > 0.02 ? "stagnant_lid" : "none";
        else if (water > 1.0e-5 && massEarth > 0.3 && massEarth < 6.0 && heatFlux > 0.02 &&
                 (b.hydrosphere == "surface_ocean" || b.hydrosphere == "global_ocean"))
            b.tectonicRegime = surf.chance(0.75) ? "mobile_lid" : "stagnant_lid";
        else if (heatFlux > 0.02)
            b.tectonicRegime = surf.chance(0.4) ? "episodic" : "stagnant_lid";
        else
            b.tectonicRegime = "stagnant_lid";
        if (icyBody)
            b.volcanism = heatFlux > 0.01 ? "cryovolcanic" : "none";
        else
            b.volcanism = heatFlux > 1.0 ? "extreme" : heatFlux > 0.1 ? "high"
                : heatFlux > 0.05 ? "moderate" : heatFlux > 0.01 ? "low" : "none";
        const bool dynamo = cmf > 0.15 && heatFlux > 0.02 && massEarth > 0.2;
        b.magneticFieldEarth = dynamo ? clampd(std::sqrt(massEarth) * std::pow(24.0 / b.rotationPeriodHours, 0.3) *
                                                   surf.logNormal10(1.0, 0.2), 0.05, 5.0)
                                      : 0.0;
    }

    b.biosphere = obs && obs->biosphere ? *obs->biosphere : "none";

    // ---- surface generation descriptor ---------------------------------
    SurfaceGenerationDescriptor& sf = b.surface;
    const double g = b.surfaceGravityMs2;
    const bool liquidSurface = b.hydrosphere == "surface_ocean" || b.hydrosphere == "global_ocean";
    if (fluid)
    {
        sf.terrainProfile = giant ? "gas_giant_bands" : "ice_giant_bands";
        sf.weatherPotential = 1.0;
    }
    else
    {
        sf.resurfacingRate = b.tectonicRegime == "mobile_lid" ? 0.8 : b.volcanism == "extreme" ? 0.95
            : b.volcanism == "high" ? 0.8 : b.tectonicRegime == "episodic" ? 0.6
            : b.volcanism == "cryovolcanic" ? 0.5 : b.tectonicRegime == "stagnant_lid" ? 0.2 : 0.02;
        sf.craterDensity = clampd(env.ageGyr / 4.0 * (1.0 - sf.resurfacingRate) / (1.0 + pressureBar), 0.0, 1.0);
        sf.erosionPotential = clampd(0.3 * std::log10(1.0 + pressureBar * 10.0) + (liquidSurface ? 0.5 : 0.0), 0.0, 1.0);
        sf.weatherPotential = clampd(0.4 * std::log10(1.0 + pressureBar * 10.0) + at.cloudPotential * 0.4, 0.0, 1.0);
        sf.glaciationPotential = clampd((water > 1e-4 ? 0.5 : 0.0) + (tSurf < 273.0 ? 0.4 : 0.0) -
                                            (tSurf > 320.0 ? 0.9 : 0.0), 0.0, 1.0);
        const double tectonic = b.tectonicRegime == "mobile_lid" ? 1.0 : b.tectonicRegime == "episodic" ? 0.7
            : b.tectonicRegime == "cryotectonic" ? 0.5 : 0.3;
        sf.reliefAmplitudeKm = clampd(10.0 * (9.81 / std::max(g, 0.05)) * tectonic * surf.logNormal10(1.0, 0.15), 0.2, 30.0);
        sf.roughness = clampd(0.3 + 0.5 * sf.craterDensity + 0.2 * tectonic - 0.3 * sf.erosionPotential, 0.0, 1.0);
        sf.caveTectonicPropensity = clampd(0.6 * tectonic + 0.2 * sf.craterDensity, 0.0, 1.0);
        sf.lavaTubePropensity = clampd((b.volcanism == "none" ? 0.1 : 0.6) * (9.81 / std::max(g, 0.5)) / 3.0, 0.0, 1.0);
        sf.karstPropensity = liquidSurface ? clampd(0.5 + 0.5 * fraction(comp, "CO2") * 100.0, 0.0, 1.0) : 0.0;
        // Ocean coverage from the surface water inventory against the
        // relief. An observed water fraction is the hydrosphere itself; a
        // generated bulk fraction keeps most of a small inventory in the
        // mantle (~0.02 storage scale). Global equivalent layer
        // D = water * M / (rho_w 4 pi R^2); a basin hypsometry of relief H
        // fills 1 - exp(-4.6 D / H) of the surface, H >= 6 km of basin
        // depth (crustal dichotomy; Earth: D 2.7 km, H 10 km -> 0.71). Dry, Earth-like and ocean worlds all occur.
        const double surfaceWater = obs && obs->waterMassFraction ? water : water * water / (water + 0.02);
        const double layerKm = surfaceWater * massEarth * 5.9722e24 /
                               (1000.0 * 4.0 * 3.14159265358979 * b.radiusKm * b.radiusKm * 1.0e6) / 1000.0;
        sf.oceanCoverage = liquidSurface
            ? clampd(1.0 - std::exp(-4.6 * layerKm / std::max(6.0, sf.reliefAmplitudeKm)), 0.03, 1.0)
            : 0.0;
        sf.iceCoverage = tSurf < 273.0 && water > 1e-4 ? clampd(water / 0.05, 0.1, 1.0)
            : liquidSurface && tSurf < 290.0 ? 0.05 : 0.0;

        if (b.equilibriumTemperatureK > 1100.0 && b.tidalLock != "none") sf.terrainProfile = "lava_ocean";
        else if (b.equilibriumTemperatureK > 1100.0) sf.terrainProfile = "lava_ocean";
        else if (b.volcanism == "extreme") sf.terrainProfile = "volcanic_plains";
        else if (b.hydrosphere == "global_ocean") sf.terrainProfile = "global_ocean_archipelago";
        else if (liquidSurface && b.tectonicRegime == "mobile_lid") sf.terrainProfile = "plate_tectonic_continents";
        else if (liquidSurface) sf.terrainProfile = "ocean_basins_stagnant_crust";
        else if (fraction(comp, "CH4") > 0.01 && pressureBar > 0.05) sf.terrainProfile = "hydrocarbon_lakes";
        else if (b.volcanism == "cryovolcanic") sf.terrainProfile = "cryovolcanic_ice";
        else if (b.hydrosphere == "subsurface_ocean") sf.terrainProfile = "ice_shell_fractured";
        else if (water > 0.1) sf.terrainProfile = "cratered_ice";
        else if (pressureBar > 10.0) sf.terrainProfile = "volcanic_plains";
        else if (pressureBar > 0.003) sf.terrainProfile = "dune_desert";
        else sf.terrainProfile = "cratered_highlands";

        if (water > 0.1)
            sf.surfaceMaterials = {{"water_ice", 0.7}, {"silicate_dust", 0.15},
                                   {tSurf < 60.0 ? "nitrogen_ice" : "co2_ice", 0.1}, {"tholins", 0.05}};
        else if (sf.terrainProfile == "lava_ocean")
            sf.surfaceMaterials = {{"molten_silicate", 0.7}, {"basalt", 0.3}};
        else if (liquidSurface)
            sf.surfaceMaterials = {{"basalt", 0.35}, {"granite", 0.25}, {"sediment", 0.25}, {"carbonate", 0.15}};
        else
            sf.surfaceMaterials = {{"basalt", 0.45}, {"regolith", 0.35},
                                   {pressureBar > 0.003 ? "iron_oxide" : "anorthosite", 0.15}, {"sulfate", 0.05}};
    }

    // ---- render projection ---------------------------------------------
    std::string preset;
    if (giant) preset = "gas_giant_ammonia";
    else if (fluid) preset = "ice_giant_methane";
    else if (sf.terrainProfile == "lava_ocean") preset = "lava_world";
    else if (pressureBar > 10.0 && tSurf > 400.0) preset = "venusian_dense";
    else if (liquidSurface) preset = sf.oceanCoverage > 0.9 ? "ocean_world" : "terrestrial_temperate";
    else if (sf.terrainProfile == "hydrocarbon_lakes") preset = "methane_haze_world";
    else if (water > 0.1 && pressureBar > 1e-6 && fraction(comp, "N2") > 0.5) preset = "thin_nitrogen_ice_world";
    else if (water > 0.1) preset = "airless_icy";
    else if (b.volcanism == "extreme" || b.volcanism == "high") preset = "airless_volcanic";
    else if (pressureBar > 0.003 && tSurf < 260.0) preset = "cold_co2_world";
    else if (pressureBar > 0.003) preset = "desert_thin_atmosphere";
    else preset = "airless_rocky";
    b.environmentPresetId = preset;
}

} // namespace world::generation
