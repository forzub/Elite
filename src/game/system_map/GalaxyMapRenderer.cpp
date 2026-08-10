#include "src/game/system_map/GalaxyMapRenderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/system_map/GalaxyMapRenderContext.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{

    float starTypeVisualScale(
        const std::string& starType
    )
    {
        if (starType.empty())
            return 1.0f;

        const char spectralClass =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        starType.front()
                    )
                )
            );

        switch (spectralClass)
        {
            case 'O': return 1.65f;
            case 'B': return 1.45f;
            case 'A': return 1.25f;
            case 'F': return 1.12f;
            case 'G': return 1.00f;
            case 'K': return 0.90f;
            case 'M': return 0.78f;
            case 'D': return 0.68f;
            case 'L': return 0.66f;
            case 'T': return 0.62f;
            default:  return 1.00f;
        }
    }

    glm::vec4 colorForStarType(
        const std::string& starType
    )
    {
        if (starType.empty())
            return {1.0f, 0.86f, 0.65f, 1.0f};

        const char type =
            static_cast<char>(
                std::toupper(
                    static_cast<unsigned char>(
                        starType.front()
                    )
                )
            );

        switch (type)
        {
            case 'O': return {0.61f, 0.69f, 1.00f, 1.0f};
            case 'B': return {0.66f, 0.75f, 1.00f, 1.0f};
            case 'A': return {0.86f, 0.91f, 1.00f, 1.0f};
            case 'F': return {0.97f, 0.97f, 1.00f, 1.0f};
            case 'G': return {1.00f, 0.92f, 0.62f, 1.0f};
            case 'K': return {1.00f, 0.73f, 0.45f, 1.0f};
            case 'M': return {1.00f, 0.43f, 0.31f, 1.0f};
            default:  return {1.00f, 0.86f, 0.65f, 1.0f};
        }
    }

    glm::vec2 projectToScreen(
        const glm::vec3& world,
        const glm::mat4& mvp,
        const Viewport& viewport,
        bool& visible,
        float& depth
    )
    {
        const glm::vec4 clip =
            mvp * glm::vec4(world, 1.0f);

        visible = false;
        depth = 1.0f;

        if (std::abs(clip.w) < 0.00001f)
            return {0.0f, 0.0f};

        const glm::vec3 ndc =
            glm::vec3(clip) / clip.w;

        visible =
            ndc.x >= -1.0f && ndc.x <= 1.0f &&
            ndc.y >= -1.0f && ndc.y <= 1.0f &&
            ndc.z >= -1.0f && ndc.z <= 1.0f;

        depth = ndc.z;

        return {
            (ndc.x * 0.5f + 0.5f) *
                static_cast<float>(viewport.width),
            (1.0f - (ndc.y * 0.5f + 0.5f)) *
                static_cast<float>(viewport.height)
        };
    }

    glm::dvec3 playerPositionLy(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& navigation,
        bool& outInsideKnownSystem
    )
    {
        const auto marker =
            game::presentation::resolveGalaxyPlayerMarkerPosition(
                galaxy,
                navigation
            );

        outInsideKnownSystem = marker.insideKnownSystem;
        return marker.positionLy;
    }

    glm::dvec3 playerPositionLy(
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& navigation
    )
    {
        return
            game::presentation::resolveGalaxyPlayerMarkerPosition(
                galaxy,
                navigation
            ).positionLy;
    }

    std::string formatDistanceLy(double distanceLy)
    {
        std::ostringstream stream;

        if (distanceLy < 0.01)
        {
            stream
                << std::fixed
                << std::setprecision(4)
                << distanceLy
                << " ly";
        }
        else if (distanceLy < 10.0)
        {
            stream
                << std::fixed
                << std::setprecision(2)
                << distanceLy
                << " ly";
        }
        else
        {
            stream
                << std::fixed
                << std::setprecision(1)
                << distanceLy
                << " ly";
        }

        return stream.str();
    }

    void addNavigationCubeEdges(
        game::system_map::GalaxyMapRenderContext& context,
        const glm::vec3& center,
        const glm::vec3& halfAxisX,
        const glm::vec3& halfAxisY,
        const glm::vec3& halfAxisZ,
        const glm::vec4& color
    )
    {
        const std::array<glm::vec3, 8> corners =
        {
            center - halfAxisX - halfAxisY - halfAxisZ,
            center + halfAxisX - halfAxisY - halfAxisZ,
            center - halfAxisX + halfAxisY - halfAxisZ,
            center + halfAxisX + halfAxisY - halfAxisZ,
            center - halfAxisX - halfAxisY + halfAxisZ,
            center + halfAxisX - halfAxisY + halfAxisZ,
            center - halfAxisX + halfAxisY + halfAxisZ,
            center + halfAxisX + halfAxisY + halfAxisZ
        };

        static constexpr int edges[12][2] =
        {
            {0, 1}, {2, 3}, {4, 5}, {6, 7},
            {0, 2}, {1, 3}, {4, 6}, {5, 7},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        };

        const glm::vec4 weakColor(
            color.r,
            color.g,
            color.b,
            color.a * 0.38f
        );

        for (const auto& edge : edges)
        {
            const glm::vec3 a = corners[edge[0]];
            const glm::vec3 b = corners[edge[1]];
            const glm::vec3 delta = b - a;

            context.addLine(a, a + delta * 0.18f, color);
            context.addLine(
                a + delta * 0.18f,
                a + delta * 0.32f,
                weakColor
            );
            context.addLine(
                a + delta * 0.68f,
                a + delta * 0.82f,
                weakColor
            );
            context.addLine(a + delta * 0.82f, b, color);
        }
    }

    void addStarHalo(
        game::system_map::GalaxyMapRenderContext& context,
        const glm::vec3& center,
        float starRadius,
        float outerRadiusScale,
        float baseAlpha,
        const glm::vec4& color,
        const glm::mat4& view,
        int ringCount,
        int segments
    )
    {
        if (starRadius <= 0.0f ||
            outerRadiusScale <= 1.0f ||
            baseAlpha <= 0.0f ||
            ringCount <= 0 ||
            segments < 8)
        {
            return;
        }

        glm::vec3 cameraRight(
            view[0][0],
            view[1][0],
            view[2][0]
        );

        glm::vec3 cameraUp(
            view[0][1],
            view[1][1],
            view[2][1]
        );

        if (glm::length(cameraRight) < 0.000001f ||
            glm::length(cameraUp) < 0.000001f)
        {
            return;
        }

        cameraRight = glm::normalize(cameraRight);
        cameraUp = glm::normalize(cameraUp);

        ringCount = std::max(ringCount, 1);
        segments = std::max(segments, 8);

        for (int ring = 0; ring < ringCount; ++ring)
        {
            const float t =
                ringCount > 1
                    ? static_cast<float>(ring) /
                        static_cast<float>(ringCount - 1)
                    : 0.0f;

            constexpr float innerHaloRadiusScale = 2.10f;

            const float radiusScale =
                innerHaloRadiusScale +
                (outerRadiusScale - innerHaloRadiusScale) * t;

            const float radius = starRadius * radiusScale;
            const float inverseT = 1.0f - t;

            const float fade =
                0.28f +
                0.72f * std::pow(inverseT, 1.65f);

            glm::vec4 ringColor = color;
            ringColor.w = baseAlpha * fade;

            for (int segment = 0;
                 segment < segments;
                 ++segment)
            {
                const float angle0 =
                    glm::two_pi<float>() *
                    static_cast<float>(segment) /
                    static_cast<float>(segments);

                const float angle1 =
                    glm::two_pi<float>() *
                    static_cast<float>(segment + 1) /
                    static_cast<float>(segments);

                const glm::vec3 point0 =
                    center +
                    (
                        std::cos(angle0) * cameraRight +
                        std::sin(angle0) * cameraUp
                    ) * radius;

                const glm::vec3 point1 =
                    center +
                    (
                        std::cos(angle1) * cameraRight +
                        std::sin(angle1) * cameraUp
                    ) * radius;

                context.addLine(
                    point0,
                    point1,
                    ringColor
                );
            }
        }
    }
}

namespace game::system_map
{
    void GalaxyMapRenderer::render(
        GalaxyMapView& viewState,
        GalaxyMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& navigation
    ) const
    {
        viewState.synchronizeCatalogRoots(galaxy);

        const GalaxyMapCameraSnapshot camera =
            viewState.cameraSnapshot(viewport);

        const glm::mat4& projection = camera.projection;
        const glm::mat4& view = camera.view;
        const glm::mat4& mvp = camera.mvp;

        if (viewState.visuals().drawStarfield)
        {
            context.drawMapStarfield(
                viewport,
                playerPositionLy(galaxy, navigation),
                view,
                viewState.visuals().starfieldFieldOfViewDeg,
                viewState.visuals().starfieldSizeScale,
                true,
                viewState.visuals().starfieldBrightnessScale,
                viewState.visuals().milkyWayIntensityScale,
                viewState.visuals().milkyWayColorTint
            );
        }

        if (viewState.visuals().drawAtmosphereVeil)
        {
            context.drawMapAtmosphereVeil(
                viewState.visuals().atmosphereVeilCenterAlpha,
                viewState.visuals().atmosphereVeilEdgeAlpha,
                viewState.visuals().atmosphereVeilAquaStrength
            );
        }

        context.beginLines();
        context.beginSolids();

        if (viewState.state().navigationGrid.enabled())
        {
            drawNavigationGrid(
                viewState,
                context,
                viewport,
                camera
            );
        }
        else
        {
            const glm::vec4 gridColor {
                0.10f,
                0.28f,
                0.43f,
                0.22f
            };

            for (int i = -20; i <= 20; ++i)
            {
                const float value =
                    static_cast<float>(i) * 5.0f;

                context.addLine(
                    {-100.0f, 0.0f, value},
                    { 100.0f, 0.0f, value},
                    gridColor
                );

                context.addLine(
                    {value, 0.0f, -100.0f},
                    {value, 0.0f,  100.0f},
                    gridColor
                );
            }
        }

        // Galaxy route, beacon and restriction layers are intentionally
        // empty until their world data is introduced.

        viewState.state().screenPoints.clear();

        const glm::vec3 cameraDirection =
            glm::vec3(camera.basis.direction);

        const glm::vec3 cameraPosition =
            glm::vec3(camera.eye);

        const float safeViewportHeight =
            static_cast<float>(
                std::max(viewport.height, 1)
            );

        const float tanHalfFov =
            std::tan(
                glm::radians(48.0f) * 0.5f
            );

        for (const auto& system : galaxy.systems)
        {
            const glm::vec3 position =
                viewState.positionLyToRender(
                    system.positionLy
                );

            const bool isCurrent =
                system.id == navigation.currentSystemId;

            const bool isSelected =
                system.id ==
                    viewState.state().selectedSystemId;

            glm::vec4 color =
                colorForStarType(system.starType);

            const float viewDepth =
                std::max(
                    0.1f,
                    glm::dot(
                        position - cameraPosition,
                        -cameraDirection
                    )
                );

            const float worldUnitsPerPixel =
                2.0f *
                viewDepth *
                tanHalfFov /
                safeViewportHeight;

            float starScale =
                starTypeVisualScale(system.starType);

            if (system.starsCount > 1)
            {
                starScale *=
                    1.0f +
                    std::min(
                        0.24f,
                        static_cast<float>(
                            system.starsCount - 1
                        ) *
                        viewState.visuals().multipleStarScale
                    );
            }

            if (isCurrent)
            {
                starScale *=
                    viewState.visuals().currentStarScale;
            }

            if (isSelected)
            {
                starScale *=
                    viewState.visuals().selectedStarScale;
            }

            const float starRadius =
                viewState.visuals().starBaseRadiusPx *
                starScale *
                worldUnitsPerPixel;

            bool insideSelectedCube = false;
            bool insideHoveredCube = false;
            float hoveredCubeVisualAlpha = 0.0f;

            if (viewState.state().navigationGrid.enabled())
            {
                const auto starCellIndex =
                    viewState.state().navigationGrid
                        .nearestIndexForPositionLy(
                            system.positionLy,
                            viewState.state().navigationGrid.level()
                        );

                if (viewState.state().navigationGrid.hasSelectedCell())
                {
                    const auto& selectedCell =
                        viewState.state().navigationGrid.selectedCell();

                    if (selectedCell.level ==
                        viewState.state().navigationGrid.level())
                    {
                        insideSelectedCube =
                            starCellIndex ==
                            selectedCell.index;
                    }
                }

                if (viewState.state().hoverVisualCell.has_value() &&
                    starCellIndex ==
                        viewState.state().hoverVisualCell->index)
                {
                    hoveredCubeVisualAlpha =
                        std::max(
                            hoveredCubeVisualAlpha,
                            viewState.state().hoverVisualAlpha
                        );
                }

                if (viewState.state().hoverOutgoingCell.has_value() &&
                    starCellIndex ==
                        viewState.state().hoverOutgoingCell->index)
                {
                    hoveredCubeVisualAlpha =
                        std::max(
                            hoveredCubeVisualAlpha,
                            viewState.state().hoverOutgoingAlpha
                        );
                }

                insideHoveredCube =
                    hoveredCubeVisualAlpha > 0.001f;
            }

            if (insideHoveredCube)
            {
                addStarHalo(
                    context,
                    position,
                    starRadius,
                    viewState.visuals().hoveredCubeHaloRadiusScale,
                    viewState.visuals().hoveredCubeHaloAlpha *
                        hoveredCubeVisualAlpha,
                    color,
                    view,
                    viewState.visuals().starHaloRingCount,
                    viewState.visuals().starHaloSegments
                );
            }
            else if (insideSelectedCube)
            {
                addStarHalo(
                    context,
                    position,
                    starRadius,
                    viewState.visuals().fixedCubeHaloRadiusScale,
                    viewState.visuals().fixedCubeHaloAlpha,
                    color,
                    view,
                    viewState.visuals().starHaloRingCount,
                    viewState.visuals().starHaloSegments
                );
            }

            color.w = 1.0f;

            context.addBillboardBall(
                position,
                starRadius,
                color,
                view,
                32
            );

            GalaxyMapScreenPoint screenPoint;
            screenPoint.systemId = system.id;
            screenPoint.name = system.name;
            screenPoint.world = position;
            screenPoint.screen =
                projectToScreen(
                    position,
                    mvp,
                    viewport,
                    screenPoint.visible,
                    screenPoint.depth
                );

            viewState.state().screenPoints.push_back(
                std::move(screenPoint)
            );
        }

        context.flushSolids(mvp);
        context.flushLines(mvp);

        drawLabels(
            viewState,
            context,
            viewport,
            galaxy,
            navigation,
            mvp
        );

        drawPlayerMarker(
            viewState,
            context,
            viewport,
            galaxy,
            navigation,
            mvp
        );
    }

void GalaxyMapRenderer::drawNavigationGrid(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const Viewport& vp,
    const GalaxyMapCameraSnapshot& camera
) const
{
    if (!viewState.state().navigationGrid.enabled())
        return;

    const auto& frame =
        viewState.state().navigationGrid.frame();

    const bool currentLevelCellsInteractive =
        viewState.navigationCellsInteractive(vp);

    /*
        Logical hover is used for input. Visual hover has its own lifetime,
        so a cube can fade in and fade out instead of blinking on cell
        boundaries or when the cursor leaves the map.
    */
    const double hoverNowSeconds =
        context.currentTimeSeconds();

    double hoverDeltaSeconds = 0.0;

    if (viewState.state().hoverVisualLastTimeSeconds > 0.0)
    {
        hoverDeltaSeconds =
            std::clamp(
                hoverNowSeconds -
                    viewState.state().hoverVisualLastTimeSeconds,
                0.0,
                0.10
            );
    }

    viewState.state().hoverVisualLastTimeSeconds =
        hoverNowSeconds;

    std::optional<game::navigation::GalaxyNavigationCell>
        hoverTargetCell;

    const auto anchorCell =
        viewState.state().navigationGrid.anchorCell();

    if (currentLevelCellsInteractive &&
        viewState.state().navigationGrid.hasHoveredCell())
    {
        const auto& logicalHovered =
            viewState.state().navigationGrid.hoveredCell();

        if (logicalHovered.level != anchorCell.level ||
            logicalHovered.index != anchorCell.index)
        {
            hoverTargetCell = logicalHovered;
        }
    }

    const bool hoverTargetChanged =
        hoverTargetCell.has_value() &&
        (
            !viewState.state().hoverVisualCell.has_value() ||
            hoverTargetCell->level !=
                viewState.state().hoverVisualCell->level ||
            hoverTargetCell->index !=
                viewState.state().hoverVisualCell->index
        );

    if (hoverTargetChanged)
    {
        if (viewState.state().hoverVisualCell.has_value() &&
            viewState.state().hoverVisualAlpha > 0.001f)
        {
            viewState.state().hoverOutgoingCell =
                viewState.state().hoverVisualCell;

            viewState.state().hoverOutgoingAlpha =
                viewState.state().hoverVisualAlpha;
        }

        viewState.state().hoverVisualCell =
            hoverTargetCell;

        viewState.state().hoverVisualAlpha =
            0.0f;
    }

    if (hoverTargetCell.has_value())
    {
        const float fadeInSeconds =
            std::max(
                0.001f,
                viewState.controls().navigationHoverFadeInSeconds
            );

        viewState.state().hoverVisualAlpha =
            std::min(
                1.0f,
                viewState.state().hoverVisualAlpha +
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeInSeconds
            );
    }
    else
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                viewState.controls().navigationHoverFadeOutSeconds
            );

        viewState.state().hoverVisualAlpha =
            std::max(
                0.0f,
                viewState.state().hoverVisualAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (viewState.state().hoverVisualAlpha <= 0.001f)
        {
            viewState.state().hoverVisualAlpha = 0.0f;
            viewState.state().hoverVisualCell.reset();
        }
    }

    if (viewState.state().hoverOutgoingCell.has_value())
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                viewState.controls().navigationHoverFadeOutSeconds
            );

        viewState.state().hoverOutgoingAlpha =
            std::max(
                0.0f,
                viewState.state().hoverOutgoingAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (viewState.state().hoverOutgoingAlpha <= 0.001f)
        {
            viewState.state().hoverOutgoingAlpha = 0.0f;
            viewState.state().hoverOutgoingCell.reset();
        }
    }


        std::vector<
            game::navigation::GalaxyNavigationCell
        > cells;

        cells.reserve(4);

        cells.push_back(anchorCell);

        /*
            Selection is independent from the view anchor.
            Draw it only when its stored precision matches the
            currently displayed level.
        */
        if (viewState.state().navigationGrid.hasSelectedCell())
        {
            const auto& selectedCell =
                viewState.state().navigationGrid.selectedCell();

            if (selectedCell.level == anchorCell.level &&
                selectedCell.index != anchorCell.index)
            {
                cells.push_back(selectedCell);
            }
        }

        if (viewState.state().hoverVisualCell.has_value() &&
            viewState.state().hoverVisualAlpha > 0.001f)
        {
            const auto& hoveredCell =
                viewState.state().hoverVisualCell.value();

            const bool alreadyPresent =
                std::any_of(
                    cells.begin(),
                    cells.end(),
                    [&](const auto& existing)
                    {
                        return
                            existing.level == hoveredCell.level &&
                            existing.index == hoveredCell.index;
                    }
                );

            if (!alreadyPresent)
            {
                cells.push_back(hoveredCell);
            }
        }

        if (viewState.state().hoverOutgoingCell.has_value() &&
            viewState.state().hoverOutgoingAlpha > 0.001f)
        {
            const auto& outgoingCell =
                viewState.state().hoverOutgoingCell.value();

            const bool alreadyPresent =
                std::any_of(
                    cells.begin(),
                    cells.end(),
                    [&](const auto& existing)
                    {
                        return
                            existing.level == outgoingCell.level &&
                            existing.index == outgoingCell.index;
                    }
                );

            if (!alreadyPresent)
                cells.push_back(outgoingCell);
        }

        const glm::vec3 cameraRight =
            glm::vec3(camera.basis.right);

        const glm::vec3 cameraUp =
            glm::vec3(camera.basis.up);


        /*
            Положение и направление Galaxy-камеры нужны,
            чтобы экранный ромб сохранял постоянный размер
            независимо от удаления камеры.
        */
        const glm::vec3 cameraDirection =
            glm::vec3(camera.basis.direction);

        const glm::vec3 cameraPosition =
            glm::vec3(camera.eye);

    const glm::vec3 cameraRayDirection =
        -cameraDirection;

    /*
        Расстояние от камеры до первого пересечения центрального луча
        с ориентированным кубом.

        0 означает, что камера находится внутри куба.
        max() означает, что куб не находится перед камерой.
    */
    const auto cameraRayEntryDistanceToCube =
        [&](const glm::vec3& cubeCenter,
            const glm::vec3& halfAxisX,
            const glm::vec3& halfAxisY,
            const glm::vec3& halfAxisZ) -> float
        {
            const std::array<glm::vec3, 3> halfAxes =
            {
                halfAxisX,
                halfAxisY,
                halfAxisZ
            };

            float nearDistance =
                -std::numeric_limits<float>::max();

            float farDistance =
                std::numeric_limits<float>::max();

            const glm::vec3 originFromCenter =
                cameraPosition - cubeCenter;

            for (const glm::vec3& halfAxis : halfAxes)
            {
                const float extent =
                    glm::length(halfAxis);

                if (extent <= 0.000001f)
                    return std::numeric_limits<float>::max();

                const glm::vec3 axis =
                    halfAxis / extent;

                const float originOnAxis =
                    glm::dot(
                        originFromCenter,
                        axis
                    );

                const float directionOnAxis =
                    glm::dot(
                        cameraRayDirection,
                        axis
                    );

                if (std::abs(directionOnAxis) <= 0.000001f)
                {
                    if (std::abs(originOnAxis) > extent)
                    {
                        return
                            std::numeric_limits<float>::max();
                    }

                    continue;
                }

                float distanceA =
                    (-extent - originOnAxis) /
                    directionOnAxis;

                float distanceB =
                    ( extent - originOnAxis) /
                    directionOnAxis;

                if (distanceA > distanceB)
                    std::swap(distanceA, distanceB);

                nearDistance =
                    std::max(
                        nearDistance,
                        distanceA
                    );

                farDistance =
                    std::min(
                        farDistance,
                        distanceB
                    );

                if (nearDistance > farDistance)
                {
                    return
                        std::numeric_limits<float>::max();
                }
            }

            if (farDistance < 0.0f)
                return std::numeric_limits<float>::max();

            return std::max(nearDistance, 0.0f);
        };

    /*
        Экранный размер, при котором сетка имеет полную заданную
        непрозрачность. Значение 0.60 соответствует примерно тому
        масштабу куба, который показан на контрольном скриншоте.

        Когда куб становится меньше на треть, сетка полностью исчезает.
    */
    const float gridFullSizePx =
        static_cast<float>(
            std::max(
                1,
                std::min(vp.width, vp.height)
            )
        ) * 0.60f;

    const float gridHiddenSizePx =
        gridFullSizePx * (2.0f / 3.0f);

    const float tanHalfGalaxyFov =
        std::tan(
            glm::radians(48.0f) * 0.5f
        );

    const auto cubeGridVisibility =
        [&](const glm::vec3& cubeCenter,
            const glm::vec3& halfAxisX,
            const glm::vec3& halfAxisY,
            const glm::vec3& halfAxisZ) -> float
        {
            const float viewDepth =
                glm::dot(
                    cubeCenter - cameraPosition,
                    -cameraDirection
                );

            if (viewDepth <= 0.001f)
                return 0.0f;

            const float cubeEdgeWorld =
                2.0f *
                std::max({
                    glm::length(halfAxisX),
                    glm::length(halfAxisY),
                    glm::length(halfAxisZ)
                });

            /*
                1.35 приближает обычный экранный bounding box
                повёрнутого куба, а не только проекцию одной грани.
            */
            const float cubeSizePx =
                cubeEdgeWorld *
                1.35f *
                static_cast<float>(std::max(vp.height, 1)) /
                (2.0f * viewDepth * tanHalfGalaxyFov);

            if (cubeSizePx <= gridHiddenSizePx)
                return 0.0f;

            if (cubeSizePx >= gridFullSizePx)
                return 1.0f;

            float visibility =
                (cubeSizePx - gridHiddenSizePx) /
                (gridFullSizePx - gridHiddenSizePx);

            visibility =
                std::clamp(
                    visibility,
                    0.0f,
                    1.0f
                );

            /* Smoothstep: исчезновение без заметной ступеньки. */
            return
                visibility *
                visibility *
                (3.0f - 2.0f * visibility);
        };

    const int faceDivisions =
        viewState.state().navigationGrid.subdivision();

    const auto addCubeFarFaceGrids =
        [&](const glm::vec3& cubeCenter,
            const glm::vec3& halfAxisX,
            const glm::vec3& halfAxisY,
            const glm::vec3& halfAxisZ,
            glm::vec4 gridColor)
        {
            const float visibility =
                cubeGridVisibility(
                    cubeCenter,
                    halfAxisX,
                    halfAxisY,
                    halfAxisZ
                );

            if (visibility <= 0.001f)
                return;

            gridColor.a *= visibility;

            const auto addFarFace =
                [&](const glm::vec3& faceHalfAxis,
                    const glm::vec3& gridHalfAxisU,
                    const glm::vec3& gridHalfAxisV)
                {
                    const glm::vec3 toCamera =
                        cameraPosition - cubeCenter;

                    const float farSide =
                        glm::dot(
                            toCamera,
                            faceHalfAxis
                        ) >= 0.0f
                            ? -1.0f
                            : 1.0f;

                    const glm::vec3 faceCenter =
                        cubeCenter +
                        faceHalfAxis * farSide;

                    for (int division = 1;
                         division < faceDivisions;
                         ++division)
                    {
                        const float t =
                            -1.0f +
                            2.0f *
                                static_cast<float>(division) /
                                static_cast<float>(faceDivisions);

                        const glm::vec3 alongU =
                            gridHalfAxisU * t;

                        const glm::vec3 alongV =
                            gridHalfAxisV * t;

                        context.addLine(
                            faceCenter + alongU - gridHalfAxisV,
                            faceCenter + alongU + gridHalfAxisV,
                            gridColor
                        );

                        context.addLine(
                            faceCenter + alongV - gridHalfAxisU,
                            faceCenter + alongV + gridHalfAxisU,
                            gridColor
                        );
                    }
                };

            addFarFace(
                halfAxisX,
                halfAxisY,
                halfAxisZ
            );

            addFarFace(
                halfAxisY,
                halfAxisX,
                halfAxisZ
            );

            addFarFace(
                halfAxisZ,
                halfAxisX,
                halfAxisY
            );
        };

    /*
        Root не является выбираемым уровнем карты, но пять разрешённых
        архивкубов образуют видимую границу игрового пространства.
    */
    const double rootEdgeLy =
        viewState.state().navigationGrid.config().rootEdgeLy();

    const double rootHalfEdgeLy =
        rootEdgeLy * 0.5;

    const glm::vec3 rootHalfAxisX =
        viewState.vectorLyToRender(frame.axisX * rootHalfEdgeLy);
    const glm::vec3 rootHalfAxisY =
        viewState.vectorLyToRender(frame.axisY * rootHalfEdgeLy);
    const glm::vec3 rootHalfAxisZ =
        viewState.vectorLyToRender(frame.axisZ * rootHalfEdgeLy);

    std::array<std::int64_t, 3>
        focusedRootIndex {0, 0, 0};

    bool hasFocusedRoot = false;
    bool focusedRootWasHitByRay = false;

    float focusedRootRayDistance =
        std::numeric_limits<float>::max();

    float focusedRootFallbackDistanceSquared =
        std::numeric_limits<float>::max();

    /*
        Выбираем ровно один Root:

        1. ближайший куб, пересечённый центральным лучом камеры;
        2. если луч не пересёк ни один Root — ближайший к camera.target.
    */
    for (const auto& rootIndex :
         viewState.state().navigationGrid.allowedRootCells())
    {
        const glm::dvec3 rootCenterLy =
            frame.origin +
            frame.axisX *
                (static_cast<double>(rootIndex[0]) * rootEdgeLy) +
            frame.axisY *
                (static_cast<double>(rootIndex[1]) * rootEdgeLy) +
            frame.axisZ *
                (static_cast<double>(rootIndex[2]) * rootEdgeLy);

        const glm::vec3 rootCenter =
            viewState.positionLyToRender(rootCenterLy);

        const float rayDistance =
            cameraRayEntryDistanceToCube(
                rootCenter,
                rootHalfAxisX,
                rootHalfAxisY,
                rootHalfAxisZ
            );

        const bool rayHit =
            rayDistance <
                std::numeric_limits<float>::max();

        const glm::vec3 fallbackDelta =
            rootCenter - viewState.state().camera.target;

        const float fallbackDistanceSquared =
            glm::dot(
                fallbackDelta,
                fallbackDelta
            );

        if (rayHit)
        {
            if (!focusedRootWasHitByRay ||
                rayDistance < focusedRootRayDistance)
            {
                focusedRootIndex = rootIndex;
                focusedRootRayDistance = rayDistance;
                hasFocusedRoot = true;
                focusedRootWasHitByRay = true;
            }
        }
        else if (!focusedRootWasHitByRay &&
                 (!hasFocusedRoot ||
                  fallbackDistanceSquared <
                      focusedRootFallbackDistanceSquared))
        {
            focusedRootIndex = rootIndex;
            focusedRootFallbackDistanceSquared =
                fallbackDistanceSquared;
            hasFocusedRoot = true;
        }
    }

    for (const auto& rootIndex :
         viewState.state().navigationGrid.allowedRootCells())
    {
        const glm::dvec3 rootCenterLy =
            frame.origin +
            frame.axisX *
                (static_cast<double>(rootIndex[0]) * rootEdgeLy) +
            frame.axisY *
                (static_cast<double>(rootIndex[1]) * rootEdgeLy) +
            frame.axisZ *
                (static_cast<double>(rootIndex[2]) * rootEdgeLy);

        const glm::vec3 rootCenter =
            viewState.positionLyToRender(rootCenterLy);

        addNavigationCubeEdges(
            context,
            rootCenter,
            rootHalfAxisX,
            rootHalfAxisY,
            rootHalfAxisZ,
            viewState.visuals().navigationGrid.rootEdgeColor
        );

        const bool isFocusedRoot =
            hasFocusedRoot &&
            rootIndex == focusedRootIndex;

        if (isFocusedRoot &&
            viewState.state().navigationGrid.level() ==
                viewState.state().navigationGrid.minimumLevel())
        {
            addCubeFarFaceGrids(
                rootCenter,
                rootHalfAxisX,
                rootHalfAxisY,
                rootHalfAxisZ,
                viewState.visuals().navigationGrid.rootFaceGridColor
            );
        }
    }

    /*
        Начиная с G2 непосредственным родителем является уже не Root,
        а куб предыдущего рабочего уровня. Для выбранного и наведённого
        кубов родители могут различаться, поэтому собираем уникальный список.
    */
    if (viewState.state().navigationGrid.level() >
        viewState.state().navigationGrid.minimumLevel())
    {
        std::vector<game::navigation::GalaxyNavigationCell>
            parentCells;

        parentCells.reserve(cells.size());

        for (const auto& currentCell : cells)
        {
            game::navigation::GalaxyGridIndex parentIndex;

            const double subdivision =
                static_cast<double>(
                    viewState.state().navigationGrid.subdivision()
                );

            parentIndex.x =
                static_cast<std::int64_t>(
                    std::llround(
                        static_cast<double>(currentCell.index.x) /
                        subdivision
                    )
                );

            parentIndex.y =
                static_cast<std::int64_t>(
                    std::llround(
                        static_cast<double>(currentCell.index.y) /
                        subdivision
                    )
                );

            parentIndex.z =
                static_cast<std::int64_t>(
                    std::llround(
                        static_cast<double>(currentCell.index.z) /
                        subdivision
                    )
                );

            const auto parentCell =
                viewState.state().navigationGrid.cell(
                    parentIndex,
                    currentCell.level - 1
                );

            const bool alreadyPresent =
                std::any_of(
                    parentCells.begin(),
                    parentCells.end(),
                    [&](const auto& existing)
                    {
                        return
                            existing.level == parentCell.level &&
                            existing.index == parentCell.index;
                    }
                );

            if (!alreadyPresent)
                parentCells.push_back(parentCell);
        }

        std::size_t focusedParentIndex = 0;

        bool hasFocusedParent = false;
        bool focusedParentWasHitByRay = false;

        float focusedParentRayDistance =
            std::numeric_limits<float>::max();

        float focusedParentFallbackDistanceSquared =
            std::numeric_limits<float>::max();

        for (std::size_t parentIndex = 0;
             parentIndex < parentCells.size();
             ++parentIndex)
        {
            const auto& parentCell =
                parentCells[parentIndex];

            const glm::vec3 parentCenter =
                viewState.positionLyToRender(
                    parentCell.center
                );

            const double parentHalfSizeLy =
                parentCell.size * 0.5;

            const glm::vec3 parentHalfAxisX =
                viewState.vectorLyToRender(
                    frame.axisX * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisY =
                viewState.vectorLyToRender(
                    frame.axisY * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisZ =
                viewState.vectorLyToRender(
                    frame.axisZ * parentHalfSizeLy
                );

            const float rayDistance =
                cameraRayEntryDistanceToCube(
                    parentCenter,
                    parentHalfAxisX,
                    parentHalfAxisY,
                    parentHalfAxisZ
                );

            const bool rayHit =
                rayDistance <
                    std::numeric_limits<float>::max();

            const glm::vec3 fallbackDelta =
                parentCenter - viewState.state().camera.target;

            const float fallbackDistanceSquared =
                glm::dot(
                    fallbackDelta,
                    fallbackDelta
                );

            if (rayHit)
            {
                if (!focusedParentWasHitByRay ||
                    rayDistance < focusedParentRayDistance)
                {
                    focusedParentIndex = parentIndex;
                    focusedParentRayDistance = rayDistance;
                    hasFocusedParent = true;
                    focusedParentWasHitByRay = true;
                }
            }
            else if (!focusedParentWasHitByRay &&
                     (!hasFocusedParent ||
                      fallbackDistanceSquared <
                          focusedParentFallbackDistanceSquared))
            {
                focusedParentIndex = parentIndex;
                focusedParentFallbackDistanceSquared =
                    fallbackDistanceSquared;
                hasFocusedParent = true;
            }
        }

        for (std::size_t parentIndex = 0;
             parentIndex < parentCells.size();
             ++parentIndex)
        {
            const auto& parentCell =
                parentCells[parentIndex];

            const glm::vec3 parentCenter =
                viewState.positionLyToRender(
                    parentCell.center
                );

            const double parentHalfSizeLy =
                parentCell.size * 0.5;

            const glm::vec3 parentHalfAxisX =
                viewState.vectorLyToRender(
                    frame.axisX * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisY =
                viewState.vectorLyToRender(
                    frame.axisY * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisZ =
                viewState.vectorLyToRender(
                    frame.axisZ * parentHalfSizeLy
                );

            addNavigationCubeEdges(
                context,
                parentCenter,
                parentHalfAxisX,
                parentHalfAxisY,
                parentHalfAxisZ,
                viewState.visuals().navigationGrid.parentEdgeColor
            );

            if (hasFocusedParent &&
                parentIndex == focusedParentIndex)
            {
                addCubeFarFaceGrids(
                    parentCenter,
                    parentHalfAxisX,
                    parentHalfAxisY,
                    parentHalfAxisZ,
                    viewState.visuals().navigationGrid.parentFaceGridColor
                );
            }
        }
    }








    for (const auto& cell : cells)
    {
        /*
            Parent and Root context remains visible at every distance.
            Only current-level cells wait until they are large enough to
            be selected without ambiguity.
        */
        if (!currentLevelCellsInteractive)
            continue;

        const glm::vec3 center =
            viewState.positionLyToRender(
                cell.center
            );

        const double halfSizeLy =
            cell.size * 0.5;

        const glm::vec3 halfAxisX =
            viewState.vectorLyToRender(
                frame.axisX * halfSizeLy
            );

        const glm::vec3 halfAxisY =
            viewState.vectorLyToRender(
                frame.axisY * halfSizeLy
            );

        const glm::vec3 halfAxisZ =
            viewState.vectorLyToRender(
                frame.axisZ * halfSizeLy
            );

        const bool isAnchor =
            cell.index ==
            viewState.state().navigationGrid.anchorIndex();

        float hoverVisualAlpha = 0.0f;

        if (viewState.state().hoverVisualCell.has_value() &&
            cell.index ==
                viewState.state().hoverVisualCell->index &&
            cell.level ==
                viewState.state().hoverVisualCell->level)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    viewState.state().hoverVisualAlpha
                );
        }

        if (viewState.state().hoverOutgoingCell.has_value() &&
            cell.index ==
                viewState.state().hoverOutgoingCell->index &&
            cell.level ==
                viewState.state().hoverOutgoingCell->level)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    viewState.state().hoverOutgoingAlpha
                );
        }

        const bool isHovered =
            hoverVisualAlpha > 0.001f;

        const bool isSelected =
            viewState.state().navigationGrid.hasSelectedCell() &&
            cell.level ==
                viewState.state().navigationGrid.selectedCell().level &&
            cell.index ==
                viewState.state().navigationGrid.selectedCell().index;

        glm::vec4 edgeColor =
            viewState.visuals().navigationGrid.currentEdgeColor;

        glm::vec4 markerColor =
            viewState.visuals().navigationGrid.currentMarkerColor;

        if (isAnchor)
        {
            edgeColor.a =
                viewState.visuals().navigationGrid.anchorEdgeAlpha;
            markerColor.a =
                viewState.visuals().navigationGrid.anchorMarkerAlpha;
        }

        if (isHovered)
        {
            edgeColor =
                viewState.visuals().navigationGrid.hoveredEdgeColor;

            markerColor =
                viewState.visuals().navigationGrid.hoveredMarkerColor;

            edgeColor.a *=
                hoverVisualAlpha;

            markerColor.a *=
                hoverVisualAlpha;
        }

        if (isSelected)
        {
            edgeColor =
                viewState.visuals().navigationGrid.selectedEdgeColor;

            markerColor =
                viewState.visuals().navigationGrid.selectedMarkerColor;
        }

        glm::vec4 currentLevelGridColor =
            edgeColor;

        currentLevelGridColor.a =
            std::clamp(
                edgeColor.a *
                    viewState.visuals()
                        .navigationGrid
                        .currentFaceGridAlphaScale,
                viewState.visuals()
                    .navigationGrid
                    .currentFaceGridMinimumAlpha,
                viewState.visuals()
                    .navigationGrid
                    .currentFaceGridMaximumAlpha
            );

        addCubeFarFaceGrids(
            center,
            halfAxisX,
            halfAxisY,
            halfAxisZ,
            currentLevelGridColor
        );

        addNavigationCubeEdges(
            context,
            center,
            halfAxisX,
            halfAxisY,
            halfAxisZ,
            edgeColor
        );






        const float viewDepth =
            std::max(
                0.001f,
                glm::dot(
                    center - cameraPosition,
                    -cameraDirection
                )
            );

        const float worldUnitsPerPixel =
            2.0f *
            viewDepth *
            std::tan(
                glm::radians(48.0f) * 0.5f
            ) /
            static_cast<float>(
                std::max(vp.height, 1)
            );

        const float markerRadiusPx =
            (isHovered || isSelected)
                ? 5.0f
                : 4.0f;

        const float markerSize =
            worldUnitsPerPixel *
            markerRadiusPx;










        /*
            Плоский ромб всегда обращён к камере.
            Он визуально отличается от трёхмерного креста звезды.
        */
        const glm::vec3 markerTop =
            center + cameraUp * markerSize;

        const glm::vec3 markerRight =
            center + cameraRight * markerSize;

        const glm::vec3 markerBottom =
            center - cameraUp * markerSize;

        const glm::vec3 markerLeft =
            center - cameraRight * markerSize;

        context.addLine(
            markerTop,
            markerRight,
            markerColor
        );

        context.addLine(
            markerRight,
            markerBottom,
            markerColor
        );

        context.addLine(
            markerBottom,
            markerLeft,
            markerColor
        );

        context.addLine(
            markerLeft,
            markerTop,
            markerColor
        );




    }
}


void GalaxyMapRenderer::drawLabels(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation,
    const glm::mat4& mvp
) const
{




    struct Label
    {
        std::string title;
        std::string subtitle;
        glm::vec2 screen;
        glm::vec2 lineEnd;
        glm::vec2 textPos;
        bool selected = false;
    };


    std::vector<Label> labels;
    std::vector<glm::vec4> occupied;





    const float screenFactor =
        std::clamp(
            static_cast<float>(vp.height) /
                viewState.visuals().labelReferenceHeightPx,
            viewState.visuals().labelMinimumScreenScale,
            viewState.visuals().labelMaximumScreenScale
        );

    const float labelFactor =
        screenFactor *
        viewState.visuals().labelScale;

    const int titlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        viewState.visuals().labelTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            viewState.visuals().labelTitleMinPx,
            viewState.visuals().labelTitleMaxPx
        );

    const int selectedTitlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        viewState.visuals().labelSelectedTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            viewState.visuals().labelTitleMinPx,
            viewState.visuals().labelTitleMaxPx
        );

    const int subtitlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        viewState.visuals().labelSubtitleBasePx
                    ) *
                    labelFactor
                )
            ),
            viewState.visuals().labelSubtitleMinPx,
            viewState.visuals().labelSubtitleMaxPx
        );








    const glm::dvec3 observerPositionLy =
        playerPositionLy(galaxy, navigation);

    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            viewState.positionLyToRender(s.positionLy),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const bool selected = s.id == viewState.state().selectedSystemId;

        const glm::dvec3 observerDeltaLy =
            s.positionLy - observerPositionLy;

        const double distanceLy =
            std::sqrt(
                glm::dot(
                    observerDeltaLy,
                    observerDeltaLy
                )
            );

        const std::string subtitle =
            (s.starType.empty() ? "Unknown" : s.starType) +
            std::string("  /  ") +
            formatDistanceLy(distanceLy);

        const int px = selected ? selectedTitlePx : titlePx;

        const float titleW =
            static_cast<float>(s.name.size()) * static_cast<float>(px) * 0.58f;

        const float subtitleW =
            static_cast<float>(subtitle.size()) * static_cast<float>(subtitlePx) * 0.50f;

        const float w = std::max(titleW, subtitleW);
        const float h =
            static_cast<float>(px + subtitlePx) + 8.0f;

        // Смещение метки от звезды.
        // Чем выше разрешение — тем чуть дальше, но без дикости.


    // pos.y — это baseline текста, поэтому учитываем высоту блока.
    const float gap =
        std::clamp(static_cast<float>(vp.height) * 0.018f, 14.0f, 24.0f);

    glm::vec2 lineEnd {
        screen.x + gap,
        screen.y - gap
    };

    glm::vec2 textPos {
        lineEnd.x + 6.0f,
        lineEnd.y - 4.0f
    };

    // if (pos.x + w > vp.x + vp.width - 12.0f)
    //     pos.x = screen.x - w - gap;

    // if (pos.y - h < vp.y + 12.0f)
    //     pos.y = screen.y + h + gap;


        const glm::vec4 rect {
            textPos.x,
            textPos.y - static_cast<float>(px),
            textPos.x + w,
            textPos.y + static_cast<float>(subtitlePx) + 10.0f
        };

        bool overlap = false;

        for (const auto& r : occupied)
        {
            if (rect.x < r.z && rect.z > r.x &&
                rect.y < r.w && rect.w > r.y)
            {
                overlap = true;
                break;
            }
        }

        if (overlap && !selected)
            continue;

        occupied.push_back(rect);



        labels.push_back({
                s.name,
                subtitle,
                screen,
                lineEnd,
                textPos,
                selected
            });





    }

context.beginLines();

for (const auto& label : labels)
{
    const glm::vec4 lineColor =
        label.selected
            ? viewState.visuals().labels.selectedLeaderColor
            : viewState.visuals().labels.normalLeaderColor;

    context.addLine(
        glm::vec3(label.screen.x, label.screen.y, 0.0f),
        glm::vec3(label.lineEnd.x, label.lineEnd.y, 0.0f),
        lineColor
    );
}


// Label coordinates are local to the system-map viewport:
// x = 0..vp.width
// y = 0..vp.height
//
// Do NOT switch to full-window viewport here.
// Text rendering also normalizes coordinates using the map viewport,
// so lines and text must stay in the same map-local coordinate space.
const glm::mat4 labelOrtho =
    glm::ortho(
        0.0f,
        static_cast<float>(vp.width),
        static_cast<float>(vp.height),
        0.0f,
        -1.0f,
        1.0f
    );


context.flushLines(labelOrtho);


context.beginTextFrame(
    vp.width,
    vp.height
);

for (const auto& l : labels)
{
    const int px = l.selected ? selectedTitlePx : titlePx;

    const glm::vec4 titleColor =
        l.selected
            ? viewState.visuals().labels.selectedTitleColor
            : viewState.visuals().labels.normalTitleColor;

    const glm::vec4 subtitleColor =
        l.selected
            ? viewState.visuals().labels.selectedSubtitleColor
            : viewState.visuals().labels.normalSubtitleColor;


    context.drawTextPx(
        l.title,
        l.textPos.x,
        l.textPos.y,
        px,
        titleColor
    );

    context.drawTextPx(
        l.subtitle,
        l.textPos.x,
        l.textPos.y + static_cast<float>(px) + 2.0f,
        subtitlePx,
        subtitleColor
    );
}

context.endTextFrame();

}


void GalaxyMapRenderer::drawPlayerMarker(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav,
    const glm::mat4& mvp
) const
{
    bool insideKnownSystem = false;

    const glm::dvec3 playerPosition =
        playerPositionLy(
            galaxy,
            nav,
            insideKnownSystem
        );

    const glm::vec3 playerWorld =
        viewState.positionLyToRender(
            playerPosition
        );

    bool playerVisible = false;
    float playerDepth = 1.0f;

    const glm::vec2 playerScreen =
        projectToScreen(
            playerWorld,
            mvp,
            vp,
            playerVisible,
            playerDepth
        );

    (void)playerDepth;

    if (!playerVisible)
        return;

    const float screenScale =
        std::clamp(
            static_cast<float>(vp.height) / 1080.0f,
            0.72f,
            1.35f
        );

    const glm::vec4 markerColor {
        0.88f,
        0.75f,
        0.32f,
        0.78f
    };

    /*
        Вне известной системы показываем не условную звезду,
        а реальный терминальный Galaxy-куб, содержащий игрока.
    */
    if (!insideKnownSystem)
    {
        const int terminalLevel =
            viewState.state().navigationGrid.maximumLevel();

        const auto terminalIndex =
            viewState.state().navigationGrid.nearestIndexForPositionLy(
                playerPosition,
                terminalLevel
            );

        const auto terminalCell =
            viewState.state().navigationGrid.cell(
                terminalIndex,
                terminalLevel
            );

        const float halfEdge =
            static_cast<float>(terminalCell.size * 0.5) *
            GalaxyMapView::RenderUnitsPerLightYear;

        context.beginLines();

        addNavigationCubeEdges(
            context,
            viewState.positionLyToRender(terminalCell.center),
            glm::vec3(halfEdge, 0.0f, 0.0f),
            glm::vec3(0.0f, halfEdge, 0.0f),
            glm::vec3(0.0f, 0.0f, halfEdge),
            glm::vec4(
                markerColor.r,
                markerColor.g,
                markerColor.b,
                viewState.visuals().navigationGrid.terminalCubeAlpha
            )
        );

        context.flushLines(mvp);
    }

    const float arrowWidth = 5.0f * screenScale;
    const float arrowHeight = 8.0f * screenScale;
    const float leaderLength = 14.0f * screenScale;

    const glm::vec2 tip = playerScreen;
    const glm::vec2 left {
        playerScreen.x - arrowWidth,
        playerScreen.y - arrowHeight
    };
    const glm::vec2 right {
        playerScreen.x + arrowWidth,
        playerScreen.y - arrowHeight
    };

    const glm::vec2 leaderEnd {
        right.x + leaderLength,
        right.y - leaderLength * 0.45f
    };

    context.beginLines();
    context.addLine(
        glm::vec3(tip, 0.0f),
        glm::vec3(left, 0.0f),
        markerColor
    );
    context.addLine(
        glm::vec3(left, 0.0f),
        glm::vec3(right, 0.0f),
        markerColor
    );
    context.addLine(
        glm::vec3(right, 0.0f),
        glm::vec3(tip, 0.0f),
        markerColor
    );
    context.addLine(
        glm::vec3(right, 0.0f),
        glm::vec3(leaderEnd, 0.0f),
        glm::vec4(
            markerColor.r,
            markerColor.g,
            markerColor.b,
            viewState.visuals().navigationGrid.terminalLeaderAlpha
        )
    );

    const glm::mat4 markerOrtho =
        glm::ortho(
            0.0f,
            static_cast<float>(vp.width),
            static_cast<float>(vp.height),
            0.0f,
            -1.0f,
            1.0f
        );

    context.flushLines(markerOrtho);

    context.beginTextFrame(
        vp.width,
        vp.height
    );

    context.drawTextPx(
        "PLAYER",
        leaderEnd.x + 4.0f * screenScale,
        leaderEnd.y + 3.0f * screenScale,
        std::clamp(
            static_cast<int>(std::lround(11.0f * screenScale)),
            9,
            15
        ),
        markerColor
    );

    context.endTextFrame();
}

}
