#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/generation/NaturalResources.h"
#include "src/world/generation/GenerationVersions.h"

namespace world::generation
{

/*
    Data model of the universe generator.

    INPUT  (authored / observed, owned by the catalog author, never written by
            the generator): system identity, galactic position, stellar
            components, stellar hierarchy, observational anchors.
    OUTPUT (owned by the generator, regenerated as a whole): galactic context,
            present-day stellar state, barycentric orbits, disk, planets,
            moons, rings, small-body reservoirs, stability report.

    Every output quantity that may come from observation carries its origin
    in `provenance` (field -> origin), so observed, literature, derived and
    generated values are never confused.
*/

// Origin of a value.
enum class Provenance
{
    Observed,     // measured, taken from an observational source
    Literature,   // compiled literature value (not a measurement we pinned)
    Authored,     // authored constraint (Sol bodies, quest anchors)
    Derived,      // physical law applied to other values
    Generated,    // sampled by the generator from its population model
};

const char* toString(Provenance p);
Provenance provenanceFromString(const std::string& s);

using ProvenanceMap = std::map<std::string, Provenance>;

// Classical elements about the parent (star, barycenter or planet), in the
// parent's reference frame: the system frame for stars/planets (XZ = the
// system invariable plane, +Y = north), the planet equator for moons.
struct KeplerOrbit
{
    double semiMajorAxisAu = 0.0;
    double eccentricity = 0.0;
    double inclinationDeg = 0.0;
    double longitudeAscendingNodeDeg = 0.0;
    double argumentOfPeriapsisDeg = 0.0;
    double meanAnomalyAtEpochDeg = 0.0;
    double epochUniverseSeconds = 0.0;  // UniverseClock seconds of the epoch
    double periodDays = 0.0;            // derived from mu of parent + body
    int direction = 1;                  // +1 prograde, -1 retrograde (i > 90)
};

// ---------------------------------------------------------------- input --

struct StarInput
{
    std::string key;                // stable component key, e.g. "alpha_centauri_a"
    std::string name;
    std::vector<std::string> alternativeNames;
    std::string spectralType;
    std::string stageHint;          // main_sequence | subgiant | giant | white_dwarf | brown_dwarf

    std::optional<double> massSolar;
    std::optional<double> radiusSolar;
    std::optional<double> teffK;
    std::optional<double> luminositySolar;
    std::optional<double> metallicityFeH;
    std::optional<double> ageGyr;
    std::optional<double> rotationPeriodDays;

    std::map<std::string, std::string> sources; // field -> source text
    nlohmann::json extra = nlohmann::json::object(); // lore etc., passed through
};

struct HierarchyNodeInput
{
    std::string id;                 // "system_barycenter", "ab", or a star key
    std::string starKey;            // non-empty for a leaf
    std::vector<HierarchyNodeInput> children; // exactly two for a binary node

    // Orbit of the two children about each other (relative orbit).
    std::optional<double> semiMajorAxisAu;
    std::optional<double> eccentricity;
    std::optional<double> periodYears;
    std::optional<double> inclinationDeg;     // to the system reference plane
    std::optional<double> projectedSeparationAu;
    std::string orbitSource;
};

struct ObservedRingInput
{
    std::string name;
    double innerRadiusKm = 0.0;
    double outerRadiusKm = 0.0;
    double opticalDepth = 0.0;
    double iceFraction = 0.0;
    std::string composition;
};

// One observed or authored body. Missing values are completed by the
// generator; present values are constraints that the generator honours.
struct ObservedBodyInput
{
    std::string designation;        // canonical name, also the id leaf
    std::vector<std::string> alternativeNames;
    std::string kind;               // planet | dwarf_planet | moon
    std::string host;               // star key, barycenter id or parent designation (moon)
    std::string source;

    std::optional<double> semiMajorAxisAu;
    std::optional<double> periodDays;
    std::optional<double> eccentricity;
    std::optional<double> inclinationDeg;
    std::optional<double> longitudeAscendingNodeDeg;
    std::optional<double> argumentOfPeriapsisDeg;
    std::optional<double> meanAnomalyAtEpochDeg;
    std::optional<double> epochUniverseSeconds; // epoch of the elements on the UniverseClock
    bool retrograde = false;

    std::optional<double> massEarth;
    std::optional<double> minimumMassEarth;   // m sin i (radial velocity)
    std::optional<double> radiusEarth;
    std::optional<double> rotationPeriodHours;
    std::optional<double> obliquityDeg;
    std::optional<double> bondAlbedo;
    std::optional<double> surfacePressurePa;
    std::optional<double> meanSurfaceTemperatureK;
    std::vector<std::pair<std::string, double>> atmosphereComposition;
    std::optional<bool> tidallyLocked;
    std::optional<double> waterMassFraction;
    std::optional<double> ironCoreFraction;
    std::optional<double> envelopeMassFraction;
    std::vector<ObservedRingInput> rings;

    nlohmann::json extra = nlohmann::json::object(); // visual/lore keys passed through
};

struct SystemInput
{
    int id = -1;
    std::string name;
    std::string slug;               // directory name part
    std::vector<std::string> alternativeNames;
    std::string catalogScope = "local";
    bool atlasVisible = true;
    bool routeTarget = true;
    std::string questRole;
    glm::dvec3 positionLy {0.0};    // ICRS Cartesian light-years from Sol
    std::string positionSource;

    std::vector<StarInput> stars;
    HierarchyNodeInput hierarchy;   // root
    std::vector<ObservedBodyInput> observedBodies;
    // Catalogued designations (incl. controversial / refuted planets) that a
    // procedural planet must not reuse.
    std::vector<std::string> reservedDesignations;

    // true: the observed bodies are the complete system (Sol); the generator
    // derives their physics but adds no procedural planets.
    bool observedSystemComplete = false;

    // Survey completeness of a real star: no procedural planet may exceed the
    // radial-velocity amplitude a survey of this baseline would have detected.
    // Absent for procedural / synthetic systems (no conditioning).
    std::optional<double> rvDetectionLimitMs;
    std::optional<double> rvBaselineDays;
    // Per-star override of the RV survey (star key -> limit m/s, baseline d):
    // the best-studied stars have literature limits well below the default,
    // brown dwarfs and white dwarfs have no precision RV survey (limit 0).
    std::map<std::string, std::pair<double, double>> hostRvSurvey;
    // Transit survey of an edge-on system: no coplanar procedural planet with
    // a shorter period and a larger radius would have escaped detection.
    std::optional<double> transitRadiusLimitEarth;
    std::optional<double> transitBaselineDays;

    nlohmann::json passthrough = nlohmann::json::object();
};

// --------------------------------------------------------------- output --

enum class GalacticPopulation
{
    ThinDisk,
    ThickDisk,
    Halo,
};

const char* toString(GalacticPopulation p);

struct GalacticContextDescriptor
{
    GalacticPopulation population = GalacticPopulation::ThinDisk;
    glm::dvec3 galactocentricKpc {0.0};   // X toward the centre from Sol, Z north
    double galactocentricRadiusKpc = 0.0;
    double heightAbovePlaneKpc = 0.0;
    double populationMetallicityMeanFeH = 0.0;
    double populationMetallicitySigmaDex = 0.2;
    double alphaEnhancement = 0.0;        // [alpha/Fe]
    double localStellarDensityPerPc3 = 0.1;
    double formationEnvironmentDensity = 1.0; // cluster density factor (disk truncation)
    double interstellarDensityPerCm3 = 0.1;
};

enum class StellarStage
{
    PreMainSequence,
    MainSequence,
    Subgiant,
    RedGiant,
    WhiteDwarf,
    BrownDwarf,
};

const char* toString(StellarStage s);

struct StarDescriptor
{
    std::string key;
    std::string id;                 // runtime body id
    std::string name;
    std::vector<std::string> alternativeNames;
    std::string spectralType;
    StellarStage stage = StellarStage::MainSequence;
    std::uint64_t seed = 0;

    double massSolar = 1.0;
    double initialMassSolar = 1.0;
    double radiusSolar = 1.0;
    double teffK = 5772.0;
    double luminositySolar = 1.0;
    double metallicityFeH = 0.0;
    double ageGyr = 4.6;
    double mainSequenceLifetimeGyr = 10.0;
    double rotationPeriodDays = 25.0;
    double muM3s2 = 0.0;

    // Barycentric orbit of this star (empty for a single star).
    std::string parentNode;
    std::optional<KeplerOrbit> orbit;

    // Stable planet zone around this star alone (S-type), AU.
    double stableZoneInnerAu = 0.0;
    double stableZoneOuterAu = 0.0;

    ProvenanceMap provenance;
    std::map<std::string, std::string> sources;
    nlohmann::json extra = nlohmann::json::object();
};

struct BarycenterDescriptor
{
    std::string id;                 // node id, e.g. "ab", "system_barycenter"
    std::string bodyId;             // runtime id "system_<n>.<id>"
    std::string parentNode;         // empty for the root
    std::vector<std::string> children; // node ids or star keys
    double massSolar = 0.0;
    std::optional<KeplerOrbit> orbit; // orbit of this barycenter about its parent
    // Relative orbit of the two children, the observable binary orbit.
    std::optional<KeplerOrbit> relativeOrbit;
    // Circumbinary (P-type) stable inner edge, AU; 0 for the root of a single star.
    double circumbinaryInnerAu = 0.0;
    ProvenanceMap provenance;
};

struct DiskDescriptor
{
    std::string hostNode;           // star key or barycenter id
    double gasMassSolar = 0.0;
    double solidMassEarth = 0.0;
    double dustToGas = 0.01;
    double innerEdgeAu = 0.05;
    double characteristicRadiusAu = 50.0;
    double outerTruncationAu = 0.0; // 0 = not truncated
    double lifetimeMyr = 3.0;
    double snowLineAu = 2.7;
    double turbulenceAlpha = 1.0e-3;
    std::string migrationRegime;    // weak | moderate | strong
};

struct CompositionDescriptor
{
    double ironCore = 0.0;
    double silicateMantle = 0.0;
    double waterIce = 0.0;
    double otherVolatiles = 0.0;    // CO, CO2, CH4, NH3, N2 ices
    double hydrogenHeliumEnvelope = 0.0;
};

struct AtmosphereDescriptor
{
    double surfacePressurePa = 0.0;
    std::vector<std::pair<std::string, double>> composition; // volume mixing ratios
    double meanMolecularWeight = 0.0;
    double scaleHeightKm = 0.0;
    double greenhouseWarmingK = 0.0;
    double cloudPotential = 0.0;    // 0..1
    std::string origin;             // none | primordial | outgassed | captured | steam | sublimated
    double retentionIndex = 0.0;    // >1 retains heavy gases over its age
    double outerRadiusKm = 0.0;     // top of the atmosphere for navigation (~10 scale heights)
};

struct RingDescriptor
{
    std::string id;
    std::string name;
    double innerRadiusKm = 0.0;
    double outerRadiusKm = 0.0;
    double massKg = 0.0;
    double opticalDepth = 0.0;
    double iceFraction = 0.0;
    double rockFraction = 0.0;
    double dustFraction = 0.0;
    double ageMyr = 0.0;
    double planeInclinationDeg = 0.0; // to the planet equator
    std::string origin;             // primordial | disrupted_moon | collisional | dusty
    std::string visibilityClass;    // main | secondary | faint | diffuse
    std::vector<std::pair<double, double>> gapsKm; // resonant gaps
};

struct SurfaceGenerationDescriptor
{
    std::string terrainProfile;     // e.g. cratered_highlands, plate_tectonic, cryovolcanic
    double reliefAmplitudeKm = 0.0;
    double roughness = 0.0;         // 0..1
    double craterDensity = 0.0;     // 0..1 (saturation = 1)
    double resurfacingRate = 0.0;   // 0..1
    double erosionPotential = 0.0;  // 0..1
    double glaciationPotential = 0.0;
    double caveTectonicPropensity = 0.0;
    double lavaTubePropensity = 0.0;
    double karstPropensity = 0.0;
    double weatherPotential = 0.0;
    double oceanCoverage = 0.0;     // surface fraction
    double iceCoverage = 0.0;
    std::vector<std::pair<std::string, double>> surfaceMaterials;
};

struct BodyDescriptor
{
    std::string id;                 // runtime id, deterministic
    std::string designation;
    std::vector<std::string> alternativeNames;
    std::string kind;               // planet | dwarf_planet | moon
    std::string parentId;           // star / barycenter / planet runtime id
    std::string hostStarKey;        // star whose light dominates (thermal)
    std::string origin;             // observed_anchor | authored | generated
    std::string formationOrigin;    // in_situ | migrated | scattered | captured | giant_impact | regular | ring_derived
    std::uint64_t seed = 0;

    KeplerOrbit orbit;

    double massKg = 0.0;
    double radiusKm = 0.0;
    double densityKgM3 = 0.0;
    double muM3s2 = 0.0;
    double surfaceGravityMs2 = 0.0;
    double escapeVelocityKms = 0.0;
    double hillRadiusKm = 0.0;
    double sphereOfInfluenceKm = 0.0;

    std::string bodyClass;          // rocky | iron_rich | water_rich | sub_neptune | ice_giant | gas_giant | super_jupiter | dwarf_icy | dwarf_rocky
    CompositionDescriptor composition;
    double waterMassFraction = 0.0;
    double formationDistanceAu = 0.0;

    double instellationEarth = 0.0;
    double equilibriumTemperatureK = 0.0;
    double meanSurfaceTemperatureK = 0.0;
    double minSurfaceTemperatureK = 0.0;
    double maxSurfaceTemperatureK = 0.0;
    std::string temperatureRegime;  // scorched | hot | temperate | cold | frozen | cryogenic
    double bondAlbedo = 0.3;
    double habitableZonePosition = 0.0; // a / HZ inner (1 = runaway edge)

    double radiogenicHeatFluxWm2 = 0.0;
    double tidalHeatFluxWm2 = 0.0;
    std::string tectonicRegime;     // none | stagnant_lid | episodic | mobile_lid | cryotectonic | fluid
    std::string volcanism;          // none | low | moderate | high | extreme | cryovolcanic
    double magneticFieldEarth = 0.0;
    std::string hydrosphere;        // none | subsurface_ocean | ice_shell | surface_ocean | global_ocean | supercritical | steam
    AtmosphereDescriptor atmosphere;

    double rotationPeriodHours = 0.0;
    double obliquityDeg = 0.0;
    int rotationDirection = 1;
    std::string tidalLock;          // none | synchronous | spin_orbit_3_2
    double axisNodeDeg = 0.0;

    SurfaceGenerationDescriptor surface;
    std::vector<RingDescriptor> rings;
    std::string environmentPresetId; // render projection of the descriptor

    // Natural endowment / industrial accessibility / habitability / fuel,
    // derived from everything above (NaturalResources.h).
    ResourceProfiles resources;

    ProvenanceMap provenance;
    std::string source;
    nlohmann::json extra = nlohmann::json::object();
};

struct ReservoirDescriptor
{
    std::string id;
    std::string name;
    std::string kind;               // asteroid_belt | debris_disk | kuiper_belt | scattered_disk | comet_cloud | trojans | irregular_swarm
    std::string parentId;           // star / barycenter / planet id (trojans)
    std::uint64_t seed = 0;
    double innerAu = 0.0;
    double outerAu = 0.0;
    double peakAu = 0.0;
    double totalMassEarth = 0.0;
    double eccentricityMean = 0.0;
    double inclinationMeanDeg = 0.0;
    double sizeSlope = -3.5;        // dN/dD ~ D^slope
    double minBodyKm = 0.1;
    double maxBodyKm = 100.0;
    double clumpiness = 0.0;        // 0..1
    std::vector<std::pair<std::string, double>> composition;
    std::vector<std::pair<double, double>> gapsAu;
    std::string resonance;          // e.g. "1:1 with <planet>"
    double estimatedBodiesAbove1Km = 0.0;
    std::string formationOrigin;
    ProvenanceMap provenance;
    ResourceProfiles resources;
};

struct StabilityCheck
{
    std::string name;
    bool passed = true;
    std::string detail;
};

struct SystemDescriptor
{
    int systemId = -1;
    std::string name;
    std::uint64_t universeSeed = 0;
    std::uint64_t systemSeed = 0;
    UniverseGenerationVersions versions;
    int generationAttempt = 0;

    GalacticContextDescriptor context;
    std::vector<StarDescriptor> stars;
    std::vector<BarycenterDescriptor> barycenters;
    std::vector<DiskDescriptor> disks;
    std::string architectureMode;
    std::vector<BodyDescriptor> bodies;      // parents before children
    std::vector<ReservoirDescriptor> reservoirs;
    std::vector<StabilityCheck> stability;
    std::vector<std::string> warnings;
};

} // namespace world::generation
