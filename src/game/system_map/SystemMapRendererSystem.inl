/*
    System map implementation.

    Этот файл включается из SystemMapRenderer.cpp.
    Не добавлять его в CMake как отдельную единицу компиляции.
*/

// ============================================================================
// System camera matrices
// ============================================================================



glm::mat4 SystemMapRenderer::systemViewMatrix() const
{
    const glm::vec3 dir =
        orbitCameraDirectionFromYawPitch(
            m_systemCamera.yaw,
            m_systemCamera.pitch
        );

    const glm::vec3 up =
        orbitCameraUpFromYawPitch(
            m_systemCamera.yaw,
            m_systemCamera.pitch
        );

    // В system map все render-позиции уже будут relative:
    // renderPosition = absolutePosition - m_systemCamera.target.
    //
    // Поэтому камера смотрит в локальный ноль.
    const glm::vec3 eye =
        dir *
        systemMapOrthoEyeDistance(
            m_systemCamera.distance
        );

    return glm::lookAt(
        eye,
        glm::vec3(0.0f, 0.0f, 0.0f),
        up
    );
}


glm::mat4 SystemMapRenderer::systemProjectionMatrix(
    const Viewport& vp
) const
{
    const float aspect =
        vp.height > 0
            ? static_cast<float>(vp.width) /
              static_cast<float>(vp.height)
            : 1.0f;

    const float halfHeight =
        std::clamp(
            m_systemCamera.distance,
            SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
            SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
        );

    const float halfWidth =
        halfHeight *
        aspect;

    return glm::ortho(
        -halfWidth,
         halfWidth,
        -halfHeight,
         halfHeight,
        systemMapOrthoNearPlane(
            halfHeight
        ),
        systemMapOrthoFarPlane(
            halfHeight
        )
    );
}


// ============================================================================
// System navigation and input
// ============================================================================

// updateSystemNavigationHoverFromCursor()
// pickSystemNavigationCell()
// systemNavigationAnchorDiameterPx()
// handleSystemInput()



void SystemMapRenderer::updateSystemNavigationHoverFromCursor(
    const Viewport& vp,
    double localMouseX,
    double localMouseY
)
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        !systemNavigationCellsInteractive(vp))
    {
        m_systemNavigationGrid.clearHoveredCell();
        return;
    }

    const glm::mat4 view = systemViewMatrix();

    const glm::dvec3 cursorMapPosition =
        systemMapTargetPlanePointFromScreen(
            vp,
            view,
            m_systemCamera.target,
            static_cast<double>(m_systemCamera.distance),
            localMouseX,
            localMouseY
        );

    const glm::dvec3 cursorAu =
        cursorMapPosition /
        static_cast<double>(m_lastSystemScale);

    const auto index = m_systemNavigationGrid.nearestIndexForPosition(
        cursorAu,
        m_systemNavigationGrid.level()
    );





    const int minimumLevel =
        m_systemNavigationGrid
            .definition()
            .minimumLevel;

    /*
        The complete System map is one fixed S0 domain.

        S1...S6 are view resolutions inside that domain, not isolated
        child trees of the currently selected or anchored cube.
    */
    const game::navigation::CubicGridIndex
        systemRootIndex {};

    const auto rootIndexForCursor =
        m_systemNavigationGrid.nearestIndexForPosition(
            cursorAu,
            minimumLevel
        );

    if (rootIndexForCursor != systemRootIndex)
    {
        m_systemNavigationGrid.clearHoveredCell();
        return;
    }






    m_systemNavigationGrid.setHoveredCell(
        m_systemNavigationGrid.cell(
            index,
            m_systemNavigationGrid.level()
        )
    );
}

bool SystemMapRenderer::pickSystemNavigationCell(
    const Viewport& vp,
    double localMouseX,
    double localMouseY,
    game::navigation::CubicNavigationCell& outCell
) const
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        !systemNavigationCellsInteractive(vp))
        return false;

    std::vector<game::navigation::CubicNavigationCell> cells;
    cells.reserve(3);

    const auto anchor = m_systemNavigationGrid.anchorCell();
    cells.push_back(anchor);

    const auto addUniqueCell =
        [&](const game::navigation::CubicNavigationCell& cell)
        {
            const bool alreadyPresent =
                std::any_of(
                    cells.begin(),
                    cells.end(),
                    [&](const auto& existing)
                    {
                        return
                            existing.level == cell.level &&
                            existing.index == cell.index;
                    }
                );

            if (!alreadyPresent)
                cells.push_back(cell);
        };

    if (m_systemNavigationGrid.hasSelectedCell())
    {
        const auto selected =
            m_systemNavigationGrid.selectedCell();

        if (selected.level ==
            m_systemNavigationGrid.level())
        {
            addUniqueCell(selected);
        }
    }

    if (m_systemNavigationGrid.hasHoveredCell())
    {
        const auto hovered = m_systemNavigationGrid.hoveredCell();
        addUniqueCell(hovered);
    }

    const glm::mat4 mvp = systemProjectionMatrix(vp) * systemViewMatrix();
    bool found = false;
    float bestDistance = 18.0f;
    float bestDepth = 1.0f;

    for (const auto& cell : cells)
    {
        const glm::dvec3 absoluteMap =
            cell.center * static_cast<double>(m_lastSystemScale);

        const glm::vec3 relative =
            glm::vec3(absoluteMap - m_systemCamera.target);

        bool visible = false;
        float depth = 1.0f;

        const glm::vec2 screen = projectToScreen(
            relative,
            mvp,
            vp,
            visible,
            depth
        );

        if (!visible)
            continue;

        const glm::vec2 delta =
            screen - glm::vec2(
                static_cast<float>(localMouseX),
                static_cast<float>(localMouseY)
            );

        const float distance = glm::length(delta);

        if (distance < bestDistance ||
            (std::abs(distance - bestDistance) < 0.01f && depth < bestDepth))
        {
            bestDistance = distance;
            bestDepth = depth;
            outCell = cell;
            found = true;
        }
    }

    return found;
}

float SystemMapRenderer::systemNavigationAnchorDiameterPx(
    const Viewport& vp
) const
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        vp.height <= 0)
    {
        return 0.0f;
    }

    const auto cell = m_systemNavigationGrid.anchorCell();

    const float cellEdgeWorld =
        static_cast<float>(
            cell.size *
            static_cast<double>(m_lastSystemScale)
        );

    return
        game::navigation::
            cubicNavigationOrthographicProjectedDiameterPx(
                cellEdgeWorld,
                m_systemCamera.distance,
                vp.height
            );
}


bool SystemMapRenderer::systemNavigationCellsInteractive(
    const Viewport& vp
) const
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        vp.width <= 0 ||
        vp.height <= 0)
    {
        return false;
    }

    const float viewportReferencePx =
        static_cast<float>(
            std::max(
                1,
                std::min(vp.width, vp.height)
            )
        );

    const float minimumInteractiveDiameterPx =
        std::max(
            m_systemControls.navigationCellInteractiveMinPx,
            viewportReferencePx *
                m_systemControls
                    .navigationCellInteractiveViewportFraction
        );

    const float projectedCellDiameterPx =
        systemNavigationAnchorDiameterPx(
            vp
        );

    return
        projectedCellDiameterPx >=
            minimumInteractiveDiameterPx;
}





glm::dvec3 SystemMapRenderer::systemNavigationCursorAu() const
{
    if (m_lastSystemScale <= 0.0f)
        return glm::dvec3(0.0);

    /*
        В System map camera.target — абсолютная точка карты,
        находящаяся в центре экрана, в render units.

        Деление на масштаб возвращает её в AU. Это и есть
        навигационный курсор. Он не зависит от выбранного тела.
    */
    return
        m_systemCamera.target /
        static_cast<double>(
            m_lastSystemScale
        );
}


void SystemMapRenderer::focusSystemBody(
    const std::string& bodyId
)
{
    if (bodyId.empty() ||
        !m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f)
    {
        return;
    }

    const auto absoluteIt =
        m_lastSystemBodyAbsolutePosById.find(
            bodyId
        );

    if (absoluteIt ==
        m_lastSystemBodyAbsolutePosById.end())
    {
        return;
    }

    m_selectedBodyId =
        bodyId;

    const glm::dvec3 bodyPositionAu =
        absoluteIt->second /
        static_cast<double>(
            m_lastSystemScale
        );

    /*
        A body behaves like a star in Galaxy:
        - preserve the exact object position as the camera target;
        - select the current-level cube which contains it;
        - keep the current zoom level.
    */
    m_systemNavigationGrid.setAnchorFromPosition(
        bodyPositionAu
    );

    m_systemNavigationGrid.selectCell(
        m_systemNavigationGrid.anchorCell()
    );

    m_systemNavigationGrid.clearHoveredCell();

    m_systemHoverVisualCell.reset();
    m_systemHoverVisualAlpha = 0.0f;
    m_systemHoverOutgoingCell.reset();
    m_systemHoverOutgoingAlpha = 0.0f;

    m_systemCubeClickTracker.reset();

    beginSystemCameraFlight(
        absoluteIt->second,
        m_systemCamera.distance
    );
}


void SystemMapRenderer::syncSystemNavigationAnchorToCursor()
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f)
    {
        return;
    }

    const int minimumLevel =
        m_systemNavigationGrid
            .definition()
            .minimumLevel;

    const glm::dvec3 cursorAu =
        systemNavigationCursorAu();

    const game::navigation::CubicGridIndex
        systemRootIndex {};

    if (m_systemNavigationGrid.nearestIndexForPosition(
            cursorAu,
            minimumLevel
        ) != systemRootIndex)
    {
        return;
    }

    const auto candidateIndex =
        m_systemNavigationGrid.nearestIndexForPosition(
            cursorAu,
            m_systemNavigationGrid.level()
        );

    if (candidateIndex ==
        m_systemNavigationGrid.anchorIndex())
    {
        return;
    }

    /*
        Camera movement owns the view. Updating the anchor only changes
        which current-level cell is rendered at the view centre.
    */
    m_systemNavigationGrid.setAnchorIndex(
        candidateIndex
    );

    m_systemNavigationGrid.clearHoveredCell();

    m_systemHoverVisualCell.reset();
    m_systemHoverVisualAlpha = 0.0f;

    m_systemHoverOutgoingCell.reset();
    m_systemHoverOutgoingAlpha = 0.0f;

    m_systemCubeClickTracker.reset();
}


float SystemMapRenderer::systemNavigationMaximumCameraDistance(
    const Viewport& vp
) const
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        vp.width <= 0 ||
        vp.height <= 0)
    {
        return SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT;
    }

    /*
        The zoom-out boundary is always the single S0 domain.
        It must not shrink when the view changes from S2 to S3, etc.
    */
    const int boundaryLevel =
        m_systemNavigationGrid.definition().minimumLevel;

    const double boundaryEdgeWorld =
        m_systemNavigationGrid.cellSize(boundaryLevel) *
        static_cast<double>(m_lastSystemScale);

    const double viewportReferencePx =
        static_cast<double>(
            std::max(
                1,
                std::min(vp.width, vp.height)
            )
        );

    const double minimumParentDiameterPx =
        viewportReferencePx *
        static_cast<double>(
            m_systemControls
                .navigationParentMinViewportFraction
        );

    /*
        Для ортографической камеры:

        projectedPx =
            edgeWorld * 1.35 / worldUnitsPerPixel

        worldUnitsPerPixel =
            2 * halfHeight / viewportHeight
    */
    const double maximumHalfHeight =
        boundaryEdgeWorld *
        1.35 *
        static_cast<double>(
            std::max(vp.height, 1)
        ) /
        (
            2.0 *
            std::max(
                minimumParentDiameterPx,
                1.0
            )
        );

    return std::clamp(
        static_cast<float>(maximumHalfHeight),
        SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
        SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
    );
}





glm::dvec3
SystemMapRenderer::systemNavigationBoundaryCenterWorld() const
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f)
    {
        return m_systemCamera.target;
    }

    const int boundaryLevel =
        m_systemNavigationGrid
            .definition()
            .minimumLevel;

    const game::navigation::CubicGridIndex
        boundaryIndex {};

    const auto boundaryCell =
        m_systemNavigationGrid.cell(
            boundaryIndex,
            boundaryLevel
        );

    return
        boundaryCell.center *
        static_cast<double>(
            m_lastSystemScale
        );
}








void SystemMapRenderer::
constrainSystemCameraToNavigationBoundary(
    const Viewport& vp
)
{
    if (!m_systemNavigationGrid.enabled() ||
        m_lastSystemScale <= 0.0f ||
        vp.width <= 0 ||
        vp.height <= 0)
    {
        return;
    }

    const float maximumDistance =
        systemNavigationMaximumCameraDistance(
            vp
        );

    const float boundaryEpsilon =
        std::max(
            0.000001f,
            maximumDistance * 0.0005f
        );

    /*
        S0 is the only camera boundary at every System resolution.
        Intermediate cells never recenter or constrain the camera.
    */
    const int boundaryLevel =
        m_systemNavigationGrid
            .definition()
            .minimumLevel;

    const game::navigation::CubicGridIndex
        boundaryIndex {};

    const auto boundaryCell =
        m_systemNavigationGrid.cell(
            boundaryIndex,
            boundaryLevel
        );

    const auto& frame =
        m_systemNavigationGrid.frame();

    const glm::dvec3 axisX =
        glm::normalize(frame.axisX);

    const glm::dvec3 axisY =
        glm::normalize(frame.axisY);

    const glm::dvec3 axisZ =
        glm::normalize(frame.axisZ);

    const double renderScale =
        static_cast<double>(
            m_lastSystemScale
        );

    const glm::dvec3 boundaryCenter =
        boundaryCell.center *
        renderScale;

    /*
        Неподвижная внешняя граница System map.
        Внутри неё при достаточном приближении можно
        панорамировать между соседними родительскими кубами.
    */
    const game::navigation::CubicGridIndex
        systemRootIndex {};

    const auto rootCell =
        m_systemNavigationGrid.cell(
            systemRootIndex,
            m_systemNavigationGrid
                .definition()
                .minimumLevel
        );

    const glm::dvec3 rootCenter =
        rootCell.center *
        renderScale;

    const double rootHalfEdge =
        rootCell.size *
        renderScale *
        0.5;

    /*
        Ограничение зависит только от масштаба,
        а не от yaw/pitch.

        Поэтому при вращении допустимая область
        больше не меняется и камера не дёргается.
    */
    const auto clampTarget =
        [&](const glm::dvec3& requestedTarget,
            float requestedDistance)
        {
            const float safeDistance =
                std::clamp(
                    requestedDistance,
                    SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                    maximumDistance
                );

            if (safeDistance >=
                maximumDistance -
                boundaryEpsilon)
            {
                return boundaryCenter;
            }

            /*
                На максимальном отдалении freedom = 0:
                куб строго по центру.

                При приближении freedom плавно стремится к 1:
                становится доступно панорамирование внутри куба.
            */
            const double linearFreedom =
                std::clamp(
                    1.0 -
                    static_cast<double>(safeDistance) /
                    static_cast<double>(
                        std::max(
                            maximumDistance,
                            0.000001f
                        )
                    ),
                    0.0,
                    1.0
                );

            const double smoothFreedom =
                linearFreedom *
                linearFreedom *
                (
                    3.0 -
                    2.0 * linearFreedom
                );

            const glm::dvec3 relative =
                requestedTarget -
                boundaryCenter;

            const glm::dvec3 rootRelative =
                rootCenter -
                boundaryCenter;

            /*
                На дальнем масштабе оба предела равны нулю:
                текущий материнский куб стоит в центре.

                При приближении пределы раскрываются до полного S0.
                Поэтому камера может перейти в соседний родитель,
                но никогда не покидает выбранный Galaxy-сектор.
            */
            const auto clampAxis =
                [&](const glm::dvec3& axis)
                {
                    const double rootCenterOnAxis =
                        glm::dot(
                            rootRelative,
                            axis
                        );

                    const double minimumOffset =
                        (
                            rootCenterOnAxis -
                            rootHalfEdge
                        ) *
                        smoothFreedom;

                    const double maximumOffset =
                        (
                            rootCenterOnAxis +
                            rootHalfEdge
                        ) *
                        smoothFreedom;

                    return std::clamp(
                        glm::dot(relative, axis),
                        minimumOffset,
                        maximumOffset
                    );
                };

            const double localX =
                clampAxis(axisX);

            const double localY =
                clampAxis(axisY);

            const double localZ =
                clampAxis(axisZ);

            return
                boundaryCenter +
                axisX * localX +
                axisY * localY +
                axisZ * localZ;
        };

    m_systemCamera.distance =
        std::min(
            m_systemCamera.distance,
            maximumDistance
        );

    m_systemCamera.target =
        clampTarget(
            m_systemCamera.target,
            m_systemCamera.distance
        );

    if (m_systemCameraFlight.active)
    {
        m_systemCameraFlight.destinationDistance =
            std::min(
                m_systemCameraFlight.destinationDistance,
                maximumDistance
            );

        m_systemCameraFlight.destinationTarget =
            clampTarget(
                m_systemCameraFlight.destinationTarget,
                m_systemCameraFlight.destinationDistance
            );
    }
}







void SystemMapRenderer::handleSystemInput(
    const Viewport& vp,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown
)
{
    // =========================================================
    // SYSTEM MODE INPUT
    // =========================================================
    
        const SystemControlSettings& controls = m_systemControls;
        const double dx = mx - m_systemCamera.lastMouseX;
        const double dy = my - m_systemCamera.lastMouseY;


        
        constrainSystemCameraToNavigationBoundary(
            vp
        );


        

        bool leftStartedThisFrame = false;
        bool rightStartedThisFrame = false;
        const bool wheelInputPending =
            inside &&
            m_pendingScrollY != 0.0;

        const bool manualFlightCancel =
        (
            inside &&
            leftDown &&
            !m_systemCamera.leftWasDown
        ) ||
        (
            inside &&
            rightDown &&
            !m_systemCamera.rightWasDown
        ) ||
        wheelInputPending;




        if (m_systemCameraFlight.active &&
            manualFlightCancel)
        {
            cancelSystemCameraFlight(false);
        }

        if (m_systemCameraFlight.active)
        {
            m_pendingScrollY = 0.0;

            m_systemNavigationGrid.clearHoveredCell();

            m_systemCamera.rotating = false;
            m_systemCamera.panning = false;






            /*
                Повторная проверка после zoom и pan.

                Если этим действием камера достигла внешней границы,
                сразу возвращаем материнский куб в центр.
            */
            constrainSystemCameraToNavigationBoundary(
                vp
            );


            m_systemCamera.leftWasDown = leftDown;
            m_systemCamera.rightWasDown = rightDown;
            m_systemCamera.lastMouseX = mx;
            m_systemCamera.lastMouseY = my;

            return;
        }

        if (inside)
        {
            updateSystemNavigationHoverFromCursor(
                vp,
                localMx,
                localMy
            );
        }
        else
        {
            m_systemNavigationGrid.clearHoveredCell();
        }

        auto projectSystemAbsoluteToScreen =
            [&](const glm::dvec3& absolutePos,
                bool& visible,
                float& depth) -> glm::vec2
            {
                const glm::dvec3 relative =
                    absolutePos -
                    m_systemCamera.target;

                const glm::vec3 renderPos(
                    static_cast<float>(relative.x),
                    static_cast<float>(relative.y),
                    static_cast<float>(relative.z)
                );

                const glm::mat4 mvp =
                    systemProjectionMatrix(vp) *
                    systemViewMatrix();

                return projectToScreen(
                    renderPos,
                    mvp,
                    vp,
                    visible,
                    depth
                );
            };



            
auto captureSystemOrbitPivot =
[&]()
{
    const float maximumDistance =
        systemNavigationMaximumCameraDistance(
            vp
        );

    const float maximumDistanceEpsilon =
        std::max(
            0.000001f,
            maximumDistance * 0.0005f
        );

    /*
        На самом дальнем обзоре положение камеры
        зафиксировано на материнском кубе.

        Здесь нельзя одновременно пытаться удерживать
        планету под мышью: ограничитель камеры всё равно
        вернёт target к центру и получится рывок.
    */
    if (m_systemCamera.distance >=
        maximumDistance -
            maximumDistanceEpsilon)
    {
        m_systemOrbitPivotAbsolute =
            systemNavigationBoundaryCenterWorld();

        m_systemOrbitPivotActive =
            false;

        return;
    }

    /*
        Маркер куба имеет первый приоритет.
    */
    game::navigation::CubicNavigationCell
        pivotCell;

    const bool cubePivotFound =
        m_systemNavigationGrid.enabled() &&
        pickSystemNavigationCell(
            vp,
            localMx,
            localMy,
            pivotCell
        );

    if (cubePivotFound)
    {
        m_systemOrbitPivotAbsolute =
            pivotCell.center *
            static_cast<double>(
                m_lastSystemScale
            );

        m_systemOrbitPivotActive =
            true;

        return;
    }

    /*
        Тело может стать pivot только при строгом
        попадании рядом с его видимым диском.
    */
    const int pivotBodyIndex =
        pickSystemOrbitPivotBody(
            localMx,
            localMy,
            vp
        );

    if (pivotBodyIndex >= 0 &&
        pivotBodyIndex <
            static_cast<int>(
                m_lastSystemBodyScreenPoints.size()
            ))
    {
        const std::string& bodyId =
            m_lastSystemBodyScreenPoints[
                pivotBodyIndex
            ].bodyId;

        const auto absoluteIt =
            m_lastSystemBodyAbsolutePosById.find(
                bodyId
            );

        if (absoluteIt !=
            m_lastSystemBodyAbsolutePosById.end())
        {
            m_systemOrbitPivotAbsolute =
                absoluteIt->second;

            m_systemOrbitPivotActive =
                true;

            return;
        }
    }

    /*
        Рядом с мышью нет ни куба, ни тела.

        camera.target в ортографической System-камере —
        это мировая точка, которая проецируется точно
        в центр экрана.

        active=false означает, что дополнительная
        pivot-компенсация применяться не будет.
    */
    m_systemOrbitPivotAbsolute =
        m_systemCamera.target;

    m_systemOrbitPivotActive =
        false;
};











        if (inside &&
            leftDown &&
            !m_systemCamera.leftWasDown)
        {
            leftStartedThisFrame =
                true;

            m_systemCamera.mouseDownX =
                mx;

            m_systemCamera.mouseDownY =
                my;

            m_systemCamera.rotating =
                true;

            m_systemCamera.lastMouseX =
                mx;

            m_systemCamera.lastMouseY =
                my;

            captureSystemOrbitPivot();
        }

        if (!leftDown &&
            m_systemCamera.leftWasDown)
        {
            if (inside)
            {
                const double move =
                    std::abs(mx - m_systemCamera.mouseDownX) +
                    std::abs(my - m_systemCamera.mouseDownY);



                if (move < controls.clickMoveThresholdPx)
                {
                    game::navigation::CubicNavigationCell
                        cubeCenterCell;

                    /*
                        Маркер куба имеет приоритет над телом системы.
                        Иначе тело, находящееся под маркером, перехватит
                        второй клик.
                    */
                    const bool cubeCenterPicked =
                        m_systemNavigationGrid.enabled() &&
                        pickSystemNavigationCell(
                            vp,
                            localMx,
                            localMy,
                            cubeCenterCell
                        );

                    const int pickedIndex =
                        cubeCenterPicked
                            ? -1
                            : pickSystemBody(
                                localMx,
                                localMy
                            );

                    if (pickedIndex >= 0 &&
                        pickedIndex <
                            static_cast<int>(
                                m_lastSystemBodyScreenPoints.size()
                            ))
                    {
                        focusSystemBody(
                            m_lastSystemBodyScreenPoints[
                                pickedIndex
                            ].bodyId
                        );
                    }
                    else if (m_systemNavigationGrid.enabled())
                    {
                        if (cubeCenterPicked)
                        {
                            const bool isCubeDoubleClick =
                                m_systemCubeClickTracker
                                    .registerClick(
                                        glfwGetTime(),
                                        localMx,
                                        localMy,
                                        cubeCenterCell.level,
                                        cubeCenterCell.index,
                                        controls
                                            .cubeDoubleClickMaxIntervalSeconds,
                                        controls
                                            .cubeDoubleClickMaxDistancePx
                                    );

                            m_systemNavigationGrid.selectCell(
                                cubeCenterCell
                            );

                            /*
                                Одиночный клик только выбирает куб.
                                Никакого движения камеры здесь нет.
                            */
                            if (isCubeDoubleClick)
                            {
                                const auto levelAction =
                                    game::navigation::
                                        cubicNavigationDoubleClickAction(
                                            m_systemNavigationGrid
                                                .canRefine(),
                                            false
                                        );

                                /*
                                    После уточнения именно этот куб
                                    становится материнским.
                                */
                                const auto newParentCell =
                                    cubeCenterCell;

                                const bool levelChanged =
                                    game::navigation::
                                        applyCubicNavigationLevelActionAtPosition(
                                            levelAction,
                                            m_systemNavigationGrid,
                                            cubeCenterCell.center,
                                            [](
                                                auto& grid,
                                                const glm::dvec3& position
                                            )
                                            {
                                                grid.setAnchorFromPosition(
                                                    position
                                                );
                                            },
                                            false
                                        );

                                if (levelChanged)
                                {
                                    announceNavigationLevel(
                                        'S',
                                        m_systemNavigationGrid.level()
                                    );

                                    m_systemHoverVisualCell.reset();
                                    m_systemHoverVisualAlpha = 0.0f;

                                    m_systemHoverOutgoingCell.reset();
                                    m_systemHoverOutgoingAlpha = 0.0f;

                                    m_systemCubeClickTracker.reset();

                                    const float parentEdgeRender =
                                        static_cast<float>(
                                            newParentCell.size *
                                            static_cast<double>(
                                                m_lastSystemScale
                                            )
                                        );

                                    const float fittedHalfHeight =
                                        game::navigation::
                                            cubicNavigationOrthographicFitHalfHeight(
                                                parentEdgeRender,
                                                vp.width,
                                                vp.height
                                            );

                                    beginSystemCameraFlight(
                                        newParentCell.center *
                                            static_cast<double>(
                                                m_lastSystemScale
                                            ),
                                        std::clamp(
                                            fittedHalfHeight,
                                            SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                                            systemNavigationMaximumCameraDistance(
                                                vp
                                            )
                                        )
                                    );
                                }
                            }
                        }
                        else
                        {
                            m_systemCubeClickTracker.reset();
                        }
                    }
                }

               
                    
           
           
           
           
            }

            m_systemCamera.rotating =
                false;

            m_systemOrbitPivotActive =
                false;
        }

        if (!leftDown)
        {
            m_systemCamera.rotating =
                false;

            m_systemOrbitPivotActive =
                false;
        }

        if (inside &&
            rightDown &&
            !m_systemCamera.rightWasDown)
        {
            rightStartedThisFrame =
                true;

            m_systemCamera.panning =
                true;

            m_systemCamera.lastMouseX =
                mx;

            m_systemCamera.lastMouseY =
                my;
        }

        if (!rightDown)
        {
            m_systemCamera.panning =
                false;
        }

        if (m_systemCamera.rotating &&
            leftDown &&
            !leftStartedThisFrame)
        {
            bool beforeVisible =
                false;

            float beforeDepth =
                1.0f;

            const glm::vec2 pivotBefore =
                projectSystemAbsoluteToScreen(
                    m_systemOrbitPivotAbsolute,
                    beforeVisible,
                    beforeDepth
                );


                



            const float yawStep =
                std::clamp(
                    -static_cast<float>(dx) *
                        controls.rotateSensitivity,
                    -controls.rotationMaxStepRad,
                    controls.rotationMaxStepRad
                );

            const float pitchStep =
                std::clamp(
                    static_cast<float>(dy) *
                        controls.rotateSensitivity,
                    -controls.rotationMaxStepRad,
                    controls.rotationMaxStepRad
                );

            m_systemCamera.yaw +=
                yawStep;

            m_systemCamera.pitch +=
                pitchStep;

















            m_systemCamera.yaw =
                wrapAngleRadF(
                    m_systemCamera.yaw
                );

            m_systemCamera.pitch =
                std::clamp(
                    m_systemCamera.pitch,
                    -controls.pitchLimitRad,
                    controls.pitchLimitRad
                );

            if (m_systemOrbitPivotActive)
            {
                bool afterVisible =
                    false;

                float afterDepth =
                    1.0f;

                const glm::vec2 pivotAfter =
                    projectSystemAbsoluteToScreen(
                        m_systemOrbitPivotAbsolute,
                        afterVisible,
                        afterDepth
                    );

                const glm::vec2 screenDelta =
                    pivotBefore -
                    pivotAfter;

                if (beforeVisible &&
                    afterVisible &&
                    std::isfinite(screenDelta.x) &&
                    std::isfinite(screenDelta.y))
                {
                    const glm::mat4 viewAfter =
                        systemViewMatrix();

                    const glm::vec3 rightF =
                        systemMapViewRight(
                            viewAfter
                        );

                    const glm::vec3 upF =
                        systemMapViewUp(
                            viewAfter
                        );

                    const glm::dvec3 right(
                        rightF.x,
                        rightF.y,
                        rightF.z
                    );

                    const glm::dvec3 up(
                        upF.x,
                        upF.y,
                        upF.z
                    );

                    const double worldUnitsPerPixel =
                        systemMapWorldUnitsPerPixel(
                            static_cast<double>(m_systemCamera.distance),
                            vp.height
                        );

                    m_systemCamera.target -=
                        right *
                        static_cast<double>(screenDelta.x) *
                        worldUnitsPerPixel;

                    m_systemCamera.target +=
                        up *
                        static_cast<double>(screenDelta.y) *
                        worldUnitsPerPixel;
                }
            }
        }

        if (m_systemCamera.panning &&
            rightDown &&
            !rightStartedThisFrame)
        {
            const glm::mat4 view =
                systemViewMatrix();

            const glm::vec3 rightF =
                systemMapViewRight(
                    view
                );

            const glm::vec3 upF =
                systemMapViewUp(
                    view
                );

            const glm::dvec3 right(
                rightF.x,
                rightF.y,
                rightF.z
            );

            const glm::dvec3 up(
                upF.x,
                upF.y,
                upF.z
            );

            const double worldUnitsPerPixel =
                systemMapWorldUnitsPerPixel(
                    static_cast<double>(m_systemCamera.distance),
                    vp.height
                );

            m_systemCamera.target -=
                right *
                dx *
                worldUnitsPerPixel;

            m_systemCamera.target +=
                up *
                dy *
                worldUnitsPerPixel;

            /*
                Навигационный курсор находится в центре экрана.
                Если панорамирование уверенно завело его в соседний
                материнский куб, меняем иерархический якорь без
                перемещения камеры.
            */
            syncSystemNavigationAnchorToCursor();
        }

        if (inside)
        {
            float zoom = 0.0f;

            if (m_pendingScrollY != 0.0)
            {
                zoom +=
                    m_pendingScrollY > 0.0
                        ? 1.0f
                        : -1.0f;

                m_pendingScrollY = 0.0;
            }

            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS)
            {
                zoom += 1.0f;
            }

            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS)
            {
                zoom -= 1.0f;
            }

            if (zoom != 0.0f)
            {
                /*
                    System uses the same navigation-point priority as Galaxy:

                    1. body under the mouse;
                    2. highlighted cube under the mouse;
                    3. current centre of the view.

                    Explicit selection is not consulted. A selected planet
                    therefore cannot pull later level changes back to itself.
                */
                glm::dvec3 navigationPointAu =
                    systemNavigationCursorAu();

                const int pivotBodyIndex =
                    pickSystemOrbitPivotBody(
                        localMx,
                        localMy,
                        vp
                    );

                if (pivotBodyIndex >= 0 &&
                    pivotBodyIndex <
                        static_cast<int>(
                            m_lastSystemBodyScreenPoints.size()
                        ))
                {
                    const std::string& bodyId =
                        m_lastSystemBodyScreenPoints[
                            pivotBodyIndex
                        ].bodyId;

                    const auto absoluteIt =
                        m_lastSystemBodyAbsolutePosById.find(
                            bodyId
                        );

                    if (absoluteIt !=
                        m_lastSystemBodyAbsolutePosById.end())
                    {
                        navigationPointAu =
                            absoluteIt->second /
                            static_cast<double>(
                                m_lastSystemScale
                            );
                    }
                }
                else if (
                    m_systemNavigationGrid.enabled() &&
                    m_systemNavigationGrid.hasHoveredCell())
                {
                    navigationPointAu =
                        m_systemNavigationGrid
                            .hoveredCell()
                            .center;
                }

                const auto rootIndexForNavigationPoint =
                    m_systemNavigationGrid.nearestIndexForPosition(
                        navigationPointAu,
                        m_systemNavigationGrid
                            .definition()
                            .minimumLevel
                    );

                if (rootIndexForNavigationPoint !=
                    game::navigation::CubicGridIndex {})
                {
                    navigationPointAu =
                        systemNavigationCursorAu();
                }

                const glm::mat4 viewBefore =
                    systemViewMatrix();

                const glm::dvec3 anchorBefore =
                    systemMapTargetPlanePointFromScreen(
                        vp,
                        viewBefore,
                        m_systemCamera.target,
                        static_cast<double>(m_systemCamera.distance),
                        localMx,
                        localMy
                    );

                const float zoomStep = controls.zoomStep;

                const float factor =
                    std::pow(
                        zoomStep,
                        -zoom
                    );

                m_systemCamera.distance *= factor;

                const float minHalfHeightFor20KmPerPx =
                    static_cast<float>(
                        (controls.minKmPerPixel *
                         static_cast<double>(m_lastSystemScale) /
                         AU_KM) *
                        static_cast<double>(vp.height) *
                        0.5
                    );

                const float dynamicMinHalfHeight =
                    std::max(
                        SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                        minHalfHeightFor20KmPerPx
                    );

                m_systemCamera.distance =
                    std::clamp(
                        m_systemCamera.distance,
                        dynamicMinHalfHeight,
                        SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
                    );

                const glm::mat4 viewAfter =
                    systemViewMatrix();

                const glm::dvec3 anchorAfter =
                    systemMapTargetPlanePointFromScreen(
                        vp,
                        viewAfter,
                        m_systemCamera.target,
                        static_cast<double>(m_systemCamera.distance),
                        localMx,
                        localMy
                    );

                m_systemCamera.target +=
                    anchorBefore -
                    anchorAfter;

                syncSystemNavigationAnchorToCursor();

                if (m_systemNavigationGrid.enabled())
                {

                    const float cubeDiameterPx =
                        systemNavigationAnchorDiameterPx(vp);



                    const auto levelAction =
                        game::navigation::
                            cubicNavigationWheelAction(
                                zoom,
                                cubeDiameterPx,
                                vp.width,
                                vp.height,
                                m_systemNavigationGrid.canRefine(),
                                m_systemNavigationGrid.canCoarsen(),
                                false
                            );

                    const bool levelChanged =
                        game::navigation::
                            applyCubicNavigationLevelActionAtPosition(
                                levelAction,
                                m_systemNavigationGrid,
                                navigationPointAu,
                                [](
                                    auto& grid,
                                    const glm::dvec3& position
                                )
                                    {
                                        grid.setAnchorFromPosition(
                                            position
                                        );
                                    },
                                    false
                                );

                    if (levelChanged)
                    {
                        announceNavigationLevel(
                            'S',
                            m_systemNavigationGrid.level()
                        );

                        m_systemHoverVisualCell.reset();
                        m_systemHoverVisualAlpha = 0.0f;

                        m_systemHoverOutgoingCell.reset();
                        m_systemHoverOutgoingAlpha = 0.0f;

                        m_systemCubeClickTracker.reset();
                    }


                        






                    const float terminalMinHalfHeight =
                        static_cast<float>(
                            (controls.minKmPerPixel *
                             static_cast<double>(m_lastSystemScale) /
                             AU_KM) *
                            static_cast<double>(vp.height) *
                            0.5
                        );

                    const float navigationMaximumHalfHeight =
                        systemNavigationMaximumCameraDistance(vp);

                    const float navigationMinimumHalfHeight =
                        std::min(
                            navigationMaximumHalfHeight,
                            std::max(
                                SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                                terminalMinHalfHeight
                            )
                        );

                    m_systemCamera.distance = std::clamp(
                        m_systemCamera.distance,
                        navigationMinimumHalfHeight,
                        navigationMaximumHalfHeight
                    );
                }
            }
        }

        /*
            Ограничение должно применяться в том же кадре,
            в котором zoom/pan/rotation изменили камеру.
            Иначе следующий кадр возвращает target назад
            и создаёт видимый скачок.
        */
        constrainSystemCameraToNavigationBoundary(
            vp
        );

        m_systemCamera.leftWasDown = leftDown;
        m_systemCamera.rightWasDown = rightDown;
        m_systemCamera.lastMouseX = mx;
        m_systemCamera.lastMouseY = my;

        
}


// ============================================================================
// System camera flight
// ============================================================================




void SystemMapRenderer::beginSystemCameraFlight(
    const glm::dvec3& destinationTarget,
    float destinationDistance
)
{
    destinationDistance = std::clamp(
        destinationDistance,
        SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
        SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
    );

    m_systemCameraFlight.startTarget =
        m_systemCamera.target;

    m_systemCameraFlight.destinationTarget =
        destinationTarget;

    m_systemCameraFlight.startDistance =
        m_systemCamera.distance;

    m_systemCameraFlight.destinationDistance =
        destinationDistance;

    m_systemCameraFlight.startTimeSeconds =
        glfwGetTime();

    m_systemCameraFlight.durationSeconds =
        0.58;

    m_systemCameraFlight.active =
        true;

    m_systemCamera.rotating = false;
    m_systemCamera.panning = false;
    m_systemOrbitPivotActive = false;
}

void SystemMapRenderer::updateSystemCameraFlight(
    double nowSeconds
)
{
    if (!m_systemCameraFlight.active)
        return;

    const double elapsed =
        std::max(
            0.0,
            nowSeconds -
                m_systemCameraFlight.startTimeSeconds
        );

    const float linearProgress =
        static_cast<float>(
            std::clamp(
                elapsed /
                    m_systemCameraFlight.durationSeconds,
                0.0,
                1.0
            )
        );

    const float progress =
        linearProgress *
        linearProgress *
        linearProgress *
        (
            linearProgress *
            (
                linearProgress * 6.0f -
                15.0f
            ) +
            10.0f
        );

    m_systemCamera.target =
        glm::mix(
            m_systemCameraFlight.startTarget,
            m_systemCameraFlight.destinationTarget,
            static_cast<double>(progress)
        );

const float startDistance =
    std::max(
        m_systemCameraFlight.startDistance,
        0.0000001f
    );

const float destinationDistance =
    std::max(
        m_systemCameraFlight.destinationDistance,
        0.0000001f
    );

m_systemCamera.distance =
    std::exp(
        std::log(startDistance) +
        (
            std::log(destinationDistance) -
            std::log(startDistance)
        ) *
        progress
    );

    if (linearProgress >= 1.0f)
    {
        m_systemCamera.target =
            m_systemCameraFlight.destinationTarget;

        m_systemCamera.distance =
            m_systemCameraFlight.destinationDistance;

        m_systemCameraFlight.active =
            false;
    }
}

void SystemMapRenderer::cancelSystemCameraFlight(
    bool snapToDestination
)
{
    if (!m_systemCameraFlight.active)
        return;

    if (snapToDestination)
    {
        m_systemCamera.target =
            m_systemCameraFlight.destinationTarget;

        m_systemCamera.distance =
            m_systemCameraFlight.destinationDistance;
    }

    m_systemCameraFlight.active =
        false;
}


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
                static_cast<double>(m_systemCamera.distance),
                vp.height
            );

    for (const auto& b : system.bodies)
    {
        if (b.type != BodyType::Planet &&
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
            b.id == m_selectedBodyId;

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
                static_cast<double>(m_lastSystemScale),
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
                screenRadiusPx + 10.0f,
                14.0f
            );

        const float x = screen.x + labelOffsetPx;
        const float y = screen.y - 6.0f;

        const int titlePixelSize =
            selected
                ? 16
                : 14;

        const glm::vec4 titleColor =
            selected
                ? glm::vec4(
                    1.0f,
                    0.78f,
                    0.30f,
                    0.96f
                  )
                : glm::vec4(
                    0.62f,
                    0.84f,
                    1.0f,
                    0.88f
                  );

        text.textDrawPx(
            b.name,
            x,
            y,
            titlePixelSize,
            titleColor
        );

        if (!subtitle.empty() &&
            (selected || screenRadiusPx >= 10.0f))
        {
            text.textDrawPx(
                "(" + subtitle + ")",
                x,
                y + 16.0f,
                10,
                glm::vec4(0.55f, 0.67f, 0.78f, 0.62f)
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
    if (!m_systemNavigationGrid.enabled() || systemScale <= 0.0f)
        return;

    const bool currentLevelCellsInteractive =
        systemNavigationCellsInteractive(vp);

    const double hoverNowSeconds =
        glfwGetTime();

    double hoverDeltaSeconds = 0.0;

    if (m_systemHoverVisualLastTimeSeconds > 0.0)
    {
        hoverDeltaSeconds =
            std::clamp(
                hoverNowSeconds -
                    m_systemHoverVisualLastTimeSeconds,
                0.0,
                0.10
            );
    }

    m_systemHoverVisualLastTimeSeconds =
        hoverNowSeconds;

    const auto anchor =
        m_systemNavigationGrid.anchorCell();

    const std::optional<game::navigation::CubicNavigationCell>
        selectedCell =
            m_systemNavigationGrid.hasSelectedCell()
                ? std::optional<game::navigation::CubicNavigationCell>(
                    m_systemNavigationGrid.selectedCell()
                  )
                : std::nullopt;

    std::optional<game::navigation::CubicNavigationCell>
        hoverTargetCell;

    if (currentLevelCellsInteractive &&
        m_systemNavigationGrid.hasHoveredCell())
    {
        const auto& logicalHovered =
            m_systemNavigationGrid.hoveredCell();

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
            !m_systemHoverVisualCell.has_value() ||
            hoverTargetCell->level !=
                m_systemHoverVisualCell->level ||
            hoverTargetCell->index !=
                m_systemHoverVisualCell->index
        );

    if (hoverTargetChanged)
    {
        if (m_systemHoverVisualCell.has_value() &&
            m_systemHoverVisualAlpha > 0.001f)
        {
            m_systemHoverOutgoingCell =
                m_systemHoverVisualCell;

            m_systemHoverOutgoingAlpha =
                m_systemHoverVisualAlpha;
        }

        m_systemHoverVisualCell =
            hoverTargetCell;

        m_systemHoverVisualAlpha =
            0.0f;
    }

    if (hoverTargetCell.has_value())
    {
        const float fadeInSeconds =
            std::max(
                0.001f,
                m_systemControls.navigationHoverFadeInSeconds
            );

        m_systemHoverVisualAlpha =
            std::min(
                1.0f,
                m_systemHoverVisualAlpha +
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeInSeconds
            );
    }
    else
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_systemControls.navigationHoverFadeOutSeconds
            );

        m_systemHoverVisualAlpha =
            std::max(
                0.0f,
                m_systemHoverVisualAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_systemHoverVisualAlpha <= 0.001f)
        {
            m_systemHoverVisualAlpha = 0.0f;
            m_systemHoverVisualCell.reset();
        }
    }

    if (m_systemHoverOutgoingCell.has_value())
    {
        const float fadeOutSeconds =
            std::max(
                0.001f,
                m_systemControls.navigationHoverFadeOutSeconds
            );

        m_systemHoverOutgoingAlpha =
            std::max(
                0.0f,
                m_systemHoverOutgoingAlpha -
                    static_cast<float>(hoverDeltaSeconds) /
                        fadeOutSeconds
            );

        if (m_systemHoverOutgoingAlpha <= 0.001f)
        {
            m_systemHoverOutgoingAlpha = 0.0f;
            m_systemHoverOutgoingCell.reset();
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

    if (m_systemHoverVisualCell.has_value() &&
        m_systemHoverVisualAlpha > 0.001f)
    {
        const auto& hovered =
            m_systemHoverVisualCell.value();

        if (hovered.level != anchor.level ||
            hovered.index != anchor.index)
        {
            cells.push_back(hovered);
        }
    }

    if (m_systemHoverOutgoingCell.has_value() &&
        m_systemHoverOutgoingAlpha > 0.001f)
    {
        const auto& outgoing =
            m_systemHoverOutgoingCell.value();

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

    const glm::mat4 view = systemViewMatrix();

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

            return glm::vec3(absoluteMap - m_systemCamera.target);
        };

    const glm::vec3 cameraDirection =
        orbitCameraDirectionFromYawPitch(
            m_systemCamera.yaw,
            m_systemCamera.pitch
        );

    const glm::vec3 cameraPosition =
        cameraDirection *
        systemMapOrthoEyeDistance(
            m_systemCamera.distance
        );

    const double worldUnitsPerPixel =
        systemMapWorldUnitsPerPixel(
            static_cast<double>(m_systemCamera.distance),
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
        m_systemNavigationGrid.subdivision();

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
    if (m_systemNavigationGrid.level() >
        m_systemNavigationGrid.definition().minimumLevel)
    {
        std::vector<game::navigation::CubicNavigationCell>
            parentCells;

        parentCells.reserve(cells.size());

        const double subdivision =
            static_cast<double>(
                m_systemNavigationGrid.subdivision()
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
                m_systemNavigationGrid.cell(
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
                m_systemCamera.target;

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
                glm::vec4(0.22f, 0.48f, 0.64f, 0.085f)
            );

            if (i == focusedParentIndex)
            {
                addCubeFarFaceGrids(
                    center,
                    halfAxisX,
                    halfAxisY,
                    halfAxisZ,
                    glm::vec4(0.22f, 0.48f, 0.64f, 0.052f)
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
                m_systemNavigationGrid
                    .definition()
                    .minimumLevel;

        const bool selected =
            !rootBoundary &&
            selectedCell.has_value() &&
            cell.level == selectedCell->level &&
            cell.index == selectedCell->index;







        float hoverVisualAlpha = 0.0f;

        if (m_systemHoverVisualCell.has_value() &&
            cell.level == m_systemHoverVisualCell->level &&
            cell.index == m_systemHoverVisualCell->index)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_systemHoverVisualAlpha
                );
        }

        if (m_systemHoverOutgoingCell.has_value() &&
            cell.level == m_systemHoverOutgoingCell->level &&
            cell.index == m_systemHoverOutgoingCell->index)
        {
            hoverVisualAlpha =
                std::max(
                    hoverVisualAlpha,
                    m_systemHoverOutgoingAlpha
                );
        }

        const bool hovered =
            hoverVisualAlpha > 0.001f;

        glm::vec4 edgeColor =
            selected
                ? glm::vec4(0.92f, 0.66f, 0.20f, 0.24f)
                : glm::vec4(0.38f, 0.72f, 0.94f, 0.08f);

        if (hovered)
        {
            edgeColor =
                glm::vec4(
                    0.45f,
                    0.78f,
                    0.92f,
                    0.18f * hoverVisualAlpha
                );
        }

        glm::vec4 currentLevelGridColor =
            edgeColor;

        currentLevelGridColor.a =
            std::clamp(
                edgeColor.a * 0.28f,
                0.025f * (hovered ? hoverVisualAlpha : 1.0f),
                0.070f * (hovered ? hoverVisualAlpha : 1.0f)
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
            (selected ? 5.0f : 4.0f);









        glm::vec4 markerColor = selected
            ? glm::vec4(1.00f, 0.76f, 0.24f, 0.78f)
            : glm::vec4(0.54f, 0.82f, 1.00f, 0.58f);

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
// System scene rendering
// ============================================================================




void SystemMapRenderer::renderSystem(
        const Viewport& vp,
        const world::celestial::SystemMapSnapshot& system,
        const world::celestial::PlayerNavigationState& nav
)
{
    ensureTexturedGlObjects();
    ensureTexturedShader();
    ensureGeneratedCelestialAssets();


    const bool navigationSystemChanged =
        m_systemNavigationGrid.systemId() !=
            system.systemId;

    if (navigationSystemChanged)
    {
        m_systemNavigationGrid.activateSystem(
            system.systemId
        );

        m_systemHoverVisualCell.reset();
        m_systemHoverVisualAlpha = 0.0f;
        m_systemHoverOutgoingCell.reset();
        m_systemHoverOutgoingAlpha = 0.0f;
        m_systemHoverVisualLastTimeSeconds = 0.0;
        m_systemCubeClickTracker.reset();
    }

    const auto& bodies = system.bodies;

    m_lastSystemBodyScreenPoints.clear();

    /*
        Пустой межзвёздный сектор всё равно должен рисовать
        System navigation grid.

        Для известной системы масштаб определяется орбитами.
        Для пустого сектора S0 вписывается целиком и становится
        исходным материнским кубом.
    */
    double maxAu =
        bodies.empty()
            ? std::max(
                1.0,
                m_systemNavigationGrid.cellSize(
                    m_systemNavigationGrid
                        .definition()
                        .minimumLevel
                ) * 0.5
            )
            : 1.0;

    for (const auto& b : bodies)
    {
        const double r =
            std::sqrt(
                b.positionAu.x * b.positionAu.x +
                b.positionAu.y * b.positionAu.y +
                b.positionAu.z * b.positionAu.z
            );

        maxAu = std::max(maxAu, r);

        if (b.drawOrbit &&
            b.orbitRadiusAu > 0.0)
        {
            const double orbitCenterRadius =
                std::sqrt(
                    b.orbitCenterAu.x * b.orbitCenterAu.x +
                    b.orbitCenterAu.y * b.orbitCenterAu.y +
                    b.orbitCenterAu.z * b.orbitCenterAu.z
                );

            maxAu =
                std::max(
                    maxAu,
                    orbitCenterRadius +
                        b.orbitRadiusAu
                );
        }

    }

    const float systemScale =
        m_systemControls.fittedSystemRadiusWorld /
        static_cast<float>(maxAu);

    m_lastSystemScale = systemScale;

    /*
        Fit each system once. Returning from another map mode preserves the
        existing view, while opening a different system starts with readable
        orbits instead of inheriting an unrelated extreme zoom.
    */
    if (m_lastSystemCameraFitSystemId != system.systemId)
    {
        cancelSystemCameraFlight(false);

        m_systemCamera.target =
            glm::dvec3(0.0);

        m_systemCamera.distance =
            std::clamp(
                m_systemControls.fittedSystemRadiusWorld *
                    m_systemControls.initialFitPadding,
                SYSTEM_MAP_ORTHO_MIN_HALF_HEIGHT,
                SYSTEM_MAP_ORTHO_MAX_HALF_HEIGHT
            );

        m_systemNavigationGrid.setAnchorFromPosition(
            glm::dvec3(0.0)
        );

        m_systemNavigationGrid.selectCell(
            m_systemNavigationGrid.anchorCell()
        );

        m_lastSystemCameraFitSystemId =
            system.systemId;
    }

    const glm::mat4 proj =
        systemProjectionMatrix(
            vp
        );

    const glm::mat4 view =
        systemViewMatrix();

    const glm::mat4 mvp =
        proj * view;

    const double systemWorldUnitsPerPixel =
        systemMapWorldUnitsPerPixel(
            static_cast<double>(
                m_systemCamera.distance
            ),
            vp.height
        );

    using world::celestial::BodyType;

    // =========================================================
    // Static lookup data.
    // Эти таблицы нужны дальше для выбора, радиусов, орбит,
    // подписей и selection overlay.
    // =========================================================
    std::unordered_map<
        std::string,
        const world::celestial::SystemMapBody*
    > bodyById;

    std::unordered_map<
        std::string,
        float
    > drawRadiusById;




    std::unordered_map<
        std::string,
        float
    > selectionRadiusById;






    for (const auto& b : bodies)
    {
        bodyById[b.id] =
            &b;

        drawRadiusById[b.id] =
            bodyVisualRadius(
                b,
                systemScale
            );
    }

// Если выбранная цель исчезла или это звезда — сбрасываем выбор.
if (!m_selectedBodyId.empty())
{
    auto selectedIt =
        bodyById.find(
            m_selectedBodyId
        );

    if (selectedIt == bodyById.end() ||
        selectedIt->second->type == BodyType::Star)
    {
        m_selectedBodyId.clear();
    }
}

// =========================================================
// Floating origin для system map.
//
// absolutePosById хранит точные double-позиции в map units.
// posById хранит render-relative float-позиции около нуля.
// В GPU отдаём только relative position.
// =========================================================
std::unordered_map<
    std::string,
    glm::dvec3
> absolutePosById;

std::unordered_map<
    std::string,
    glm::vec3
> posById;

const glm::dvec3 systemCameraOrigin =
    m_systemCamera.target;

auto auToMapUnits =
    [&](const glm::dvec3& au) -> glm::dvec3
    {
        return glm::dvec3(
            au.x * static_cast<double>(systemScale),
            au.y * static_cast<double>(systemScale),
            au.z * static_cast<double>(systemScale)
        );
    };

auto toRenderPos =
    [&](const glm::dvec3& absoluteMapUnits) -> glm::vec3
    {
        const glm::dvec3 relative =
            absoluteMapUnits -
            systemCameraOrigin;

        return glm::vec3(
            static_cast<float>(relative.x),
            static_cast<float>(relative.y),
            static_cast<float>(relative.z)
        );
    };

for (const auto& b : bodies)
{
    const glm::dvec3 absolutePos =
        auToMapUnits(
            b.positionAu
        );

    absolutePosById[b.id] =
        absolutePos;

    posById[b.id] =
        toRenderPos(
            absolutePos
        );
}


m_lastSystemBodyAbsolutePosById = absolutePosById;















    beginLines();
    beginSolids();
    beginTexturedBodies();


    



    drawSystemNavigationGrid(
        vp,
        mvp,
        systemScale
    );

    

    // Орбиты планет, лун и астероидных поясов.
    for (const auto& b : bodies)
    {
        if (b.type != BodyType::Planet &&
            b.type != BodyType::Moon)
        {
            continue;
        }

        
        if (!b.drawOrbit || b.orbitRadiusAu <= 0.0)
            continue;

       


        const glm::dvec3 orbitCenterAbsolute =
            auToMapUnits(
                b.orbitCenterAu
            );

        const glm::vec3 center =
            toRenderPos(
                orbitCenterAbsolute
            );

        float orbitR =
            static_cast<float>(b.orbitRadiusAu) * systemScale;


        

        




        const glm::vec4 orbitColor =
            b.type == BodyType::Moon
                ? glm::vec4(0.72f, 0.78f, 0.86f, 0.24f)
                : b.type == BodyType::AsteroidBelt
                    ? glm::vec4(0.62f, 0.66f, 0.72f, 0.30f)
                    : glm::vec4(0.48f, 0.76f, 1.00f, 0.34f);

        addCircleXZ(
            center,
            orbitR,
            orbitColor,
            b.type == BodyType::Moon ? 64 : 160
        );
    }




 




    // Тела, кольца, пояса.
    for (const auto& b : bodies)
    {
        const glm::vec3 p = posById[b.id];
        const glm::vec4 c = colorForBodyType(b.type);
        const float r = drawRadiusById[b.id];


        
       






        
        



       const SystemBodyVisualMetrics bodyMetrics =
    computeSystemBodyVisualMetrics(
        b,
        r,
        systemWorldUnitsPerPixel
    );

const float selectionRadiusWorld =
    std::max(
        bodyMetrics.physicalRadiusWorld,
        static_cast<float>(
            systemWorldUnitsPerPixel *
            static_cast<double>(
                bodyMetrics.pickRadiusPx
            )
        )
    );

selectionRadiusById[b.id] =
    selectionRadiusWorld;

if (b.type == BodyType::Planet ||
    b.type == BodyType::Moon)
{
    BodyScreenPoint bp;

    bp.bodyId = b.id;
    bp.name = b.name;

    // Это уже не "физический радиус диска".
    // Это интерактивный радиус выбора.
    bp.screenRadiusPx =
        bodyMetrics.pickRadiusPx;

    bp.screen =
        projectToScreen(
            p,
            mvp,
            vp,
            bp.visible,
            bp.depth
        );

    m_lastSystemBodyScreenPoints.push_back(
        bp
    );
}

if (b.type == BodyType::AsteroidBelt)
{
    const glm::dvec3 orbitCenterAbsolute =
        auToMapUnits(
            b.orbitCenterAu
        );

    const glm::vec3 center =
        toRenderPos(
            orbitCenterAbsolute
        );

    const float beltR =
        static_cast<float>(b.orbitRadiusAu) *
        systemScale;

    addCircleXZ(
        center,
        beltR - 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    addCircleXZ(
        center,
        beltR,
        {0.65f, 0.68f, 0.72f, 0.24f},
        160
    );

    addCircleXZ(
        center,
        beltR + 0.12f,
        {0.65f, 0.68f, 0.72f, 0.12f},
        160
    );

    continue;
}

if (bodyMetrics.drawPhysicalBody)
{
    GLuint bodyAlbedoTexture =
        0;

    if (b.type != BodyType::Star)
    {
        bodyAlbedoTexture =
            globalAlbedoTextureForBody(
                b
            );
    }

    if (bodyAlbedoTexture != 0 &&
        m_texturedShader != 0 &&
        m_texturedVao != 0 &&
        m_texturedVbo != 0)
    {
        const bool largeBody =
            b.type == BodyType::Planet &&
            bodyMetrics.physicalRadiusWorld > 0.16f;

        const int latSeg =
            largeBody ? 64 : 24;

        const int lonSeg =
            largeBody ? 128 : 48;

        addTexturedSystemBodySphere(
            b,
            bodyAlbedoTexture,
            p,
            bodyMetrics.physicalRadiusWorld,
            glm::vec4(1.0f, 1.0f, 1.0f, 1.0f),
            latSeg,
            lonSeg
        );
    }
    else
    {
        // Fallback для звёзд и тел без generated-текстуры.
        // Радиус здесь физический, не proxy.
        addBillboardBall(
            p,
            bodyMetrics.physicalRadiusWorld,
            c,
            view,
            32
        );
    }
}

if (bodyMetrics.drawMarker)
{
    glm::vec4 markerColor =
        c;

    if (b.type == BodyType::Moon)
    {
        markerColor =
            glm::vec4(
                0.72f,
                0.78f,
                0.86f,
                0.90f
            );
    }
    else if (b.type == BodyType::Planet)
    {
        markerColor =
            glm::vec4(
                0.48f,
                0.76f,
                1.00f,
                0.90f
            );
    }
    else if (b.type == BodyType::Star)
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
        p,
        bodyMetrics.markerRadiusWorld,
        markerColor,
        view,
        32
    );
} 

        
        


       

        





        
    }

    if (system.systemId == nav.currentSystemId)
    {
        const glm::dvec3 playerAbsolute(
                nav.systemLocalAu.x * static_cast<double>(systemScale),
                nav.systemLocalAu.y * static_cast<double>(systemScale),
                nav.systemLocalAu.z * static_cast<double>(systemScale)
            );

        const glm::dvec3 playerRelative =
            playerAbsolute -
            systemCameraOrigin;

        glm::vec3 player {
            static_cast<float>(playerRelative.x),
            static_cast<float>(playerRelative.y),
            static_cast<float>(playerRelative.z)
        };

        const float playerCrossSize =
            systemWorldUnitsPerPixel *
            10.0f;

        const float playerCircleRadius =
            systemWorldUnitsPerPixel *
            17.0f;

        addCross(
            player,
            playerCrossSize,
            glm::vec4(1.0f, 0.82f, 0.35f, 1.0f)
        );

        addCircleXZ(
            player,
            playerCircleRadius,
            glm::vec4(1.0f, 0.82f, 0.35f, 0.55f),
            48
        );
    }


    std::unordered_map<uint32_t, glm::vec3> objectVisualPosById;










    for (const auto& obj : system.objects)
    {
        const glm::dvec3 objectAbsolute =
            auToMapUnits(
                obj.positionAu
            );

        const glm::vec3 p =
            toRenderPos(
                objectAbsolute
            );

        objectVisualPosById[obj.id.value] =
            p;
    }

    





    flushLines(mvp);

    // Draw old solid bodies first.
    // Textured bodies are drawn after them.
    flushSolids(mvp);
    flushTexturedBodies(mvp);


    

    // Selection overlay must be drawn AFTER bodies,
    // otherwise the planet texture overpaints the marker.
    if (!m_selectedBodyId.empty())
    {
        auto posIt =
            posById.find(m_selectedBodyId);

        auto radiusIt =
            selectionRadiusById.find(m_selectedBodyId);

        if (posIt != posById.end() &&
                radiusIt != selectionRadiusById.end())
        {
            beginLines();

            const glm::vec3 selectedPos =
                posIt->second;

            const float selectedRadius =
                radiusIt->second;

            glm::vec4 haloColor(
                1.0f,
                0.78f,
                0.28f,
                1.0f
            );

            const auto selectedBodyIt =
                bodyById.find(
                    m_selectedBodyId
                );

            if (selectedBodyIt != bodyById.end())
            {
                haloColor =
                    colorForBodyType(
                        selectedBodyIt->second->type
                    );

                haloColor.a = 1.0f;
            }

            /*
                A selected planet gets a separate, visible halo around the
                sharp body marker. This mirrors selected stars in Galaxy.
            */
            addGalaxyStarHalo(
                selectedPos,
                selectedRadius,
                4.60f,
                0.42f,
                haloColor,
                view,
                7,
                96
            );

            addCircleXZ(
                selectedPos,
                selectedRadius * 1.95f,
                glm::vec4(1.0f, 0.82f, 0.25f, 0.98f),
                96
            );

            addCircleXY(
                selectedPos,
                selectedRadius * 2.10f,
                glm::vec4(1.0f, 0.82f, 0.25f, 0.34f),
                96
            );

            flushLines(mvp);
        }
    }



    drawSystemObjectOverlays(
        system,
        view,
        mvp,
        objectVisualPosById,
        posById,
        drawRadiusById,
        systemWorldUnitsPerPixel,
        systemScale
    );









    

    drawSystemLabels(
        vp,
        system,
        mvp,
        posById,
        drawRadiusById
    );




    drawSystemObjectLabels(
        vp,
        system,
        mvp,
        view,
        objectVisualPosById,
        posById,
        drawRadiusById
    );
   



    {
        const double worldUnitsPerPixel =
            systemMapWorldUnitsPerPixel(
                static_cast<double>(m_systemCamera.distance),
                vp.height
            );

        const double kmPerPixel =
            static_cast<double>(worldUnitsPerPixel) /
            static_cast<double>(systemScale) *
            AU_KM;

        if (kmPerPixel > 0.0 &&
            std::isfinite(kmPerPixel))
        {
            const double desiredBarPx =
                150.0;

            const double niceKm =
                niceSystemMapScaleNumber(
                    kmPerPixel * desiredBarPx
                );

            const double barPx =
                niceKm / kmPerPixel;

            const float x0 =
                28.0f;

            const float scaleOverlay =
                std::clamp(
                    static_cast<float>(vp.height) /
                        1080.0f,
                    0.72f,
                    1.35f
                );

            const float y0 =
                static_cast<float>(vp.height) -
                96.0f * scaleOverlay;

            const float x1 =
                x0 +
                static_cast<float>(
                    std::clamp(
                        barPx,
                        48.0,
                        260.0
                    )
                );

            const glm::mat4 scaleOrtho =
                glm::ortho(
                    0.0f,
                    static_cast<float>(vp.width),
                    static_cast<float>(vp.height),
                    0.0f,
                    -1.0f,
                    1.0f
                );

            beginLines();

            const glm::vec4 scaleColor(
                0.48f,
                0.78f,
                1.0f,
                0.72f
            );

            addLine(
                glm::vec3(x0, y0, 0.0f),
                glm::vec3(x1, y0, 0.0f),
                scaleColor
            );

            addLine(
                glm::vec3(x0, y0 - 5.0f, 0.0f),
                glm::vec3(x0, y0 + 5.0f, 0.0f),
                scaleColor
            );

            addLine(
                glm::vec3(x1, y0 - 5.0f, 0.0f),
                glm::vec3(x1, y0 + 5.0f, 0.0f),
                scaleColor
            );

            flushLines(
                scaleOrtho
            );

            auto& text =
                TextRenderer::instance();

            text.beginFrameForViewport(
                vp.width,
                vp.height
            );

            text.textDrawPx(
                fmtSystemMapScaleDistance(
                    niceKm
                ),
                x0,
                y0 - 24.0f,
                12,
                glm::vec4(
                    0.58f,
                    0.82f,
                    1.0f,
                    0.78f
                )
            );

            std::ostringstream pxLabel;

            pxLabel
                << "1 px = "
                << fmtSystemMapScaleDistance(
                    kmPerPixel
                );

            text.textDrawPx(
                pxLabel.str(),
                x0,
                y0 + 10.0f,
                10,
                glm::vec4(
                    0.42f,
                    0.62f,
                    0.82f,
                    0.58f
                )
            );

            text.endFrame();
        }
    }








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


SystemMapRenderer::SystemBodyVisualMetrics
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
        out.physicalRadiusPx >= m_systemControls.minPhysicalBodyRadiusPx;

    float desiredMarkerRadiusPx =
        0.0f;

    if (body.type == BodyType::Star)
    {
        if (out.physicalRadiusPx < m_systemControls.starMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.starMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Planet)
    {
        if (out.physicalRadiusPx < m_systemControls.planetMarkerRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.planetMarkerRadiusPx;
        }
    }
    else if (body.type == BodyType::Moon)
    {
        if (out.physicalRadiusPx < m_systemControls.tinyMoonProxyRadiusPx)
        {
            desiredMarkerRadiusPx =
                m_systemControls.tinyMoonProxyRadiusPx;
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
            m_systemControls.pickMinBodyRadiusPx
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
    const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
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
                obj.id.value
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
                    1.0f,
                    0.78f,
                    0.30f,
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
                1.0f,
                0.78f,
                0.30f,
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
    const std::unordered_map<uint32_t, glm::vec3>& objectVisualPosById,
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
                obj.id.value
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

        text.textDrawPx(
            obj.name,
            screen.x + 8.0f,
            screen.y - 7.0f,
            13,
            glm::vec4(
                1.0f,
                0.86f,
                0.42f,
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
                    0.75f,
                    0.72f,
                    0.58f,
                    alpha * 0.70f
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
    return m_selectedBodyId;
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
         i < static_cast<int>(m_lastSystemBodyScreenPoints.size());
         ++i)
    {
        const BodyScreenPoint& p =
            m_lastSystemBodyScreenPoints[i];

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
                m_systemControls.pickMaxBodyRadiusPx
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
                    m_systemControls.pickHaloRadiusFactor +
                    m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloBasePx,
                m_systemControls.pickHaloMaxPx
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
                    m_systemControls.pickScoreDiskWeight +
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
                m_lastSystemBodyScreenPoints.size()
            );
         ++i)
    {
        const BodyScreenPoint& point =
            m_lastSystemBodyScreenPoints[i];

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
            m_systemControls
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
