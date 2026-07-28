/*
    Galaxy map implementation.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/




glm::vec3 SystemMapRenderer::galaxyPositionLyToRender(
    const glm::dvec3& positionLy
) const
{
    return m_galaxyView.positionLyToRender(positionLy);
}

glm::vec3 SystemMapRenderer::galaxyVectorLyToRender(
    const glm::dvec3& vectorLy
) const
{
    return m_galaxyView.vectorLyToRender(vectorLy);
}

glm::dvec3 SystemMapRenderer::galaxyRenderToPositionLy(
    const glm::vec3& renderPosition
) const
{
    return m_galaxyView.renderToPositionLy(renderPosition);
}

glm::vec3 SystemMapRenderer::galaxyStarPosition(
    const world::celestial::GalaxyMapSystem& s
) const
{
    return galaxyPositionLyToRender(
        s.positionLy
    );
}


glm::dvec3 SystemMapRenderer::playerGalaxyPositionLy(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav,
    bool& outInsideKnownSystem
) const
{
    const auto system = std::find_if(
        galaxy.systems.begin(),
        galaxy.systems.end(),
        [&](const auto& candidate)
        {
            return candidate.id == nav.currentSystemId;
        }
    );

    outInsideKnownSystem =
        system != galaxy.systems.end();

    if (outInsideKnownSystem)
    {
        return system->positionLy +
            nav.systemLocalAu /
                game::navigation::SystemNavigationGrid::AuPerLightYear;
    }

    return world::coordinates::toGalacticLy(
        nav.worldPosition
    );
}


void SystemMapRenderer::onGalaxyMapEntered(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav
)
{
    /*
        Camera state is deliberately preserved when the player region did
        not change, but transient mouse state must never survive closing the
        map or returning from another map mode.
    */
    m_galaxyView.state().navigationGrid.clearHoveredCell();
    m_galaxyView.state().hoverVisualCell.reset();
    m_galaxyView.state().hoverVisualAlpha = 0.0f;
    m_galaxyView.state().hoverOutgoingCell.reset();
    m_galaxyView.state().hoverOutgoingAlpha = 0.0f;
    m_galaxyView.state().hoverVisualLastTimeSeconds = 0.0;
    m_galaxyView.state().cubeClickTracker.reset();

    bool insideKnownSystem = false;

    const glm::dvec3 playerPositionLy =
        playerGalaxyPositionLy(
            galaxy,
            nav,
            insideKnownSystem
        );

    m_galaxyView.state().entry.positionLy =
        playerPositionLy;

    const int terminalLevel =
        m_galaxyView.state().navigationGrid.maximumLevel();

    const auto terminalCell =
        m_galaxyView.state().navigationGrid.nearestIndexForPositionLy(
            playerPositionLy,
            terminalLevel
        );

    const int entrySystemId =
        insideKnownSystem
            ? nav.currentSystemId
            : -1;

    const bool playerRegionChanged =
        !m_galaxyView.state().entry.valid ||
        entrySystemId != m_galaxyView.state().entry.systemId ||
        terminalCell != m_galaxyView.state().entry.terminalCell;

    if (playerRegionChanged)
    {
        cancelGalaxyCameraFlight(false);

        m_galaxyView.state().navigationGrid.reset();
        m_galaxyView.state().navigationGrid.setAnchorFromPositionLy(
            playerPositionLy
        );
        m_galaxyView.state().navigationGrid.selectCell(
            m_galaxyView.state().navigationGrid.anchorCell()
        );

        m_galaxyView.state().navigationFocusLy =
            playerPositionLy;
        m_galaxyView.state().navigationFocusValid = true;

        m_galaxyView.state().camera.target =
            galaxyPositionLyToRender(
                m_galaxyView.state().navigationGrid
                    .anchorCell()
                    .centerLy
            );

        const float initialCellEdgeRender =
            static_cast<float>(
                m_galaxyView.state().navigationGrid
                    .anchorCell()
                    .sizeLy
            ) * GALAXY_RENDER_UNITS_PER_LY;

        m_galaxyView.state().camera.distance =
            std::clamp(
                initialCellEdgeRender * 2.35f,
                m_galaxyView.controls().minDistance,
                m_galaxyView.controls().maxDistance
            );

        m_galaxyView.state().selectedSystemId = entrySystemId;
        m_galaxyView.state().focusedSystemId = entrySystemId;
    }

    m_galaxyView.state().entry.systemId =
        entrySystemId;
    m_galaxyView.state().entry.terminalCell =
        terminalCell;
    m_galaxyView.state().entry.valid = true;
}





void SystemMapRenderer::drawGalaxyNavigationGrid(
    const Viewport& vp,
    const glm::mat4&
)
{
    if (!m_galaxyView.state().navigationGrid.enabled())
        return;

    const auto& frame =
        m_galaxyView.state().navigationGrid.frame();

    const bool currentLevelCellsInteractive =
        galaxyNavigationCellsInteractive(vp);

    /*
        Logical hover is used for input. Visual hover has its own lifetime,
        so a cube can fade in and fade out instead of blinking on cell
        boundaries or when the cursor leaves the map.
    */
    const double hoverNowSeconds =
        glfwGetTime();

    double hoverDeltaSeconds = 0.0;

    if (m_galaxyView.state().hoverVisualLastTimeSeconds > 0.0)
    {
        hoverDeltaSeconds =
            std::clamp(
                hoverNowSeconds -
                    m_galaxyView.state().hoverVisualLastTimeSeconds,
                0.0,
                0.10
            );
    }

    m_galaxyView.state().hoverVisualLastTimeSeconds =
        hoverNowSeconds;

    std::optional<game::navigation::GalaxyNavigationCell>
        hoverTargetCell;

    const auto anchorCell =
        m_galaxyView.state().navigationGrid.anchorCell();

    if (currentLevelCellsInteractive &&
        m_galaxyView.state().navigationGrid.hasHoveredCell())
    {
        const auto& logicalHovered =
            m_galaxyView.state().navigationGrid.hoveredCell();

        if (logicalHovered.level != anchorCell.level ||
            logicalHovered.index != anchorCell.index)
        {
            hoverTargetCell = logicalHovered;
        }
    }

    const bool hoverTargetChanged =
        hoverTargetCell.has_value() &&
        (
            !m_galaxyView.state().hoverVisualCell.has_value() ||
            hoverTargetCell->level !=
                m_galaxyView.state().hoverVisualCell->level ||
            hoverTargetCell->index !=
                m_galaxyView.state().hoverVisualCell->index
        );

    if (hoverTargetChanged)
    {
        if (m_galaxyView.state().hoverVisualCell.has_value() &&
            m_galaxyView.state().hoverVisualAlpha > 0.001f)
        {
            m_galaxyView.state().hoverOutgoingCell =
                m_galaxyView.state().hoverVisualCell;

            m_galaxyView.state().hoverOutgoingAlpha =
                m_galaxyView.state().hoverVisualAlpha;
        }

        m_galaxyView.state().hoverVisualCell =
            hoverTargetCell;

        m_galaxyView.state().hoverVisualAlpha =
            0.0f;
    }

    if (hoverTargetCell.has_value())
    {
        const float fadeInSeconds =
            std::max(
                0.001f,
                m_galaxyView.controls().navigationHoverFadeInSeconds
            );

        m_galaxyView.state().hoverVisualAlpha =
            std::min(
                1.0f,
                m_galaxyView.state().hoverVisualAlpha +
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeInSeconds
            );
    }
    else
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_galaxyView.controls().navigationHoverFadeOutSeconds
            );

        m_galaxyView.state().hoverVisualAlpha =
            std::max(
                0.0f,
                m_galaxyView.state().hoverVisualAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_galaxyView.state().hoverVisualAlpha <= 0.001f)
        {
            m_galaxyView.state().hoverVisualAlpha = 0.0f;
            m_galaxyView.state().hoverVisualCell.reset();
        }
    }

    if (m_galaxyView.state().hoverOutgoingCell.has_value())
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_galaxyView.controls().navigationHoverFadeOutSeconds
            );

        m_galaxyView.state().hoverOutgoingAlpha =
            std::max(
                0.0f,
                m_galaxyView.state().hoverOutgoingAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_galaxyView.state().hoverOutgoingAlpha <= 0.001f)
        {
            m_galaxyView.state().hoverOutgoingAlpha = 0.0f;
            m_galaxyView.state().hoverOutgoingCell.reset();
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
        if (m_galaxyView.state().navigationGrid.hasSelectedCell())
        {
            const auto& selectedCell =
                m_galaxyView.state().navigationGrid.selectedCell();

            if (selectedCell.level == anchorCell.level &&
                selectedCell.index != anchorCell.index)
            {
                cells.push_back(selectedCell);
            }
        }

        if (m_galaxyView.state().hoverVisualCell.has_value() &&
            m_galaxyView.state().hoverVisualAlpha > 0.001f)
        {
            const auto& hoveredCell =
                m_galaxyView.state().hoverVisualCell.value();

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

        if (m_galaxyView.state().hoverOutgoingCell.has_value() &&
            m_galaxyView.state().hoverOutgoingAlpha > 0.001f)
        {
            const auto& outgoingCell =
                m_galaxyView.state().hoverOutgoingCell.value();

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

        const glm::mat4 cameraView =
            galaxyViewMatrix();

        const glm::vec3 cameraRight(
            cameraView[0][0],
            cameraView[1][0],
            cameraView[2][0]
        );

        const glm::vec3 cameraUp(
            cameraView[0][1],
            cameraView[1][1],
            cameraView[2][1]
        );


        /*
            Положение и направление Galaxy-камеры нужны,
            чтобы экранный ромб сохранял постоянный размер
            независимо от удаления камеры.
        */
        const glm::vec3 cameraDirection =
            orbitCameraDirectionFromYawPitch(
                m_galaxyView.state().camera.yaw,
                m_galaxyView.state().camera.pitch
            );

        const glm::vec3 cameraPosition =
            m_galaxyView.state().camera.target +
            cameraDirection *
            m_galaxyView.state().camera.distance;

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
        m_galaxyView.state().navigationGrid.subdivision();

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
        m_galaxyView.state().navigationGrid.config().rootEdgeLy();

    const double rootHalfEdgeLy =
        rootEdgeLy * 0.5;

    const glm::vec3 rootHalfAxisX =
        galaxyVectorLyToRender(frame.axisX * rootHalfEdgeLy);
    const glm::vec3 rootHalfAxisY =
        galaxyVectorLyToRender(frame.axisY * rootHalfEdgeLy);
    const glm::vec3 rootHalfAxisZ =
        galaxyVectorLyToRender(frame.axisZ * rootHalfEdgeLy);

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
         m_galaxyView.state().navigationGrid.config().allowedRootCells)
    {
        const glm::dvec3 rootCenterLy =
            frame.originLy +
            frame.axisX *
                (static_cast<double>(rootIndex[0]) * rootEdgeLy) +
            frame.axisY *
                (static_cast<double>(rootIndex[1]) * rootEdgeLy) +
            frame.axisZ *
                (static_cast<double>(rootIndex[2]) * rootEdgeLy);

        const glm::vec3 rootCenter =
            galaxyPositionLyToRender(rootCenterLy);

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
            rootCenter - m_galaxyView.state().camera.target;

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
         m_galaxyView.state().navigationGrid.config().allowedRootCells)
    {
        const glm::dvec3 rootCenterLy =
            frame.originLy +
            frame.axisX *
                (static_cast<double>(rootIndex[0]) * rootEdgeLy) +
            frame.axisY *
                (static_cast<double>(rootIndex[1]) * rootEdgeLy) +
            frame.axisZ *
                (static_cast<double>(rootIndex[2]) * rootEdgeLy);

        const glm::vec3 rootCenter =
            galaxyPositionLyToRender(rootCenterLy);

        addNavigationCubeEdges(
            rootCenter,
            rootHalfAxisX,
            rootHalfAxisY,
            rootHalfAxisZ,
            m_galaxyView.visuals().navigationGrid.rootEdgeColor
        );

        const bool isFocusedRoot =
            hasFocusedRoot &&
            rootIndex == focusedRootIndex;

        if (isFocusedRoot &&
            m_galaxyView.state().navigationGrid.level() ==
                m_galaxyView.state().navigationGrid.minimumLevel())
        {
            addCubeFarFaceGrids(
                rootCenter,
                rootHalfAxisX,
                rootHalfAxisY,
                rootHalfAxisZ,
                m_galaxyView.visuals().navigationGrid.rootFaceGridColor
            );
        }
    }

    /*
        Начиная с G2 непосредственным родителем является уже не Root,
        а куб предыдущего рабочего уровня. Для выбранного и наведённого
        кубов родители могут различаться, поэтому собираем уникальный список.
    */
    if (m_galaxyView.state().navigationGrid.level() >
        m_galaxyView.state().navigationGrid.minimumLevel())
    {
        std::vector<game::navigation::GalaxyNavigationCell>
            parentCells;

        parentCells.reserve(cells.size());

        for (const auto& currentCell : cells)
        {
            game::navigation::GalaxyGridIndex parentIndex;

            const double subdivision =
                static_cast<double>(
                    m_galaxyView.state().navigationGrid.subdivision()
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
                m_galaxyView.state().navigationGrid.cell(
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
                galaxyPositionLyToRender(
                    parentCell.centerLy
                );

            const double parentHalfSizeLy =
                parentCell.sizeLy * 0.5;

            const glm::vec3 parentHalfAxisX =
                galaxyVectorLyToRender(
                    frame.axisX * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisY =
                galaxyVectorLyToRender(
                    frame.axisY * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisZ =
                galaxyVectorLyToRender(
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
                parentCenter - m_galaxyView.state().camera.target;

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
                galaxyPositionLyToRender(
                    parentCell.centerLy
                );

            const double parentHalfSizeLy =
                parentCell.sizeLy * 0.5;

            const glm::vec3 parentHalfAxisX =
                galaxyVectorLyToRender(
                    frame.axisX * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisY =
                galaxyVectorLyToRender(
                    frame.axisY * parentHalfSizeLy
                );

            const glm::vec3 parentHalfAxisZ =
                galaxyVectorLyToRender(
                    frame.axisZ * parentHalfSizeLy
                );

            addNavigationCubeEdges(
                parentCenter,
                parentHalfAxisX,
                parentHalfAxisY,
                parentHalfAxisZ,
                m_galaxyView.visuals().navigationGrid.parentEdgeColor
            );

            if (hasFocusedParent &&
                parentIndex == focusedParentIndex)
            {
                addCubeFarFaceGrids(
                    parentCenter,
                    parentHalfAxisX,
                    parentHalfAxisY,
                    parentHalfAxisZ,
                    m_galaxyView.visuals().navigationGrid.parentFaceGridColor
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
            galaxyPositionLyToRender(
                cell.centerLy
            );

        const double halfSizeLy =
            cell.sizeLy * 0.5;

        const glm::vec3 halfAxisX =
            galaxyVectorLyToRender(
                frame.axisX * halfSizeLy
            );

        const glm::vec3 halfAxisY =
            galaxyVectorLyToRender(
                frame.axisY * halfSizeLy
            );

        const glm::vec3 halfAxisZ =
            galaxyVectorLyToRender(
                frame.axisZ * halfSizeLy
            );

        const bool isAnchor =
            cell.index ==
            m_galaxyView.state().navigationGrid.anchorIndex();

        float hoverVisualAlpha = 0.0f;

        if (m_galaxyView.state().hoverVisualCell.has_value() &&
            cell.index ==
                m_galaxyView.state().hoverVisualCell->index &&
            cell.level ==
                m_galaxyView.state().hoverVisualCell->level)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_galaxyView.state().hoverVisualAlpha
                );
        }

        if (m_galaxyView.state().hoverOutgoingCell.has_value() &&
            cell.index ==
                m_galaxyView.state().hoverOutgoingCell->index &&
            cell.level ==
                m_galaxyView.state().hoverOutgoingCell->level)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_galaxyView.state().hoverOutgoingAlpha
                );
        }

        const bool isHovered =
            hoverVisualAlpha > 0.001f;

        const bool isSelected =
            m_galaxyView.state().navigationGrid.hasSelectedCell() &&
            cell.level ==
                m_galaxyView.state().navigationGrid.selectedCell().level &&
            cell.index ==
                m_galaxyView.state().navigationGrid.selectedCell().index;

        glm::vec4 edgeColor =
            m_galaxyView.visuals().navigationGrid.currentEdgeColor;

        glm::vec4 markerColor =
            m_galaxyView.visuals().navigationGrid.currentMarkerColor;

        if (isAnchor)
        {
            edgeColor.a =
                m_galaxyView.visuals().navigationGrid.anchorEdgeAlpha;
            markerColor.a =
                m_galaxyView.visuals().navigationGrid.anchorMarkerAlpha;
        }

        if (isHovered)
        {
            edgeColor =
                m_galaxyView.visuals().navigationGrid.hoveredEdgeColor;

            markerColor =
                m_galaxyView.visuals().navigationGrid.hoveredMarkerColor;

            edgeColor.a *=
                hoverVisualAlpha;

            markerColor.a *=
                hoverVisualAlpha;
        }

        if (isSelected)
        {
            edgeColor =
                m_galaxyView.visuals().navigationGrid.selectedEdgeColor;

            markerColor =
                m_galaxyView.visuals().navigationGrid.selectedMarkerColor;
        }

        glm::vec4 currentLevelGridColor =
            edgeColor;

        currentLevelGridColor.a =
            std::clamp(
                edgeColor.a *
                    m_galaxyView.visuals()
                        .navigationGrid
                        .currentFaceGridAlphaScale,
                m_galaxyView.visuals()
                    .navigationGrid
                    .currentFaceGridMinimumAlpha,
                m_galaxyView.visuals()
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

        addLine(
            markerTop,
            markerRight,
            markerColor
        );

        addLine(
            markerRight,
            markerBottom,
            markerColor
        );

        addLine(
            markerBottom,
            markerLeft,
            markerColor
        );

        addLine(
            markerLeft,
            markerTop,
            markerColor
        );




    }
}



float SystemMapRenderer::galaxyNavigationAnchorDiameterPx(
    const Viewport& vp
) const
{
    return m_galaxyView.navigationAnchorDiameterPx(vp);
}


bool SystemMapRenderer::galaxyNavigationCellsInteractive(
    const Viewport& vp
) const
{
    return m_galaxyView.navigationCellsInteractive(vp);
}


void SystemMapRenderer::
syncGalaxyNavigationAnchorToCameraTarget()
{
    m_galaxyView.syncNavigationAnchorToCameraTarget();
}


SystemMapRenderer::SystemEntryRequest
SystemMapRenderer::galaxySystemEntryForPosition(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const glm::dvec3& positionLy,
    int explicitSystemId
) const
{
    return m_galaxyView.systemEntryForPosition(
        galaxy,
        positionLy,
        explicitSystemId
    );
}


glm::mat4 SystemMapRenderer::galaxyViewMatrix() const
{
    return m_galaxyView.viewMatrix();
}


glm::mat4 SystemMapRenderer::galaxyProjectionMatrix(
    const Viewport& vp
) const
{
    return m_galaxyView.projectionMatrix(vp);
}



// ============================================================================
// Galaxy input, scene rendering and labels
// ============================================================================




void SystemMapRenderer::renderGalaxy(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav
)
{
    const auto& systems = galaxy.systems;

    const glm::mat4 proj = galaxyProjectionMatrix(vp);
    const glm::mat4 view = galaxyViewMatrix();
    const glm::mat4 mvp = proj * view;

    if (m_galaxyView.visuals().drawStarfield)
    {
        bool playerInsideKnownSystem = false;

        const glm::dvec3 observerPositionLy =
            playerGalaxyPositionLy(
                galaxy,
                nav,
                playerInsideKnownSystem
            );

        drawMapStarfield(
            vp,
            observerPositionLy,
            view,
            m_galaxyView.visuals().starfieldFieldOfViewDeg,
            m_galaxyView.visuals().starfieldSizeScale,
            true,
            m_galaxyView.visuals().starfieldBrightnessScale,
            m_galaxyView.visuals().milkyWayIntensityScale,
            m_galaxyView.visuals().milkyWayColorTint
        );
    }

    if (m_galaxyView.visuals().drawAtmosphereVeil)
    {
        drawMapAtmosphereVeil(
            m_galaxyView.visuals().atmosphereVeilCenterAlpha,
            m_galaxyView.visuals().atmosphereVeilEdgeAlpha,
            m_galaxyView.visuals().atmosphereVeilAquaStrength
        );
    }

    beginLines();
    beginSolids();

    if (m_galaxyView.state().navigationGrid.enabled())
    {
        /*
            The old decorative plane is hidden in navigation mode.
            The cubic grid is the active coordinate instrument.
        */
        drawGalaxyNavigationGrid(
            vp,
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
            const float v =
                static_cast<float>(i) *
                5.0f;

            addLine(
                {-100.0f, 0.0f, v},
                { 100.0f, 0.0f, v},
                gridColor
            );

            addLine(
                {v, 0.0f, -100.0f},
                {v, 0.0f,  100.0f},
                gridColor
            );
        }
    }

    // Пустой будущий слой навигации.
    // Здесь позже будут гравитационные складки, трассы, маяки, опасные зоны.
    drawNavigationLayerPlaceholder();

    m_galaxyView.state().screenPoints.clear();



    const glm::vec3 cameraDirection =
        orbitCameraDirectionFromYawPitch(
            m_galaxyView.state().camera.yaw,
            m_galaxyView.state().camera.pitch
        );

    const glm::vec3 cameraPosition =
        m_galaxyView.state().camera.target +
        cameraDirection *
        m_galaxyView.state().camera.distance;

    const float safeViewportHeight =
        static_cast<float>(
            std::max(vp.height, 1)
        );

    const float tanHalfFov =
        std::tan(
            glm::radians(48.0f) * 0.5f
        );



    for (const auto& s : systems)
    {
        const glm::vec3 p = galaxyStarPosition(s);

        const bool isCurrent = s.id == nav.currentSystemId;
        const bool isSelected = s.id == m_galaxyView.state().selectedSystemId;






        /*
            Цвет всегда определяется спектральным классом.
            Выбор системы больше не перекрашивает звезду в голубой.
        */
        glm::vec4 c =
            colorForStarType(
                s.starType
            );

        const float viewDepth =
            std::max(
                0.1f,
                glm::dot(
                    p - cameraPosition,
                    -cameraDirection
                )
            );

        /*
            Перевод экранных пикселей в единицы карты.
            Благодаря этому звезда остаётся читаемой при изменении
            разрешения и расстояния камеры.
        */
        const float worldUnitsPerPixel =
            2.0f *
            viewDepth *
            tanHalfFov /
            safeViewportHeight;

        float starScale =
            galaxyStarTypeVisualScale(
                s.starType
            );

        if (s.starsCount > 1)
        {
            starScale *=
                1.0f +
                std::min(
                    0.24f,
                    static_cast<float>(
                        s.starsCount - 1
                    ) *
                    m_galaxyView.visuals().multipleStarScale
                );
        }

        if (isCurrent)
        {
            starScale *=
                m_galaxyView.visuals().currentStarScale;
        }

        if (isSelected)
        {
            starScale *=
                m_galaxyView.visuals().selectedStarScale;
        }

        const float starRadius =
            m_galaxyView.visuals().starBaseRadiusPx *
            starScale *
            worldUnitsPerPixel;

        /*
            Проверяем, находится ли система внутри:
            - явно выбранного куба;
            - куба под курсором.
        */
        bool insideSelectedCube = false;
        bool insideHoveredCube = false;
        float hoveredCubeVisualAlpha = 0.0f;

        if (m_galaxyView.state().navigationGrid.enabled())
        {
            const auto starCellIndex =
                m_galaxyView.state().navigationGrid
                    .nearestIndexForPositionLy(
                        s.positionLy,
                        m_galaxyView.state().navigationGrid.level()
                    );

            if (m_galaxyView.state().navigationGrid.hasSelectedCell())
            {
                const auto& selectedCell =
                    m_galaxyView.state().navigationGrid.selectedCell();

                if (selectedCell.level ==
                    m_galaxyView.state().navigationGrid.level())
                {
                    insideSelectedCube =
                        starCellIndex ==
                        selectedCell.index;
                }
            }

            if (m_galaxyView.state().hoverVisualCell.has_value() &&
                starCellIndex ==
                    m_galaxyView.state().hoverVisualCell->index)
            {
                hoveredCubeVisualAlpha =
                    std::max(
                        hoveredCubeVisualAlpha,
                        m_galaxyView.state().hoverVisualAlpha
                    );
            }

            if (m_galaxyView.state().hoverOutgoingCell.has_value() &&
                starCellIndex ==
                    m_galaxyView.state().hoverOutgoingCell->index)
            {
                hoveredCubeVisualAlpha =
                    std::max(
                        hoveredCubeVisualAlpha,
                        m_galaxyView.state().hoverOutgoingAlpha
                    );
            }

            insideHoveredCube =
                hoveredCubeVisualAlpha > 0.001f;
        }






        /*
            Hovered имеет приоритет: если звезда одновременно
            находится в выбранном и подсвеченном кубе,
            двойное гало не рисуем.
        */
        if (insideHoveredCube)
        {
            addGalaxyStarHalo(
                p,
                starRadius,
                m_galaxyView.visuals()
                    .hoveredCubeHaloRadiusScale,
                m_galaxyView.visuals()
                    .hoveredCubeHaloAlpha *
                        hoveredCubeVisualAlpha,
                c,
                view,
                m_galaxyView.visuals()
                    .starHaloRingCount,
                m_galaxyView.visuals()
                    .starHaloSegments
            );
        }
        else if (insideSelectedCube)
        {
            addGalaxyStarHalo(
                p,
                starRadius,
                m_galaxyView.visuals()
                    .fixedCubeHaloRadiusScale,
                m_galaxyView.visuals()
                    .fixedCubeHaloAlpha,
                c,
                view,
                m_galaxyView.visuals()
                    .starHaloRingCount,
                m_galaxyView.visuals()
                    .starHaloSegments
            );
        }




        /*
            Само ядро звезды.
        */
        c.a = 1.0f;

        addBillboardBall(
            p,
            starRadius,
            c,
            view,
            32
        );








        game::system_map::GalaxyMapScreenPoint sp;
        sp.systemId = s.id;
        sp.name = s.name;
        sp.world = p;
        sp.screen = projectToScreen(p, mvp, vp, sp.visible, sp.depth);
        m_galaxyView.state().screenPoints.push_back(sp);
    }

    /*
        Сначала непрозрачные и полупрозрачные диски звёзд,
        затем линии кубов поверх них.
    */
    flushSolids(mvp);
    flushLines(mvp);



    drawGalaxyLabels(
        vp,
        galaxy,
        mvp
    );

    drawGalaxyPlayerMarker(
        vp,
        galaxy,
        nav,
        mvp
    );


}


void SystemMapRenderer::drawGalaxyPlayerMarker(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& nav,
    const glm::mat4& mvp
)
{
    bool insideKnownSystem = false;

    const glm::dvec3 playerPositionLy =
        playerGalaxyPositionLy(
            galaxy,
            nav,
            insideKnownSystem
        );

    const glm::vec3 playerWorld =
        galaxyPositionLyToRender(
            playerPositionLy
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
            m_galaxyView.state().navigationGrid.maximumLevel();

        const auto terminalIndex =
            m_galaxyView.state().navigationGrid.nearestIndexForPositionLy(
                playerPositionLy,
                terminalLevel
            );

        const auto terminalCell =
            m_galaxyView.state().navigationGrid.cell(
                terminalIndex,
                terminalLevel
            );

        const float halfEdge =
            static_cast<float>(terminalCell.sizeLy * 0.5) *
            GALAXY_RENDER_UNITS_PER_LY;

        beginLines();

        addNavigationCubeEdges(
            galaxyPositionLyToRender(terminalCell.centerLy),
            glm::vec3(halfEdge, 0.0f, 0.0f),
            glm::vec3(0.0f, halfEdge, 0.0f),
            glm::vec3(0.0f, 0.0f, halfEdge),
            glm::vec4(
                markerColor.r,
                markerColor.g,
                markerColor.b,
                m_galaxyView.visuals().navigationGrid.terminalCubeAlpha
            )
        );

        flushLines(mvp);
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

    beginLines();
    addLine(
        glm::vec3(tip, 0.0f),
        glm::vec3(left, 0.0f),
        markerColor
    );
    addLine(
        glm::vec3(left, 0.0f),
        glm::vec3(right, 0.0f),
        markerColor
    );
    addLine(
        glm::vec3(right, 0.0f),
        glm::vec3(tip, 0.0f),
        markerColor
    );
    addLine(
        glm::vec3(right, 0.0f),
        glm::vec3(leaderEnd, 0.0f),
        glm::vec4(
            markerColor.r,
            markerColor.g,
            markerColor.b,
            m_galaxyView.visuals().navigationGrid.terminalLeaderAlpha
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

    flushLines(markerOrtho);

    TextRenderer& text =
        TextRenderer::instance();

    text.beginFrameForViewport(
        vp.width,
        vp.height
    );

    text.textDrawPx(
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

    text.endFrame();
}


void SystemMapRenderer::drawNavigationLayerPlaceholder()
{
    // Пока намеренно пусто.
    // Это точка расширения под:
    // - fold lanes;
    // - beacon coverage;
    // - gravity distortion fields;
    // - restricted jump zones.
}


void SystemMapRenderer::drawGalaxyLabels(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const glm::mat4& mvp
)
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
                m_galaxyView.visuals().labelReferenceHeightPx,
            m_galaxyView.visuals().labelMinimumScreenScale,
            m_galaxyView.visuals().labelMaximumScreenScale
        );

    const float labelFactor =
        screenFactor *
        m_galaxyView.visuals().labelScale;

    const int titlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_galaxyView.visuals().labelTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_galaxyView.visuals().labelTitleMinPx,
            m_galaxyView.visuals().labelTitleMaxPx
        );

    const int selectedTitlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_galaxyView.visuals().labelSelectedTitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_galaxyView.visuals().labelTitleMinPx,
            m_galaxyView.visuals().labelTitleMaxPx
        );

    const int subtitlePx =
        std::clamp(
            static_cast<int>(
                std::lround(
                    static_cast<float>(
                        m_galaxyView.visuals().labelSubtitleBasePx
                    ) *
                    labelFactor
                )
            ),
            m_galaxyView.visuals().labelSubtitleMinPx,
            m_galaxyView.visuals().labelSubtitleMaxPx
        );








    for (const auto& s : galaxy.systems)
    {
        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            galaxyStarPosition(s),
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const bool selected = s.id == m_galaxyView.state().selectedSystemId;

        // Пока игрок находится у текущей системы. Для galaxy-map этого достаточно.
        // Если позже игрок будет в межзвездном пространстве — добавим точную playerPositionLy.
        double distanceLy = 0.0;

        for (const auto& cur : galaxy.systems)
        {
            // У тебя текущая система сейчас визуально совпадает с Sol.
            // До передачи nav сюда считаем от системы id=0, если она есть.
            if (cur.id == 0)
            {
                const double dx = s.positionLy.x - cur.positionLy.x;
                const double dy = s.positionLy.y - cur.positionLy.y;
                const double dz = s.positionLy.z - cur.positionLy.z;

                distanceLy = std::sqrt(dx * dx + dy * dy + dz * dz);
                break;
            }
        }

        const std::string subtitle =
            (s.starType.empty() ? "Unknown" : s.starType) +
            std::string("  /  ") +
            fmtDistanceLy(distanceLy);

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

    // debugLabelTraceToFile(s.name, screen, textPos);




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

beginLines();

static int debugFrame = 0;
const bool logThisFrame = debugFrame < 20;

std::ofstream dbg;

if (logThisFrame)
{
    dbg.open("system_map_label_debug.txt", std::ios::app);

    dbg
        << "\nFRAME " << debugFrame
        << " vp=(" << vp.x << "," << vp.y
        << "," << vp.width << "," << vp.height << ")"
        << " cameraDistance=" << m_galaxyView.state().camera.distance
        << "\n";
}

for (const auto& l : labels)
{
    const glm::vec4 lineColor =
        l.selected
            ? m_galaxyView.visuals().labels.selectedLeaderColor
            : m_galaxyView.visuals().labels.normalLeaderColor;



    addLine(
        glm::vec3(l.screen.x, l.screen.y, 0.0f),
        glm::vec3(l.lineEnd.x, l.lineEnd.y, 0.0f),
        lineColor
    );
}

if (logThisFrame)
    ++debugFrame;

// Label coordinates are local to the system-map viewport:
// x = 0..vp.width
// y = 0..vp.height
//
// Do NOT switch to full-window viewport here.
// TextRenderer also normalizes text using StateContext viewport size,
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


flushLines(labelOrtho);


auto& text = TextRenderer::instance();

text.beginFrameForViewport(
    vp.width,
    vp.height
);

for (const auto& l : labels)
{
    const int px = l.selected ? selectedTitlePx : titlePx;

    const glm::vec4 titleColor =
        l.selected
            ? m_galaxyView.visuals().labels.selectedTitleColor
            : m_galaxyView.visuals().labels.normalTitleColor;

    const glm::vec4 subtitleColor =
        l.selected
            ? m_galaxyView.visuals().labels.selectedSubtitleColor
            : m_galaxyView.visuals().labels.normalSubtitleColor;


    text.textDrawPx(
        l.title,
        l.textPos.x,
        l.textPos.y,
        px,
        titleColor
    );

    text.textDrawPx(
        l.subtitle,
        l.textPos.x,
        l.textPos.y + static_cast<float>(px) + 2.0f,
        subtitlePx,
        subtitleColor
    );
}

text.endFrame();

}



// ============================================================================
// Galaxy camera flight
// ============================================================================




void SystemMapRenderer::beginGalaxyCameraFlight(
    const glm::vec3& destinationTarget,
    float destinationDistance
)
{
    m_galaxyView.beginCameraFlight(
        destinationTarget,
        destinationDistance,
        glfwGetTime()
    );
}




void SystemMapRenderer::updateGalaxyCameraFlight(
    double nowSeconds
)
{
    m_galaxyView.updateCameraFlight(nowSeconds);
}




void SystemMapRenderer::cancelGalaxyCameraFlight(
    bool snapToDestination
)
{
    m_galaxyView.cancelCameraFlight(snapToDestination);
}


// ============================================================================
// Galaxy selection and focus
// ============================================================================



void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    m_galaxyView.focusSystem(
        systemId,
        galaxy,
        m_mode == Mode::Galaxy,
        glfwGetTime()
    );
}


// ============================================================================
// Galaxy star rendering
// ============================================================================




void SystemMapRenderer::addGalaxyStarHalo(
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

    cameraRight =
        glm::normalize(cameraRight);

    cameraUp =
        glm::normalize(cameraUp);

    ringCount =
        std::max(
            ringCount,
            1
        );

    segments =
        std::max(
            segments,
            8
        );

    for (int ring = 0; ring < ringCount; ++ring)
    {
        const float t =
            ringCount > 1
                ? static_cast<float>(ring) /
                    static_cast<float>(ringCount - 1)
                : 0.0f;

        /*
            Между ядром звезды и гало оставляем заметный
            пустой промежуток. Поэтому шарик остаётся чётким,
            а гало читается как отдельный объект.
        */
        constexpr float innerHaloRadiusScale =
            2.10f;

        const float radiusScale =
            innerHaloRadiusScale +
            (
                outerRadiusScale -
                innerHaloRadiusScale
            ) *
            t;

        const float radius =
            starRadius *
            radiusScale;


        /*
            Даже внешнее кольцо остаётся видимым.
            Внутренние кольца заметно ярче.
        */
        const float inverseT =
            1.0f - t;

        const float fade =
            0.28f +
            0.72f *
            std::pow(
                inverseT,
                1.65f
            );

        glm::vec4 ringColor = color;

        ringColor.a =
            baseAlpha *
            fade;

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
                    std::cos(angle0) *
                    cameraRight +
                    std::sin(angle0) *
                    cameraUp
                ) *
                radius;

            const glm::vec3 point1 =
                center +
                (
                    std::cos(angle1) *
                    cameraRight +
                    std::sin(angle1) *
                    cameraUp
                ) *
                radius;

            addLine(
                point0,
                point1,
                ringColor
            );
        }
    }
}






glm::vec4 SystemMapRenderer::colorForStarType(const std::string& starType) const
{
    if (starType.empty())
        return { 1.0f, 0.86f, 0.65f, 1.0f };

    const char t = static_cast<char>(std::toupper(starType[0]));

    switch (t)
    {
        case 'O': return { 0.61f, 0.69f, 1.00f, 1.0f };
        case 'B': return { 0.66f, 0.75f, 1.00f, 1.0f };
        case 'A': return { 0.86f, 0.91f, 1.00f, 1.0f };
        case 'F': return { 0.97f, 0.97f, 1.00f, 1.0f };
        case 'G': return { 1.00f, 0.92f, 0.62f, 1.0f };
        case 'K': return { 1.00f, 0.73f, 0.45f, 1.0f };
        case 'M': return { 1.00f, 0.43f, 0.31f, 1.0f };
        default:  return { 1.00f, 0.86f, 0.65f, 1.0f };
    }
}


// ============================================================================
// Galaxy picking
// ============================================================================



void SystemMapRenderer::debugLabelTraceToFile(
    const std::string& name,
    const glm::vec2& screen,
    const glm::vec2& pos
) const
{
    if (name.find("Tau") == std::string::npos &&
        name.find("Ceti") == std::string::npos)
    {
        return;
    }

    std::ofstream out(
        "galaxy_label_trace.txt",
        std::ios::app
    );

    out
        << name
        << " screen=(" << screen.x << "," << screen.y << ")"
        << " pos=(" << pos.x << "," << pos.y << ")"
        << " delta=(" << (pos.x - screen.x) << "," << (pos.y - screen.y) << ")"
        << " cameraDistance=" << m_galaxyView.state().camera.distance
        << " target=("
        << m_galaxyView.state().camera.target.x << ","
        << m_galaxyView.state().camera.target.y << ","
        << m_galaxyView.state().camera.target.z << ")"
        << "\n";
}
