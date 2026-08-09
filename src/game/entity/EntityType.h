#pragma once

#include <cstdint>

namespace game::entity
{

enum class EntityType : std::uint8_t
{
    Unknown = 0,
    Ship,
    CelestialBody,
    Hub,
    StationModule,
    Asteroid,
    Projectile,
    FleetAggregate,
    DiagnosticProbe
};

} // namespace game::entity
