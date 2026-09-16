#include "src/render/legacy/CoreGlLegacyBridge.h"
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

    elite::render::core_legacy::matrixMode(elite::render::core_legacy::ProjectionToken);
    elite::render::core_legacy::loadIdentity();

    elite::render::core_legacy::ortho(
        0.0,
        viewport.width,
        viewport.height,
        0.0,
        -1.0,
        1.0
    );

    elite::render::core_legacy::matrixMode(elite::render::core_legacy::ModelViewToken);
    elite::render::core_legacy::loadIdentity();

    elite::render::core_legacy::color4f(
        m_resources.detailVisuals().backgroundColor.r,
        m_resources.detailVisuals().backgroundColor.g,
        m_resources.detailVisuals().backgroundColor.b,
        m_resources.detailVisuals().backgroundColor.a
    );

    elite::render::core_legacy::begin(elite::render::core_legacy::QuadsToken);
    elite::render::core_legacy::vertex2f(0.0f, 0.0f);
    elite::render::core_legacy::vertex2f(static_cast<float>(viewport.width), 0.0f);
    elite::render::core_legacy::vertex2f(
        static_cast<float>(viewport.width),
        static_cast<float>(viewport.height)
    );
    elite::render::core_legacy::vertex2f(0.0f, static_cast<float>(viewport.height));
    elite::render::core_legacy::end();

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
