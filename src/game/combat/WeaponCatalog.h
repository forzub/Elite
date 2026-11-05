#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>

// Weapons data (assets/data/combat/weapons.json): lasers, missiles with
// burning-through warheads only (shaped melting jet / kinetic slug), the
// material constants of the penetration model and the default loadout.
namespace game::combat
{
struct MaterialConstants
{
    double reflectArmourScale = 25.0;   // reflected fraction = armour / (armour + scale)
    double coolingMW = 0.15;            // conduction from a heated spot
    double burnMJPerPen = 0.5;          // MJ to burn through one unit of penetrationResistance
    double healthPerMJ = 2.0;           // volume health lost per absorbed MJ
    double slugPenFactor = 1.6;         // a slug spends more than a jet per unit of resistance
    double spotMergeM = 0.6;            // a beam within this of the last spot heats the same spot
};

struct LaserSpec
{
    std::string id, name;
    double powerMW = 1.0;
    double duty = 1.0;        // pulsed: the fraction of time the beam is on
    double pulseHz = 0.0;     // 0 = continuous
    double rangeM = 3000.0;
    double beamRadiusM = 0.1;
    glm::vec3 colour {1.0f, 0.3f, 0.2f};
};

enum class WarheadKind { Shaped, Slug };

struct MissileSpec
{
    std::string id, name;
    WarheadKind warhead = WarheadKind::Shaped;
    double jetMJ = 0.0;        // shaped
    double slugKg = 0.0;       // slug
    double massKg = 60.0;
    double accelMps2 = 300.0;
    double maxSpeedMps = 1500.0;
    double fuelS = 12.0;
    double turnDps = 90.0;
    double seekerDeg = 30.0;
    double rangeM = 8000.0;
};

struct WeaponCatalog
{
    MaterialConstants materials;
    std::vector<LaserSpec> lasers;
    std::vector<MissileSpec> missiles;
    std::string defaultLaser, defaultMissile;
    int defaultMissiles = 4;

    const LaserSpec* laser(const std::string& id) const;
    const MissileSpec* missile(const std::string& id) const;
};

// Loaded once (assets/data/combat/weapons.json, then src/assets/...); built-in
// defaults when the file is missing.
const WeaponCatalog& weaponCatalog();
} // namespace game::combat
