#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include "src/game/combat/WeaponCatalog.h"
#include "src/game/damage/HitComponent.h"

// Penetration model on a ship's runtime hit volumes (ship-local metres, the
// hull's own basis: +X right, +Y up, -Z forward).
//
// A volume is one layer of resistance: burning through it costs
// penetrationResistance x burnMJPerPen MJ (a structural link / seam with no
// resistance costs a nominal 0.5 MJ - it is cut). Armour reflects a part of a
// beam (armour / (armour + reflectArmourScale)).
//
// Laser: the absorbed power heats a spot on the first volume along the beam
// that is not holed there; conduction cools spots (coolingMW), so a weak
// laser on a thick plate may never get through. A spot whose heat reaches
// the burn-through energy leaves a hole and the beam goes on to the next
// volume. Absorbed energy is health loss (healthPerMJ).
//
// Penetrator (missile warheads, no fragmentation): a shaped (melting) jet or
// a kinetic slug walks the volumes along its line, paying each one's
// burn-through cost (a slug pays slugPenFactor more); it holes every volume
// it gets through. A jet is spent where its energy ends; a slug that stops
// stays stuck in that volume; one that gets through everything flies out.
namespace game::combat
{
struct RayHit
{
    int volume = -1;
    float tEnter = 0.0f;
    float tExit = 0.0f;
};

// Live volumes cut by the ray o + d t, 0 <= t <= maxT, by entry distance.
std::vector<RayHit> castRay(const game::damage::HitComponent& hit, const glm::vec3& o, const glm::vec3& d, float maxT);

// "module::<id>" or "link::<id>" - the key ShipCore::applyVolumeDamage takes.
std::string volumeKey(const game::damage::HitVolume& v);

// Burn-through cost of a volume (MJ).
double burnThroughMJ(const game::damage::HitVolume& v, const MaterialConstants& m);

// Per target ship: heated spots, holes, stuck slugs and the health loss not
// yet handed to the ShipCore.
struct BurnState
{
    struct Spot
    {
        glm::vec3 p {0.0f};
        double heatMJ = 0.0;
    };
    struct Hole
    {
        std::string key;
        glm::vec3 p {0.0f};
        float radiusM = 0.1f;
    };
    struct Stuck
    {
        std::string key;
        glm::vec3 p {0.0f};
        double massKg = 0.0;
    };
    std::unordered_map<std::string, Spot> spots;
    std::vector<Hole> holes;
    std::vector<Stuck> stuck;
    std::unordered_map<std::string, double> pendingHealthLoss;

    bool holed(const std::string& key, const glm::vec3& p) const;
    void cool(double dt, const MaterialConstants& m);
};

struct BeamHit
{
    bool hit = false;
    glm::vec3 pointLocal {0.0f};
    float distanceM = 0.0f;
    std::string key, moduleId;
    double absorbedMJ = 0.0;
    double spotHeatMJ = 0.0, burnMJ = 0.0;
    int burnedThrough = 0;   // volumes holed during this step
};

// One step of a beam (powerMW for dt). Mutates the burn state only (spots,
// holes, pending health loss); ShipCore damage follows with the pending loss.
BeamHit stepBeam(const game::damage::HitComponent& hit, BurnState& burn, const glm::vec3& o, const glm::vec3& d, float maxT,
                 double powerMW, double dt, double beamRadiusM, const MaterialConstants& m);

struct PenetrationResult
{
    struct Layer
    {
        std::string key, moduleId;
        double spentMJ = 0.0;
        bool holed = false;
        glm::vec3 entry {0.0f};
    };
    std::vector<Layer> layers;
    bool hitHull = false;
    bool stuck = false;            // slug: stopped inside (Layer back())
    bool exited = false;           // got through every volume on its line
    double residualMJ = 0.0;
    glm::vec3 stopLocal {0.0f};
};

PenetrationResult penetrate(const game::damage::HitComponent& hit, BurnState& burn, const glm::vec3& o, const glm::vec3& d,
                            float maxT, double energyMJ, WarheadKind kind, double slugKg, const MaterialConstants& m);
} // namespace game::combat
