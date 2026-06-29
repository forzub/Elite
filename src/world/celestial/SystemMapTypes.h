#pragma once

#include <string>
#include <vector>

#include <glm/glm.hpp>
#include "src/scene/EntityId.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/types/ObjectType.h"

namespace world::celestial
{
    struct GalaxyMapSystem
    {
        int id = -1;

        std::string name;
        std::string starType;

        int starsCount = 1;

        glm::dvec3 positionLy {0.0};
        std::string jurisdiction;
    };

    struct GalaxyMapSnapshot
    {
        double universeTimeSeconds = 0.0;
        std::string universeDate;

        std::vector<GalaxyMapSystem> systems;
    };

    struct SystemMapRing
    {
        std::string name;

        double innerRadiusKm = 0.0;
        double outerRadiusKm = 0.0;

        std::string composition;
    };

    struct SystemMapBody
    {
        std::string id;
        std::string name;
        std::vector<CelestialBodyDisplayName> alternativeNames;
  
        std::string parentId;

        BodyType type = BodyType::Unknown;

        glm::dvec3 positionAu {0.0};

        // Готовые данные для рендера орбиты.
        // Рендерер больше не должен угадывать центр орбиты по parentId.
        glm::dvec3 orbitCenterAu {0.0};
        double orbitRadiusAu = 0.0;
        bool drawOrbit = false;

        double radiusKm = 0.0;
        double rotationPhaseRad = 0.0;
        double dayLengthHours = 0.0;

        double axialTiltDeg = 0.0;
        double axisNodeDeg = 0.0;
        double textureLongitudeOffsetDeg = 0.0;

        // Пока цвет fallback-овый.
        // Потом можно брать из system_details.json, если поле цвета есть.
        glm::vec4 color {0.6f, 0.8f, 1.0f, 1.0f};

        std::vector<SystemMapRing> rings;
    };

    enum class SystemMapObjectKind
    {
        Unknown,
        Station,
        Mine,
        Buoy,
        Relay,
        Ship
    };

    struct SystemMapObject
    {
        EntityId id {};
        std::string name;
        std::string owner;
        std::string parentBodyId;
        SystemMapObjectKind kind = SystemMapObjectKind::Unknown;
        glm::dvec3 positionAu {0.0};
        int systemId = -1;

        // Optional orbital metadata for map rendering.
        // Renderer must not invent this from a single current position.
        bool hasOrbit = false;

        glm::dvec3 orbitCenterAu {0.0};

        double orbitRadiusAu = 0.0;

        double orbitInclinationDeg = 0.0;
        double orbitLongitudeOfAscendingNodeDeg = 0.0;
        double orbitArgumentOfPeriapsisDeg = 0.0;
    };

    struct SystemMapSnapshot
    {
        int systemId = -1;
        std::string systemName;
        double universeTimeSeconds = 0.0;
        std::string universeDate;
        std::vector<SystemMapBody> bodies;
        std::vector<SystemMapObject> objects;
    };


    struct PlanetMapAxisSet
{
    glm::dvec3 x {1.0, 0.0, 0.0};
    glm::dvec3 y {0.0, 1.0, 0.0};
    glm::dvec3 z {0.0, 0.0, 1.0};
};

struct PlanetMapOrbit
{
    std::string id;
    std::string name;
    std::string parentBodyId;

    glm::dvec3 centerMeters {0.0};
    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};

    double radiusMeters = 0.0;
    double altitudeMeters = 0.0;
    double speedMps = 0.0;

    glm::dvec3 radialAxis {0.0};
    glm::dvec3 progradeAxis {0.0};
    glm::dvec3 normalAxis {0.0};

    bool valid = false;
};

struct PlanetMapObject
{
    EntityId id {};
    std::string stableId;
    std::string name;
    std::string kind;

    glm::dvec3 positionMeters {0.0};
    glm::dvec3 velocityMps {0.0};

    PlanetMapAxisSet axes;

    bool valid = false;
};

struct PlanetMapSnapshot
{
    bool valid = false;

    int systemId = -1;

    std::string planetBodyId;
    std::string planetName;

    glm::dvec3 planetCenterMeters {0.0};

    double planetRadiusMeters = 0.0;
    double gravitationalParameterM3s2 = 0.0;
    double universeTimeSeconds = 0.0;

    double planetRotationPhaseRad = 0.0;
    double planetDayLengthHours = 0.0;

    double planetAxialTiltDeg = 0.0;
    double planetAxisNodeDeg = 0.0;
    double planetTextureLongitudeOffsetDeg = 0.0;

    std::vector<PlanetMapOrbit> hubOrbits;
    std::vector<PlanetMapOrbit> playerOrbits;

    std::vector<PlanetMapObject> hubs;
    std::vector<PlanetMapObject> stations;
    std::vector<PlanetMapObject> ships;
};


struct HubMapModule
{
    EntityId id {};
    ObjectType typeId = ObjectType::None;
    
    std::string stableId;
    std::string name;
    std::string kind;

    glm::dvec3 localPositionMeters {0.0};
    PlanetMapAxisSet localAxes;

    glm::dvec3 sizeMeters {80.0, 30.0, 30.0};

    bool prime = false;
    bool valid = false;
};

struct HubMapShip
{
    EntityId id {};
    ObjectType typeId = ObjectType::None;

    std::string name;

    glm::dvec3 localPositionMeters {0.0};
    glm::dvec3 localVelocityMps {0.0};

    PlanetMapAxisSet localAxes;

    glm::dvec3 sizeMeters {1.0, 1.0, 1.0};

    bool player = false;
    bool valid = false;
};

struct HubMapSnapshot
{
    bool valid = false;

    int systemId = -1;

    std::string hubId;
    std::string parentBodyId;
    std::string displayName;

    glm::dvec3 hubWorldPositionMeters {0.0};
    glm::dvec3 hubWorldVelocityMps {0.0};

    // Локальная система карты хаба:
    // X = prograde, Y = radial, Z = normal.
    PlanetMapAxisSet hubAxes;

    // Родительская планета в локальных координатах хаба.
    // Для круговой орбиты хаб стоит в (0,0,0),
    // а центр планеты находится примерно по -Y.
    glm::dvec3 parentPlanetCenterLocalMeters {0.0};

    double parentPlanetRadiusMeters = 0.0;
    double hubAltitudeMeters = 0.0;
    double hubOrbitRadiusMeters = 0.0;
    double hubOrbitalPeriodSeconds = 0.0;

    std::vector<HubMapModule> modules;
    std::vector<HubMapShip> ships;

    double universeTimeSeconds = 0.0;
};

















}