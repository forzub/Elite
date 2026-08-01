#pragma once

#include <glm/glm.hpp>

#include "src/game/system_map/LocalMapPresentation.h"
#include "src/world/celestial/SystemMapTypes.h"

class SystemMapRenderer;
struct Viewport;

namespace game::system_map
{
class DetailMapGeometryPass
{
public:
    explicit DetailMapGeometryPass(SystemMapRenderer& host) noexcept
        : m_host(host)
    {
    }

    void renderScene(
        const DetailMapPresentation& presentation,
        const Viewport& viewport,
        const world::celestial::DetailMapSnapshot& snapshot
    );

private:
    const LocalMapCameraSnapshot& activeCamera() const;

    void drawPlanetMapLine(
        const glm::dvec2& a,
        const glm::dvec2& b
    );

    void drawPlanetMapCross(
        const glm::dvec2& p,
        float size
    );

    void drawPlanetMapCircle(
        const glm::dvec2& center,
        double radiusPx,
        int segments
    );

    void drawPlanetMapAxes(
        const glm::dvec3& originMeters,
        const world::celestial::LocalSceneAxes& axes,
        const world::celestial::DetailMapSnapshot& snapshot,
        double scale,
        const glm::dvec2& centerPx,
        double axisLenMeters
    );

    void drawPlanetMapVelocityArrow(
        const glm::dvec3& originMeters,
        const glm::dvec3& velocityMps,
        const world::celestial::DetailMapSnapshot& snapshot,
        double scale,
        const glm::dvec2& centerPx,
        double lenMeters
    );

    void drawDetailMapOrbit3D(
        const world::celestial::DetailMapOrbit& orbit,
        const world::celestial::DetailMapSnapshot& snapshot,
        double scale,
        const glm::dvec2& centerPx,
        int segments
    );

private:
    SystemMapRenderer& m_host;
    const LocalMapCameraSnapshot* m_activeCamera = nullptr;
};
}
