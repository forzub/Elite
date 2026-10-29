#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    Satellites and small bodies, generated after the planets they belong to.

    Moons (Canup & Ward 2006 satellite mass scaling ~1e-4 M_planet; Domingos
    2006 Hill stability 0.49 / 0.93 R_H; fluid Roche limit), rings (inside the
    Roche zone of their material, fed by disrupted moons / collisions), and
    statistical small-body reservoirs (never millions of bodies: the largest
    few are materialised as dwarf planets, the rest are a population).
*/

struct MoonCandidate
{
    double massEarth = 0.0;
    double aKm = 0.0;
    double e = 0.0;
    double iDeg = 0.0;              // to the planet equator
    bool retrograde = false;
    double waterFraction = 0.0;
    std::string origin;             // regular | giant_impact | captured | ring_derived
};

std::vector<MoonCandidate> generateMoons(
    const BodyDescriptor& planet,
    double hostMassKg,
    double snowLineAu,
    double ageGyr,
    std::uint64_t planetSeed,
    std::vector<StabilityCheck>& checks);

std::vector<RingDescriptor> generateRings(
    const BodyDescriptor& planet,
    const std::vector<const BodyDescriptor*>& moons,
    double snowLineAu,
    std::uint64_t planetSeed);

struct ReservoirHostInput
{
    std::string hostBodyId;
    std::string hostName;
    double hostMassSolar = 1.0;
    double luminositySolar = 1.0;
    double ageGyr = 4.6;
    double snowLineAu = 2.7;
    double outerLimitAu = 1.0e5;
    double outerPlanetesimalEdgeAu = 100.0;
    double leftoverInnerSolidsEarth = 0.0;
    double leftoverOuterSolidsEarth = 0.0;
    std::uint64_t seed = 0;
};

struct DwarfPlanetCandidate
{
    std::string reservoirId;
    double massEarth = 0.0;
    double aAu = 0.0;
    double e = 0.0;
    double iDeg = 0.0;
    double waterFraction = 0.0;
};

void generateReservoirs(
    const ReservoirHostInput& host,
    const std::vector<const BodyDescriptor*>& planets,
    std::vector<ReservoirDescriptor>& reservoirs,
    std::vector<DwarfPlanetCandidate>& dwarfs);

std::string romanNumeral(int n);

} // namespace world::generation
