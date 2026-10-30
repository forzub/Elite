#include "src/game/system_map/GalaxyMapRenderer.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/core/RuntimeTrace.h"
#include "src/game/navigation/SystemNavigationGrid.h"
#include "src/game/presentation/GalaxyNavigationPresentation.h"
#include "src/game/system_map/GalaxyMapInteraction.h"
#include "src/game/system_map/GalaxyMapRenderContext.h"
#include "src/game/system_map/GalaxyMapView.h"
#include "src/game/system_map/HierarchyFramePainter.h"
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

        // Behind the camera (w <= 0) is never visible.
        if (clip.w <= 0.00001f)
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
namespace
{
    // Presented medium per frame (blobs / shell ranges) for the trace.
    int g_mediumBlobs = 0;
    int g_mediumShells = 0;

    // [GalaxyMapPerf] interstellar-medium draw cost (ELITE_TRACE_RUNTIME=1).
    void traceMediumCost(double ms)
    {
        if (!core::runtimeTraceEnabled())
            return;
        static int frames = 0;
        static double sum = 0.0, peak = 0.0;
        static auto begin = std::chrono::steady_clock::now();
        ++frames;
        sum += ms;
        peak = std::max(peak, ms);
        const auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration<double>(now - begin).count() >= 2.0)
        {
            std::cerr << "[GalaxyMapPerf] medium_avg_ms=" << sum / frames << " medium_max_ms=" << peak
                      << " frames=" << frames << " blobs=" << g_mediumBlobs << " shells=" << g_mediumShells
                      << std::endl;
            frames = 0;
            sum = peak = 0.0;
            begin = now;
        }
    }
}
    void GalaxyMapRenderer::render(
        GalaxyMapView& viewState,
        GalaxyMapRenderContext& context,
        const Viewport& viewport,
        const world::celestial::GalaxyMapSnapshot& galaxy,
        const world::celestial::PlayerNavigationState& navigation
    ) const
    {
        viewState.synchronizeCatalogRoots(galaxy);
        viewState.state().lastViewport = viewport;

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

        // Pointer contract: the resolver hits what THIS frame presents.
        viewState.state().presentedCubeMarkers.clear();

        std::vector<GalaxyFrameText> frameTexts;

        if (viewState.state().navigationGrid.enabled())
        {
            drawNavigationGrid(
                viewState,
                context,
                viewport,
                camera,
                galaxy,
                frameTexts
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

        viewState.state().presentedStars.clear();

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

        {
            const auto mediumBegin = std::chrono::steady_clock::now();
            drawMediumVolumes(viewState, context, galaxy, view, mvp, viewport);
            traceMediumCost(std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - mediumBegin).count());
        }

        // Halo of the stars in the selected / hovered child: meaningful for
        // cubes holding a few stars, noise for an overview root holding the
        // whole catalogue neighbourhood.
        const bool childCubeHalos =
            !viewState.hierarchyBox().parent.overview();

        /*
            The map shows what is INSIDE the current parent cube only:
            stars, objects and medium outside it are neither drawn nor
            pointer targets.
        */
        const GalaxyHierarchyBox parentBox = viewState.hierarchyBox();

        for (const auto& system : galaxy.systems)
        {
            if (!galaxyHierarchyContains(parentBox, system.positionLy))
                continue;

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

            if (viewState.state().navigationGrid.enabled() &&
                childCubeHalos)
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

            GalaxyPointerStar presented;
            presented.systemId = system.id;
            presented.glyphRadiusPx =
                viewState.visuals().starBaseRadiusPx * starScale;
            presented.screen =
                projectToScreen(
                    position,
                    mvp,
                    viewport,
                    presented.visible,
                    presented.depth
                );

            viewState.state().presentedStars.push_back(presented);
        }

        // Interstellar layer: compact objects as small dim points (the medium
        // volumes were drawn before the systems, see drawMediumVolumes).
        /*
            Object LOD: compact interstellar objects are presented only from
            the parent level where they are visually meaningful, and only in
            (or next to) the current parent: brown dwarfs and stellar
            remnants inside a root cube, rogue bodies, comets and icy fields
            from a G1 parent down. Stars and medium structures stay at every
            level (the medium has its own projected-size LOD).
        */
        const GalaxyHierarchyBox lodBox = viewState.hierarchyBox();
        const GalaxyHierarchyBox& lodBoxMargin = lodBox;
        const auto objectVisibleFromLevel = [](const std::string& type) {
            if (type == "brown_dwarf" || type == "stellar_remnant")
                return 0;
            return 1;
        };

        for (const auto& object : galaxy.objects)
        {
            // Cavities describe the medium (no body); the medium has its proxies.
            if (object.mediumVolume || object.objectType == "interstellar_cavity")
                continue;

            if (lodBox.parent.level < objectVisibleFromLevel(object.objectType) ||
                !galaxyHierarchyContains(lodBoxMargin, object.positionLy))
            {
                continue;
            }

            const glm::vec3 position =
                viewState.positionLyToRender(object.positionLy);

            // Icy debris field: solid bodies, not gas - a sparse cluster of
            // tiny pale grey-cyan points over its extent, never a shell.
            if (object.objectType == "icy_debris_field")
            {
                static const glm::vec3 offsets[] = {
                    {0.0f, 0.0f, 0.0f},     {0.62f, 0.18f, -0.3f}, {-0.48f, 0.35f, 0.22f}, {0.15f, -0.6f, 0.4f},
                    {-0.3f, -0.25f, -0.62f}, {0.4f, 0.5f, 0.45f},  {-0.65f, -0.1f, 0.1f},  {0.1f, 0.3f, -0.7f}};
                const float extentRender = glm::length(viewState.vectorLyToRender(
                    glm::dvec3(std::max(object.markerRadiusLy, 0.05f), 0.0, 0.0)));
                const float viewDepthField =
                    std::max(0.1f, glm::dot(position - cameraPosition, -cameraDirection));
                const float pixel = 2.0f * viewDepthField * tanHalfFov / safeViewportHeight;
                for (const auto& offset : offsets)
                    context.addBillboardBall(position + offset * extentRender,
                                             viewState.visuals().starBaseRadiusPx * 0.18f * pixel,
                                             glm::vec4(0.70f, 0.86f, 0.88f, 0.45f), view, 8);
                continue;
            }

            glm::vec4 color {0.6f, 0.6f, 0.55f, 0.8f};
            if (object.objectType == "brown_dwarf") color = {0.78f, 0.32f, 0.25f, 0.85f};
            else if (object.objectType == "rogue_planet" || object.objectType == "binary_rogue_planet" ||
                     object.objectType == "rogue_moon") color = {0.55f, 0.42f, 0.36f, 0.8f};
            else if (object.objectType == "stellar_remnant") color = {0.72f, 0.82f, 1.0f, 0.85f};
            else if (object.objectType == "interstellar_comet") color = {0.55f, 0.9f, 0.9f, 0.8f};

            const float viewDepth =
                std::max(0.1f, glm::dot(position - cameraPosition, -cameraDirection));
            const float worldUnitsPerPixel =
                2.0f * viewDepth * tanHalfFov / safeViewportHeight;
            context.addBillboardBall(
                position,
                viewState.visuals().starBaseRadiusPx * 0.35f * worldUnitsPerPixel,
                color,
                view,
                12
            );
        }

        context.flushSolids(mvp);
        context.flushLines(mvp);

        drawFrameTexts(
            viewState,
            context,
            viewport,
            camera,
            frameTexts
        );

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

    /*
        "What is here?" (Alt + click): a small translucent list next to the
        cursor - not a panel over the map.
    */
    void GalaxyMapRenderer::drawWhatIsHere(
        GalaxyMapView& viewState,
        GalaxyMapRenderContext& context,
        const Viewport& viewport
    ) const
    {
        const auto& menu = viewState.state().whatIsHere;
        if (!menu.open || menu.labels.empty())
            return;

        const glm::mat4 ortho =
            glm::ortho(0.0f, static_cast<float>(viewport.width), static_cast<float>(viewport.height), 0.0f,
                       -1.0f, 1.0f);
        const glm::vec4 b = menu.bounds();

        context.beginLines();
        // Background: dense horizontal lines (the context draws lines only).
        for (float y = b.y; y <= b.w; y += 1.0f)
            context.addLine(glm::vec3(b.x, y, 0.0f), glm::vec3(b.z, y, 0.0f), glm::vec4(0.03f, 0.07f, 0.10f, 0.72f));
        if (menu.hovered >= 0)
        {
            const glm::vec4 r = menu.rowBounds(menu.hovered);
            for (float y = r.y + 1.0f; y < r.w - 1.0f; y += 1.0f)
                context.addLine(glm::vec3(r.x + 1.0f, y, 0.0f), glm::vec3(r.z - 1.0f, y, 0.0f),
                                glm::vec4(0.20f, 0.42f, 0.56f, 0.45f));
        }
        const glm::vec4 frame(0.40f, 0.70f, 0.88f, 0.55f);
        context.addLine(glm::vec3(b.x, b.y, 0.0f), glm::vec3(b.z, b.y, 0.0f), frame);
        context.addLine(glm::vec3(b.z, b.y, 0.0f), glm::vec3(b.z, b.w, 0.0f), frame);
        context.addLine(glm::vec3(b.z, b.w, 0.0f), glm::vec3(b.x, b.w, 0.0f), frame);
        context.addLine(glm::vec3(b.x, b.w, 0.0f), glm::vec3(b.x, b.y, 0.0f), frame);
        context.flushLines(ortho);

        context.beginTextFrame(viewport.width, viewport.height);
        context.drawTextPx("What is here?", b.x + 8.0f, b.y + 16.0f, 12, glm::vec4(0.66f, 0.80f, 0.90f, 0.80f));
        for (int i = 0; i < static_cast<int>(menu.labels.size()); ++i)
        {
            const glm::vec4 r = menu.rowBounds(i);
            context.drawTextPx(menu.labels[static_cast<std::size_t>(i)], r.x + 12.0f, r.y + 15.0f, 12,
                               i == menu.hovered ? glm::vec4(0.96f, 0.86f, 0.52f, 0.98f)
                                                 : glm::vec4(0.62f, 0.82f, 0.96f, 0.88f));
        }
        context.endTextFrame();
    }

/*
    Interstellar medium on the map: only the distinct structures, as their
    presentation proxies (GalaxyMapMediumAtlas, built once per snapshot). The
    background medium exists physically but is not drawn. Per structure:
    frustum culling of its bounding sphere, then by projected radius a soft
    blob (far), the coarse shell or the detailed shell (+ the faint inner
    shell of a dense molecular structure). All shells are one static GPU
    buffer drawn with one multi-draw; nothing is rebuilt per frame. Visual
    priority stays stars / route / selection > compact objects > medium.
*/
void GalaxyMapRenderer::drawMediumVolumes(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const glm::mat4& view,
    const glm::mat4& mvp,
    const Viewport& viewport
) const
{
    const auto& atlas = galaxy.mediumAtlas;
    if (!atlas || atlas->proxies.empty())
        return;
    using Range = world::celestial::GalaxyMapMediumAtlas::Range;
    constexpr float BlobBelowPx = 6.0f;      // far zoom: one soft blob
    constexpr float DetailFromPx = 70.0f;    // near zoom: the detailed shell
    // Object LOD: a structure projected below this radius is not drawn; the
    // selected / hovered one always is. The galaxy overview frames thousands
    // of small structures: there only the large ones (no blobs) are shown.
    const float HiddenBelowPx =
        viewState.hierarchyBox().parent.overview() ? BlobBelowPx : 1.5f;

    const float renderPerLy = viewState.vectorLyToRender(glm::dvec3(1.0, 0.0, 0.0)).x;
    const glm::mat4 mvpLy = mvp * glm::scale(glm::mat4(1.0f), glm::vec3(renderPerLy));
    // Frustum planes (Gribb-Hartmann) in light-years.
    const auto row = [&](int r) { return glm::vec4(mvpLy[0][r], mvpLy[1][r], mvpLy[2][r], mvpLy[3][r]); };
    const glm::vec4 planes[6] = {row(3) + row(0), row(3) - row(0), row(3) + row(1),
                                 row(3) - row(1), row(3) + row(2), row(3) - row(2)};
    const float clipYPerLy = glm::length(glm::vec3(row(1)));
    const float halfHeightPx = 0.5f * static_cast<float>(std::max(viewport.height, 1));

    static thread_local std::vector<Range> ranges;
    ranges.clear();
    bool anyBlob = false;
    int blobCount = 0;
    context.beginVolumes();
    // Only structures reaching into the current parent cube.
    const GalaxyHierarchyBox clipBox = viewState.hierarchyBox();
    for (const auto& proxy : atlas->proxies)
    {
        if (!galaxyHierarchyIntersectsSphere(clipBox, proxy.centreLy, proxy.radiusLy))
            continue;
        const glm::vec3 c(proxy.centreLy);
        const float r = proxy.radiusLy;
        bool inside = true;
        for (const auto& plane : planes)
            if (glm::dot(glm::vec3(plane), c) + plane.w < -r * glm::length(glm::vec3(plane)))
            {
                inside = false;
                break;
            }
        if (!inside)
            continue;
        const float w = std::max((mvpLy * glm::vec4(c, 1.0f)).w, 1.0e-4f);
        const float radiusPx = r * clipYPerLy / w * halfHeightPx;
        if (radiusPx < HiddenBelowPx &&
            proxy.objectIndex != viewState.state().selectedMediumObject &&
            proxy.objectIndex != viewState.state().hoveredMediumObject)
        {
            continue;
        }
        if (radiusPx < BlobBelowPx)
        {
            context.addVolumeSprite(viewState.positionLyToRender(proxy.centreLy),
                                    0.8f * r * renderPerLy, proxy.blobRgba, view);
            anyBlob = true;
            ++blobCount;
            continue;
        }
        const bool detailed = radiusPx >= DetailFromPx;
        ranges.push_back(detailed ? proxy.detailed : proxy.coarse);
        if (detailed && proxy.inner.count > 0)
            ranges.push_back(proxy.inner);
    }
    // Hover: the hovered structure's shell is drawn once more (a slight
    // brightening, no outline). Selection: a quiet edge outline of its coarse
    // shell faces (presentation only).
    const auto proxyOf = [&](int objectIndex) -> const world::celestial::GalaxyMapMediumAtlas::Proxy* {
        if (objectIndex < 0 || objectIndex >= static_cast<int>(galaxy.objects.size()))
            return nullptr;
        const int proxy = galaxy.objects[static_cast<std::size_t>(objectIndex)].mediumProxy;
        return proxy >= 0 && proxy < static_cast<int>(atlas->proxies.size())
            ? &atlas->proxies[static_cast<std::size_t>(proxy)] : nullptr;
    };
    if (const auto* hovered = proxyOf(viewState.state().hoveredMediumObject))
        ranges.push_back(hovered->coarse);
    if (core::runtimeTraceEnabled())
    {
        g_mediumShells = static_cast<int>(ranges.size());
        g_mediumBlobs = blobCount;
    }
    if (anyBlob)
        context.flushVolumes(mvp);
    context.drawMediumShells(*atlas, ranges, mvpLy);
    if (const auto* selected = proxyOf(viewState.state().selectedMediumObject))
    {
        /*
            Readable but quiet: a faint phase-tinted edge, never a solid body.
            A structure much larger than the current parent (seen from deep
            inside it) fades further: its edges would cross the whole frame.
            The lines join the scene batch (a separate begin would drop the
            spatial frame drawn before it).
        */
        const float parentEdgeLy = static_cast<float>(viewState.hierarchyBox().edgeLy);
        const float scaleFade =
            std::clamp(3.0f * parentEdgeLy / std::max(selected->radiusLy, 1.0e-6f), 0.2f, 1.0f);
        const glm::vec4 rgba(glm::vec3(selected->blobRgba) * 0.8f + glm::vec3(0.2f), 0.10f * scaleFade);
        const auto& range = selected->coarse;
        for (std::uint32_t v = range.first; v + 5 < range.first + range.count; v += 6)
        {
            // Quad corners 0, 1, 2, 3 = vertices v, v+1, v+2, v+5.
            const glm::vec3 q[4] = {atlas->vertices[v].positionLy, atlas->vertices[v + 1].positionLy,
                                    atlas->vertices[v + 2].positionLy, atlas->vertices[v + 5].positionLy};
            for (int e = 0; e < 4; ++e)
                context.addLine(q[e] * renderPerLy, q[(e + 1) % 4] * renderPerLy, rgba);
        }
    }
}

void GalaxyMapRenderer::drawNavigationGrid(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const Viewport& vp,
    const GalaxyMapCameraSnapshot& camera,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    std::vector<GalaxyFrameText>& frameTexts
) const
{
    auto& state = viewState.state();
    if (!state.navigationGrid.enabled())
        return;

    // Logical hover is input; the visual hover fades in / out.
    updateHierarchyHoverFade(
        {&state.hoverVisualCell, &state.hoverVisualAlpha, &state.hoverOutgoingCell, &state.hoverOutgoingAlpha,
         &state.hoverVisualLastTimeSeconds},
        state.navigationGrid.hasHoveredCell()
            ? std::optional<game::navigation::CubicNavigationCell>(state.navigationGrid.hoveredCell())
            : std::nullopt,
        context.currentTimeSeconds(),
        viewState.controls().navigationHoverFadeInSeconds,
        viewState.controls().navigationHoverFadeOutSeconds);

    const auto smooth = [](float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    };

    HierarchyFramePaintInput in;
    in.box = viewState.hierarchyBox();
    in.faces = &state.hierarchyFaces;
    in.eyeUnits = viewState.renderToPositionLy(glm::vec3(camera.eye));
    in.viewInto = -camera.basis.direction;
    in.cameraRight = glm::vec3(camera.basis.right);
    in.cameraUp = glm::vec3(camera.basis.up);
    in.cameraDirection = glm::vec3(camera.basis.direction);
    in.cameraEyeRender = glm::vec3(camera.eye);
    in.mvp = camera.mvp;
    in.viewport = vp;
    in.tanHalfFovY = std::tan(glm::radians(48.0f) * 0.5f);
    in.toRender = [&viewState](const glm::dvec3& ly) { return viewState.positionLyToRender(ly); };
    in.visuals = &viewState.visuals().navigationGrid;
    in.levelPrefix = 'G';
    // While the camera flies between nested parents the new frame's grids
    // fade in (no x25 pop of a dense lattice); the old frame fades out.
    in.frameFade =
        state.cameraFlight.active()
            ? smooth(static_cast<float>(state.cameraFlight.progress()) * 1.4f - 0.2f)
            : 1.0f;
    if (state.flightContextBox && state.cameraFlight.active())
        in.flightContextBox = state.flightContextBox;

    in.hoverCell = state.hoverVisualCell;
    in.hoverAlpha = state.hoverVisualAlpha;
    in.outgoingCell = state.hoverOutgoingCell;
    in.outgoingAlpha = state.hoverOutgoingAlpha;
    in.hoverOnLayer =
        state.hoverTargetValid && state.hoverTarget.reason == GalaxyPointerHitReason::HierarchyLayer;
    in.layerAxis = state.hoverLayerAxis;
    if (state.hoverTargetValid &&
        (state.hoverTarget.kind == GalaxyPointerTargetKind::Star ||
         state.hoverTarget.kind == GalaxyPointerTargetKind::Medium))
    {
        in.hoverObjectPoint = GalaxyMapInteraction::semanticPointLy(galaxy, state.hoverTarget);
    }

    if (state.navigationGrid.hasSelectedCell())
        in.selectedCell = state.navigationGrid.selectedCell();
    in.objectSelection = state.selectedSystemId >= 0 || state.selectedMediumObject >= 0;
    if (in.objectSelection && state.navigationFocusValid)
        in.selectedObjectPoint = state.navigationFocusLy;

    paintHierarchyFrame(context, in, state.presentedCubeMarkers, frameTexts);
}

/*
    Frame texts: coordinate labels on the parent's faces (world anchored),
    the hovered / selected child's coordinates and the XYZ triad (screen
    anchored). Drawn after the scene lines, before the star labels.
*/
void GalaxyMapRenderer::drawFrameTexts(
    GalaxyMapView& viewState,
    GalaxyMapRenderContext& context,
    const Viewport& vp,
    const GalaxyMapCameraSnapshot& camera,
    const std::vector<GalaxyFrameText>& frameTexts
) const
{
    (void)viewState;
    paintHierarchyFrameTexts(context, vp, camera.mvp, camera.view, frameTexts);
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

    const GalaxyHierarchyBox parentBox = viewState.hierarchyBox();
    for (const auto& s : galaxy.systems)
    {
        if (!galaxyHierarchyContains(parentBox, s.positionLy))
            continue;

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

    if (!playerVisible ||
        !galaxyHierarchyContains(viewState.hierarchyBox(), playerPosition))
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
        m_playerLabel,
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
