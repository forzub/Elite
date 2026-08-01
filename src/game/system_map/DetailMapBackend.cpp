#include "src/game/system_map/DetailMapBackend.h"
#include "src/game/system_map/SystemMapRenderer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "render/HUD/TextRenderer.h"

namespace
{
    constexpr double AU_KM = 149597870.7;

    std::string fmtSystemMapScaleDistance(double km)
    {
        std::ostringstream ss;
        if (km >= AU_KM * 0.1)
            ss << std::fixed << std::setprecision(3) << (km / AU_KM) << " AU";
        else if (km >= 1000000.0)
            ss << std::fixed << std::setprecision(2) << (km / 1000000.0) << " M km";
        else if (km >= 1000.0)
            ss << std::fixed << std::setprecision(0) << km << " km";
        else
            ss << std::fixed << std::setprecision(1) << km << " km";
        return ss.str();
    }
}

namespace game::system_map
{
void DetailMapBackend::renderDetailMapPasses(
    const game::system_map::DetailMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& planet
)
{
    const auto* previousCameraSnapshot =
        m_host.m_activeLocalCameraSnapshot;
    m_host.m_activeLocalCameraSnapshot =
        &presentation.camera;

    struct RestoreCameraSnapshot
    {
        const game::system_map::LocalMapCameraSnapshot*& slot;
        const game::system_map::LocalMapCameraSnapshot* previous;
        ~RestoreCameraSnapshot() { slot = previous; }
    } restoreCameraSnapshot {
        m_host.m_activeLocalCameraSnapshot,
        previousCameraSnapshot
    };

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
    glVertex2f(static_cast<float>(viewport.width), static_cast<float>(viewport.height));
    glVertex2f(0.0f, static_cast<float>(viewport.height));
    glEnd();

    if (!planet.valid)
        return;


    if (m_host.m_detailVisuals.drawStarfield)
    {
        m_host.drawMapStarfield(
            viewport,
            planet.systemPositionLy
        );
    }



    if (planet.hasCentralBody)
    {
        m_host.beginEnvironmentRenderSessionIfNeeded(
            SystemMapRenderer::Mode::Detail,
            planet.systemId,
            planet.planetBodyId
        );
    }

    const glm::dvec2& centerPx =
        presentation.centerPx;
    const double maxRadiusMeters =
        presentation.maxRadiusMeters;
    const double scale =
        presentation.scale;


    if (planet.hasCentralBody)
    {
    std::vector<
        world::celestial::SystemMapRing
    > normalizedRingBands;

    const auto ringContext =
        m_host.planetRingRenderContext(
            planet,
            scale,
            centerPx,
            normalizedRingBands
        );





        const auto environmentProfile =
    m_host.resolvedEnvironmentProfileForBody(
        planet.systemId,
        planet.planetBodyId,
        planet.planetName,
        planet.environmentPresetId
    );

const bool hidePhysicalSurface =
    environmentProfile.found &&
    (
        environmentProfile.rendering.surfaceVisibility ==
            "hidden" ||
        !environmentProfile.rendering.loadSurfaceTextures
    );






m_host.m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Back
);



const bool shapeModelDrawn =
    !hidePhysicalSurface &&
    m_host.drawPlanetShapeModelDetail(
        planet,
        scale,
        centerPx
    );





// ------------------------------------------------------------
// 1. Базовая поверхность планеты.
// Если shape model не нарисован, рисуем fallback disk + textured globe.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
    /*
        Даже при скрытой поверхности оставляем тёмный
        непрозрачный fallback disk под primary cloud deck.

        Он нужен только как страховка от щелей на краю.
    */
    m_host.drawPlanetFilledDisk(
        planet,
        scale,
        centerPx
    );

    if (!hidePhysicalSurface)
    {
        m_host.drawPlanetTexturedGlobe(
            planet,
            scale,
            centerPx
        );
    }
}

// ------------------------------------------------------------
// 2. Environment layer.
// Для Planet Details НЕ используем Hub screen-space clouds.
// Здесь должна работать отдельная spherical Details-цепочка:
// m_host.drawPlanetEnvironmentLayers()
//     -> m_host.drawPlanetAtmosphereInterior()
//     -> m_host.drawPlanetAnimatedCloudLayers()
//         -> m_host.drawPlanetProceduralCloudGlobeLayer()
//     -> m_host.drawPlanetAtmosphereLimb()
// ------------------------------------------------------------
m_host.drawPlanetEnvironmentLayers(
    planet,
    scale,
    centerPx,
    !shapeModelDrawn
);



m_host.m_planetRingRenderer.render(
    ringContext,
    render::celestial::rings::
        PlanetRingRenderPart::Front
);





// ------------------------------------------------------------
// 3. Сетка/ориентационный оверлей поверх поверхности,
// облаков и атмосферы.
// ------------------------------------------------------------
if (!shapeModelDrawn)
{
    m_host.drawPlanetSphereGrid(
        planet,
        scale,
        centerPx
    );
}

    }






    if (presentation.sceneIsSpatialVolume &&
        planet.detailHalfExtentMeters > 0.0)
    {
        const double h =
            planet.detailHalfExtentMeters;

        const std::array<glm::dvec3, 8> corners = {
            planet.planetCenterMeters + glm::dvec3(-h, -h, -h),
            planet.planetCenterMeters + glm::dvec3( h, -h, -h),
            planet.planetCenterMeters + glm::dvec3( h,  h, -h),
            planet.planetCenterMeters + glm::dvec3(-h,  h, -h),
            planet.planetCenterMeters + glm::dvec3(-h, -h,  h),
            planet.planetCenterMeters + glm::dvec3( h, -h,  h),
            planet.planetCenterMeters + glm::dvec3( h,  h,  h),
            planet.planetCenterMeters + glm::dvec3(-h,  h,  h)
        };

        std::array<glm::dvec2, 8> projected;

        for (std::size_t i = 0; i < corners.size(); ++i)
        {
            projected[i] =
                m_host.activeLocalCameraSnapshot().project(corners[i]);
        }

        constexpr std::array<std::array<int, 2>, 12> edges = {{
            {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
            {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
        }};

        glColor4f(0.30f, 0.66f, 0.92f, 0.42f);

        for (const auto& edge : edges)
        {
            m_host.drawPlanetMapLine(
                projected[edge[0]],
                projected[edge[1]]
            );
        }
    }


    // Орбиты хабов.
    glColor4f(0.45f, 0.78f, 1.0f, 0.75f);

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        m_host.drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    // Орбита игрока.
    glColor4f(1.0f, 0.75f, 0.25f, 0.9f);

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        m_host.drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192
        );
    }

    const double axisLenMeters =
        std::max(
            50000.0,
            maxRadiusMeters * 0.035
        );

    const double velocityArrowLenMeters =
        std::max(
            70000.0,
            maxRadiusMeters * 0.05
        );

    // Hub anchors in the current local scene.
    for (const auto& hub : planet.scene.objects)
    {
        if (!hub.valid ||
            hub.objectClass !=
                world::celestial::DetailObjectClass::Hub ||
            hub.kind != "hub")
        {
            continue;
        }

        const glm::dvec2 p =
            m_host.activeLocalCameraSnapshot().project(hub.positionMeters);

        glColor4f(0.3f, 0.9f, 1.0f, 1.0f);
        m_host.drawPlanetMapCross(p, 7.0f);

        if (hub.stableId ==
            presentation.selectedHubId)
        {
            glColor4f(
                0.38f,
                0.95f,
                1.0f,
                0.98f
            );

            constexpr int segments = 64;
            constexpr double radiusPx = 14.0;

            glBegin(GL_LINE_LOOP);

            for (int segment = 0;
                 segment < segments;
                 ++segment)
            {
                const double angle =
                    glm::two_pi<double>() *
                    static_cast<double>(segment) /
                    static_cast<double>(segments);

                glVertex2d(
                    p.x +
                        std::cos(angle) *
                        radiusPx,
                    p.y +
                        std::sin(angle) *
                        radiusPx
                );
            }

            glEnd();
        }

        m_host.drawPlanetMapAxes(
            hub.positionMeters,
            hub.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 hubRelativeVelocityMps =
            hub.velocityMps -
            planet.planetVelocityMps;

        m_host.drawPlanetMapVelocityArrow(
            hub.positionMeters,
            hubRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    if (!presentation.selectedHubId.empty())
    {
        const auto selectedPoint =
            std::find_if(
                presentation.frame.hubScreenPoints.begin(),
                presentation.frame.hubScreenPoints.end(),
                [&](const DetailHubScreenPoint& point)
                {
                    return
                        point.hubId ==
                        presentation.selectedHubId;
                }
            );

        if (selectedPoint !=
                presentation.frame.hubScreenPoints.end() &&
            selectedPoint->visible)
        {
            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                viewport.width,
                viewport.height
            );

            text.textDrawPx(
                selectedPoint->name,
                selectedPoint->screen.x + 18.0f,
                selectedPoint->screen.y - 9.0f,
                13,
                m_host.m_detailVisuals.selectedHubLabelColor
            );

            text.endFrame();
        }
    }

    // Context celestial bodies, including authored or procedural asteroids.
    for (const auto& body : planet.scene.objects)
    {
        if (!body.valid ||
            body.objectClass !=
                world::celestial::DetailObjectClass::CelestialBody)
        {
            continue;
        }

        const glm::dvec2 p =
            m_host.activeLocalCameraSnapshot().project(body.positionMeters);

        glColor4f(0.72f, 0.74f, 0.78f, 0.92f);

        const double radiusPx =
            body.boundingRadiusMeters *
            scale *
            m_host.activeLocalCameraSnapshot().state.zoom;

        if (radiusPx >= 1.0)
        {
            m_host.drawPlanetMapCircle(
                p,
                radiusPx,
                96
            );
        }

        if (radiusPx < 12.0)
        {
            m_host.drawPlanetMapCross(p, 3.0f);
        }
    }

    // Infrastructure: station, mine, base, beacon and relay are all Hub
    // class objects. The scene role/kind decides whether it is the selected
    // hub anchor or another infrastructure object in the volume.
    for (const auto& station : planet.scene.objects)
    {
        if (!station.valid ||
            station.objectClass !=
                world::celestial::DetailObjectClass::Hub ||
            station.kind == "hub")
        {
            continue;
        }

        const glm::dvec2 p =
            m_host.activeLocalCameraSnapshot().project(station.positionMeters);

        glColor4f(
            m_host.m_detailVisuals.stationMarkerColor.r,
            m_host.m_detailVisuals.stationMarkerColor.g,
            m_host.m_detailVisuals.stationMarkerColor.b,
            m_host.m_detailVisuals.stationMarkerColor.a
        );
        m_host.drawPlanetMapCross(p, 6.0f);

        m_host.drawPlanetMapAxes(
            station.positionMeters,
            station.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 stationRelativeVelocityMps =
            station.velocityMps -
            planet.planetVelocityMps;

        m_host.drawPlanetMapVelocityArrow(
            station.positionMeters,
            stationRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters * 0.8
        );
    }

    // Self-propelled craft.
    for (const auto& ship : planet.scene.objects)
    {
        if (!ship.valid ||
            ship.objectClass !=
                world::celestial::DetailObjectClass::Ship)
        {
            continue;
        }

        const glm::dvec2 p =
            m_host.activeLocalCameraSnapshot().project(ship.positionMeters);

        glColor4f(
            m_host.m_detailVisuals.selectedStationMarkerColor.r,
            m_host.m_detailVisuals.selectedStationMarkerColor.g,
            m_host.m_detailVisuals.selectedStationMarkerColor.b,
            m_host.m_detailVisuals.selectedStationMarkerColor.a
        );
        m_host.drawPlanetMapCross(p, 8.0f);

        m_host.drawPlanetMapAxes(
            ship.positionMeters,
            ship.axes,
            planet,
            scale,
            centerPx,
            axisLenMeters
        );

        const glm::dvec3 shipRelativeVelocityMps =
            ship.velocityMps -
            planet.planetVelocityMps;

        m_host.drawPlanetMapVelocityArrow(
            ship.positionMeters,
            shipRelativeVelocityMps,
            planet,
            scale,
            centerPx,
            velocityArrowLenMeters
        );
    }

    const bool detailVolumeEmpty =
        presentation.sceneIsSpatialVolume &&
        planet.scene.empty();

    if (detailVolumeEmpty)
    {
        auto& text = TextRenderer::instance();
        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );
        text.textDrawPx(
            "NO OBJECTS DETECTED",
            static_cast<float>(centerPx.x - 84.0),
            static_cast<float>(viewport.height - 45),
            12,
            glm::vec4(0.58f, 0.68f, 0.76f, 0.82f)
        );
        text.endFrame();
    }

    if (presentation.sceneIsSpatialVolume &&
        planet.detailHalfExtentMeters > 0.0)
    {
        const double edgeKm =
            planet.detailHalfExtentMeters *
            2.0 /
            1000.0;

        const std::string edgeLabel =
            "CELL EDGE: " +
            fmtSystemMapScaleDistance(
                edgeKm
            );

        auto& text =
            TextRenderer::instance();

        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );

        text.textDrawPx(
            edgeLabel,
            static_cast<float>(
                centerPx.x -
                static_cast<double>(
                    edgeLabel.size()
                ) *
                3.25
            ),
            static_cast<float>(
                viewport.height - 22
            ),
            12,
            glm::vec4(
                0.62f,
                0.72f,
                0.82f,
                0.88f
            )
        );

        text.endFrame();
    }

    if (m_host.m_detailVisuals.drawBodyTitle &&
        planet.hasCentralBody &&
        !planet.planetName.empty())
    {
        const int titleSizePx =
            std::clamp(
                static_cast<int>(
                    std::lround(
                        static_cast<double>(
                            viewport.height
                        ) *
                        static_cast<double>(
                            m_host.m_detailVisuals
                                .bodyTitleHeightFraction
                        )
                    )
                ),
                m_host.m_detailVisuals.bodyTitleMinimumPx,
                m_host.m_detailVisuals.bodyTitleMaximumPx
            );

        const float titleMarginPx =
            std::max(
                12.0f,
                static_cast<float>(
                    viewport.height
                ) *
                m_host.m_detailVisuals
                    .bodyTitleMarginHeightFraction
            );

        auto& text =
            TextRenderer::instance();

        text.beginFrameForViewport(
            viewport.width,
            viewport.height
        );

        text.textDrawPx(
            planet.planetName,
            titleMarginPx,
            static_cast<float>(
                viewport.height -
                titleSizePx
            ) -
            titleMarginPx,
            titleSizePx,
            m_host.m_detailVisuals.bodyTitleColor
        );

        text.endFrame();
    }

    glEnable(GL_DEPTH_TEST);
}




// ============================================================================
// Details geometry and textured planet rendering
// ============================================================================





}
