#pragma once

#include <cstdint>
#include <memory>
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
#include "src/game/system_map/ProceduralBodyTextureCache.h"
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

    // the curated shape model (Phobos, Deimos) from assets/source when the baked cache has none / is off
    std::string sourceShapeModelPath(
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

    // The albedo of a body by identity (Detail / Hub maps): the cached
    // generated texture or the procedural bake of the body's descriptor,
    // per ProceduralBodyTextureCache policy. usedProcedural (optional) tells
    // the caller the cached normal / preview maps do not belong to it.
    // Procedural policy "all", or the body's appearance passport chose the
    // generator: the procedural bake wins over the cached texture.
    bool preferProceduralAlbedo(
        const std::string& bodyId
    ) const;

    // The procedural albedo only when preferred (else 0); for the shape
    // model meshes (Phobos, Deimos) that carry their own cached texture.
    // the map draws the bodies' own clouds (procedural policy on) instead of the presets' styled layers
    bool fieldCloudsEnabled() const
    {
        return m_proceduralBodies &&
               m_proceduralBodies->policy() != ProceduralBodyTextureCache::Policy::Off;
    }

    // The body's cloud layer from its CloudWeatherField (the flight's clouds); 0 while baking or when the
    // procedural policy is off; noClouds: the body has none (no preset clouds should be drawn either).
    GLuint fieldCloudTextureForIdentity(
        int systemId,
        const std::string& bodyId,
        double worldTimeSeconds,
        bool& noClouds
    );

    // the relief normal map of the body's field bake (Hub map shading); 0 until baked / policy off
    // a small body's field as a mesh (Detail map shape), nullptr until baked / larger bodies / policy off
    const std::vector<render::celestial::CelestialShapeTriangle>* proceduralShapeForIdentity(
        int systemId,
        const std::string& bodyId
    );

    GLuint proceduralNormalTextureForIdentity(
        int systemId,
        const std::string& bodyId
    );

    GLuint proceduralAlbedoTextureForIdentity(
        int systemId,
        const std::string& bodyId
    );

    GLuint globalAlbedoTextureForIdentity(
        int systemId,
        const std::string& bodyId,
        const std::string& displayName,
        bool* usedProcedural = nullptr
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

    std::string m_environmentRenderSessionKey;

    // One texture policy for every map texture (docs/CELESTIAL_RESOURCE_AUDIT.md step 2): a byte budget
    // (ELITE_MAP_TEXTURE_BUDGET_MB, default 256) evicts the least recently used textures not drawn in the
    // last frames; a texture unused for ~5 min is released anyway. An evicted texture loads / bakes again
    // on its next request.
    struct MapTextureEntry
    {
        GLuint texture = 0;          // 0 = the load failed (kept: no retry every frame)
        std::size_t bytes = 0;
        std::uint64_t lastUsedFrame = 0;
    };
    using MapTextureCache = std::unordered_map<std::string, MapTextureEntry>;
    GLuint useCachedTexture(MapTextureCache& cache, const std::string& key, bool& found);
    void storeCachedTexture(MapTextureCache& cache, const std::string& key, GLuint texture);
    void enforceMapTextureBudget();
    std::uint64_t m_mapFrame = 0;
    std::size_t m_mapTextureBudgetBytes = 0;
    std::uint64_t m_mapTextureEvictions = 0;

    // DEV memory ledger (beginFrame, every 60 frames)
    void reportMemoryLedger();
    std::uint64_t m_memoryLedgerFrame = 0;

    // created in init() (GL thread); baking runs on its own worker
    std::unique_ptr<ProceduralBodyTextureCache> m_proceduralBodies;

    MapTextureCache
        m_mapPreviewTextureByAssetKey;
    MapTextureCache
        m_globalAlbedoTextureByAssetKey;
    MapTextureCache
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
