#pragma once

#include <string>
#include <vector>

#include "src/world/generation/UniverseDescriptors.h"

namespace world::generation
{

/*
    Planet formation layer: one protoplanetary disk per planet host (a star
    for S-type planets, a binary barycenter for circumbinary P-type planets)
    and a correlated planetary architecture grown from it.

    Causal chain (cheap analytic stand-ins for the Bern global model):
      disk mass / metallicity / lifetime / snow line
        -> oligarchic embryos (isolation mass, Kokubo & Ida 1998/2002)
        -> core accretion of gas (critical core ~10 M_earth, Ikoma 2000)
        -> type I migration into resonant chains, type II hot/warm giants
        -> post-gas giant scattering (ejections feed the rogue population)
        -> giant-impact consolidation of small planets (Kepler spacing)
        -> observational conditioning (anchors, RV detection limits)
        -> stability gates and repair.
*/

struct PlanetHost
{
    std::string nodeId;             // star key or barycenter id
    std::string bodyId;             // runtime parent id
    bool circumbinary = false;
    std::string lightStarKey;       // star whose light dominates
    double massSolar = 1.0;         // present-day host mass (gravity)
    double initialMassSolar = 1.0;  // mass at formation (disk)
    double zamsLuminositySolar = 1.0;
    double luminositySolar = 1.0;   // present-day total luminosity
    double radiusSolar = 1.0;
    double metallicityFeH = 0.0;
    double ageGyr = 4.6;
    double innerLimitAu = 0.0;      // circumbinary edge / stellar surface
    double outerLimitAu = 1.0e5;    // Holman-Wiegert S-type limit
    StellarStage stage = StellarStage::MainSequence;
    double rvDetectionLimitMs = 0.0; // 0 = not surveyed, no conditioning
    double rvBaselineDays = 0.0;
    std::uint64_t seed = 0;
};

struct PlanetCandidate
{
    double aAu = 0.0;
    double massEarth = 0.0;
    double coreMassEarth = 0.0;
    double envelopeFraction = 0.0;
    double formationAu = 0.0;
    double waterFraction = 0.0;     // of the solid core
    double e = 0.0;
    double iDeg = 0.0;
    double nodeDeg = 0.0;
    double periDeg = 0.0;
    double meanAnomalyDeg = 0.0;
    bool giant = false;
    bool anchor = false;
    bool resonantChain = false;
    std::string formationOrigin = "in_situ";
    int anchorIndex = -1;           // into the host's observed bodies
    ProvenanceMap provenance;
};

struct FormationResult
{
    DiskDescriptor disk;
    std::vector<PlanetCandidate> planets;  // sorted by a
    std::vector<PlanetCandidate> ejected;  // rogue planets born here
    std::string architectureMode;
    // Solids left where planets did not finish growing (reservoir inputs).
    double leftoverInnerSolidsEarth = 0.0;
    double leftoverOuterSolidsEarth = 0.0;
    double outerPlanetesimalEdgeAu = 0.0;
    int attempts = 0;
};

FormationResult formPlanets(
    const PlanetHost& host,
    const GalacticContextDescriptor& context,
    const std::vector<const ObservedBodyInput*>& anchors,
    bool observedSystemComplete,
    std::vector<StabilityCheck>& checks,
    std::vector<std::string>& warnings);

// Mutual Hill radius in AU of two bodies (Earth masses) about a host (solar masses).
double mutualHillRadiusAu(double a1, double m1Earth, double a2, double m2Earth, double hostMassSolar);

// Radial-velocity semi-amplitude in m/s.
double radialVelocityAmplitudeMs(double massEarth, double periodDays, double hostMassSolar, double e);

} // namespace world::generation
