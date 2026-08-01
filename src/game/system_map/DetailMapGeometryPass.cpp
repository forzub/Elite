#include "src/game/system_map/DetailMapGeometryPass.h"
#include "src/game/system_map/LocalMapPrimitiveRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "render/HUD/TextRenderer.h"

namespace
{
constexpr double AU_KM = 149597870.7;

std::string formatDetailScaleDistance(double km)
{
    std::ostringstream stream;

    if (km >= AU_KM * 0.1)
        stream << std::fixed << std::setprecision(3) << (km / AU_KM) << " AU";
    else if (km >= 1000000.0)
        stream << std::fixed << std::setprecision(2) << (km / 1000000.0) << " M km";
    else if (km >= 1000.0)
        stream << std::fixed << std::setprecision(0) << km << " km";
    else
        stream << std::fixed << std::setprecision(1) << km << " km";

    return stream.str();
}
}

namespace game::system_map
{
const LocalMapCameraSnapshot& DetailMapGeometryPass::activeCamera() const
{
    if (!m_activeCamera)
        throw std::logic_error("DetailMapGeometryPass camera is unavailable");

    return *m_activeCamera;
}

void DetailMapGeometryPass::renderScene(
    const DetailMapPresentation& presentation,
    const Viewport& viewport,
    const world::celestial::DetailMapSnapshot& planet
)
{
    const LocalMapCameraSnapshot* previousCamera = m_activeCamera;
    m_activeCamera = &presentation.camera;

    struct RestoreCamera
    {
        const LocalMapCameraSnapshot*& slot;
        const LocalMapCameraSnapshot* previous;

        ~RestoreCamera()
        {
            slot = previous;
        }
    } restoreCamera {m_activeCamera, previousCamera};

    const glm::dvec2& centerPx = presentation.centerPx;
    const double maxRadiusMeters = presentation.maxRadiusMeters;
    const double scale = presentation.scale;

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
                activeCamera().project(corners[i]);
        }

        constexpr std::array<std::array<int, 2>, 12> edges = {{
            {{0, 1}}, {{1, 2}}, {{2, 3}}, {{3, 0}},
            {{4, 5}}, {{5, 6}}, {{6, 7}}, {{7, 4}},
            {{0, 4}}, {{1, 5}}, {{2, 6}}, {{3, 7}}
        }};

        glColor4f(0.30f, 0.66f, 0.92f, 0.42f);

        for (const auto& edge : edges)
        {
            drawPlanetMapLine(
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

        drawDetailMapOrbit3D(
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

        drawDetailMapOrbit3D(
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
            activeCamera().project(hub.positionMeters);

        glColor4f(0.3f, 0.9f, 1.0f, 1.0f);
        drawPlanetMapCross(p, 7.0f);

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

        drawPlanetMapAxes(
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

        drawPlanetMapVelocityArrow(
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
                m_visuals.selectedHubLabelColor
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
            activeCamera().project(body.positionMeters);

        glColor4f(0.72f, 0.74f, 0.78f, 0.92f);

        const double radiusPx =
            body.boundingRadiusMeters *
            scale *
            activeCamera().state.zoom;

        if (radiusPx >= 1.0)
        {
            drawPlanetMapCircle(
                p,
                radiusPx,
                96
            );
        }

        if (radiusPx < 12.0)
        {
            drawPlanetMapCross(p, 3.0f);
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
            activeCamera().project(station.positionMeters);

        glColor4f(
            m_visuals.stationMarkerColor.r,
            m_visuals.stationMarkerColor.g,
            m_visuals.stationMarkerColor.b,
            m_visuals.stationMarkerColor.a
        );
        drawPlanetMapCross(p, 6.0f);

        drawPlanetMapAxes(
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

        drawPlanetMapVelocityArrow(
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
            activeCamera().project(ship.positionMeters);

        glColor4f(
            m_visuals.selectedStationMarkerColor.r,
            m_visuals.selectedStationMarkerColor.g,
            m_visuals.selectedStationMarkerColor.b,
            m_visuals.selectedStationMarkerColor.a
        );
        drawPlanetMapCross(p, 8.0f);

        drawPlanetMapAxes(
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

        drawPlanetMapVelocityArrow(
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
            formatDetailScaleDistance(
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

    if (m_visuals.drawBodyTitle &&
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
                            m_visuals
                                .bodyTitleHeightFraction
                        )
                    )
                ),
                m_visuals.bodyTitleMinimumPx,
                m_visuals.bodyTitleMaximumPx
            );

        const float titleMarginPx =
            std::max(
                12.0f,
                static_cast<float>(
                    viewport.height
                ) *
                m_visuals
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
            m_visuals.bodyTitleColor
        );

        text.endFrame();
    }


}

void DetailMapGeometryPass::drawPlanetMapLine(
    const glm::dvec2& a,
    const glm::dvec2& b
)
{
    drawLocalMapLine(a, b);
}

void DetailMapGeometryPass::drawPlanetMapCross(
    const glm::dvec2& point,
    float size
)
{
    drawLocalMapCross(point, size);
}

void DetailMapGeometryPass::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments
)
{
    drawLocalMapCircle(center, radiusPx, segments);
}

void DetailMapGeometryPass::drawPlanetMapAxes(
    const glm::dvec3& originMeters,
    const world::celestial::LocalSceneAxes& axes,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double axisLenMeters
)
{
    const glm::dvec2 o =
        activeCamera().project(originMeters);

    const glm::dvec2 x =
        activeCamera().project(originMeters + axes.x * axisLenMeters);

    const glm::dvec2 y =
        activeCamera().project(originMeters + axes.y * axisLenMeters);

    const glm::dvec2 z =
        activeCamera().project(originMeters + axes.z * axisLenMeters);

    glColor4f(1.0f, 0.25f, 0.25f, 0.9f);
    drawPlanetMapLine(o, x);

    glColor4f(0.25f, 1.0f, 0.25f, 0.9f);
    drawPlanetMapLine(o, y);

    glColor4f(0.25f, 0.55f, 1.0f, 0.9f);
    drawPlanetMapLine(o, z);
}

void DetailMapGeometryPass::drawPlanetMapVelocityArrow(
    const glm::dvec3& originMeters,
    const glm::dvec3& velocityMps,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    double lenMeters
)
{
    const double speed =
        glm::length(velocityMps);

    if (speed < 0.001)
        return;

    const glm::dvec3 dir =
        velocityMps / speed;

    const glm::dvec2 a =
        activeCamera().project(originMeters);

    const glm::dvec2 b =
        activeCamera().project(originMeters + dir * lenMeters);

    glColor4f(1.0f, 0.92f, 0.25f, 0.95f);
    drawPlanetMapLine(a, b);

    drawPlanetMapCross(
        b,
        4.0f
    );
}

void DetailMapGeometryPass::drawDetailMapOrbit3D(
    const world::celestial::DetailMapOrbit& orbit,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments
)
{
    if (!orbit.valid || orbit.radiusMeters <= 1.0)
        return;

    segments =
        std::max(segments, 32);

    glm::dvec3 radial =
        orbit.radialAxis;

    glm::dvec3 prograde =
        orbit.progradeAxis;

    if (glm::length(radial) < 0.001)
        radial = glm::dvec3(1.0, 0.0, 0.0);

    if (glm::length(prograde) < 0.001)
        prograde = glm::dvec3(0.0, 0.0, 1.0);

    radial =
        glm::normalize(radial);

    prograde =
        glm::normalize(
            prograde -
            radial * glm::dot(prograde, radial)
        );

    GLfloat baseColor[4] =
    {
        1.0f,
        1.0f,
        1.0f,
        1.0f
    };

    glGetFloatv(
        GL_CURRENT_COLOR,
        baseColor
    );

    auto orbitPoint =
        [&](int i) -> glm::dvec3
        {
            const double a =
                glm::two_pi<double>() *
                static_cast<double>(i) /
                static_cast<double>(segments);

            return
                orbit.centerMeters +
                radial * std::cos(a) * orbit.radiusMeters +
                prograde * std::sin(a) * orbit.radiusMeters;
        };

    auto isHiddenBehindPlanet =
        [&](const glm::dvec3& worldPoint) -> bool
        {
            const glm::dvec3 relative =
                worldPoint -
                planet.planetCenterMeters;

            const glm::dvec3 cameraSpace =
                activeCamera().vectorToCamera(relative);

            const double projectedDistance2 =
                cameraSpace.x * cameraSpace.x +
                cameraSpace.y * cameraSpace.y;

            const double planetRadius =
                planet.planetRadiusMeters;

            const bool behindPlanetCenter =
                cameraSpace.z < 0.0;

            const bool insidePlanetDisc =
                projectedDistance2 <
                planetRadius * planetRadius;

            return
                behindPlanetCenter &&
                insidePlanetDisc;
        };

    glBegin(GL_LINES);

    for (int i = 0; i < segments; ++i)
    {
        const glm::dvec3 p0 =
            orbitPoint(i);

        const glm::dvec3 p1 =
            orbitPoint((i + 1) % segments);



const glm::dvec3 mid =
    (p0 + p1) * 0.5;

/*
    В Planet Details дальняя половина орбиты остаётся
    видимой как очень слабая навигационная подсказка.

    Это относится только к карте Details. На карте Hub
    эта функция не используется.
*/
const bool hidden =
    isHiddenBehindPlanet(mid);

const float alpha =
    hidden
        ? baseColor[3] * 0.16f
        : baseColor[3];

glColor4f(
    baseColor[0],
    baseColor[1],
    baseColor[2],
    alpha
);




        const glm::dvec2 s0 =
            activeCamera().project(p0);

        const glm::dvec2 s1 =
            activeCamera().project(p1);

        glVertex2d(s0.x, s0.y);
        glVertex2d(s1.x, s1.y);
    }

    glEnd();

    glColor4f(
        baseColor[0],
        baseColor[1],
        baseColor[2],
        baseColor[3]
    );
}

}
