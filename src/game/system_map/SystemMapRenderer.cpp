#include "src/game/system_map/SystemMapRendererInternal.h"






















double SystemMapRenderer::currentTimeSeconds() const
{
    return glfwGetTime();
}


void SystemMapRenderer::drawMapStarfield(
    const Viewport& viewport,
    const glm::dvec3& observerPositionLy,
    const glm::mat4& cameraView,
    float fieldOfViewDeg,
    float sizeScale,
    bool distantGalaxyBackdrop,
    float starBrightnessScale,
    float milkyWayIntensityScale,
    const glm::vec3& milkyWayColorTint
)
{
    m_mapResources.drawStarfield(
        viewport,
        observerPositionLy,
        cameraView,
        fieldOfViewDeg,
        sizeScale,
        distantGalaxyBackdrop,
        starBrightnessScale,
        milkyWayIntensityScale,
        milkyWayColorTint
    );
}


void SystemMapRenderer::beginTextFrame(
    int viewportWidth,
    int viewportHeight
)
{
    TextRenderer::instance().beginFrameForViewport(
        viewportWidth,
        viewportHeight
    );
}


void SystemMapRenderer::drawTextPx(
    const std::string& text,
    float x,
    float y,
    int pixelHeight,
    const glm::vec4& color
)
{
    TextRenderer::instance().textDrawPx(
        text,
        x,
        y,
        pixelHeight,
        color
    );
}


void SystemMapRenderer::endTextFrame()
{
    TextRenderer::instance().endFrame();
}


SystemMapRenderer::SystemMapRenderer(
    game::navigation::ClientNavigationWorkspace& navigationWorkspace
)
    : m_navigationWorkspace(navigationWorkspace),
      m_detailBackend(m_mapResources),
      m_hubBackend(m_mapResources)
{
}


void SystemMapRenderer::init()
{
    ensureGlObjects();
    ensureShader();

    ensureTexturedGlObjects();
    ensureTexturedShader();

    ensureBackground();

    m_mapResources.init(
        m_galaxyView.visuals().starfieldMinimumDistanceLy
    );

    if (!m_navigationRegionCatalog.loaded())
    {
        const bool namesLoaded =
            m_navigationRegionCatalog.loadFromRuntimeOrSource(
                "assets/localization/world/navigation_regions",
                "src/assets/localization/world/navigation_regions"
            );

        if (!namesLoaded)
        {
            std::cerr
                << "[SystemMapRenderer] navigation region names not loaded\n";
        }
    }

    m_initialized = true;
}


void SystemMapRenderer::resetView()
{
    m_modeState.reset(Mode::Galaxy);
    m_galaxyView.reset();
    m_systemView.reset();

    m_navigationLevelAnnouncement.text.clear();
    m_navigationLevelAnnouncement.startedAtSeconds = -1.0;

    m_detailView.reset();
    m_hubView.reset();
    m_detailPresentation =
        game::system_map::DetailMapPresentation{};
    m_hubPresentation =
        game::system_map::HubMapPresentation{};
    m_pendingScrollY = 0.0;
    m_systemPresentation =
        game::system_map::SystemMapPresentation{};
    m_systemSceneFrame =
        game::system_map::SystemMapSceneFrame{};
    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;


    m_systemView.state().lastScale = 1.0f;


    m_mapResources.resetPresentationTime();

    m_hoveredMapButton.clear();
    m_navigationOverlayLeftWasDown = false;
    // A new map session starts without back history or an open menu.
    m_mapHistory.clear();
    m_pendingHistoryRestore.reset();
    m_pendingGalaxyBookmark.reset();
    m_contextMenu.close();
    m_galaxySystemCard.reset();
    m_galaxySystemCardSystemId = -1;
    m_objectOverlayState.clearTransientDrag();
    m_routeOverlayState.clearTransientDrag();

}





















void SystemMapRenderer::onGalaxyMapEntered(
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::PlayerNavigationState& navigation
)
{
    m_galaxyView.onEntered(
        galaxy,
        navigation
    );

    // Back history: the saved Galaxy view replaces the entry framing.
    if (m_pendingGalaxyBookmark)
    {
        m_galaxyView.restoreBookmark(*m_pendingGalaxyBookmark);
        m_pendingGalaxyBookmark.reset();
    }
}


void SystemMapRenderer::focusGalaxySystem(
    int systemId,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    m_galaxyView.focusSystem(
        systemId,
        galaxy,
        m_modeState.current() == Mode::Galaxy,
        glfwGetTime()
    );
}


int SystemMapRenderer::selectedSystemId() const
{
    return m_galaxyView.state().selectedSystemId;
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::selectedGalaxyEntryIntent(
    const world::celestial::GalaxyMapSnapshot& galaxy) const
{
    if (m_modeState.current() != Mode::Galaxy)
        return std::nullopt;

    const auto& state = m_galaxyView.state();

    // A named system selected from the native list can be opened at any
    // Galaxy grid level, matching the old STAR ATLAS dropdown semantics.
    if (state.selectedSystemId >= 0)
    {
        const auto system = std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& candidate)
            {
                return candidate.id == state.selectedSystemId;
            });
        if (system != galaxy.systems.end())
        {
            return game::system_map::MapIntent::enterKnownSystem(
                system->id,
                system->positionLy);
        }
    }

    // Empty-space entry is meaningful only at the terminal Galaxy cube.
    if (!state.navigationFocusValid ||
        state.navigationGrid.level() != state.navigationGrid.maximumLevel())
    {
        return std::nullopt;
    }

    const auto intent = m_galaxyView.entryIntentForPosition(
        galaxy,
        state.navigationFocusLy);
    return intent.valid() ? std::optional<game::system_map::MapIntent>(intent)
                          : std::nullopt;
}

int SystemMapRenderer::focusedSystemId() const
{
    return m_galaxyView.state().focusedSystemId;
}























void SystemMapRenderer::setMode(Mode mode)
{
    if (m_modeState.current() == mode)
        return;

    const Mode previousMode = m_modeState.current();

    // The map left is remembered (back arrow) unless this change is the
    // back navigation itself.
    if (previousMode == Mode::Galaxy)
        m_galaxyView.cancelCameraFlight(true);
    recordMapHistory(previousMode, mode);
    m_contextMenu.close();

    if (previousMode == Mode::Detail)
    {
        auto& systemState = m_systemView.state();
        const auto& detailState = m_detailView.state();

        systemState.selectedHubId =
            detailState.selectedHubId;
        systemState.selectedHubParentBodyId =
            detailState.selectedHubParentBodyId;

        if (!systemState.selectedHubId.empty())
            systemState.selectedBodyId.clear();
    }

    (void)m_modeState.transition(mode);

    m_systemFramePrepared = false;
    m_systemSceneFrameDirty = true;
    m_detailFramePrepared = false;
    m_detailFrameDirty = true;
    m_hubFramePrepared = false;
    m_hubFrameDirty = true;
    m_objectOverlayState.clearTransientDrag();
    m_routeOverlayState.clearTransientDrag();

    /*
        Если пользователь открыл другую карту во время перелёта,
        сохраняем конечную позицию Galaxy-камеры.
    */
    if (m_modeState.current() != Mode::Galaxy)
    {
        m_galaxyView.cancelCameraFlight(
            true
        );
    }

    if (m_modeState.current() == Mode::Detail)
    {
        m_detailView.reset();
        m_detailView.selectHub(
            m_systemView.state().selectedHubId,
            m_systemView.state().selectedHubParentBodyId
        );
        m_detailPresentation =
            game::system_map::DetailMapPresentation{};
    }

    if (m_modeState.current() == Mode::Hub)
    {
        if (previousMode != Mode::Detail)
        {
            m_detailView.selectHub(
                m_systemView.state().selectedHubId,
                m_systemView.state().selectedHubParentBodyId
            );
        }

        m_hubView.beginScene();
        m_hubPresentation =
            game::system_map::HubMapPresentation{};
    }

    applyMapHistoryRestore(mode);
}



SystemMapRenderer::Mode SystemMapRenderer::mode() const
{
    return m_modeState.current();
}










void SystemMapRenderer::render(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& planet,
    const world::celestial::HubMapSnapshot& hub,
    const world::celestial::PlayerNavigationState& nav
)
{


    if (!m_initialized)
        init();

    const double nowSeconds =
        glfwGetTime();

    m_galaxyView.updateCameraFlight(
        nowSeconds
    );


    m_mapTransition.update(
        nowSeconds
    );



    /*
        Начало нового кадра процедурных облаков.

        Это должно выполняться ровно один раз до любого
        вызова textureForStyle(), независимо от режима карты
        и наличия старой solid geometry.
    */
    m_mapResources.beginFrame();



    const Viewport& vp = viewport;

    glViewport(vp.x, vp.y, vp.width, vp.height);
    glScissor(vp.x, vp.y, vp.width, vp.height);

    GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);

    glDisable(GL_DEPTH_TEST);

    drawBackground();

    // System map рисует свою 3D-сцену поверх игрового кадра.
    // Поэтому depth buffer надо очистить, иначе карта может
    // наследовать глубину от предыдущего рендера.
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);



    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);




    if (m_modeState.current() == Mode::Detail)
    {
        if (!m_detailFramePrepared || m_detailFrameDirty)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    viewport,
                    planet
                );

            m_detailFrameDirty = false;
        }

        m_detailSceneRenderer.render(
            m_detailPresentation,
            m_detailBackend,
            viewport,
            planet
        );

        m_detailFramePrepared = false;
        m_detailFrameDirty = true;
    }
    else if (m_modeState.current() == Mode::Hub)
    {
        if (!m_hubFramePrepared || m_hubFrameDirty)
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    viewport,
                    hub
                );
            decorateHubDockingOverlay(hub);

            m_hubFrameDirty = false;
        }

        m_hubSceneRenderer.render(
            m_hubPresentation,
            m_hubBackend,
            viewport,
            hub
        );

        m_hubFramePrepared = false;
        m_hubFrameDirty = true;
    }
    else if (m_modeState.current() == Mode::Galaxy)
    {
        m_galaxyRenderer.render(
            m_galaxyView,
            *this,
            vp,
            galaxy,
            nav
        );
        refreshGalaxyWaypointCandidate(vp, galaxy);
    }
    else if (m_modeState.current() == Mode::System)
    {
        renderSystem(
            vp,
            system,
            nav
        );
        refreshSystemWaypointCandidate(vp, system);
    }

    // The planner owns world-space guidance samples; each map owns only their
    // projection. This makes the same active docking task visible as a smooth
    // planned trajectory on System/Detail/Hub without teaching the planner
    // anything about cameras or render scale.
    decorateActiveGuidanceTrajectory(vp, system, planet, hub);

    if (m_modeState.current() == Mode::Galaxy)
    {
        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
        m_objectOverlayRenderer.render(
            vp,
            m_galaxyInfoOverlayFrame,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
        m_galaxyRenderer.drawWhatIsHere(m_galaxyView, *this, vp);
    }
    else if (m_modeState.current() == Mode::System)
    {
        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_systemSceneFrame.interaction.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }
    else if (m_modeState.current() == Mode::Detail)
    {
        synchronizeNavigationTracking(
            m_detailPresentation.frame.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_detailPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }
    else if (m_modeState.current() == Mode::Hub)
    {
        synchronizeNavigationTracking(
            m_hubPresentation.frame.objectOverlay
        );
        m_objectOverlayRenderer.render(
            vp,
            m_hubPresentation.frame.objectOverlay,
            m_objectOverlayState,
            m_navigationMapTextProfile
        );
    }

    m_routeOverlayRenderer.render(
        vp,
        m_navigationWorkspace.routePlan(),
        m_routeOverlayState,
        m_navigationMapTextProfile
    );

    drawNavigationCoordinateOverlay(
        vp,
        galaxy,
        system,
        nav
    );

    // The context menu is above everything of the map.
    drawContextMenu(vp);

    /*
        Настоящий crossfade.

        Если переход только начался, framebuffer ещё содержит
        старое состояние. Сохраняем его в текстуру и только
        после этого выполняем смену камеры или режима.

        На следующих кадрах уже рисуется новое состояние,
        а старый снимок постепенно растворяется поверх него.
    */
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

    if (m_mapTransition.needsOutgoingCapture())
    {
        captureMapTransitionSnapshot(
            viewport
        );

        m_mapTransition.outgoingCaptured(
            glfwGetTime()
        );
    }
    else if (m_mapTransition.needsIncomingWarmup())
    {
        // The new mode has now rendered one complete frame, but it remains
        // fully covered by the outgoing snapshot. Lazy presentation builders,
        // geometry and shader state therefore cannot flash half-prepared.
        drawMapTransitionSnapshot(
            viewport,
            1.0f
        );
        m_mapTransition.incomingFrameRendered(
            glfwGetTime()
        );
    }
    else if (m_mapTransition.active())
    {
        drawMapTransitionSnapshot(
            viewport,
            m_mapTransition.outgoingAlpha()
        );
    }


    if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);

    if (blendWasEnabled) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}




#include "src/game/system_map/SystemMapRendererSystem.inl"





glm::vec2 SystemMapRenderer::projectToScreen(
    const glm::vec3& world,
    const glm::mat4& mvp,
    const Viewport& vp,
    bool& visible,
    float& depth
) const
{
    const glm::vec4 clip = mvp * glm::vec4(world, 1.0f);

    visible = false;
    depth = 2.0f;

    if (clip.w <= 0.00001f)
        return {0.0f, 0.0f};

    const glm::vec3 ndc = glm::vec3(clip) / clip.w;

    visible =
        ndc.x >= -1.0f && ndc.x <= 1.0f &&
        ndc.y >= -1.0f && ndc.y <= 1.0f &&
        ndc.z >= -1.0f && ndc.z <= 1.0f;

    depth = ndc.z;

    return {
        (ndc.x * 0.5f + 0.5f) * static_cast<float>(vp.width),
        (1.0f - (ndc.y * 0.5f + 0.5f)) * static_cast<float>(vp.height)
    };
}




























void SystemMapRenderer::setRightPanelRatio(float ratio)
{
    m_rightPanelRatio = std::clamp(ratio, 0.0f, 0.45f);
}







#include "src/game/system_map/SystemMapRendererCommon.inl"
