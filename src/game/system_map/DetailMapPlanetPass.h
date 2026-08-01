#pragma once

#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/game/system_map/LocalMapEnvironmentStyle.h"
#include "src/game/system_map/LocalMapPresentation.h"
#include "src/render/celestial/CelestialShapeMesh.h"
#include "src/render/celestial/ProceduralCloudLayer.h"
#include "src/render/celestial/rings/PlanetRingRenderer.h"
#include "src/world/celestial/SystemMapTypes.h"

class SystemMapRenderer;

namespace game::system_map
{
class DetailMapPlanetPass
{
public:
    explicit DetailMapPlanetPass(SystemMapRenderer& host) noexcept
        : m_host(host)
    {
    }

    void renderCentralBody(
        const DetailMapPresentation& presentation,
        const world::celestial::DetailMapSnapshot& snapshot
    );

private:
    const LocalMapCameraSnapshot& activeCamera() const;

    glm::mat3 planetBodyToDetailCameraMatrix(
        const world::celestial::DetailMapSnapshot& planet
    ) const;

    void drawPlanetMapCross(
        const glm::dvec2& point,
        float size
    );

    void drawPlanetSphereGrid(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetFilledDisk(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawPlanetTexturedGlobe(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    std::vector<render::celestial::ProceduralCloudStyle>
    planetCloudStylesForPlanet(
        const world::celestial::DetailMapSnapshot& planet
    ) const;

    LocalMapAtmosphereStyle planetAtmosphereStyleForPlanet(
        const world::celestial::DetailMapSnapshot& planet
    ) const;

    render::celestial::rings::PlanetRingRenderContext
    planetRingRenderContext(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        std::vector<world::celestial::SystemMapRing>& normalizedBands
    ) const;

    void drawPlanetDetailSculpt(
        const glm::dvec2& planetCenterPx,
        double planetRadiusPx
    );

    void drawPlanetEnvironmentLayers(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        bool applySphericalSculpt
    );

    void drawPlanetAtmosphereInterior(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const LocalMapAtmosphereStyle& style
    );

    void drawPlanetAtmosphereLimb(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const LocalMapAtmosphereStyle& style
    );

    void drawPlanetAnimatedCloudLayers(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        const render::celestial::ProceduralCloudStyle& baseStyle
    );

    void drawPlanetProceduralCloudGlobeLayer(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx,
        double cloudRadiusScale,
        double longitudeOffset,
        double timeSeconds,
        const render::celestial::ProceduralCloudStyle& style
    );

    bool drawPlanetShapeModelDetail(
        const world::celestial::DetailMapSnapshot& planet,
        double scale,
        const glm::dvec2& centerPx
    );

    GLuint globalAlbedoTextureForPlanetSnapshot(
        const world::celestial::DetailMapSnapshot& planet
    );

private:
    SystemMapRenderer& m_host;
    const LocalMapCameraSnapshot* m_activeCamera = nullptr;

    render::celestial::CelestialShapeMeshLibrary m_shapeMeshes;

    GLuint m_detailSculptShader = 0;
    GLuint m_detailSculptVao = 0;
    bool m_detailSculptWarningPrinted = false;
};
}
