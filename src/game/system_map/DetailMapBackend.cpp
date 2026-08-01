#include "src/game/system_map/DetailMapBackend.h"
#include "src/game/system_map/SystemMapRenderer.h"

#include <GLFW/glfw3.h>

namespace game::system_map
{
DetailMapBackend::DetailMapBackend(SystemMapRenderer& host) noexcept
    : m_host(host),
      m_planetPass(host),
      m_geometryPass(host)
{
}

void DetailMapBackend::renderDetailMapPasses(
    const DetailMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& planet
)
{
    m_host.ensureGeneratedCelestialAssets();
    m_host.ensureEnvironmentProfiles();

    glViewport(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glScissor(
        viewport.x,
        viewport.y,
        viewport.width,
        viewport.height
    );

    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glColor4f(
        m_host.m_detailVisuals.backgroundColor.r,
        m_host.m_detailVisuals.backgroundColor.g,
        m_host.m_detailVisuals.backgroundColor.b,
        m_host.m_detailVisuals.backgroundColor.a
    );

    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(static_cast<float>(viewport.width), 0.0f);
    glVertex2f(
        static_cast<float>(viewport.width),
        static_cast<float>(viewport.height)
    );
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!planet.valid)
        return;

    if (m_host.m_detailVisuals.drawStarfield)
    {
        m_host.drawMapStarfield(
            viewport,
            planet.systemPositionLy,
            presentation.camera.starfieldViewMatrix(),
            m_host.m_detailVisuals.starfieldFieldOfViewDeg,
            m_host.m_detailVisuals.starfieldSizeScale,
            false,
            m_host.m_detailVisuals.starfieldBrightnessScale,
            m_host.m_detailVisuals.milkyWayIntensityScale,
            m_host.m_detailVisuals.milkyWayColorTint
        );
    }

    m_planetPass.renderCentralBody(
        presentation,
        planet
    );

    m_geometryPass.renderScene(
        presentation,
        viewport,
        planet
    );

    glEnable(GL_DEPTH_TEST);
}
}
