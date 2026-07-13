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
inline constexpr double GravitationalConstant = 6.67430e-11;


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


enum class CelestialRingDisplayMode
{
    LayeredBands = 0,
    ParticleCloud = 1
};

enum class CelestialRingVisibilityClass
{
    Main = 0,
    Secondary = 1,
    Faint = 2,
    Diffuse = 3
};

struct CelestialRingSystemVisualProfile
{
    std::string displayProfile;

    CelestialRingDisplayMode renderMode =
        CelestialRingDisplayMode::LayeredBands;

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

    /*
        Particle-cloud mode.
    */
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

    /*
        Физическое освещение намеренно отключено.
    */
    bool useLighting = false;
    bool usePlanetShadow = false;
    bool useRingShadowOnPlanet = false;

    /*
        Нефизическое лёгкое затемнение задней половины.
    */
    bool artisticOcclusionEnabled = false;
    float backHalfBrightness = 1.0f;
    float innerEdgeDarkening = 0.0f;
};

struct CelestialRingRenderProfile
{
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

    CelestialRingVisibilityClass visibilityClass =
        CelestialRingVisibilityClass::Faint;

    CelestialRingDisplayMode displayMode =
        CelestialRingDisplayMode::LayeredBands;

    /*
        Дополнительный художественный множитель конкретного band-а.
    */
    float visualOpacityScale = 1.0f;
    float radialStructureScale = 1.0f;

    /*
        Параметры разреженного облака частиц.
    */
    float particleDensityScale = 1.0f;
    float particleClumpiness = 0.4f;
    float particleRadialJitter = 0.25f;

    glm::vec2 particleSizePxRange {
        0.5f,
        1.3f
    };

    bool castsShadow = true;
};

struct CelestialRingDefinition
{
    std::string name;

    double innerRadiusKm = 0.0;
    double outerRadiusKm = 0.0;

    std::string composition;

    CelestialRingRenderProfile render;
};


struct CelestialBodyDisplayName
    {
        std::string name;
        std::vector<std::string> actors;
    };

struct CelestialBodyDefinition
{

    std::string id;
    std::string name;


    BodyType type = BodyType::Unknown;

    std::string parentId;
    // Environment preset from system_details.json.
    // Example: terrestrial_temperate, gas_giant_ammonia.
    std::string environmentPresetId;

    double diameterKm = 0.0;
    double massKg = 0.0;
    double gravitationalParameterM3s2 = 0.0;

    double radiusKm = 0.0;

    /*
        Кольца по умолчанию лежат в экваториальной плоскости.

        inclinationOffsetDeg позволяет создавать редкие
        вымышленные системы с небольшим отклонением.
    */
    std::string ringPlaneMode = "planet_equatorial";
    double ringPlaneInclinationOffsetDeg = 0.0;
    CelestialRingSystemVisualProfile ringVisual;
    std::vector<CelestialRingDefinition> rings;
  


    double distanceAu = 0.0;
    double orbitalPeriodDays = 0.0;
    double dayLengthHours = 0.0;

    // Body orientation convention:
    // +Y = north pole.
    // XZ = equatorial plane.
    // +X = prime meridian at rotation phase 0.
    // Equirectangular texture: top=north, bottom=south.
    double axialTiltDeg = 0.0;
    double axisNodeDeg = 0.0;
    double rotationOffsetDeg = 0.0;
    double textureLongitudeOffsetDeg = 0.0;

    glm::dvec3 staticPositionAu {0.0};

    std::vector<CelestialBodyDisplayName> alternativeNames;
    

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
    std::string environmentPresetId;

    glm::dvec3 positionAu {0.0};
    glm::dvec3 worldMeters {0.0};

    double radiusKm = 0.0;

    double orbitalPhaseRad = 0.0;
    double rotationPhaseRad = 0.0;

    double dayLengthHours = 0.0;

    double axialTiltDeg = 0.0;
    double axisNodeDeg = 0.0;
    double textureLongitudeOffsetDeg = 0.0;

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