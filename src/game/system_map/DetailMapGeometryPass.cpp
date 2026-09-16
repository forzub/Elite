#include "src/game/system_map/DetailMapGeometryPass.h"
#include "src/game/system_map/LocalMapPrimitiveRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

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
    (void)maxRadiusMeters;

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

        const glm::vec4 volumeColor(
            0.30f,
            0.66f,
            0.92f,
            0.42f
        );

        for (const auto& edge : edges)
        {
            drawPlanetMapLine(
                projected[edge[0]],
                projected[edge[1]],
                volumeColor
            );
        }
    }

    const glm::vec4 hubOrbitColor(
        0.45f,
        0.78f,
        1.0f,
        0.75f
    );

    for (const auto& orbit : planet.hubOrbits)
    {
        if (!orbit.valid)
            continue;

        drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192,
            hubOrbitColor
        );
    }

    const glm::vec4 playerOrbitColor(
        1.0f,
        0.75f,
        0.25f,
        0.9f
    );

    for (const auto& orbit : planet.playerOrbits)
    {
        if (!orbit.valid)
            continue;

        drawDetailMapOrbit3D(
            orbit,
            planet,
            scale,
            centerPx,
            192,
            playerOrbitColor
        );
    }

    // Hub/ship/infrastructure markers are rendered by the shared tactical
    // object overlay. This geometry pass keeps celestial/orbit structure only.

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

    const glm::vec4 bodyMarkerColor(
        0.72f,
        0.74f,
        0.78f,
        0.92f
    );

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

        const double radiusPx =
            body.boundingRadiusMeters *
            scale *
            activeCamera().state.zoom;

        if (radiusPx >= 1.0)
        {
            drawPlanetMapCircle(
                p,
                radiusPx,
                96,
                bodyMarkerColor
            );
        }

        if (radiusPx < 12.0)
        {
            drawPlanetMapCross(
                p,
                3.0f,
                bodyMarkerColor
            );
        }
    }

    // Infrastructure and self-propelled craft use the same tactical glyph
    // layer as System and Hub maps. No legacy crosses/axes are drawn here.

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
    const glm::dvec2& b,
    const glm::vec4& color
)
{
    drawLocalMapLine(a, b, color);
}

void DetailMapGeometryPass::drawPlanetMapCross(
    const glm::dvec2& point,
    float size,
    const glm::vec4& color
)
{
    drawLocalMapCross(point, size, color);
}

void DetailMapGeometryPass::drawPlanetMapCircle(
    const glm::dvec2& center,
    double radiusPx,
    int segments,
    const glm::vec4& color
)
{
    drawLocalMapCircle(center, radiusPx, segments, color);
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
    (void)planet;
    (void)scale;
    (void)centerPx;

    const glm::dvec2 o =
        activeCamera().project(originMeters);

    const glm::dvec2 x =
        activeCamera().project(originMeters + axes.x * axisLenMeters);

    const glm::dvec2 y =
        activeCamera().project(originMeters + axes.y * axisLenMeters);

    const glm::dvec2 z =
        activeCamera().project(originMeters + axes.z * axisLenMeters);

    drawPlanetMapLine(
        o,
        x,
        glm::vec4(1.0f, 0.25f, 0.25f, 0.9f)
    );

    drawPlanetMapLine(
        o,
        y,
        glm::vec4(0.25f, 1.0f, 0.25f, 0.9f)
    );

    drawPlanetMapLine(
        o,
        z,
        glm::vec4(0.25f, 0.55f, 1.0f, 0.9f)
    );
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
    (void)planet;
    (void)scale;
    (void)centerPx;

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

    const glm::vec4 color(
        1.0f,
        0.92f,
        0.25f,
        0.95f
    );

    drawPlanetMapLine(a, b, color);
    drawPlanetMapCross(b, 4.0f, color);
}

void DetailMapGeometryPass::drawDetailMapOrbit3D(
    const world::celestial::DetailMapOrbit& orbit,
    const world::celestial::DetailMapSnapshot& planet,
    double scale,
    const glm::dvec2& centerPx,
    int segments,
    const glm::vec4& color
)
{
    (void)scale;
    (void)centerPx;

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

    std::vector<glm::dvec2> visibleEndpoints;
    std::vector<glm::dvec2> hiddenEndpoints;
    visibleEndpoints.reserve(static_cast<std::size_t>(segments) * 2u);
    hiddenEndpoints.reserve(static_cast<std::size_t>(segments) * 2u);

    for (int i = 0; i < segments; ++i)
    {
        const glm::dvec3 p0 =
            orbitPoint(i);

        const glm::dvec3 p1 =
            orbitPoint((i + 1) % segments);

        const glm::dvec3 mid =
            (p0 + p1) * 0.5;

        /*
            In Planet Details the far half of the orbit remains visible as a
            very weak navigation cue. Hub Map does not use this function.
        */
        const bool hidden =
            isHiddenBehindPlanet(mid);

        auto& endpoints =
            hidden
                ? hiddenEndpoints
                : visibleEndpoints;

        endpoints.push_back(activeCamera().project(p0));
        endpoints.push_back(activeCamera().project(p1));
    }

    glm::vec4 hiddenColor = color;
    hiddenColor.a *= 0.16f;

    drawLocalMapLines(
        hiddenEndpoints.data(),
        hiddenEndpoints.size(),
        hiddenColor
    );

    drawLocalMapLines(
        visibleEndpoints.data(),
        visibleEndpoints.size(),
        color
    );
}

}
