#include "src/render/legacy/CoreGlLegacyBridge.h"
#include "src/game/system_map/DetailMapBackend.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <vector>

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

    // A scene without a central body (an F12 terminal cube, a deep-space hub
    // scene): the bodies in view with the same body renderer, far first, each
    // as a body-centred orthographic view at its projected place and depth
    // scale (the Moon surface next to a lunar-orbit cube).
    if (!planet.hasCentralBody && !planet.contextBodies.empty())
    {
        struct Visible
        {
            const world::celestial::DetailMapContextBody* body;
            glm::dvec3 position;
            double depth;
        };
        std::vector<Visible> visible;
        for (const auto& body : planet.contextBodies)
            for (const auto& object : planet.scene.objects)
                if (object.valid && object.stableId == body.id &&
                    object.objectClass == world::celestial::DetailObjectClass::CelestialBody)
                {
                    const double z = presentation.camera.pointToCamera(object.positionMeters).z;
                    // the eye inside / behind the body: no meaningful view of it
                    if (presentation.camera.perspectiveEnabled &&
                        z + body.radiusMeters >= presentation.camera.perspectiveCameraDistanceMeters * 0.95)
                        break;
                    visible.push_back({&body, object.positionMeters, z});
                    break;
                }
        std::sort(visible.begin(), visible.end(),
                  [](const Visible& a, const Visible& b) { return a.depth < b.depth; });
        for (const auto& v : visible)
        {
            world::celestial::DetailMapSnapshot body;
            body.valid = true;
            body.hasCentralBody = true;
            body.systemId = planet.systemId;
            body.systemPositionLy = planet.systemPositionLy;
            body.universeTimeSeconds = planet.universeTimeSeconds;
            body.planetBodyId = v.body->id;
            body.planetName = v.body->name;
            body.environmentPresetId = v.body->environmentPresetId;
            body.planetCenterMeters = v.position;
            body.planetRadiusMeters = v.body->radiusMeters;
            body.planetRotationPhaseRad = v.body->rotationPhaseRad;
            body.planetDayLengthHours = v.body->dayLengthHours;
            body.planetRotationDirection = v.body->rotationDirection;
            body.planetAxialTiltDeg = v.body->axialTiltDeg;
            body.planetAxisNodeDeg = v.body->axisNodeDeg;
            body.planetTextureLongitudeOffsetDeg = v.body->textureLongitudeOffsetDeg;
            body.scene.originWorldMeters = v.position;

            DetailMapPresentation local = presentation;
            const double depthScale = presentation.camera.perspectiveFactor(v.depth);
            const glm::dvec2 screen = presentation.camera.project(v.position);
            local.scale = presentation.scale * depthScale;
            local.camera.scale = local.scale;
            local.camera.originMeters = v.position;
            local.camera.perspectiveEnabled = false;
            local.centerPx = screen - local.camera.state.pan;
            local.camera.centerPx = local.centerPx;
            m_planetPass.renderCentralBody(local, body);
        }
    }

    m_geometryPass.renderScene(
        presentation,
        viewport,
        planet
    );

    glEnable(GL_DEPTH_TEST);
}
}
