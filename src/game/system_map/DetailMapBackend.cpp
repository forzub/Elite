#include "src/game/system_map/DetailMapBackend.h"

#include <GLFW/glfw3.h>

namespace game::system_map
{
DetailMapBackend::DetailMapBackend(
    MapCelestialRenderResources& resources
) noexcept
    : m_resources(resources),
      m_planetPass(resources),
      m_geometryPass(resources.detailVisuals())
{
}

void DetailMapBackend::renderDetailMapPasses(
    const DetailMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& planet
)
{
    m_resources.ensureGeneratedCelestialAssets();
    m_resources.ensureEnvironmentProfiles();

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
        m_resources.detailVisuals().backgroundColor.r,
        m_resources.detailVisuals().backgroundColor.g,
        m_resources.detailVisuals().backgroundColor.b,
        m_resources.detailVisuals().backgroundColor.a
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

    if (m_resources.detailVisuals().drawStarfield)
    {
        m_resources.drawStarfield(
            viewport,
            planet.systemPositionLy,
            presentation.camera.starfieldViewMatrix(),
            m_resources.detailVisuals().starfieldFieldOfViewDeg,
            m_resources.detailVisuals().starfieldSizeScale,
            false,
            m_resources.detailVisuals().starfieldBrightnessScale,
            m_resources.detailVisuals().milkyWayIntensityScale,
            m_resources.detailVisuals().milkyWayColorTint
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
