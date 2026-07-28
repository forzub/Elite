#include "src/game/system_map/GalaxyMapRenderer.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <string>
#include <utility>

#include <glm/gtc/constants.hpp>

#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/system_map/GalaxyMapRenderContext.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/world/celestial/CelestialTypes.h"
#include "src/world/celestial/SystemMapTypes.h"
#include "src/world/coordinates/WorldPosition.h"

namespace
{
    glm::vec3 orbitCameraDirectionFromYawPitch(
        float yaw,
        float pitch
    )
    {
        const float cp = std::cos(pitch);
        const float sp = std::sin(pitch);
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);

        return glm::vec3(
            cp * sy,
            sp,
            cp * cy
        );
    }

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
        const world::celestial::PlayerNavigationState& navigation
    )
    {
        const auto system =
            std::find_if(
                galaxy.systems.begin(),
                galaxy.systems.end(),
                [&](const auto& candidate)
                {
                    return
                        candidate.id ==
                        navigation.currentSystemId;
                }
            );

        if (system != galaxy.systems.end())
        {
            return
                system->positionLy +
                navigation.systemLocalAu /
                    game::navigation::SystemNavigationGrid::AuPerLightYear;
        }

        return world::coordinates::toGalacticLy(
            navigation.worldPosition
        );
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
        const glm::mat4 projection =
            viewState.projectionMatrix(viewport);

        const glm::mat4 view =
            viewState.viewMatrix();

        const glm::mat4 mvp =
            projection * view;

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
            context.drawGalaxyNavigationGrid(
                viewport,
                mvp
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

        context.drawNavigationLayerPlaceholder();

        viewState.state().screenPoints.clear();

        const glm::vec3 cameraDirection =
            orbitCameraDirectionFromYawPitch(
                viewState.state().camera.yaw,
                viewState.state().camera.pitch
            );

        const glm::vec3 cameraPosition =
            viewState.state().camera.target +
            cameraDirection *
                viewState.state().camera.distance;

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

        context.drawGalaxyLabels(
            viewport,
            galaxy,
            mvp
        );

        context.drawGalaxyPlayerMarker(
            viewport,
            galaxy,
            navigation,
            mvp
        );
    }
}
