#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "render/types/Viewport.h"
#include "src/game/system_map/DetailMapVisualSettings.h"
#include "src/game/system_map/HubMapVisualSettings.h"
#include "src/game/system_map/LocalMapEnvironmentStyle.h"
#include "src/game/system_map/MapMode.h"
#include "src/render/celestial/HubPlanetSurfaceRenderer.h"
#include "src/render/celestial/PlanetGlobeMeshRenderer.h"
#include "src/render/celestial/ProceduralCloudLayer.h"
#include "src/render/celestial/rings/PlanetRingRenderer.h"
#include "src/render/starfield/GalaxyStarfieldRenderer.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/celestial/visual/CelestialEnvironmentProfile.h"
#include "src/world/celestial/visual/CelestialGeneratedAssetLibrary.h"

namespace game::system_map
{
class MapCelestialRenderResources
{
public:
    void init(float galaxyBackdropMinimumDistanceLy);
    void beginFrame();
    void resetPresentationTime();

    const DetailMapVisualSettings& detailVisuals() const noexcept
    {
        return m_detailVisuals;
    }

    const HubMapVisualSettings& hubVisuals() const noexcept
    {
        return m_hubVisuals;
    }

    void ensureGeneratedCelestialAssets();
    void ensureEnvironmentProfiles();

    void beginEnvironmentRenderSessionIfNeeded(
        MapMode mode,
        int systemId,
        const std::string& bodyId
    );

    world::celestial::visual::CelestialEnvironmentProfile
    resolvedEnvironmentProfileForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId
    ) const;

    std::vector<render::celestial::ProceduralCloudStyle>
    cloudStylesForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId,
        double planetRadiusMeters,
        int textureWidth,
        int textureHeight
    ) const;

    LocalMapAtmosphereStyle atmosphereStyleForBody(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        const std::string& environmentPresetId
    ) const;

    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForBody(
        const world::celestial::SystemMapBody& body
    ) const;

    const world::celestial::visual::CelestialGeneratedAssetSet*
    generatedAssetForIdentity(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName
    ) const;

    GLuint mapPreviewTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalAlbedoTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalAlbedoTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    GLuint globalNormalTextureForGeneratedAsset(
        const world::celestial::visual::CelestialGeneratedAssetSet& asset
    );

    GLuint globalAlbedoTextureForBody(
        const world::celestial::SystemMapBody& body
    );

    double visualEffectTimeSeconds(
        double sourceTimeSeconds
    );

    void drawStarfield(
        const Viewport& viewport,
        const glm::dvec3& observerPositionLy,
        const glm::mat4& cameraView,
        float fieldOfViewDeg,
        float sizeScale,
        bool distantGalaxyBackdrop,
        float starBrightnessScale = 1.0f,
        float milkyWayIntensityScale = 1.0f,
        const glm::vec3& milkyWayColorTint = glm::vec3(1.0f)
    );

    render::celestial::HubPlanetSurfaceRenderer&
    hubPlanetSurfaceRenderer() noexcept
    {
        return m_hubPlanetSurfaceRenderer;
    }

    render::celestial::PlanetGlobeMeshRenderer&
    planetGlobeMeshRenderer() noexcept
    {
        return m_planetGlobeMeshRenderer;
    }

    render::celestial::rings::PlanetRingRenderer&
    planetRingRenderer() noexcept
    {
        return m_planetRingRenderer;
    }

    render::celestial::ProceduralCloudLayer&
    proceduralCloudLayer() noexcept
    {
        return m_proceduralCloudLayer;
    }

private:
    DetailMapVisualSettings m_detailVisuals;
    HubMapVisualSettings m_hubVisuals;

    world::celestial::visual::CelestialGeneratedAssetLibrary
        m_generatedCelestialAssets;
    bool m_generatedCelestialAssetsAttempted = false;
    bool m_generatedCelestialAssetsLoaded = false;

    world::celestial::visual::CelestialEnvironmentProfileLibrary
        m_environmentProfiles;
    bool m_environmentProfilesAttempted = false;
    bool m_environmentProfilesLoaded = false;

    std::uint32_t m_environmentMapOpenSeed = 0u;
    std::string m_environmentRenderSessionKey;

    std::unordered_map<std::string, GLuint>
        m_mapPreviewTextureByAssetKey;
    std::unordered_map<std::string, GLuint>
        m_globalAlbedoTextureByAssetKey;
    std::unordered_map<std::string, GLuint>
        m_globalNormalTextureByAssetKey;

    render::celestial::HubPlanetSurfaceRenderer
        m_hubPlanetSurfaceRenderer;
    render::celestial::PlanetGlobeMeshRenderer
        m_planetGlobeMeshRenderer;
    render::celestial::rings::PlanetRingRenderer
        m_planetRingRenderer;
    render::celestial::ProceduralCloudLayer
        m_proceduralCloudLayer;

    GalaxyStarfieldRenderer m_mapStarfieldRenderer;
    GalaxyStarfieldRenderer m_galaxyBackdropStarfieldRenderer;
    bool m_mapStarfieldInitialized = false;
    bool m_galaxyBackdropStarfieldInitialized = false;

};
}
