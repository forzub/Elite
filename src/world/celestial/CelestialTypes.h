#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "src/world/coordinates/WorldPosition.h"

namespace world::celestial
{

inline constexpr double MetersPerAu = 149597870700.0;
inline constexpr double SecondsPerDay = 86400.0;

enum class BodyType
{
    Star,
    Planet,
    Moon,
    AsteroidBelt,
    Station,
    Unknown
};

inline std::string toString(BodyType type)
{
    switch (type)
    {
        case BodyType::Star:         return "star";
        case BodyType::Planet:       return "planet";
        case BodyType::Moon:         return "moon";
        case BodyType::AsteroidBelt: return "asteroid_belt";
        case BodyType::Station:      return "station";
        default:                     return "unknown";
    }
}

struct StarSystemSummary
{
    int id = -1;
    std::string name;

    glm::dvec3 positionLy {0.0};
    int starsCount = 1;
    std::string starType;
};

struct CelestialRingDefinition
{
    std::string name;

    double innerRadiusKm = 0.0;
    double outerRadiusKm = 0.0;

    std::string composition;
};

struct CelestialBodyDefinition
{
    std::string id;
    std::string name;
    BodyType type = BodyType::Unknown;

    std::string parentId;

    double radiusKm = 0.0;
    double diameterKm = 0.0;

    double distanceAu = 0.0;
    double orbitalPeriodDays = 0.0;
    double dayLengthHours = 0.0;

    glm::dvec3 staticPositionAu {0.0};

    std::vector<std::string> alternativeNames;
    std::vector<CelestialRingDefinition> rings;
};

struct CelestialSystemDefinition
{
    int systemId = -1;
    std::string name;

    glm::dvec3 barycenterAu {0.0};

    std::vector<CelestialBodyDefinition> bodies;
};



struct CelestialBodyState
{
    std::string id;
    std::string name;
    BodyType type = BodyType::Unknown;

    std::string parentId;

    glm::dvec3 positionAu {0.0};
    glm::dvec3 worldMeters {0.0};

    double radiusKm = 0.0;
    double orbitalPhaseRad = 0.0;
    double rotationPhaseRad = 0.0;
    std::vector<CelestialRingDefinition> rings;
};

struct CelestialSystemSnapshot
{
    int systemId = -1;
    std::string systemName;
    double simTimeSeconds = 0.0;

    std::vector<CelestialBodyState> bodies;
};

struct PlayerNavigationState
{
    int currentSystemId = 0;

    world::coordinates::WorldPosition worldPosition;
    glm::mat4 orientation {1.0f};

    glm::dvec3 systemLocalMeters {0.0};
    glm::dvec3 systemLocalAu {0.0};

    glm::vec3 forward {0.0f, 0.0f, -1.0f};
    glm::vec3 up {0.0f, 1.0f, 0.0f};
};

} // namespace world::celestial