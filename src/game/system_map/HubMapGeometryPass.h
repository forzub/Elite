#pragma once

#include <glm/glm.hpp>

#include "render/types/Viewport.h"
#include "src/render/system_map/HubMapGpuGeometryRenderer.h"
#include "src/game/system_map/HubMapVisualSettings.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/types/ObjectType.h"


namespace game::system_map
{
class HubMapBackend;

class HubMapGeometryPass
{
public:
    HubMapGeometryPass(
        HubMapBackend& owner,
        const HubMapVisualSettings& visuals
    ) noexcept
        : m_owner(owner),
          m_visuals(visuals)
    {
    }

    void beginFrame(
        int viewportWidth,
        int viewportHeight,
        const glm::dvec2& screenOriginPx,
        double pixelsPerMeter,
        double yaw,
        double pitch
    )
    {
        m_gpuGeometryRenderer.beginFrame(
            viewportWidth,
            viewportHeight,
            screenOriginPx,
            pixelsPerMeter,
            yaw,
            pitch
        );
    }

    bool active() const noexcept
    {
        return m_gpuGeometryRenderer.active();
    }

    void submitScreenCross(
        const glm::dvec2& centerPx,
        double halfSizePx,
        const glm::vec4& color
    )
    {
        m_gpuGeometryRenderer.submitScreenCross(
            centerPx,
            halfSizePx,
            color
        );
    }

    void flush()
    {
        m_gpuGeometryRenderer.flush();
    }

    void drawHubMapBox(
        const glm::dvec3& center,
        const world::celestial::LocalSceneAxes& axes,
        const glm::dvec3& size,
        const glm::vec4& color,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapAxes(
        const glm::dvec3& center,
        const world::celestial::LocalSceneAxes& axes,
        double axisLenMeters,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapVelocityArrow(
        const glm::dvec3& center,
        const glm::dvec3& velocity,
        double lenMeters,
        double scale,
        const glm::dvec2& centerPx
    );

    void drawHubMapScreenMarker(
        const glm::dvec2& screenPx,
        double radiusPx,
        const glm::vec4& color,
        bool drawCross,
        int segments = 32
    );

    bool drawHubMapAssemblyWire(
        ObjectType typeId,
        const glm::dvec3& objectCenter,
        const world::celestial::LocalSceneAxes& objectAxes,
        const glm::vec4& color
    );

    void drawHubMapAdaptiveGrid(
        const Viewport& viewport,
        double scale,
        const glm::dvec2& centerPx,
        double worldRadiusMeters
    );

    glm::dvec3 visualSizeForHubShip(
        const world::celestial::HubMapShip& ship,
        double scale
    ) const;

private:
    HubMapBackend& m_owner;
    const HubMapVisualSettings& m_visuals;
    render::system_map::HubMapGpuGeometryRenderer
        m_gpuGeometryRenderer;
};
}
