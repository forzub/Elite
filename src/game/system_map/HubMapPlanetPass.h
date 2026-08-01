#pragma once

#include <vector>

#include <glad/gl.h>
#include <glm/glm.hpp>

#include "src/game/system_map/LocalMapEnvironmentStyle.h"
#include "src/game/system_map/MapCelestialRenderResources.h"
#include "src/render/celestial/HubSphericalGridRenderer.h"
#include "src/render/celestial/ProceduralCloudLayer.h"
#include "src/render/system_map/HubPlanetOverlayRenderer.h"
#include "src/world/celestial/SystemMapTypes.h"


namespace game::system_map
{
class HubMapBackend;

class HubMapPlanetPass
{
public:
    HubMapPlanetPass(
        MapCelestialRenderResources& resources,
        HubMapBackend& owner
    ) noexcept
        : m_resources(resources),
          m_owner(owner)
    {
    }

    void drawHubMapPlanetSurfaceHint(
        const world::celestial::HubMapSnapshot& hub,
        double scale,
        const glm::dvec2& centerPx
    );

private:
    void drawHubMapCircleLocalXY(
        const glm::dvec3& center,
        double radiusMeters,
        double scale,
        const glm::dvec2& centerPx,
        int segments = 192
    );

    glm::mat3 hubCameraToParentPlanetBodyMatrix(
        const world::celestial::HubMapSnapshot& hub
    ) const;

    GLuint mapPreviewTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    GLuint globalNormalTextureForHubSnapshot(
        const world::celestial::HubMapSnapshot& hub
    );

    LocalMapAtmosphereStyle
    hubPlanetAtmosphereStyleForHub(
        const world::celestial::HubMapSnapshot& hub
    ) const;

    render::celestial::HubSphericalGridStyle
    hubSphericalGridStyleForHub(
        const world::celestial::HubMapSnapshot& hub
    ) const;

    std::vector<render::celestial::ProceduralCloudStyle>
    hubPlanetCloudStylesForHub(
        const world::celestial::HubMapSnapshot& hub
    ) const;

private:
    MapCelestialRenderResources& m_resources;
    HubMapBackend& m_owner;

    render::system_map::HubPlanetOverlayRenderer
        m_overlayRenderer;
    render::celestial::HubSphericalGridRenderer
        m_sphericalGridRenderer;

    double m_lastHubPlanetVisualRadiusPx = 0.0;
    glm::dvec2 m_lastHubPlanetVisualCenterPx {0.0, 0.0};
};
}
