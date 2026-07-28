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
    m_galaxyRenderer.render(
        m_galaxyView,
        *this,
        vp,
        galaxy,
        nav
    );
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
