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






    enum class SystemMapRingDisplayMode
    {
        LayeredBands = 0,
        ParticleCloud = 1
    };

    enum class SystemMapRingVisibilityClass
    {
        Main = 0,
        Secondary = 1,
        Faint = 2,
        Diffuse = 3
    };

    struct SystemMapRingVisualProfile
    {
        std::string displayProfile;

        SystemMapRingDisplayMode renderMode =
            SystemMapRingDisplayMode::LayeredBands;

        float recognizabilityPriority = 0.5f;

        float artisticWidthScale = 1.0f;

        float mainBandEmphasis = 1.0f;
        float secondaryBandEmphasis = 1.0f;
        float faintBandEmphasis = 1.0f;
        float diffuseBandEmphasis = 1.0f;

        float gapContrast = 1.0f;

        float radialStructureStrength = 0.0f;
        float fineStructureStrength = 0.0f;

        float edgeSoftnessScale = 1.0f;
        float brightnessVariation = 0.0f;

        float minimumVisibleWidthPx = 0.5f;
        float minimumMainBandWidthPx = 1.0f;

        float continuousFill = 0.0f;
        float particleDensity = 0.3f;
        float particleOpacityScale = 0.4f;

        glm::vec2 particleSizePxRange {
            0.5f,
            1.3f
        };

        float radialJitter = 0.25f;
        float azimuthalClumping = 0.4f;
        float clusterScale = 0.7f;
        float softness = 0.65f;

        bool artisticOcclusionEnabled = false;
        float backHalfBrightness = 1.0f;
        float innerEdgeDarkening = 0.0f;
    };






    struct SystemMapRing
    {
        std::string name;

        double innerRadiusKm = 0.0;
        double outerRadiusKm = 0.0;

        std::string composition;

        glm::vec3 tint {
            0.78f,
            0.78f,
            0.78f
        };

        float opacity = 0.25f;
        float opticalDepth = 0.25f;

        float radialNoiseStrength = 0.15f;
        float radialBrightnessVariation = 0.15f;
        float azimuthalAsymmetry = 0.0f;

        float edgeSoftness = 0.04f;

        SystemMapRingVisibilityClass visibilityClass =
            SystemMapRingVisibilityClass::Faint;

        SystemMapRingDisplayMode displayMode =
            SystemMapRingDisplayMode::LayeredBands;

        float visualOpacityScale = 1.0f;
        float radialStructureScale = 1.0f;

        float particleDensityScale = 1.0f;
        float particleClumpiness = 0.4f;
        float particleRadialJitter = 0.25f;

        glm::vec2 particleSizePxRange {
            0.5f,
            1.3f
        };

        bool castsShadow = true;
    };

    struct SystemMapBody
    {
        std::string id;
        std::string name;
        std::vector<CelestialBodyDisplayName> alternativeNames;
  
        std::string parentId;
        std::string environmentPresetId;

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

        double ringPlaneInclinationOffsetDeg = 0.0;
        SystemMapRingVisualProfile ringVisual;
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
        glm::dvec3 systemPositionLy {0.0};
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
    glm::dvec3 systemPositionLy {0.0};

    std::string planetBodyId;
    std::string planetName;
    std::string environmentPresetId;

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
    double ringPlaneInclinationOffsetDeg = 0.0;

    SystemMapRingVisualProfile ringVisual; 
    std::vector<SystemMapRing> rings;

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
    glm::dvec3 systemPositionLy {0.0};

    std::string hubId;
    std::string parentBodyId;
    std::string parentEnvironmentPresetId;
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