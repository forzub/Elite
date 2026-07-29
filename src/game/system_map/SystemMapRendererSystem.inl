/*
    System map implementation.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/

#include "src/world/celestial/CelestialOrbitKinematics.h"

namespace
{
    std::string systemMapObjectStableKey(
        const world::celestial::SystemMapObject& object
    )
    {
        if (!object.stableId.empty())
            return object.stableId;

        return
            "entity:" +
            std::to_string(object.id.value);
    }
}




// ============================================================================
// System camera matrices
// ============================================================================







// ============================================================================
// System interaction is implemented by SystemMapInteraction.
// Scene orchestration is implemented by SystemMapSceneRenderer.
// This file contains shared rendering backends and pick-cache adapters.
// ============================================================================


// ============================================================================
// System camera flight
// ============================================================================










// ============================================================================
// System labels
// ============================================================================



void SystemMapRenderer::drawSystemLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById
)
{
    using world::celestial::BodyType;

    auto& text = TextRenderer::instance();

        text.beginFrameForViewport(
            vp.width,
            vp.height
        );

        const double worldUnitsPerPixel =
            systemMapWorldUnitsPerPixel(
                static_cast<double>(m_systemView.state().camera.distance),
                vp.height
            );

    const float screenFactor =
        std::clamp(
            static_cast<float>(vp.height) /
                m_systemView.visuals().labelReferenceHeightPx,
            m_systemView.visuals().labelMinimumScreenScale,
            m_systemView.visuals().labelMaximumScreenScale
        );

    const float labelFactor =
        screenFactor *
        m_systemView.visuals().labelScale;

    const int titlePixelSize =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_systemView.visuals().labelTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_systemView.visuals().labelTitleMinPx,
            m_systemView.visuals().labelTitleMaxPx
        );

    const int selectedTitlePixelSize =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_systemView.visuals().labelSelectedTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_systemView.visuals().labelTitleMinPx,
            m_systemView.visuals().labelTitleMaxPx
        );

    const int subtitlePixelSize =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_systemView.visuals().labelSubtitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_systemView.visuals().labelSubtitleMinPx,
            m_systemView.visuals().labelSubtitleMaxPx
        );

    for (const auto& b : system.bodies)
    {
        if (b.type != BodyType::Star &&
            b.type != BodyType::Planet &&
            b.type != BodyType::Moon &&
            b.type != BodyType::AsteroidBelt)
        {
            continue;
        }

        auto posIt = posById.find(b.id);
        if (posIt == posById.end())
            continue;

        
        auto radiusIt =
            drawRadiusById.find(
                b.id
            );

        const float physicalRadiusWorld =
            radiusIt != drawRadiusById.end()
                ? radiusIt->second
                : 0.0f;

        const SystemBodyVisualMetrics labelMetrics =
            computeSystemBodyVisualMetrics(
                b,
                physicalRadiusWorld,
                worldUnitsPerPixel
            );

        const bool selected =
            b.id == m_systemView.state().selectedBodyId;

        const float screenRadiusPx =
            std::max(
                labelMetrics.physicalRadiusPx,
                labelMetrics.markerRadiusPx
            );








       if (!selected)
        {
            // Планеты показываем всегда.
            // В true-scale физический диск может быть меньше пикселя,
            // но подпись нужна как навигационный маяк.

            const double kmPerPixel =
            static_cast<double>(worldUnitsPerPixel) *
            AU_KM /
            std::max(
                static_cast<double>(m_systemView.state().lastScale),
                0.000001
            );

            if (b.type == BodyType::Moon &&
                kmPerPixel > 200.0)
            {
                continue;
            }

            if (b.type == BodyType::AsteroidBelt &&
                screenRadiusPx < 2.0f)
            {
                continue;
            }
        }

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen =
            projectToScreen(posIt->second, mvp, vp, visible, depth);

        if (!visible)
            continue;

        std::string subtitle;

        for (size_t i = 0; i < b.alternativeNames.size(); ++i)
        {
            const auto& alt = b.alternativeNames[i];

            if (alt.name.empty())
                continue;

            if (!subtitle.empty())
                subtitle += ", ";

            subtitle += alt.name;

            if (!alt.actors.empty())
            {
                subtitle += " (";

                for (size_t a = 0; a < alt.actors.size(); ++a)
                {
                    if (a > 0)
                        subtitle += ", ";

                    subtitle += alt.actors[a];
                }

                subtitle += ")";
            }
        }

        const float labelOffsetPx =
            std::max(
                screenRadiusPx +
                    m_systemView.visuals().labelBodyGapBasePx *
                        labelFactor,
                m_systemView.visuals().labelMinimumOffsetBasePx *
                    labelFactor
            );

        const float x = screen.x + labelOffsetPx;
        const float y =
            screen.y +
            m_systemView.visuals().labelTitleYOffsetBasePx *
                labelFactor;

        const int bodyTitlePixelSize =
            selected
                ? selectedTitlePixelSize
                : titlePixelSize;

        const glm::vec4 titleColor =
            selected
                ? m_systemView.visuals().bodyLabels.selectedTitleColor
                : b.type == BodyType::Star
                    ? m_systemView.visuals().bodyLabels.starTitleColor
                    : m_systemView.visuals().bodyLabels.bodyTitleColor;

        text.textDrawPx(
            b.name,
            x,
            y,
            bodyTitlePixelSize,
            titleColor
        );

        if (!subtitle.empty() &&
            (selected || screenRadiusPx >= 10.0f))
        {
            text.textDrawPx(
                "(" + subtitle + ")",
                x,
                y +
                    m_systemView.visuals().labelSubtitleOffsetBasePx *
                        labelFactor,
                subtitlePixelSize,
                m_systemView.visuals().bodyLabels.subtitleColor
            );
        }
    }

    text.endFrame();
}



// ============================================================================
// System navigation rendering
// ============================================================================




void SystemMapRenderer::drawSystemNavigationGrid(
    const Viewport& vp,
    const glm::mat4&,
    float systemScale
)
{
    if (!m_systemView.state().navigationGrid.enabled() || systemScale <= 0.0f)
        return;

    const bool currentLevelCellsInteractive =
        m_systemView.navigationCellsInteractive(vp);

    const double hoverNowSeconds =
        glfwGetTime();

    double hoverDeltaSeconds = 0.0;

    if (m_systemView.state().hoverVisualLastTimeSeconds > 0.0)
    {
        hoverDeltaSeconds =
            std::clamp(
                hoverNowSeconds -
                    m_systemView.state().hoverVisualLastTimeSeconds,
                0.0,
                0.10
            );
    }

    m_systemView.state().hoverVisualLastTimeSeconds =
        hoverNowSeconds;

    const auto anchor =
        m_systemView.state().navigationGrid.anchorCell();

    const std::optional<game::navigation::CubicNavigationCell>
        selectedCell =
            m_systemView.state().navigationGrid.hasSelectedCell()
                ? std::optional<game::navigation::CubicNavigationCell>(
                    m_systemView.state().navigationGrid.selectedCell()
                  )
                : std::nullopt;

    std::optional<game::navigation::CubicNavigationCell>
        hoverTargetCell;

    if (currentLevelCellsInteractive &&
        m_systemView.state().navigationGrid.hasHoveredCell())
    {
        const auto& logicalHovered =
            m_systemView.state().navigationGrid.hoveredCell();

        const bool sameAsAnchor =
            logicalHovered.level == anchor.level &&
            logicalHovered.index == anchor.index;

        const bool sameAsSelected =
            selectedCell.has_value() &&
            logicalHovered.level == selectedCell->level &&
            logicalHovered.index == selectedCell->index;

        if (!sameAsAnchor &&
            !sameAsSelected)
        {
            hoverTargetCell = logicalHovered;
        }
    }

    const bool hoverTargetChanged =
        hoverTargetCell.has_value() &&
        (
            !m_systemView.state().hoverVisualCell.has_value() ||
            hoverTargetCell->level !=
                m_systemView.state().hoverVisualCell->level ||
            hoverTargetCell->index !=
                m_systemView.state().hoverVisualCell->index
        );

    if (hoverTargetChanged)
    {
        if (m_systemView.state().hoverVisualCell.has_value() &&
            m_systemView.state().hoverVisualAlpha > 0.001f)
        {
            m_systemView.state().hoverOutgoingCell =
                m_systemView.state().hoverVisualCell;

            m_systemView.state().hoverOutgoingAlpha =
                m_systemView.state().hoverVisualAlpha;
        }

        m_systemView.state().hoverVisualCell =
            hoverTargetCell;

        m_systemView.state().hoverVisualAlpha =
            0.0f;
    }

    if (hoverTargetCell.has_value())
    {
        const float fadeInSeconds =
            std::max(
                0.001f,
                m_systemView.controls().navigationHoverFadeInSeconds
            );

        m_systemView.state().hoverVisualAlpha =
            std::min(
                1.0f,
                m_systemView.state().hoverVisualAlpha +
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeInSeconds
            );
    }
    else
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_systemView.controls().navigationHoverFadeOutSeconds
            );

        m_systemView.state().hoverVisualAlpha =
            std::max(
                0.0f,
                m_systemView.state().hoverVisualAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_systemView.state().hoverVisualAlpha <= 0.001f)
        {
            m_systemView.state().hoverVisualAlpha = 0.0f;
            m_systemView.state().hoverVisualCell.reset();
        }
    }

    if (m_systemView.state().hoverOutgoingCell.has_value())
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_systemView.controls().navigationHoverFadeOutSeconds
            );

        m_systemView.state().hoverOutgoingAlpha =
            std::max(
                0.0f,
                m_systemView.state().hoverOutgoingAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_systemView.state().hoverOutgoingAlpha <= 0.001f)
        {
            m_systemView.state().hoverOutgoingAlpha = 0.0f;
            m_systemView.state().hoverOutgoingCell.reset();
        }
    }

    std::vector<game::navigation::CubicNavigationCell> cells;
    cells.reserve(4);
    cells.push_back(anchor);

    if (selectedCell.has_value() &&
        selectedCell->level == anchor.level &&
        selectedCell->index != anchor.index)
    {
        cells.push_back(
            selectedCell.value()
        );
    }

    if (m_systemView.state().hoverVisualCell.has_value() &&
        m_systemView.state().hoverVisualAlpha > 0.001f)
    {
        const auto& hovered =
            m_systemView.state().hoverVisualCell.value();

        if (hovered.level != anchor.level ||
            hovered.index != anchor.index)
        {
            cells.push_back(hovered);
        }
    }

    if (m_systemView.state().hoverOutgoingCell.has_value() &&
        m_systemView.state().hoverOutgoingAlpha > 0.001f)
    {
        const auto& outgoing =
            m_systemView.state().hoverOutgoingCell.value();

        const bool alreadyPresent =
            std::any_of(
                cells.begin(),
                cells.end(),
                [&](const auto& existing)
                {
                    return
                        existing.level == outgoing.level &&
                        existing.index == outgoing.index;
                }
            );

        if (!alreadyPresent)
            cells.push_back(outgoing);
    }

    const glm::mat4 view = m_systemView.viewMatrix();

    const glm::vec3 cameraRight(
        view[0][0],
        view[1][0],
        view[2][0]
    );

    const glm::vec3 cameraUp(
        view[0][1],
        view[1][1],
        view[2][1]
    );

    auto toRender =
        [&](const glm::dvec3& positionAu) -> glm::vec3
        {
            const glm::dvec3 absoluteMap =
                positionAu * static_cast<double>(systemScale);

            return glm::vec3(absoluteMap - m_systemView.state().camera.target);
        };

    const glm::vec3 cameraDirection =
        orbitCameraDirectionFromYawPitch(
            m_systemView.state().camera.yaw,
            m_systemView.state().camera.pitch
        );

    const glm::vec3 cameraPosition =
        cameraDirection *
        systemMapPerspectiveEyeDistance(
            m_systemView.state().camera.distance,
            m_systemView.visuals().projectionFieldOfViewDeg
        );

    const double worldUnitsPerPixel =
        systemMapWorldUnitsPerPixel(
            static_cast<double>(m_systemView.state().camera.distance),
            vp.height
        );

    const float gridFullSizePx =
        static_cast<float>(
            std::max(
                1,
                std::min(vp.width, vp.height)
            )
        ) * 0.60f;

    const float gridHiddenSizePx =
        gridFullSizePx * (2.0f / 3.0f);

    const auto cubeGridVisibility =
        [&](const glm::vec3& halfAxisX,
            const glm::vec3& halfAxisY,
            const glm::vec3& halfAxisZ) -> float
        {
            const float cubeEdgeWorld =
                2.0f *
                std::max({
                    glm::length(halfAxisX),
                    glm::length(halfAxisY),
                    glm::length(halfAxisZ)
                });

            const float cubeSizePx =
                cubeEdgeWorld *
                1.35f /
                static_cast<float>(
                    std::max(worldUnitsPerPixel, 0.000000001)
                );

            if (cubeSizePx <= gridHiddenSizePx)
                return 0.0f;

            if (cubeSizePx >= gridFullSizePx)
                return 1.0f;

            float visibility =
                (cubeSizePx - gridHiddenSizePx) /
                (gridFullSizePx - gridHiddenSizePx);

            visibility =
                std::clamp(visibility, 0.0f, 1.0f);

            return
                visibility *
                visibility *
                (3.0f - 2.0f * visibility);
        };

    const int faceDivisions =
        m_systemView.state().navigationGrid.subdivision();

    const auto addCubeFarFaceGrids =
        [&](const glm::vec3& cubeCenter,
            const glm::vec3& halfAxisX,
            const glm::vec3& halfAxisY,
            const glm::vec3& halfAxisZ,
            glm::vec4 gridColor)
        {
            const float visibility =
                cubeGridVisibility(
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
                        cameraPosition -
                        cubeCenter;

                    const float farSide =
                        glm::dot(toCamera, faceHalfAxis) >= 0.0f
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

                        addLine(
                            faceCenter + alongU - gridHalfAxisV,
                            faceCenter + alongU + gridHalfAxisV,
                            gridColor
                        );

                        addLine(
                            faceCenter + alongV - gridHalfAxisU,
                            faceCenter + alongV + gridHalfAxisU,
                            gridColor
                        );
                    }
                };

            addFarFace(halfAxisX, halfAxisY, halfAxisZ);
            addFarFace(halfAxisY, halfAxisX, halfAxisZ);
            addFarFace(halfAxisZ, halfAxisX, halfAxisY);
        };

    /*
        The immediate parent is rendered separately from the current level.
        Anchor and hover can belong to different parents, so keep a unique
        list but draw the dense face grid only on the parent nearest the
        camera target.
    */
    if (m_systemView.state().navigationGrid.level() >
        m_systemView.state().navigationGrid.definition().minimumLevel)
    {
        std::vector<game::navigation::CubicNavigationCell>
            parentCells;

        parentCells.reserve(cells.size());

        const double subdivision =
            static_cast<double>(
                m_systemView.state().navigationGrid.subdivision()
            );

        for (const auto& currentCell : cells)
        {
            game::navigation::CubicGridIndex parentIndex;

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
                m_systemView.state().navigationGrid.cell(
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
        double focusedParentScore =
            std::numeric_limits<double>::max();

        for (std::size_t i = 0;
             i < parentCells.size();
             ++i)
        {
            const glm::dvec3 parentAbsoluteMap =
                parentCells[i].center *
                static_cast<double>(systemScale);

            const glm::dvec3 delta =
                parentAbsoluteMap -
                m_systemView.state().camera.target;

            const double rightDistance =
                glm::dot(
                    delta,
                    glm::dvec3(cameraRight)
                );

            const double upDistance =
                glm::dot(
                    delta,
                    glm::dvec3(cameraUp)
                );

            const double score =
                rightDistance * rightDistance +
                upDistance * upDistance;

            if (score < focusedParentScore)
            {
                focusedParentScore = score;
                focusedParentIndex = i;
            }
        }

        for (std::size_t i = 0;
             i < parentCells.size();
             ++i)
        {
            const auto& parentCell =
                parentCells[i];

            const glm::vec3 center =
                toRender(parentCell.center);

            const float halfSize =
                static_cast<float>(
                    parentCell.size * 0.5
                ) * systemScale;

            const glm::vec3 halfAxisX(halfSize, 0.0f, 0.0f);
            const glm::vec3 halfAxisY(0.0f, halfSize, 0.0f);
            const glm::vec3 halfAxisZ(0.0f, 0.0f, halfSize);

            addNavigationCubeEdges(
                center,
                halfAxisX,
                halfAxisY,
                halfAxisZ,
                m_systemView.visuals().navigationGrid.parentEdgeColor
            );

            if (i == focusedParentIndex)
            {
                addCubeFarFaceGrids(
                    center,
                    halfAxisX,
                    halfAxisY,
                    halfAxisZ,
                    m_systemView.visuals().navigationGrid.parentFaceGridColor
                );
            }
        }
    }

    for (const auto& cell : cells)
    {
        if (!currentLevelCellsInteractive)
            continue;

        const glm::vec3 center = toRender(cell.center);
        const float halfSize =
            static_cast<float>(cell.size * 0.5) * systemScale;

        const glm::vec3 halfAxisX(halfSize, 0.0f, 0.0f);
        const glm::vec3 halfAxisY(0.0f, halfSize, 0.0f);
        const glm::vec3 halfAxisZ(0.0f, 0.0f, halfSize);


        

        const bool rootBoundary =
            cell.level ==
                m_systemView.state().navigationGrid
                    .definition()
                    .minimumLevel;

        const bool selected =
            !rootBoundary &&
            selectedCell.has_value() &&
            cell.level == selectedCell->level &&
            cell.index == selectedCell->index;







        float hoverVisualAlpha = 0.0f;

        if (m_systemView.state().hoverVisualCell.has_value() &&
            cell.level == m_systemView.state().hoverVisualCell->level &&
            cell.index == m_systemView.state().hoverVisualCell->index)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_systemView.state().hoverVisualAlpha
                );
        }

        if (m_systemView.state().hoverOutgoingCell.has_value() &&
            cell.level == m_systemView.state().hoverOutgoingCell->level &&
            cell.index == m_systemView.state().hoverOutgoingCell->index)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_systemView.state().hoverOutgoingAlpha
                );
        }

        const bool hovered =
            hoverVisualAlpha > 0.001f;

        glm::vec4 edgeColor =
            selected
                ? m_systemView.visuals().navigationGrid.selectedEdgeColor
                : m_systemView.visuals().navigationGrid.currentEdgeColor;

        if (hovered)
        {
            edgeColor =
                m_systemView.visuals().navigationGrid.hoveredEdgeColor;

            edgeColor.a *=
                hoverVisualAlpha;
        }

        glm::vec4 currentLevelGridColor =
            edgeColor;

        currentLevelGridColor.a =
            std::clamp(
                edgeColor.a *
                    m_systemView.visuals().navigationGrid.faceGridAlphaScale,
                m_systemView.visuals().navigationGrid.faceGridMinimumAlpha *
                    (hovered ? hoverVisualAlpha : 1.0f),
                m_systemView.visuals().navigationGrid.faceGridMaximumAlpha *
                    (hovered ? hoverVisualAlpha : 1.0f)
            );

        addCubeFarFaceGrids(
            center,
            halfAxisX,
            halfAxisY,
            halfAxisZ,
            currentLevelGridColor
        );

        addNavigationCubeEdges(
            center,
            halfAxisX,
            halfAxisY,
            halfAxisZ,
            edgeColor
        );


        


        const float markerWorldPerPixel =
            static_cast<float>(worldUnitsPerPixel);

        const float markerSize =
            markerWorldPerPixel *
            (selected
                ? m_systemView.visuals().navigationGrid.selectedMarkerRadiusPx
                : m_systemView.visuals().navigationGrid.currentMarkerRadiusPx);









        glm::vec4 markerColor = selected
            ? m_systemView.visuals().navigationGrid.selectedMarkerColor
            : m_systemView.visuals().navigationGrid.currentMarkerColor;

        if (hovered && !selected)
            markerColor.a *= hoverVisualAlpha;

        const glm::vec3 top = center + cameraUp * markerSize;
        const glm::vec3 right = center + cameraRight * markerSize;
        const glm::vec3 bottom = center - cameraUp * markerSize;
        const glm::vec3 left = center - cameraRight * markerSize;

        addLine(top, right, markerColor);
        addLine(right, bottom, markerColor);
        addLine(bottom, left, markerColor);
        addLine(left, top, markerColor);
    }
}




// ============================================================================
// System scene rendering facade and render-context backend
// ============================================================================


void SystemMapRenderer::ensureSystemRenderResources()
{
    ensureTexturedGlObjects();
    ensureTexturedShader();
    ensureGeneratedCelestialAssets();
}


game::system_map::SystemMapFrameData&
SystemMapRenderer::systemFrameData()
{
    return m_systemFrameData;
}


void SystemMapRenderer::addSystemBodyRingVisuals(
    const world::celestial::SystemMapBody& body,
    const glm::vec3& center,
    const game::system_map::SystemBodyVisualMetrics& metrics,
    float systemScale,
    double worldUnitsPerPixel,
    const glm::mat4& view
)
{
    using world::celestial::SystemMapRingVisibilityClass;

    if (!metrics.drawPhysicalBody ||
        body.rings.empty() ||
        systemScale <= 0.0f ||
        worldUnitsPerPixel <= 0.0)
    {
        return;
    }

    double maximumOuterRadiusKm = 0.0;
    double maximumBandWidthKm = 0.0;

    for (const auto& ring : body.rings)
    {
        maximumOuterRadiusKm =
            std::max(
                maximumOuterRadiusKm,
                ring.outerRadiusKm
            );

        maximumBandWidthKm =
            std::max(
                maximumBandWidthKm,
                std::max(
                    0.0,
                    ring.outerRadiusKm - ring.innerRadiusKm
                )
            );
    }

    if (maximumOuterRadiusKm <= 0.0)
        return;

    const double outerRadiusWorld =
        maximumOuterRadiusKm /
        AU_KM * static_cast<double>(systemScale);

    const double bandWidthWorld =
        maximumBandWidthKm /
        AU_KM * static_cast<double>(systemScale);

    const double outerRadiusPx =
        outerRadiusWorld / worldUnitsPerPixel;

    const double widestBandPx =
        bandWidthWorld / worldUnitsPerPixel;

    if (outerRadiusPx < 28.0 ||
        widestBandPx < 0.75)
    {
        return;
    }

    const glm::dvec3 north =
        systemBodyNorthAxisWorld(
            body
        );

    const glm::dvec3 ringAxisXWorld =
        systemBodyPrimeAxisWorld(
            north
        );

    const glm::dvec3 ringAxisYWorld =
        systemBodyRingAxisYWorld(
            body,
            north,
            ringAxisXWorld
        );

    const glm::vec3 axisX(
        static_cast<float>(ringAxisXWorld.x),
        static_cast<float>(ringAxisXWorld.y),
        static_cast<float>(ringAxisXWorld.z)
    );

    const glm::vec3 axisY(
        static_cast<float>(ringAxisYWorld.x),
        static_cast<float>(ringAxisYWorld.y),
        static_cast<float>(ringAxisYWorld.z)
    );

    glm::vec3 cameraForward {
        view[0][2],
        view[1][2],
        view[2][2]
    };

    if (glm::length(cameraForward) <= 0.000001f)
    {
        cameraForward = glm::vec3(0.0f, 0.0f, 1.0f);
    }
    else
    {
        cameraForward = glm::normalize(cameraForward);
    }

    glm::vec3 ringNormal =
        glm::cross(
            axisX,
            axisY
        );

    if (glm::length(ringNormal) <= 0.000001f)
    {
        return;
    }

    ringNormal =
        glm::normalize(
            ringNormal
        );

    const float ringOpenFactor =
        std::abs(
            glm::dot(
                ringNormal,
                cameraForward
            )
        );

    if (ringOpenFactor < 0.16f)
    {
        return;
    }

    auto visibilityMultiplier =
        [&](SystemMapRingVisibilityClass visibility) -> float
        {
            switch (visibility)
            {
                case SystemMapRingVisibilityClass::Main:
                    return body.ringVisual.mainBandEmphasis;
                case SystemMapRingVisibilityClass::Secondary:
                    return body.ringVisual.secondaryBandEmphasis;
                case SystemMapRingVisibilityClass::Faint:
                    return body.ringVisual.faintBandEmphasis;
                case SystemMapRingVisibilityClass::Diffuse:
                    return body.ringVisual.diffuseBandEmphasis;
            }

            return 1.0f;
        };

    const float minimumVisibleWidthPx =
        std::max(
            0.85f,
            body.ringVisual.minimumVisibleWidthPx
        );

    const float widthScale =
        std::max(
            1.0f,
            body.ringVisual.artisticWidthScale
        );

    const int segments =
        outerRadiusPx > 180.0
            ? 160
            : outerRadiusPx > 96.0
                ? 128
                : 96;

    for (const auto& ring : body.rings)
    {
        const double ringInnerWorldRaw =
            ring.innerRadiusKm /
            AU_KM * static_cast<double>(systemScale);

        const double ringOuterWorldRaw =
            ring.outerRadiusKm /
            AU_KM * static_cast<double>(systemScale);

        if (ringOuterWorldRaw <= ringInnerWorldRaw)
            continue;

        double ringInnerWorld = ringInnerWorldRaw;
        double ringOuterWorld = ringOuterWorldRaw;

        const double midRadiusWorld =
            0.5 * (ringInnerWorld + ringOuterWorld);

        double halfWidthWorld =
            0.5 * (ringOuterWorld - ringInnerWorld) *
            static_cast<double>(widthScale);

        const double minimumHalfWidthWorld =
            0.5 * static_cast<double>(minimumVisibleWidthPx) *
            worldUnitsPerPixel;

        halfWidthWorld =
            std::max(
                halfWidthWorld,
                minimumHalfWidthWorld
            );

        ringInnerWorld =
            std::max(
                0.0,
                midRadiusWorld - halfWidthWorld
            );

        ringOuterWorld =
            midRadiusWorld + halfWidthWorld;

        float alpha =
            ring.opacity *
            ring.visualOpacityScale *
            visibilityMultiplier(
                ring.visibilityClass
            );

        alpha *=
            0.55f +
            0.45f *
            std::clamp(
                body.ringVisual.recognizabilityPriority,
                0.0f,
                1.0f
            );

        alpha *=
            std::clamp(
                0.35f + 0.65f * ringOpenFactor,
                0.0f,
                1.0f
            );

        alpha = std::clamp(alpha, 0.06f, 0.72f);

        glm::vec4 ringColor(
            ring.tint.r,
            ring.tint.g,
            ring.tint.b,
            alpha
        );

        addRingBand3D(
            center,
            axisX,
            axisY,
            static_cast<float>(ringInnerWorld),
            static_cast<float>(ringOuterWorld),
            ringColor,
            segments
        );
    }
}


void SystemMapRenderer::addSystemBodyVisual(
    const world::celestial::SystemMapBody& body,
    const glm::vec3& center,
    const game::system_map::SystemBodyVisualMetrics& metrics,
    const glm::vec4& fallbackColor,
    const glm::mat4& view
)
{
    using world::celestial::BodyType;

    if (metrics.drawPhysicalBody)
    {
        GLuint albedoTexture = 0;

        if (body.type != BodyType::Star)
        {
            albedoTexture =
                globalAlbedoTextureForBody(
                    body
                );
        }

        if (albedoTexture != 0 &&
            m_texturedShader != 0 &&
            m_texturedVao != 0 &&
            m_texturedVbo != 0)
        {
            const bool largeBody =
                body.type == BodyType::Planet &&
                metrics.physicalRadiusWorld > 0.16f;

            addTexturedSystemBodySphere(
                body,
                albedoTexture,
                center,
                metrics.physicalRadiusWorld,
                glm::vec4(1.0f),
                largeBody ? 64 : 24,
                largeBody ? 128 : 48
            );
        }
        else
        {
            addBillboardBall(
                center,
                metrics.physicalRadiusWorld,
                fallbackColor,
                view,
                32
            );
        }
    }

    if (!metrics.drawMarker)
        return;

    glm::vec4 markerColor =
        fallbackColor;

    if (body.type == BodyType::Moon)
    {
        markerColor =
            m_systemView.visuals().scene.moonMarkerColor;
    }
    else if (body.type == BodyType::Planet)
    {
        markerColor =
            m_systemView.visuals().scene.planetMarkerColor;
    }
    else if (body.type == BodyType::Star)
    {
        markerColor =
            glm::vec4(
                1.0f,
                0.86f,
                0.36f,
                0.90f
            );
    }

    addSystemBodyMarker(
        center,
        metrics.markerRadiusWorld,
        markerColor,
        view,
        32
    );
}


void SystemMapRenderer::renderSystem(
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::PlayerNavigationState& navigation
)
{
    m_systemSceneRenderer.render(
        m_systemView,
        *this,
        viewport,
        system,
        navigation
    );
}



















// ============================================================================
// System body rendering
// ============================================================================





void SystemMapRenderer::addSystemBodyMarker(
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    const glm::mat4& view,
    int segments
)
{
    if (radius <= 0.0f ||
        segments < 8)
    {
        return;
    }

    glm::vec3 right {
        view[0][0],
        view[1][0],
        view[2][0]
    };

    glm::vec3 up {
        view[0][1],
        view[1][1],
        view[2][1]
    };

    if (glm::length(right) <= 0.000001f ||
        glm::length(up) <= 0.000001f)
    {
        return;
    }

    right =
        glm::normalize(
            right
        );

    up =
        glm::normalize(
            up
        );

    const glm::vec4 ringColor(
        color.r,
        color.g,
        color.b,
        0.82f
    );

    const glm::vec4 crossColor(
        color.r,
        color.g,
        color.b,
        0.48f
    );

    for (int i = 0; i < segments; ++i)
    {
        const float a0 =
            glm::two_pi<float>() *
            static_cast<float>(i) /
            static_cast<float>(segments);

        const float a1 =
            glm::two_pi<float>() *
            static_cast<float>(i + 1) /
            static_cast<float>(segments);

        const glm::vec3 p0 =
            center +
            (
                std::cos(a0) * right +
                std::sin(a0) * up
            ) * radius;

        const glm::vec3 p1 =
            center +
            (
                std::cos(a1) * right +
                std::sin(a1) * up
            ) * radius;

        addLine(
            p0,
            p1,
            ringColor
        );
    }

    const float crossSize =
        radius * 0.62f;

    addLine(
        center - right * crossSize,
        center + right * crossSize,
        crossColor
    );

    addLine(
        center - up * crossSize,
        center + up * crossSize,
        crossColor
    );
}




void SystemMapRenderer::addTexturedSystemBodySphere(
    const world::celestial::SystemMapBody& body,
    GLuint texture,
    const glm::vec3& center,
    float radius,
    const glm::vec4& color,
    int latSegments,
    int lonSegments
)
{
    if (texture == 0 || radius <= 0.0f)
        return;

    latSegments =
        std::max(
            latSegments,
            8
        );

    lonSegments =
        std::max(
            lonSegments,
            16
        );

    TexturedBatch* batch = nullptr;

    const glm::dvec3 north =
    systemBodyNorthAxisWorld(
        body
    );

const glm::dvec3 prime0 =
    planetPrimeAxisWorld(
        north
    );

const glm::dvec3 east0 =
    planetEastAxisWorld(
        north,
        prime0
    );

const double textureOffset =
    degToRadD(
        body.textureLongitudeOffsetDeg
    );

auto bodyPoint =
    [&](double latitudeRad, double textureLongitudeRad) -> glm::vec3
    {
        const double worldLon =
            textureLongitudeRad +
            textureOffset +
            body.rotationPhaseRad;

        const double cosLat =
            std::cos(latitudeRad);

        const double sinLat =
            std::sin(latitudeRad);

        const glm::dvec3 local =
            prime0 * (std::cos(worldLon) * cosLat * radius) +
            north  * (sinLat * radius) +
            east0  * (std::sin(worldLon) * cosLat * radius);

        return center +
            glm::vec3(
                static_cast<float>(local.x),
                static_cast<float>(local.y),
                static_cast<float>(local.z)
            );
    };

    for (auto& b : m_texturedBatches)
    {
        if (b.texture == texture)
        {
            batch = &b;
            break;
        }
    }

    if (!batch)
    {
        TexturedBatch newBatch;
        newBatch.texture = texture;

        m_texturedBatches.push_back(
            std::move(newBatch)
        );

        batch =
            &m_texturedBatches.back();
    }



    const std::size_t vertexCountToAdd =
        static_cast<std::size_t>(latSegments) *
        static_cast<std::size_t>(lonSegments) *
        6u;

    batch->vertices.reserve(
        batch->vertices.size() + vertexCountToAdd
    );









    for (int iy = 0; iy < latSegments; ++iy)
    {
        const float v0 =
            static_cast<float>(iy) /
            static_cast<float>(latSegments);

        const float v1 =
            static_cast<float>(iy + 1) /
            static_cast<float>(latSegments);

        const double lat0 =
            -glm::half_pi<double>() +
            static_cast<double>(v0) *
            glm::pi<double>();

        const double lat1 =
            -glm::half_pi<double>() +
            static_cast<double>(v1) *
            glm::pi<double>();

        for (int ix = 0; ix < lonSegments; ++ix)
        {
            const float u0 =
                static_cast<float>(ix) /
                static_cast<float>(lonSegments);

            const float u1 =
                static_cast<float>(ix + 1) /
                static_cast<float>(lonSegments);

            const double lon0 =
                -glm::pi<double>() +
                static_cast<double>(u0) *
                glm::two_pi<double>();

            const double lon1 =
                -glm::pi<double>() +
                static_cast<double>(u1) *
                glm::two_pi<double>();

            const glm::vec3 p00 =
                bodyPoint(
                    lat0,
                    lon0
                );

            const glm::vec3 p10 =
                bodyPoint(
                    lat0,
                    lon1
                );

            const glm::vec3 p11 =
                bodyPoint(
                    lat1,
                    lon1
                );

            const glm::vec3 p01 =
                bodyPoint(
                    lat1,
                    lon0
                );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p10, glm::vec2(u1, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p00, glm::vec2(u0, v0), color }
            );

            batch->vertices.push_back(
                { p11, glm::vec2(u1, v1), color }
            );

            batch->vertices.push_back(
                { p01, glm::vec2(u0, v1), color }
            );
        }
    }
}



// ============================================================================
// System body visual metrics
// ============================================================================



glm::vec4 SystemMapRenderer::colorForBodyType(world::celestial::BodyType type) const
{
    using world::celestial::BodyType;

    switch (type)
    {
        case BodyType::Star:         return { 1.00f, 0.93f, 0.62f, 1.00f };
        case BodyType::Planet:       return { 0.36f, 0.68f, 1.00f, 1.00f };
        case BodyType::Moon:         return { 0.70f, 0.72f, 0.78f, 1.00f };
        case BodyType::AsteroidBelt: return { 0.45f, 0.45f, 0.45f, 0.65f };
        default:                     return { 0.60f, 0.82f, 1.00f, 1.00f };
    }
}


float SystemMapRenderer::bodyVisualRadius(
    const world::celestial::SystemMapBody& body,
    float distanceScale
) const
{
    if (body.radiusKm <= 0.0)
        return 0.0f;

    const double radiusAu =
        body.radiusKm / AU_KM;

    return
        static_cast<float>(
            radiusAu *
            static_cast<double>(distanceScale)
        );
}


game::system_map::SystemBodyVisualMetrics
SystemMapRenderer::computeSystemBodyVisualMetrics(
    const world::celestial::SystemMapBody& body,
    float physicalRadiusWorld,
    double worldUnitsPerPixel
) const
{
    using world::celestial::BodyType;

    SystemBodyVisualMetrics out;

    out.physicalRadiusWorld =
        std::max(
            0.0f,
            physicalRadiusWorld
        );

    if (worldUnitsPerPixel > 0.0 &&
        std::isfinite(worldUnitsPerPixel))
    {
        out.physicalRadiusPx =
            static_cast<float>(
                static_cast<double>(out.physicalRadiusWorld) /
                worldUnitsPerPixel
            );
    }

    out.drawPhysicalBody =
        out.physicalRadiusWorld > 0.0f &&
        out.physicalRadiusPx >= m_systemView.controls().minPhysicalBodyRadiusPx;

    float desiredMarkerRadiusPx =
        0.0f;

    if (body.type == BodyType::Star)
    {
        if (out.physicalRadiusPx < m_systemView.controls().starMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemView.controls().starMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Planet)
    {
        if (out.physicalRadiusPx < m_systemView.controls().planetMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemView.controls().planetMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Moon)
    {
        if (out.physicalRadiusPx < m_systemView.controls().tinyMoonProxyRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemView.controls().tinyMoonProxyRadiusPx;
        }
    }

    if (desiredMarkerRadiusPx > 0.0f &&
        worldUnitsPerPixel > 0.0 &&
        std::isfinite(worldUnitsPerPixel))
    {
        out.drawMarker = true;

        out.markerRadiusPx =
            desiredMarkerRadiusPx;

        out.markerRadiusWorld =
            static_cast<float>(
                worldUnitsPerPixel *
                static_cast<double>(desiredMarkerRadiusPx)
            );
    }

    const float visibleRadiusPx =
        std::max(
            out.physicalRadiusPx,
            out.markerRadiusPx
        );

    out.pickRadiusPx =
        std::max(
            visibleRadiusPx,
            m_systemView.controls().pickMinBodyRadiusPx
        );

    return out;
}




// ============================================================================
// System objects and overlays
// ============================================================================





glm::vec3 SystemMapRenderer::systemObjectVisualPosition(
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::SystemMapObject& obj,
    const std::unordered_map<std::string, glm::vec3>& posById,
    const std::unordered_map<std::string, float>& drawRadiusById,
    float systemScale
) const
{
    (void)system;
    (void)posById;
    (void)drawRadiusById;

    return glm::vec3 {
        static_cast<float>(obj.positionAu.x) * systemScale,
        static_cast<float>(obj.positionAu.y) * systemScale,
        static_cast<float>(obj.positionAu.z) * systemScale
    };
}


float SystemMapRenderer::systemObjectOcclusionAlpha(
    const world::celestial::SystemMapObject& obj,
    const glm::vec3& objectVisualPos,
    const glm::mat4& view,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById
) const
{
    constexpr float kFrontAlpha =
        0.96f;

    constexpr float kBehindAlpha =
        0.28f;

    if (obj.parentBodyId.empty())
        return kFrontAlpha;

    auto bodyPosIt =
        bodyVisualPosById.find(
            obj.parentBodyId
        );

    auto bodyRadiusIt =
        drawRadiusById.find(
            obj.parentBodyId
        );

    if (bodyPosIt == bodyVisualPosById.end() ||
        bodyRadiusIt == drawRadiusById.end())
    {
        return kFrontAlpha;
    }

    const float bodyRadius =
        bodyRadiusIt->second;

    if (bodyRadius <= 0.0f)
        return kFrontAlpha;

    const glm::vec4 objectView =
        view *
        glm::vec4(
            objectVisualPos,
            1.0f
        );

    const glm::vec4 bodyView =
        view *
        glm::vec4(
            bodyPosIt->second,
            1.0f
        );

    const float dx =
        objectView.x -
        bodyView.x;

    const float dy =
        objectView.y -
        bodyView.y;

    const float lateral2 =
        dx * dx +
        dy * dy;

    const float radius2 =
        bodyRadius *
        bodyRadius;

    // Объект не проецируется на диск планеты.
    // Значит планета его визуально не перекрывает.
    if (lateral2 >= radius2)
        return kFrontAlpha;

    // В view-space камера смотрит вдоль -Z.
    // Передняя поверхность сферы имеет z больше, чем центр.
    const float frontSurfaceZ =
        bodyView.z +
        std::sqrt(
            std::max(
                0.0f,
                radius2 - lateral2
            )
        );

    // Если объект дальше передней поверхности,
    // значит он находится за диском планеты.
    if (objectView.z < frontSurfaceZ)
        return kBehindAlpha;

    return kFrontAlpha;
}


void SystemMapRenderer::drawSystemObjectOverlays(
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& view,
    const glm::mat4& mvp,
    const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById,
    double worldUnitsPerPixel,
    float systemScale
)
{
    beginLines();

    for (const auto& obj : system.objects)
    {
        auto objectPosIt =
            objectVisualPosById.find(
                systemMapObjectStableKey(obj)
            );

        if (objectPosIt == objectVisualPosById.end())
            continue;

        const glm::vec3 objectPos =
            objectPosIt->second;

        const float alpha =
            systemObjectOcclusionAlpha(
                obj,
                objectPos,
                view,
                bodyVisualPosById,
                drawRadiusById
            );

        const bool isHub =
            obj.kind ==
            world::celestial::
                SystemMapObjectKind::Hub;

        const glm::vec3 objectColor =
            isHub
                ? glm::vec3(0.30f, 0.90f, 1.00f)
                : glm::vec3(1.00f, 0.78f, 0.30f);



    if (obj.hasOrbit &&
        obj.orbitRadiusAu > 0.0 &&
        !obj.parentBodyId.empty())
    {
        auto parentPosIt =
            bodyVisualPosById.find(
                obj.parentBodyId
            );

        if (parentPosIt != bodyVisualPosById.end())
        {
            const glm::vec3 orbitCenter =
                parentPosIt->second;

            const float orbitRadius =
                static_cast<float>(
                    obj.orbitRadiusAu *
                    static_cast<double>(systemScale)
                );

            addOrbitCircle3D(
                orbitCenter,
                orbitRadius,
                obj.orbitInclinationDeg,
                obj.orbitLongitudeOfAscendingNodeDeg,
                obj.orbitArgumentOfPeriapsisDeg,
                glm::vec4(
                    objectColor,
                    alpha * 0.34f
                ),
                160
            );
        }
    }







        const float markerSize =
            static_cast<float>(
                std::max(
                    worldUnitsPerPixel * 7.0,
                    worldUnitsPerPixel
                )
            );

        addMapObjectCube(
            objectPos,
            markerSize,
            glm::vec4(
                objectColor,
                alpha
            )
        );
    }

    flushLines(
        mvp
    );
}


void SystemMapRenderer::addMapObjectCube(
    const glm::vec3& center,
    float size,
    const glm::vec4& color
)
{
    const glm::vec3 p000 = center + glm::vec3(-size, -size, -size);
    const glm::vec3 p001 = center + glm::vec3(-size, -size,  size);
    const glm::vec3 p010 = center + glm::vec3(-size,  size, -size);
    const glm::vec3 p011 = center + glm::vec3(-size,  size,  size);

    const glm::vec3 p100 = center + glm::vec3( size, -size, -size);
    const glm::vec3 p101 = center + glm::vec3( size, -size,  size);
    const glm::vec3 p110 = center + glm::vec3( size,  size, -size);
    const glm::vec3 p111 = center + glm::vec3( size,  size,  size);

    addLine(p000, p001, color);
    addLine(p001, p011, color);
    addLine(p011, p010, color);
    addLine(p010, p000, color);

    addLine(p100, p101, color);
    addLine(p101, p111, color);
    addLine(p111, p110, color);
    addLine(p110, p100, color);

    addLine(p000, p100, color);
    addLine(p001, p101, color);
    addLine(p010, p110, color);
    addLine(p011, p111, color);
}


void SystemMapRenderer::drawSystemObjectLabels(
    const Viewport& vp,
    const world::celestial::SystemMapSnapshot& system,
    const glm::mat4& mvp,
    const glm::mat4& view,
    const std::unordered_map<std::string, glm::vec3>& objectVisualPosById,
    const std::unordered_map<std::string, glm::vec3>& bodyVisualPosById,
    const std::unordered_map<std::string, float>& drawRadiusById
)
{
    auto& text =
        TextRenderer::instance();

    text.beginFrameForViewport(
        vp.width,
        vp.height
    );

    for (const auto& obj : system.objects)
    {
        auto posIt =
            objectVisualPosById.find(
                systemMapObjectStableKey(obj)
            );

        if (posIt == objectVisualPosById.end())
            continue;

        const glm::vec3 p =
            posIt->second;

        bool visible =
            false;

        float depth =
            1.0f;

        const glm::vec2 screen =
            projectToScreen(
                p,
                mvp,
                vp,
                visible,
                depth
            );

        if (!visible)
            continue;

        const float alpha =
            systemObjectOcclusionAlpha(
                obj,
                p,
                view,
                bodyVisualPosById,
                drawRadiusById
            );

        const bool isHub =
            obj.kind ==
            world::celestial::
                SystemMapObjectKind::Hub;

        const glm::vec3 labelColor =
            isHub
                ? m_systemView.visuals().scene.hubObjectLabelColor
                : m_systemView.visuals().scene.otherObjectLabelColor;

        text.textDrawPx(
            obj.name,
            screen.x + 8.0f,
            screen.y - 7.0f,
            13,
            glm::vec4(
                labelColor,
                alpha
            )
        );

        if (!obj.owner.empty())
        {
            text.textDrawPx(
                "(" + obj.owner + ")",
                screen.x + 8.0f,
                screen.y + 9.0f,
                10,
                glm::vec4(
                    glm::vec3(
                        m_systemView.visuals().scene.objectOwnerLabelColor
                    ),
                    alpha *
                        m_systemView.visuals().scene.objectOwnerAlphaScale
                )
            );
        }
    }

    text.endFrame();
}



// ============================================================================
// System body selection
// ============================================================================



const std::string& SystemMapRenderer::selectedBodyId() const
{
    return m_systemView.state().selectedBodyId;
}


const std::string& SystemMapRenderer::selectedHubId() const
{
    return m_systemView.state().selectedHubId;
}


const std::string&
SystemMapRenderer::selectedHubParentBodyId() const
{
    return m_systemView.state().selectedHubParentBodyId;
}


std::optional<world::celestial::DetailSpatialCell>
SystemMapRenderer::selectedTerminalDetailCell() const
{
    const auto terminalCell =
        m_systemView.resolvedTerminalSelection();

    if (!terminalCell)
        return std::nullopt;

    world::celestial::DetailSpatialCell result;
    result.level = terminalCell->level;
    result.maximumLevel =
        m_systemView.state().navigationGrid
            .definition()
            .maximumLevel;
    result.x = terminalCell->index.x;
    result.y = terminalCell->index.y;
    result.z = terminalCell->index.z;
    result.centerAu = terminalCell->center;
    result.edgeAu = terminalCell->size;

    return result;
}


bool SystemMapRenderer::canOpenSelectedDetail() const
{
    if (!m_systemView.state().selectedBodyId.empty() ||
        !m_systemView.state().selectedHubId.empty())
    {
        return true;
    }

    return selectedTerminalDetailCell().has_value();
}


int SystemMapRenderer::pickSystemHub(
    double mouseX,
    double mouseY
) const
{
    int bestIndex = -1;
    float bestDistance = std::numeric_limits<float>::max();

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    for (int i = 0;
         i < static_cast<int>(
             m_systemFrameData.hubScreenPoints.size()
         );
         ++i)
    {
        const auto& point =
            m_systemFrameData.hubScreenPoints[i];

        if (!point.visible ||
            !std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y))
        {
            continue;
        }

        const float distance =
            glm::length(
                point.screen - mouse
            );

        if (distance <= point.screenRadiusPx &&
            distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}



int SystemMapRenderer::pickSystemBody(
    double mouseX,
    double mouseY
) const
{
    int bestIndex =
        -1;

    float bestScore =
        std::numeric_limits<float>::max();

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    for (int i = 0;
         i < static_cast<int>(m_systemFrameData.bodyScreenPoints.size());
         ++i)
    {
        const auto& p =
            m_systemFrameData.bodyScreenPoints[i];

        if (!std::isfinite(p.screen.x) ||
            !std::isfinite(p.screen.y) ||
            !std::isfinite(p.screenRadiusPx))
        {
            continue;
        }

        // Центр тела может быть за экраном, когда тело сильно увеличено.
        // Это нормально: видимый диск всё ещё может занимать viewport.
        const bool depthOk =
            p.depth >= -1.0f &&
            p.depth <= 1.0f;

        if (!p.visible &&
            !depthOk)
        {
            continue;
        }

        const glm::vec2 delta =
            p.screen -
            mouse;

        const float centerDistance =
            glm::length(
                delta
            );

        // ВАЖНО:
        // Для проверки попадания в диск используем реальный экранный радиус.
        // Нельзя ограничивать его 8000 px, иначе при большом zoom тело
        // перестаёт выбираться за пределами зоны вокруг центра.
        const float realBodyRadiusPx =
            std::max(
                0.0f,
                p.screenRadiusPx
            );

        // А вот для размера halo радиус можно ограничить,
        // чтобы огромная планета не создавала бесконечную зону липкости.
        const float haloBodyRadiusPx =
            std::clamp(
                realBodyRadiusPx,
                0.0f,
                m_systemView.controls().pickMaxBodyRadiusPx
            );

        const float distanceToRealDisk =
            std::max(
                0.0f,
                centerDistance -
                realBodyRadiusPx
            );

        const float pickHaloPx =
            std::clamp(
                haloBodyRadiusPx *
                    m_systemView.controls().pickHaloRadiusFactor +
                    m_systemView.controls().pickHaloBasePx,
                m_systemView.controls().pickHaloBasePx,
                m_systemView.controls().pickHaloMaxPx
            );

        if (distanceToRealDisk > pickHaloPx)
        {
            continue;
        }

        const bool mouseInsideRealDisk =
            centerDistance <= realBodyRadiusPx;

        float score =
            0.0f;

        if (mouseInsideRealDisk)
        {
            // Если мышь реально внутри диска тела, это сильный hit.
            // При нескольких вложенных дисках выигрывает тело,
            // чей центр ближе к курсору. Так Луна может перебить Землю,
            // если курсор возле Луны.
            score =
                centerDistance *
                0.001f;
        }
        else
        {
            // Halo-hit слабее, чем реальное попадание в диск.
            score =
                1000000.0f +
                distanceToRealDisk *
                    m_systemView.controls().pickScoreDiskWeight +
                centerDistance;
        }

        if (score < bestScore)
        {
            bestScore =
                score;

            bestIndex =
                i;
        }
    }

    return bestIndex;
}




int SystemMapRenderer::pickSystemOrbitPivotBody(
    double mouseX,
    double mouseY,
    const Viewport& vp
) const
{
    /*
        vp пока сохраняем в сигнатуре, чтобы не менять
        объявление и все места вызова.
    */
    (void)vp;

    int bestIndex =
        -1;

    float bestScore =
        std::numeric_limits<float>::max();

    const glm::vec2 mouse(
        static_cast<float>(mouseX),
        static_cast<float>(mouseY)
    );

    for (int i = 0;
         i <
            static_cast<int>(
                m_systemFrameData.bodyScreenPoints.size()
            );
         ++i)
    {
        const auto& point =
            m_systemFrameData.bodyScreenPoints[i];

        if (!std::isfinite(point.screen.x) ||
            !std::isfinite(point.screen.y) ||
            !std::isfinite(point.screenRadiusPx))
        {
            continue;
        }

        const bool depthOk =
            point.depth >= -1.0f &&
            point.depth <= 1.0f;

        if (!point.visible &&
            !depthOk)
        {
            continue;
        }

        const float bodyRadiusPx =
            std::max(
                0.0f,
                point.screenRadiusPx
            );

        const float centerDistancePx =
            glm::length(
                point.screen -
                mouse
            );

        /*
            Если курсор внутри диска, расстояние равно нулю.

            Если снаружи — измеряем расстояние именно
            до края диска, а не до центра тела.
        */
        const float distanceToDiskPx =
            std::max(
                0.0f,
                centerDistancePx -
                    bodyRadiusPx
            );

        if (distanceToDiskPx >
            m_systemView.controls()
                .rotationPivotMaxDistancePx)
        {
            continue;
        }

        /*
            Сначала выигрывает объект, ближе всего
            к курсору своим видимым диском.

            Небольшая добавка centerDistance помогает
            выбрать более близкий центр при пересечении тел.
        */
        const float score =
            distanceToDiskPx +
            centerDistancePx * 0.001f;

        if (score < bestScore)
        {
            bestScore =
                score;

            bestIndex =
                i;
        }
    }

    return bestIndex;
}


std::optional<std::string>
SystemMapRenderer::pickSystemBodyId(
    double localMouseX,
    double localMouseY
) const
{
    const int index =
        pickSystemBody(
            localMouseX,
            localMouseY
        );

    if (index < 0 ||
        index >= static_cast<int>(
            m_systemFrameData.bodyScreenPoints.size()
        ))
    {
        return std::nullopt;
    }

    return m_systemFrameData.bodyScreenPoints[
        index
    ].bodyId;
}


std::optional<game::system_map::SystemMapHubSelection>
SystemMapRenderer::pickSystemHubSelection(
    double localMouseX,
    double localMouseY
) const
{
    const int index =
        pickSystemHub(
            localMouseX,
            localMouseY
        );

    if (index < 0 ||
        index >= static_cast<int>(
            m_systemFrameData.hubScreenPoints.size()
        ))
    {
        return std::nullopt;
    }

    const auto& point =
        m_systemFrameData.hubScreenPoints[index];

    game::system_map::SystemMapHubSelection
        result;

    result.hubId = point.hubId;
    result.parentBodyId = point.parentBodyId;

    return result;
}


std::optional<std::string>
SystemMapRenderer::pickSystemOrbitPivotBodyId(
    double localMouseX,
    double localMouseY,
    const Viewport& viewport
) const
{
    const int index =
        pickSystemOrbitPivotBody(
            localMouseX,
            localMouseY,
            viewport
        );

    if (index < 0 ||
        index >= static_cast<int>(
            m_systemFrameData.bodyScreenPoints.size()
        ))
    {
        return std::nullopt;
    }

    return m_systemFrameData.bodyScreenPoints[
        index
    ].bodyId;
}


std::optional<glm::dvec3>
SystemMapRenderer::systemBodyAbsolutePosition(
    const std::string& bodyId
) const
{
    const auto it =
        m_systemFrameData.bodyAbsolutePositionById.find(
            bodyId
        );

    if (it ==
        m_systemFrameData.bodyAbsolutePositionById.end())
    {
        return std::nullopt;
    }

    return it->second;
}


std::optional<glm::dvec3>
SystemMapRenderer::systemObjectAbsolutePosition(
    const std::string& objectId
) const
{
    const auto it =
        m_systemFrameData.objectAbsolutePositionById.find(
            objectId
        );

    if (it ==
        m_systemFrameData.objectAbsolutePositionById.end())
    {
        return std::nullopt;
    }

    return it->second;
}
