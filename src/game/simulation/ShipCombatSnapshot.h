#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace game::simulation
{
// A laser beam this tick: from the muzzle to the hit point (or its range).
struct CombatBeamSnapshot
{
    world::coordinates::WorldPosition from;
    world::coordinates::WorldPosition to;
    float powerMW = 0.0f;
    std::uint8_t hit = 0;
    glm::vec3 colour {1.0f, 0.3f, 0.2f};
};

// A missile in flight (owned by the shooter's snapshot).
struct CombatMissileSnapshot
{
    std::uint32_t id = 0;
    world::coordinates::WorldPosition position;
    glm::vec3 velocityMps {0.0f};   // relative to the shooter at launch frame (direction for the render)
    std::string specId;
};

// A hit on this ship (recent ones, by serial; the client flashes each once).
struct CombatImpactSnapshot
{
    std::uint32_t serial = 0;
    world::coordinates::WorldPosition where;
    float energyMJ = 0.0f;
    std::uint8_t kind = 0;          // 0 laser burn, 1 laser burn-through, 2 jet, 3 slug, 4 collision
    std::string moduleId;
};

struct ShipCombatSnapshot
{
    std::string laserId, missileId;
    std::int32_t missiles = 0;
    std::uint8_t laserFiring = 0;
    std::vector<CombatBeamSnapshot> beams;
    std::vector<CombatMissileSnapshot> missilesInFlight;
    std::vector<CombatImpactSnapshot> impacts;
};
} // namespace game::simulation
