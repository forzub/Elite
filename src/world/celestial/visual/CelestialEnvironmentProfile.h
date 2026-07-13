#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace world::celestial::visual
{

enum class EnvironmentCloudPattern
{
    None,
    ScatteredCumulus,
    BrokenFields,
    DenseOvercast,
    Banded
};

struct EnvironmentAtmospherePhysicalProfile
{
    float surfacePressurePa = 0.0f;
    float scaleHeightKm = 0.0f;
    float outerHeightKm = 0.0f;
};

struct EnvironmentAtmosphereHazeProfile
{
    float centerStrength = 0.0f;
    float limbStrength = 0.0f;
    float contrastLoss = 0.0f;
    float desaturation = 0.0f;
    float distortionStrength = 0.0f;
};

struct EnvironmentAtmosphereProfile
{
    bool enabled = false;

    std::string kind;

    EnvironmentAtmospherePhysicalProfile physical;
    EnvironmentAtmosphereHazeProfile haze;

    float visualIntensity = 1.0f;
    float radiusScale = 1.018f;

    // Legacy/current renderer compatibility.
    float horizonFadeStart = 0.025f;
    float horizonFadeEnd = 0.34f;

    glm::vec4 surfaceHaze {0.0f};
    glm::vec4 limbCore {0.0f};
    glm::vec4 nearAtmosphere {0.0f};
    glm::vec4 outerAtmosphere {0.0f};
};

struct EnvironmentWindProfile
{
    float minimumSpeedMps = 0.0f;
    float maximumSpeedMps = 0.0f;

    // 0=north, 90=east, 180=south, 270=west.
    float predominantDirectionDeg = 0.0f;

    std::string dataStatus;
};

struct EnvironmentCloudLayerAppearanceProfile
{
    glm::vec3 baseColor {
        0.78f,
        0.80f,
        0.82f
    };

    glm::vec3 shadowColor {
        0.28f,
        0.32f,
        0.38f
    };

    float opacityScale = 1.0f;
};

struct EnvironmentCloudLayerProfile
{
    std::string id;
    std::string type;

    /*
        Роль слоя в итоговом изображении:

        primary_opaque_deck
        dynamic_belt_overlay
        high_cloud_overlay
        slow_haze_overlay
        fast_streak_overlay
        и т. п.
    */
    std::string renderRole;

    float baseHeightKm = 0.0f;
    float topHeightKm = 0.0f;

    float coverage = 0.0f;
    float density = 0.0f;
    float opacity = 0.0f;
    float particleScale = 1.0f;

    EnvironmentCloudLayerAppearanceProfile appearance;
    EnvironmentWindProfile wind;
};

struct EnvironmentCloudCirculationZone
{
    std::string id;

    float latitudeCenterDeg = 0.0f;
    float halfWidthDeg = 0.0f;

    float coverageMultiplier = 1.0f;
    float densityMultiplier = 1.0f;

    float fragmentation = 0.0f;
    float zonalStretch = 1.0f;
    float driftSpeedMultiplier = 1.0f;
};

struct EnvironmentCloudCirculationProfile
{
    std::string model = "none";

    std::vector<EnvironmentCloudCirculationZone> zones;
};

struct EnvironmentCloudGenerationProfile
{
    std::string seedPolicy = "fixed";
    std::uint32_t seedBase = 1337u;

    float weatherScale = 6.0f;
    float massScale = 14.0f;
    float erosionScale = 28.0f;
    float detailScale = 72.0f;

    float domainWarpStrength = 0.055f;
    float billowStrength = 0.78f;
    float worleyErosionStrength = 0.34f;

    float edgeFragmentStrength = 0.52f;
    float edgeFragmentScale = 56.0f;
    float detachedFragmentProbability = 0.28f;

    float softEdgeWidth = 0.10f;
};

struct EnvironmentCloudAppearanceProfile
{
    glm::vec3 baseColor {0.78f, 0.80f, 0.82f};
    glm::vec3 shadowColor {0.28f, 0.32f, 0.38f};

    float saturation = 0.0f;
    float contrast = 1.0f;
};

struct EnvironmentCloudProfile
{
    bool enabled = false;

    EnvironmentCloudPattern pattern =
        EnvironmentCloudPattern::None;

    float globalCoverage = 0.0f;

    EnvironmentCloudAppearanceProfile appearance;

    std::vector<EnvironmentCloudLayerProfile> layers;
    EnvironmentCloudCirculationProfile circulation;
    EnvironmentCloudGenerationProfile generation;

    // Derived compatibility values for current adapters.
    float opacity = 0.34f;
    float longitudeDriftSpeed = 0.0f;
};

struct EnvironmentGameUpdatePolicy
{
    std::string primaryLayer =
        "cached_and_time_blended";

    std::string overlayLayers =
        "time_sliced_one_generation_per_frame";

    std::string farLod =
        "single_cached_primary_layer";

    std::string offscreen =
        "do_not_update";
};

struct EnvironmentRenderingProfile
{
    /*
        terrestrial
        venusian_cloud_deck
        gas_giant
        ice_giant
    */
    std::string visualClass =
        "terrestrial";

    /*
        visible / hidden
    */
    std::string surfaceVisibility =
        "visible";

    bool loadSurfaceTextures = true;

    /*
        surface_with_cloud_overlays
        procedural_primary_cloud_deck
    */
    std::string baseRenderMode =
        "surface_with_cloud_overlays";

    std::string primaryLayerId;

    bool primaryLayerMustCoverSurface = false;

    float primaryLayerOpacityFloor = 0.0f;

    std::vector<std::string>
        overlayLayerIds;

    int maximumDynamicOverlayLayers = 2;

    glm::ivec2 mapGenerationResolution {
        1024,
        512
    };

    glm::ivec2 gameGenerationResolution {
        512,
        256
    };

    EnvironmentGameUpdatePolicy
        gameUpdatePolicy;
};

struct CelestialEnvironmentProfile
{
    bool found = false;
    bool loadedFromPreset = false;
    bool loadedBodyOverrides = false;

    std::string presetId;

    std::string systemFolderName;
    std::string bodyFolderName;
    std::string displayName;

    std::string classification;
    std::string dataQuality;

    EnvironmentAtmosphereProfile atmosphere;
    EnvironmentCloudProfile clouds;
    EnvironmentRenderingProfile rendering;
};

class CelestialEnvironmentProfileLibrary
{
public:
    bool loadFromRoot(
        const std::string& rootPath
    );

    const CelestialEnvironmentProfile* findBodyProfile(
        const std::string& systemFolderName,
        const std::string& bodyFolderName
    ) const;

    const CelestialEnvironmentProfile* findPreset(
        const std::string& presetId
    ) const;

    CelestialEnvironmentProfile resolve(
        const std::string& presetId,
        const std::string& systemFolderName,
        const std::string& bodyFolderName
    ) const;

    const std::vector<CelestialEnvironmentProfile>& profiles() const
    {
        return m_bodyProfiles;
    }

    const std::vector<CelestialEnvironmentProfile>& presets() const
    {
        return m_presets;
    }

    int failedCount() const
    {
        return m_failedCount;
    }

private:
    static std::string makeBodyKey(
        const std::string& systemFolderName,
        const std::string& bodyFolderName
    );

    static std::string makePresetKey(
        const std::string& presetId
    );

private:
    std::vector<CelestialEnvironmentProfile> m_bodyProfiles;
    std::vector<CelestialEnvironmentProfile> m_presets;

    std::unordered_map<std::string, std::size_t> m_bodyIndex;
    std::unordered_map<std::string, std::size_t> m_presetIndex;

    int m_failedCount = 0;
};

} // namespace world::celestial::visual