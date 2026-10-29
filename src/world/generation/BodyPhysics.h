#pragma once

#include <cstdint>

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    CelestialBodyGenerator: turns a formed planet / moon (mass, core, envelope,
    water, formation distance) into a physically defined world. Class is a
    consequence of composition, never a dice roll.

    Models (confidence):
    - rocky radius: Zeng et al. 2016 R = (1.07 - 0.21 cmf) M^(1/3.7) (EmpiricalFit)
    - envelope radius: Lopez & Fortney 2014 (EmpiricalFit)
    - giant heavy elements: Thorngren et al. 2016 M_Z = 57.9 M^0.61 (EmpiricalFit)
    - habitable zone: Kopparapu et al. 2014 (EmpiricalFit)
    - XUV escape: energy-limited, saturated activity L_X/L_bol = 10^-3.13 (PopulationModel)
    - grey greenhouse calibrated on Venus / Earth / Mars (Heuristic)
    - tidal locking: Gladman et al. 1996 timescale (PhysicalLaw + assumed Q, k2)
    - tidal heating: Peale & Cassen 1978 (PhysicalLaw + assumed Q, k2)
    - tectonic regime, dynamo, surface descriptors (Heuristic)
*/

struct BodySeedProperties
{
    double massEarth = 1.0;
    double coreMassEarth = 1.0;
    double envelopeFraction = 0.0;  // H/He mass fraction
    double waterFraction = 0.0;     // of the solid core
    double formationAu = 1.0;
    double ironFraction = -1.0;     // < 0: generate
};

struct BodyEnvironment
{
    double ageGyr = 4.6;
    double metallicityFeH = 0.0;
    double gravitationalHostMassKg = 0.0; // star / barycenter (planets) or planet (moons)
    double orbitAu = 1.0;                 // about the gravitational host
    double eccentricity = 0.0;
    double lightLuminositySolar = 1.0;    // all stars, seen from the body
    double lightDistanceAu = 1.0;         // effective distance to the light
    double lightTeffK = 5772.0;
    double lightMassSolar = 1.0;          // XUV activity duration
    double snowLineAu = 2.7;
    bool isMoon = false;
    double parentPlanetMassKg = 0.0;
    double parentPlanetRadiusKm = 0.0;
};

void deriveBodyPhysics(
    BodyDescriptor& body,
    const BodySeedProperties& seedProps,
    const BodyEnvironment& env,
    const ObservedBodyInput* observed,
    std::uint64_t seed);

// Kopparapu 2014 runaway-greenhouse (inner) and maximum-greenhouse (outer)
// habitable-zone distances in AU.
void habitableZoneAu(double luminositySolar, double teffK, double& innerAu, double& outerAu);

} // namespace world::generation
