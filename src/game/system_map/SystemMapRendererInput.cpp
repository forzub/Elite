// SystemMapRenderer: map input handling (mouse / keyboard, picking, panel interaction).

#include <unordered_set>
#include "src/game/system_map/SystemMapRendererInternal.h"



std::optional<game::system_map::MapIntent>
SystemMapRenderer::handleInput(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& detail,
    const world::celestial::HubMapSnapshot& hub
)
{
    if (m_modeState.current() != Mode::Galaxy &&
        m_modeState.current() != Mode::System &&
        m_modeState.current() != Mode::Detail &&
        m_modeState.current() != Mode::Hub)
    {
        return std::nullopt;
    }

    const double inputNowSeconds =
        glfwGetTime();

    if (m_modeState.current() == Mode::System)
    {
        m_systemView.updateCameraFlight(
            inputNowSeconds
        );
        auto& cam = m_systemView.state().camera;
        if (m_systemView.state().planView && std::abs(cam.pitch - m_systemView.state().planPitchRad) > 1.0e-4f)
        {
            // ease into the plan tilt (no jump), rotating about the camera target
            const float step = std::clamp(m_systemView.state().planPitchRad - cam.pitch, -0.08f, 0.08f);
            m_systemView.orbitCameraAroundPivot(cam.target, 0.0f, step, m_systemView.controls().pitchLimitRad);
        }
    }

    /*
        Scroll input belongs to the application-wide Input service.
        SystemMapRenderer must not replace GLFW callbacks or keep raw
        pointers in a global table. The renderer only consumes the
        frame-local wheel delta while the map owns input focus.
    */
    m_pendingScrollY +=
        Input::instance().consumeScrollY();


    /*
        Во время crossfade нельзя вращать или перемещать
        старую либо новую сцену.

        AwaitingCapture тоже считается активной фазой.
    */
    if (m_mapTransition.blocksInput())
    {
        if (m_modeState.current() == Mode::System)
        {
            m_systemView.constrainCameraToNavigationBoundary(
                vp
            );
        }

        m_pendingScrollY = 0.0;
        m_hoveredMapButton.clear();
        m_galaxyBreadcrumbHovered = -1;
        // A press held across the map change (the back arrow sits where
        // the next map has LEVEL 0) belongs to the old map: nothing of the
        // new map reacts to it until it is released.
        m_navigationOverlayLeftWasDown = true;
        m_contextSwallowLeft = true;
        return std::nullopt;
    }

    // Cross-map route recall is a small intent-driven state machine.  A prior
    // frame may have asked SpaceState to switch map/system; once that canonical
    // transition commits, continue toward the authored layer here.
    if (m_pendingRouteFocusNodeId != 0)
    {
        if (const auto intent = advancePendingRouteFocus(
                vp, galaxy, system, inputNowSeconds);
            intent.has_value())
        {
            return intent;
        }
    }


    GLFWwindow* window =
        glfwGetCurrentContext();

    if (!window)
        return std::nullopt;

    double mx = 0.0;
    double my = 0.0;

    glfwGetCursorPos(
        window,
        &mx,
        &my
    );

    bool leftDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_LEFT
        ) == GLFW_PRESS;

    bool rightDown =
        glfwGetMouseButton(
            window,
            GLFW_MOUSE_BUTTON_RIGHT
        ) == GLFW_PRESS;

    driveDevPointer(vp, mx, my, leftDown, rightDown, &galaxy);

    // DEV only: a scripted pointer replaces the GLFW pointer; the rest of
    // the input path is production.
    if (game::system_map::GalaxyPointerDevScript::enabled() &&
        m_modeState.current() == Mode::Galaxy)
    {
        if (!m_galaxyPointerDevScript.panelAt)
        {
            m_galaxyPointerDevScript.panelAt = [this](glm::vec2 p) {
                return m_objectOverlayState.panelUnderCursor(glm::dvec2(p));
            };
        }
        m_galaxyPointerDevScript.drive(
            m_galaxyView, m_galaxyInteraction, galaxy, vp, inputNowSeconds, mx, my, leftDown, rightDown);
    }
    if (game::system_map::SystemHierarchyDevScript::enabled() &&
        (m_modeState.current() == Mode::System || m_modeState.current() == Mode::Detail))
    {
        if (!m_systemHierarchyDevScript.bodyLocator)
        {
            m_systemHierarchyDevScript.bodyLocator = [this, vp] {
                std::vector<std::pair<std::string, glm::vec2>> out;
                const double scale = static_cast<double>(m_systemPresentation.systemScale);
                for (const auto& body : m_systemPresentation.bodies)
                {
                    if (body.type != world::celestial::BodyType::Planet)
                        continue;
                    bool visible = false;
                    float depth = 1.0f;
                    const glm::vec2 s =
                        m_systemView.projectAbsoluteToScreen(vp, body.positionAu * scale, visible, depth);
                    if (visible)
                        out.push_back({body.id, s});
                }
                return out;
            };
            m_systemHierarchyDevScript.moonAudit = [this]() {
                std::unordered_set<std::string> presented, pickable;
                for (const auto& body : m_systemPresentation.bodies)
                    presented.insert(body.id);
                for (const auto& point : m_systemSceneFrame.interaction.bodyScreenPoints)
                    pickable.insert(point.bodyId);
                std::string missing;
                for (const auto& [moon, planet] : m_systemPresentation.moonParents)
                {
                    if (!presented.count(planet))
                        continue;
                    if (!presented.count(moon))
                        missing += moon.substr(moon.rfind('.') + 1) + " hidden; ";
                    else if (!pickable.count(moon))
                        missing += moon.substr(moon.rfind('.') + 1) + " not pickable; ";
                }
                bool earth = false;
                for (const auto& id : presented)
                    earth = earth || id.find("Земля") != std::string::npos ||
                            id.find("Earth") != std::string::npos;
                if (!earth)
                    missing += "Earth not presented; ";
                return missing;
            };
            m_systemHierarchyDevScript.cardOpen = [this](const std::string& id) {
                return m_objectOverlayState.isOpen(
                    "body:" + std::to_string(m_systemPresentation.systemId) + ":" + id);
            };
            m_systemHierarchyDevScript.bodyPositionAu = [this](const std::string& id) -> std::optional<glm::dvec3> {
                for (const auto& body : m_systemPresentation.bodies)
                    if (body.id == id)
                        return body.positionAu;
                return std::nullopt;
            };
            m_systemHierarchyDevScript.contextMenu = [this]() -> const game::system_map::MapContextMenu* {
                return m_contextMenu.open ? &m_contextMenu : nullptr;
            };
            m_systemHierarchyDevScript.mapMode = [this] { return m_modeState.current(); };
            m_systemHierarchyDevScript.backButton = [this, vp]() -> std::optional<glm::vec2> {
                for (const auto& button : mapButtons(vp))
                    if (button.id == "back")
                        return glm::vec2((button.view.bounds.left + button.view.bounds.right) * 0.5f,
                                         (button.view.bounds.top + button.view.bounds.bottom) * 0.5f);
                return std::nullopt;
            };
        }
        m_systemHierarchyDevScript.drive(m_systemView, vp, inputNowSeconds, mx, my, leftDown, rightDown,
                                         m_pendingScrollY);
    }
    if (game::system_map::GalaxyHierarchyDevScript::enabled() &&
        m_modeState.current() == Mode::Galaxy)
    {
        if (!m_galaxyHierarchyDevScript.breadcrumbLocator)
        {
            m_galaxyHierarchyDevScript.contextMenu = [this]() -> const game::system_map::MapContextMenu* {
                return m_contextMenu.open ? &m_contextMenu : nullptr;
            };
            m_galaxyHierarchyDevScript.cardOpen = [this](const std::string& id) {
                return m_objectOverlayState.isOpen(id);
            };
            m_galaxyHierarchyDevScript.breadcrumbLocator = [this, vp] {
                std::vector<glm::vec4> out;
                for (const auto& b : m_navigationCoordinateOverlay.breadcrumbBounds(vp, galaxyBreadcrumbs()))
                    out.push_back(glm::vec4(b.left, b.top, b.right, b.bottom));
                return out;
            };
        }
        m_galaxyHierarchyDevScript.drive(
            m_galaxyView, m_galaxyInteraction, galaxy, vp, inputNowSeconds,
            mx, my, leftDown, rightDown, m_pendingScrollY);
    }

    const double localMx =
        mx - static_cast<double>(vp.x);

    const double localMy =
        my - static_cast<double>(vp.y);

    const bool inside =
        mx >= static_cast<double>(vp.x) &&
        my >= static_cast<double>(vp.y) &&
        mx <= static_cast<double>(vp.x + vp.width) &&
        my <= static_cast<double>(vp.y + vp.height);

    // The context menu (right click / Ctrl + click) first: an open menu
    // owns the pointer.
    {
        std::optional<game::system_map::MapIntent> menuIntent;
        if (handleContextMenuPointer(vp, galaxy, system, hub, window, mx, my, localMx, localMy, inside, leftDown,
                                     rightDown, inputNowSeconds, menuIntent))
        {
            if (menuIntent && menuIntent->type == game::system_map::MapIntentType::None)
                menuIntent.reset();
            return menuIntent;
        }
    }

    const auto routePointer =
        m_routeOverlayState.handlePointer(
            m_navigationWorkspace.routePlan(),
            glm::dvec2(vp.width, vp.height),
            glm::dvec2(localMx, localMy),
            inside,
            leftDown
        );
    // the route window's buttons: calculate / start / pause / resume (every map)
    if (routePointer.routeCommand == 1)
        m_pendingRouteLaunch = 3;
    else if (routePointer.routeCommand == 2)
        m_pendingRouteLaunch = 1;
    else if (routePointer.routeCommand == 3)
        m_pendingRouteControl = 1;
    else if (routePointer.routeCommand == 4)
        m_pendingRouteControl = 2;
    if (routePointer.selectedRouteNodeId != 0)
    {
        const auto* selectedNode =
            m_navigationWorkspace.routePlan().findById(
                routePointer.selectedRouteNodeId
            );
        if (selectedNode && !selectedNode->sourceObjectId.empty())
            m_objectOverlayState.activate(selectedNode->sourceObjectId);
    }
    if (routePointer.consumed)
    {
        if (game::system_map::GalaxyPointerDevScript::enabled() && leftDown &&
            m_modeState.current() == Mode::Galaxy)
            std::cerr << "[GalaxyPointerPress] consumed_by=route_overlay x=" << localMx << " y=" << localMy << std::endl;

        std::optional<game::system_map::MapIntent> routeFocusIntent;
        if (routePointer.focusRouteNodeId != 0)
        {
            routeFocusIntent = focusRouteWaypoint(
                routePointer.focusRouteNodeId,
                vp,
                galaxy,
                system,
                inputNowSeconds
            );
        }


        m_pendingScrollY = 0.0;
        m_galaxyView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );
        m_systemView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );
        if (routeFocusIntent.has_value())
            return routeFocusIntent;
        return std::nullopt;
    }

    // Map buttons (back, up, TRACK, LEVEL 0 - one row, the same in every
    // map) and the Galaxy / System breadcrumbs.
    const bool galaxyMode = m_modeState.current() == Mode::Galaxy;
    const bool hierarchyMode =
        galaxyMode ||
        (m_modeState.current() == Mode::System && m_systemView.state().navigationGrid.enabled());
    m_hoveredMapButton.clear();
    if (inside)
    {
        for (const auto& button : mapButtons(vp))
            if (button.view.enabled && button.view.bounds.contains(localMx, localMy))
                m_hoveredMapButton = button.id;
    }
    m_galaxyBreadcrumbHovered = -1;
    if (hierarchyMode && inside)
    {
        const auto crumbBounds =
            m_navigationCoordinateOverlay.breadcrumbBounds(vp, mapBreadcrumbs());
        // The last crumb is the current parent itself: not a command.
        for (std::size_t i = 0; i + 1 < crumbBounds.size(); ++i)
            if (crumbBounds[i].contains(localMx, localMy))
                m_galaxyBreadcrumbHovered = static_cast<int>(i);
    }
    const bool chromePressed =
        leftDown &&
        !m_navigationOverlayLeftWasDown;

    m_navigationOverlayLeftWasDown =
        leftDown;

    if (!m_hoveredMapButton.empty() || m_galaxyBreadcrumbHovered >= 0)
    {
        if (game::system_map::GalaxyPointerDevScript::enabled() && leftDown && galaxyMode)
            std::cerr << "[GalaxyPointerPress] consumed_by=nav_button x=" << localMx << " y=" << localMy << std::endl;

        m_pendingScrollY = 0.0;
        std::optional<game::system_map::MapIntent> buttonIntent;

        if (chromePressed && m_hoveredMapButton == "level0")
        {
            resetNavigationViewToLevelZero(vp);
        }
        else if (chromePressed && m_hoveredMapButton == "track")
        {
            toggleSelectedBodyTracking();
        }
        else if (chromePressed && m_hoveredMapButton == "back")
        {
            buttonIntent = navigateBack();
        }
        else if (chromePressed && galaxyMode)
        {
            // Up; breadcrumb i is the ancestor at level i - 1 (0 = overview).
            const bool moved =
                m_hoveredMapButton == "up"
                    ? m_galaxyView.ascendHierarchy(vp, inputNowSeconds)
                    : m_galaxyView.jumpToHierarchyLevel(
                          m_galaxyBreadcrumbHovered - 1, vp, inputNowSeconds);
            if (moved)
                announceNavigationLevel('G', m_galaxyView.state().navigationGrid.level());
        }
        else if (chromePressed && hierarchyMode)
        {
            // System: breadcrumb i is the parent at level i (0 = S0 root).
            const bool moved =
                m_hoveredMapButton == "up"
                    ? m_systemView.ascendHierarchy(vp, inputNowSeconds)
                    : m_systemView.jumpToHierarchyLevel(
                          m_galaxyBreadcrumbHovered, vp, inputNowSeconds);
            if (moved)
                announceNavigationLevel('S', m_systemView.state().navigationGrid.level());
        }

        /*
            The buttons are part of the map viewport, so explicitly prevent
            the scene behind them from receiving the same mouse gesture.
        */
        m_galaxyView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );
        m_systemView.suppressCameraGesture(
            leftDown,
            rightDown,
            mx,
            my
        );

        return buttonIntent;
    }

    if (m_modeState.current() == Mode::System)
    {
        m_systemPresentation =
            m_systemPresentationBuilder.build(
                m_systemView,
                vp,
                system,
                inputNowSeconds,
                false
            );

        updateSelectedBodyTracking(
            m_systemPresentation
        );

        m_systemSceneFrame =
            m_systemSceneFrameBuilder.build(
                m_systemView,
                *this,
                vp,
                system,
                m_systemPresentation
            );

        m_systemFramePrepared = true;
        m_systemSceneFrameDirty = false;

        const auto cameraBefore =
            m_systemView.state().camera;

        game::system_map::SystemMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.nowSeconds = inputNowSeconds;

        refreshSystemWaypointCandidate(vp, system);
        {
            // hover tooltip target: the nearest drawn object under the pointer
            std::string hovered;
            double best = 1.0e9;
            if (inside)
                for (const auto& item : m_systemSceneFrame.interaction.objectOverlay.items)
                {
                    if (!item.visible || !item.drawGlyph)
                        continue;
                    const double d = glm::length(item.screenPx - glm::dvec2(localMx, localMy));
                    if (d <= std::max(10.0, item.hitRadiusPx) && d < best)
                    {
                        best = d;
                        hovered = item.objectId;
                    }
                }
            if (!m_devHoverObjectId.empty())
                hovered = m_devHoverObjectId;
            m_objectOverlayState.setHovered(hovered);
        }
        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );
        revealPendingRouteFocus(
            m_systemSceneFrame.interaction.objectOverlay,
            vp
        );

        const game::system_map::SystemMapFrameInteractionContext
            interactionContext(
                m_systemSceneFrame.interaction,
                m_systemView.controls()
            );

        // DEV acceptance script hooks: the same selection path a click takes.
        if (!m_devFocusBodyId.empty())
        {
            m_systemInteraction.focusBodySelection(m_systemView, interactionContext, m_devFocusBodyId, inputNowSeconds);
            m_systemView.state().selectedBodyId = m_devFocusBodyId;
            m_objectOverlayState.clearActive();
            m_devFocusBodyId.clear();
        }
        if (!m_devEnterId.empty())
        {
            game::system_map::SystemPointerTarget target;
            if (const auto position = interactionContext.systemBodyAbsolutePosition(m_devEnterId))
            {
                target.kind = game::system_map::SystemPointerTarget::Kind::Body;
                target.bodyId = m_devEnterId;
                target.pointAu = *position / static_cast<double>(m_systemSceneFrame.systemScale);
            }
            else if (const auto hubPosition = interactionContext.systemObjectAbsolutePosition(m_devEnterId))
            {
                target.kind = game::system_map::SystemPointerTarget::Kind::Hub;
                target.hubId = m_devEnterId;
                target.pointAu = *hubPosition / static_cast<double>(m_systemSceneFrame.systemScale);
            }
            game::system_map::SystemMapInputResult ignored;
            if (target.kind != game::system_map::SystemPointerTarget::Kind::Empty)
                m_systemInteraction.descendTarget(m_systemView, interactionContext, vp, target, inputNowSeconds, ignored);
            m_devEnterId.clear();
        }
        if (!m_listFocusObjectId.empty())
        {
            auto& items = m_systemSceneFrame.interaction.objectOverlay.items;
            const auto item = std::find_if(items.begin(), items.end(), [&](const auto& i) { return i.objectId == m_listFocusObjectId; });
            const auto position = interactionContext.systemObjectAbsolutePosition(m_listFocusObjectId);
            if (item != items.end() && position)
            {
                game::system_map::SystemMapHubSelection hubSelection;
                hubSelection.hubId = item->objectId;
                hubSelection.parentBodyId = item->navigationHubParentBodyId;
                for (const auto& point : m_systemSceneFrame.interaction.hubScreenPoints)
                    if (point.hubId == item->objectId)
                        hubSelection.parentBodyId = point.parentBodyId;
                // selectedHubId = this object: the declutter / beacon filter always keep its marker
                m_systemInteraction.focusHubSelection(m_systemView, interactionContext, hubSelection, inputNowSeconds);
                m_objectOverlayState.activate(item->objectId);
                updateActiveTacticalLocalContext(*item);

                // camera: centre on the object; frame its planet's neighbourhood (moons and
                // Lagrange hubs of the same planet) - a heliocentric object keeps the zoom
                float distance = m_systemView.state().camera.distance;
                const std::string& parent = hubSelection.parentBodyId;
                std::size_t dot = parent.find('.');
                dot = dot == std::string::npos ? dot : parent.find('.', dot + 1);
                dot = dot == std::string::npos ? dot : parent.find('.', dot + 1);
                const std::string planet = dot == std::string::npos ? parent : parent.substr(0, dot);
                const auto& bodies = m_systemSceneFrame.interaction.bodyAbsolutePositionById;
                if (const auto p = bodies.find(planet); !planet.empty() && p != bodies.end())
                {
                    double extent = glm::length(*position - p->second);
                    for (const auto& [id, pos] : bodies)
                        if (id.rfind(planet + ".", 0) == 0)
                            extent = std::max(extent, std::min(glm::length(pos - p->second), 40.0 * std::max(extent, 1.0e-9)));
                    distance = std::min(distance, static_cast<float>(std::max(extent * 2.5, 1.0e-6)));
                }
                m_systemView.beginCameraFlight(*position, distance, inputNowSeconds);
                m_systemSceneFrameDirty = true;
                m_listFocusObjectId.clear();
            }
            else if (--m_listFocusFramesLeft <= 0)
            {
                std::cerr << "[SystemMap] list object not on the map: " << m_listFocusObjectId << std::endl;
                m_listFocusObjectId.clear();
            }
        }
        if (!m_devFocusHubId.empty())
        {
            for (const auto& point : m_systemSceneFrame.interaction.hubScreenPoints)
                if (point.hubId == m_devFocusHubId)
                {
                    game::system_map::SystemMapHubSelection hubSelection;
                    hubSelection.hubId = point.hubId;
                    hubSelection.parentBodyId = point.parentBodyId;
                    m_systemInteraction.focusHubSelection(m_systemView, interactionContext, hubSelection, inputNowSeconds);
                }
            m_objectOverlayState.activate(m_devFocusHubId);
            m_devFocusHubId.clear();
        }

        const auto overlayPointer =
            m_objectOverlayState.handlePointer(
                m_systemSceneFrame.interaction.objectOverlay,
                glm::dvec2(
                    static_cast<double>(vp.width),
                    static_cast<double>(vp.height)
                ),
                glm::dvec2(localMx, localMy),
                inside,
                leftDown,
                interactionContext
                    .largestDirectBodyPhysicalSizeMetersAt(
                        localMx,
                        localMy
                    ),
                currentTimeSeconds()
            );
        // A celestial card never outlives the selection of its body.
        m_objectOverlayState.closeCelestialCardsNotSelected(
            m_systemSceneFrame.interaction.objectOverlay,
            m_systemView.state().selectedBodyId);

        if (leftDown && overlayPointer.consumed)
        {
            std::cerr
                << "[DockUi] pointer-consumed"
                << " action_object=" << overlayPointer.actionObjectId
                << " action_key=" << overlayPointer.actionKey
                << " activated_object=" << overlayPointer.activatedObjectId
                << " closed_object=" << overlayPointer.closedObjectId
                << " mouse_x=" << localMx
                << " mouse_y=" << localMy
                << std::endl;
        }

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.actionObjectId.empty())
                applyWaypointAction(overlayPointer.actionObjectId, overlayPointer.actionKey);

            if (!overlayPointer.activatedObjectId.empty())
            {
                if (overlayPointer.activatedInfoKind ==
                        game::system_map::MapObjectInfoKind::Celestial &&
                    !overlayPointer.activatedSemanticTargetId.empty())
                {
                    m_systemInteraction.focusBodySelection(
                        m_systemView,
                        interactionContext,
                        overlayPointer.activatedSemanticTargetId,
                        inputNowSeconds
                    );
                }
                else if (overlayPointer.activatedInfoKind ==
                         game::system_map::MapObjectInfoKind::Tactical)
                {
                    const auto overlayItem = std::find_if(
                        m_systemSceneFrame.interaction.objectOverlay.items.begin(),
                        m_systemSceneFrame.interaction.objectOverlay.items.end(),
                        [&](const auto& item)
                        {
                            return item.objectId ==
                                overlayPointer.activatedObjectId;
                        }
                    );

                    const auto hubPoint = std::find_if(
                        m_systemSceneFrame.interaction.hubScreenPoints.begin(),
                        m_systemSceneFrame.interaction.hubScreenPoints.end(),
                        [&](const auto& point)
                        {
                            return point.hubId == overlayPointer.activatedObjectId;
                        }
                    );

                    if (hubPoint !=
                        m_systemSceneFrame.interaction.hubScreenPoints.end())
                    {
                        game::system_map::SystemMapHubSelection hubSelection;
                        hubSelection.hubId = hubPoint->hubId;
                        hubSelection.parentBodyId = hubPoint->parentBodyId;
                        m_systemInteraction.focusHubSelection(
                            m_systemView,
                            interactionContext,
                            hubSelection,
                            inputNowSeconds
                        );
                        if (overlayItem !=
                            m_systemSceneFrame.interaction.objectOverlay.items.end())
                        {
                            updateActiveTacticalLocalContext(*overlayItem);
                        }
                    }
                    else if (overlayItem !=
                             m_systemSceneFrame.interaction.objectOverlay.items.end())
                    {
                        m_systemInteraction.focusTacticalObjectSelection(
                            m_systemView,
                            overlayItem->navigationHubId,
                            overlayItem->navigationHubParentBodyId
                        );
                        updateActiveTacticalLocalContext(*overlayItem);
                    }
                    else
                    {
                        m_systemInteraction.focusTacticalObjectSelection(
                            m_systemView
                        );
                        m_activeTacticalLocalTargetObjectId.clear();
                        m_activeTacticalDetailCell.reset();
                    }
                }
            }

            synchronizeNavigationTracking(
                m_systemSceneFrame.interaction.objectOverlay
            );

            m_systemView.suppressCameraGesture(
                leftDown,
                rightDown,
                mx,
                my
            );
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        const auto result =
            m_systemInteraction.handleInput(
                m_systemView,
                interactionContext,
                frame,
                m_pendingScrollY
            );

        if (result.clickedNavigationCell.has_value())
        {
            const auto& cell = *result.clickedNavigationCell;
            m_systemWaypointCandidate = makeWaypointCandidate(
                'S',
                cell,
                system.systemPositionLy * world::coordinates::MetersPerLightYear +
                    cell.center * world::celestial::MetersPerAu,
                glm::dvec2(localMx, localMy)
            );
            refreshSystemWaypointCandidate(vp, system);
        }

        synchronizeNavigationTracking(
            m_systemSceneFrame.interaction.objectOverlay
        );

        const auto& semanticSelection = m_systemView.state();
        if (!semanticSelection.selectedHubId.empty())
        {
            // A tactical ship may intentionally retain its parent Hub only as
            // the local-neighborhood drill target. Do not replace the active
            // ship with that Hub on the following frame.
            const bool tacticalObjectOwnsHubContext =
                !m_activeTacticalLocalTargetObjectId.empty() &&
                m_objectOverlayState.activeObjectId() ==
                    m_activeTacticalLocalTargetObjectId;
            if (!tacticalObjectOwnsHubContext)
            {
                m_objectOverlayState.activate(
                    semanticSelection.selectedHubId
                );
            }
        }
        else if (!semanticSelection.selectedBodyId.empty() ||
                 semanticSelection.navigationCellExplicitlySelected)
        {
            m_objectOverlayState.clearActive();
        }

        if (m_systemView.state().navigationGrid.enabled() &&
            m_systemPresentation.systemScale > 0.0f)
        {
            m_systemView.updateNavigationHoverPresentation(
                vp,
                inputNowSeconds
            );
        }

        const auto& cameraAfter =
            m_systemView.state().camera;

        m_systemSceneFrameDirty =
            glm::length(
                cameraBefore.target - cameraAfter.target
            ) > 0.0 ||
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.distance != cameraAfter.distance;

        if (result.systemLevelChanged.has_value())
        {
            announceNavigationLevel(
                'S',
                result.systemLevelChanged.value()
            );
        }

        return std::nullopt;
    }

    if (m_modeState.current() == Mode::Detail ||
        m_modeState.current() == Mode::Hub)
    {
        if (m_modeState.current() == Mode::Detail)
        {
            m_detailPresentation =
                m_localMapPresentationBuilder.buildDetail(
                    m_detailView,
                    vp,
                    detail
                );
            m_detailFramePrepared = true;
            m_detailFrameDirty = false;
        }
        else
        {
            m_hubPresentation =
                m_localMapPresentationBuilder.buildHub(
                    m_hubView,
                    vp,
                    hub
                );
            decorateHubDockingOverlay(hub);
            m_hubFramePrepared = true;
            m_hubFrameDirty = false;
        }

        const auto cameraBefore =
            m_modeState.current() == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        auto& objectOverlay =
            m_modeState.current() == Mode::Hub
                ? m_hubPresentation.frame.objectOverlay
                : m_detailPresentation.frame.objectOverlay;
        synchronizeNavigationTracking(objectOverlay);
        revealPendingRouteFocus(objectOverlay, vp);

        const auto overlayPointer =
            m_objectOverlayState.handlePointer(
                objectOverlay,
                glm::dvec2(
                    static_cast<double>(vp.width),
                    static_cast<double>(vp.height)
                ),
                glm::dvec2(localMx, localMy),
                inside,
                leftDown,
                0.0,
                currentTimeSeconds()
            );

        // Hub infrastructure is not selected through a screen-space radius or
        // convex proxy.  Dock markers/cards get first refusal above; only an
        // otherwise-unconsumed press is tested against actual assembly
        // triangles.  Empty space remains available to the orbit gesture.
        if (m_modeState.current() == Mode::Hub &&
            overlayPointer.primaryPressStarted &&
            !overlayPointer.consumed)
        {
            const glm::dvec2 mousePx(localMx, localMy);
            const auto* body = pickHubInfrastructureBody(
                m_hubPresentation.camera,
                hub,
                mousePx
            );

            if (body && !body->stableId.empty())
            {
                const std::string objectId =
                    std::string(HubModuleOverlayPrefix) + body->stableId;
                const auto item = std::find_if(
                    objectOverlay.items.begin(),
                    objectOverlay.items.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.objectId == objectId;
                    }
                );

                if (item != objectOverlay.items.end())
                {
                    m_objectOverlayState.activate(objectId);
                    m_objectOverlayState.ensureOpen(
                        *item,
                        glm::dvec2(
                            static_cast<double>(vp.width),
                            static_cast<double>(vp.height)
                        )
                    );

                    m_hubFrameDirty = true;
                    auto& camera = m_hubView.camera();
                    camera.rotating = false;
                    camera.panning = false;
                    camera.lastMouseX = mx;
                    camera.lastMouseY = my;
                    m_pendingScrollY = 0.0;
                    return std::nullopt;
                }
            }
            else if (!m_objectOverlayState.activeObjectId().empty())
            {
                // Empty Hub-map press only deselects. Information-card
                // lifetime is independent from selection and is controlled
                // exclusively by the card X button. The same press remains
                // available to start orbit/pan.
                m_objectOverlayState.clearActive();
                m_hubFrameDirty = true;
            }
        }

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.closedObjectId.empty())
                cancelDockingTaskForClosedCard(overlayPointer.closedObjectId);

            if (!overlayPointer.actionObjectId.empty())
            {
                if (overlayPointer.actionKey == "show_docking_route" ||
                    overlayPointer.actionKey == "start_docking")
                {
                    applyDockingAction(
                        overlayPointer.actionObjectId,
                        overlayPointer.actionKey
                    );
                }
                else
                {
                    applyWaypointAction(
                        overlayPointer.actionObjectId,
                        overlayPointer.actionKey
                    );
                }
            }

            if (m_modeState.current() == Mode::Detail &&
                !overlayPointer.activatedObjectId.empty())
            {
                const auto item = std::find_if(
                    objectOverlay.items.begin(),
                    objectOverlay.items.end(),
                    [&](const auto& candidate)
                    {
                        return candidate.objectId ==
                            overlayPointer.activatedObjectId;
                    }
                );

                if (item != objectOverlay.items.end() &&
                    !item->navigationHubId.empty())
                {
                    m_detailView.selectHub(
                        item->navigationHubId,
                        item->navigationHubParentBodyId
                    );
                }
                else
                {
                    m_detailView.clearHubSelection();
                }

                // Double click on a hub = go to the hub (its Hub map).
                if (item != objectOverlay.items.end() &&
                    overlayPointer.doubleClickedObjectId == item->objectId &&
                    !item->navigationHubId.empty())
                {
                    auto& camera = m_detailView.camera();
                    camera.rotating = false;
                    camera.panning = false;
                    camera.lastMouseX = mx;
                    camera.lastMouseY = my;
                    m_pendingScrollY = 0.0;
                    return game::system_map::MapIntent::openHub(
                        item->navigationHubId,
                        item->navigationHubParentBodyId);
                }
            }

            if (m_modeState.current() == Mode::Hub &&
                !overlayPointer.closedObjectId.empty())
            {
                m_hubFrameDirty = true;
            }

            auto& camera =
                m_modeState.current() == Mode::Hub
                    ? m_hubView.camera()
                    : m_detailView.camera();
            camera.rotating = false;
            camera.panning = false;
            camera.lastMouseX = mx;
            camera.lastMouseY = my;
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        handleDetailAndHubInput(
            vp,
            window,
            mx,
            my,
            localMx,
            localMy,
            inside,
            leftDown,
            rightDown
        );

        const auto& cameraAfter =
            m_modeState.current() == Mode::Hub
                ? m_hubView.camera()
                : m_detailView.camera();

        const bool projectionChanged =
            cameraBefore.yaw != cameraAfter.yaw ||
            cameraBefore.pitch != cameraAfter.pitch ||
            cameraBefore.zoom != cameraAfter.zoom ||
            glm::length(
                cameraBefore.pan - cameraAfter.pan
            ) > 0.0;

        if (m_modeState.current() == Mode::Detail)
            m_detailFrameDirty = projectionChanged;
        else
            m_hubFrameDirty = projectionChanged;

        return std::nullopt;
    }

    if (m_modeState.current() == Mode::Galaxy)
    {
        refreshGalaxyWaypointCandidate(vp, galaxy);
        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
        revealPendingRouteFocus(m_galaxyInfoOverlayFrame, vp);

        game::system_map::GalaxyMapInputFrame frame;
        frame.viewport = vp;
        frame.mouseX = mx;
        frame.mouseY = my;
        frame.localMouseX = localMx;
        frame.localMouseY = localMy;
        frame.inside = inside;
        frame.leftDown = leftDown;
        frame.rightDown = rightDown;
        frame.zoomInKeyDown =
            glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        frame.zoomOutKeyDown =
            glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        frame.transitionActive = m_mapTransition.active();
        frame.nowSeconds = inputNowSeconds;
        frame.altDown =
            glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS ||
            (game::system_map::GalaxyHierarchyDevScript::enabled() &&
             m_galaxyHierarchyDevScript.altDown());

        /*
            ONE Galaxy pointer resolver: the overlay's interactive glyphs and
            cards are candidates of the same resolver as stars, cube markers
            and clouds. A glyph that presents a navigation cube (the waypoint
            candidate / waypoint of the selected cube) carries that cube as
            its semantic target.
        */
        frame.panelUnderCursor =
            inside &&
            m_objectOverlayState.panelUnderCursor(
                glm::dvec2(localMx, localMy)
            );
        {
            const auto& grid = m_galaxyView.state().navigationGrid;
            const std::string selectedCellGlyphId =
                grid.hasSelectedCell()
                    ? waypointCandidateId('G', grid.selectedCell())
                    : std::string();
            for (const auto& item : m_galaxyInfoOverlayFrame.items)
            {
                if (!item.visible || !item.pointerInteractive || item.objectId.empty())
                    continue;
                game::system_map::GalaxyPointerOverlayGlyph glyph;
                glyph.objectId = item.objectId;
                glyph.screen = glm::vec2(item.screenPx);
                glyph.hitRadiusPx = static_cast<float>(item.hitRadiusPx);
                if (!selectedCellGlyphId.empty() && item.objectId == selectedCellGlyphId)
                    glyph.semanticCell = grid.selectedCell();
                frame.overlayGlyphs.push_back(std::move(glyph));
            }
        }

        // A fresh left press is resolved ONCE; the overlay receives it only
        // when the resolver gives it to a card or an overlay glyph.
        bool overlayOwnsPress = true;
        if (m_galaxyView.state().whatIsHere.open || frame.altDown)
        {
            overlayOwnsPress = false;
        }
        else if (inside &&
            leftDown &&
            !m_galaxyView.state().pointerGesture.leftWasDown())
        {
            const auto pressTarget =
                m_galaxyInteraction.resolvePressTarget(
                    m_galaxyView,
                    galaxy,
                    frame
                );
            frame.pressTargetResolved = true;
            overlayOwnsPress =
                pressTarget.kind ==
                    game::system_map::GalaxyPointerTargetKind::Panel ||
                pressTarget.kind ==
                    game::system_map::GalaxyPointerTargetKind::OverlayGlyph;
            if (overlayOwnsPress)
                m_galaxyInteraction.traceOverlayOwnedPress(m_galaxyView, galaxy, frame);
            if (game::system_map::GalaxyPointerDevScript::enabled())
                std::cerr << "[GalaxyPointerPress] target="
                          << game::system_map::galaxyPointerTargetKindName(pressTarget.kind) << "/"
                          << game::system_map::galaxyPointerHitReasonName(pressTarget.reason)
                          << " overlay_owns=" << overlayOwnsPress << " x=" << localMx << " y=" << localMy
                          << std::endl;
        }
        else if (game::system_map::GalaxyPointerDevScript::enabled() && inside && leftDown &&
                 !m_galaxyView.state().pointerGesture.leftWasDown())
        {
            std::cerr << "[GalaxyPointerPress] unresolved" << std::endl;
        }

        // An open "What is here?" list owns the pointer (drawn above cards).
        const bool whatIsHereOpen = m_galaxyView.state().whatIsHere.open;
        const auto overlayPointer =
            whatIsHereOpen
                ? game::system_map::MapObjectOverlayPointerResult {}
                : m_objectOverlayState.handlePointer(
                      m_galaxyInfoOverlayFrame,
                      glm::dvec2(vp.width, vp.height),
                      glm::dvec2(localMx, localMy),
                      inside,
                      leftDown,
                      0.0,
                      -1.0,
                      overlayOwnsPress
                  );

        if (overlayPointer.consumed)
        {
            if (!overlayPointer.actionObjectId.empty())
                applyWaypointAction(overlayPointer.actionObjectId, overlayPointer.actionKey);

            synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);
            m_galaxyView.suppressCameraGesture(
                leftDown,
                rightDown,
                mx,
                my
            );
            m_pendingScrollY = 0.0;
            return std::nullopt;
        }

        const auto result =
            m_galaxyInteraction.handleInput(
                m_galaxyView,
                galaxy,
                frame,
                m_pendingScrollY
            );

        if (result.clickedNavigationCell.has_value() &&
            m_galaxyView.state().selectedSystemId < 0)
        {
            const auto& cell = *result.clickedNavigationCell;
            m_galaxyWaypointCandidate = makeWaypointCandidate(
                'G',
                cell,
                cell.center * world::coordinates::MetersPerLightYear,
                glm::dvec2(localMx, localMy)
            );
            refreshGalaxyWaypointCandidate(vp, galaxy);
        }

        synchronizeNavigationTracking(m_galaxyInfoOverlayFrame);

        if (result.requestWindowFocus)
            glfwFocusWindow(window);

        if (result.galaxyLevelChanged.has_value())
        {
            announceNavigationLevel(
                'G',
                result.galaxyLevelChanged.value()
            );
        }

        if (result.mapIntent.has_value())
        {
            const auto type =
                result.mapIntent->type;

            if (type ==
                    game::system_map::MapIntentType::
                        EnterKnownSystem ||
                type ==
                    game::system_map::MapIntentType::
                        EnterEmptySector)
            {
                announceNavigationLevel(
                    'S',
                    m_systemView.state().navigationGrid
                        .definition()
                        .minimumLevel
                );
            }
        }

        return result.mapIntent;
    }

    return std::nullopt;

}


bool SystemMapRenderer::moveSystemCubeSelection(int dx, int dy, double nowSeconds)
{
    auto& st = m_systemView.state();
    if (m_modeState.current() != Mode::System || !st.gridOverlayVisible || !st.navigationGrid.hasSelectedCell() || (dx == 0 && dy == 0))
        return false;
    constexpr double ArrowPlaneLockSeconds = 1.5;
    if (nowSeconds - st.arrowLastPressSeconds > ArrowPlaneLockSeconds)
    {
        // The plane facing the camera: drop the world axis closest to the view direction,
        // then map screen right / up onto the two remaining axes.
        const glm::dvec3 fwd = m_systemSceneFrame.camera.basis.direction;
        const glm::dvec3 right = m_systemSceneFrame.camera.basis.right;
        const glm::dvec3 up = m_systemSceneFrame.camera.basis.up;
        int depth = 0;
        for (int a = 1; a < 3; ++a)
            if (std::abs(fwd[a]) > std::abs(fwd[depth]))
                depth = a;
        int axes[2];
        int k = 0;
        for (int a = 0; a < 3; ++a)
            if (a != depth)
                axes[k++] = a;
        const bool firstIsRight = std::abs(right[axes[0]]) * std::abs(up[axes[1]]) >= std::abs(right[axes[1]]) * std::abs(up[axes[0]]);
        st.arrowAxisRight = firstIsRight ? axes[0] : axes[1];
        st.arrowAxisUp = firstIsRight ? axes[1] : axes[0];
        st.arrowSignRight = right[st.arrowAxisRight] >= 0.0 ? 1 : -1;
        st.arrowSignUp = up[st.arrowAxisUp] >= 0.0 ? 1 : -1;
    }
    st.arrowLastPressSeconds = nowSeconds;
    const auto cell = st.navigationGrid.selectedCell();
    glm::dvec3 step(0.0);
    step[st.arrowAxisRight] += static_cast<double>(dx * st.arrowSignRight);
    step[st.arrowAxisUp] += static_cast<double>(dy * st.arrowSignUp);
    const auto next = m_systemView.hierarchyChildAt(cell.center + step * cell.size);
    if (!next || (next->index == cell.index && next->level == cell.level))
        return false;      // the selection stays a sibling of the current parent
    st.selectedBodyId.clear();
    st.selectedHubId.clear();
    st.selectedHubParentBodyId.clear();
    st.navigationGrid.selectCell(*next);
    st.navigationCellExplicitlySelected = true;
    m_systemSceneFrameDirty = true;
    return true;
}


// ============================================================================
// DEV scripted pointer (ELITE_DEV_SOL_MAP_SCRIPT "pointer:<spec>")
// ============================================================================

bool SystemMapRenderer::driveDevPointer(
    const Viewport& vp,
    double& mx,
    double& my,
    bool& leftDown,
    bool& rightDown,
    const world::celestial::GalaxyMapSnapshot* galaxy
)
{
    if (m_devPointerSpec.empty())
        return false;
    const auto colon = m_devPointerSpec.find(':');
    const std::string verb = m_devPointerSpec.substr(0, colon);
    const std::string arg = colon == std::string::npos ? std::string() : m_devPointerSpec.substr(colon + 1);
    const int frame = m_devPointerFrame++;

    glm::dvec2 at(-1.0);
    if (verb == "menu" && frame > 0)
    {
        at = m_devPointerAt; // the press closes the menu: keep the row position
    }
    else if (verb == "menu")
    {
        if (!m_contextMenu.open)
        {
            std::cerr << "[DevPointer] menu not open for " << arg << std::endl;
            m_devPointerSpec.clear();
            return false;
        }
        const glm::vec4 b = m_contextMenu.bounds();
        for (std::size_t i = 0; i < m_contextMenu.rows.size(); ++i)
            if (m_contextMenu.rows[i].command == arg)
                at = glm::dvec2(b.x + game::system_map::MapContextMenu::WidthPx * 0.5,
                                b.y + game::system_map::MapContextMenu::HeaderHeightPx + 3.0 +
                                    game::system_map::MapContextMenu::RowHeightPx * (static_cast<double>(i) + 0.5));
    }
    else if (m_modeState.current() == Mode::Galaxy && galaxy && arg.rfind("galaxy-system:", 0) == 0)
    {
        // a star of the Galaxy map (not an overlay item): its projected position
        const int id = std::atoi(arg.c_str() + 14);
        for (const auto& star : galaxy->systems)
            if (star.id == id)
            {
                const glm::mat4 mvp = m_galaxyView.projectionMatrix(vp) * m_galaxyView.viewMatrix();
                const glm::vec4 clip = mvp * glm::vec4(m_galaxyView.positionLyToRender(star.positionLy), 1.0f);
                if (clip.w > 0.0f)
                    at = glm::dvec2((clip.x / clip.w * 0.5f + 0.5f) * vp.width, (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * vp.height);
            }
    }
    else
    {
        const game::system_map::MapObjectOverlayFrame* overlay =
            m_modeState.current() == Mode::Detail ? &m_detailPresentation.frame.objectOverlay
            : m_modeState.current() == Mode::Hub  ? &m_hubPresentation.frame.objectOverlay
                                                  : &m_systemSceneFrame.interaction.objectOverlay;
        for (const auto& item : overlay->items)
            if (item.objectId == arg && item.visible)
                at = item.screenPx;
    }
    if (at.x < 0.0)
    {
        std::cerr << "[DevPointer] target not on the map: " << m_devPointerSpec << std::endl;
        m_devPointerSpec.clear();
        return false;
    }

    // frame plan: 0 move, 1-2 press, 3 release (dclick: 4-5 press, 6 release)
    bool left = false, right = false, done = false;
    if (verb == "click" || verb == "menu")
    {
        left = frame == 1 || frame == 2;
        done = frame >= 3;
    }
    else if (verb == "dclick")
    {
        left = frame == 1 || frame == 2 || frame == 4 || frame == 5;
        done = frame >= 6;
    }
    else if (verb == "rclick")
    {
        right = frame == 1 || frame == 2;
        done = frame >= 3;
    }
    else
    {
        done = true;
    }
    mx = static_cast<double>(vp.x) + at.x;
    my = static_cast<double>(vp.y) + at.y;
    leftDown = left;
    rightDown = right;
    m_devPointerAt = at;
    if (frame == 0)
        std::cerr << "[DevPointer] " << m_devPointerSpec << " at " << at.x << "," << at.y << std::endl;
    if (done)
        m_devPointerSpec.clear();
    return true;
}

std::string SystemMapRenderer::devOverlayReport() const
{
    const game::system_map::MapObjectOverlayFrame* overlay =
        m_modeState.current() == Mode::Detail ? &m_detailPresentation.frame.objectOverlay
        : m_modeState.current() == Mode::Hub  ? &m_hubPresentation.frame.objectOverlay
                                              : &m_systemSceneFrame.interaction.objectOverlay;
    std::ostringstream out;
    out << "mode " << static_cast<int>(m_modeState.current()) << " active '" << m_objectOverlayState.activeObjectId()
        << "' detailHub '" << m_detailView.state().selectedHubId << "' items:";
    for (const auto& item : overlay->items)
        if (!item.visible)
            out << " [" << item.objectId << " | hidden]";
        else
            out << " [" << item.objectId << " | " << item.name << " | hub=" << item.navigationHubId
                << (item.selectOnClick ? " | sel" : "") << (m_objectOverlayState.isOpen(item.objectId) ? " | CARD" : "")
                << " @" << static_cast<int>(item.screenPx.x) << "," << static_cast<int>(item.screenPx.y) << "]";
    return out.str();
}

bool SystemMapRenderer::activeTacticalObjectContext(
    std::string& hubId,
    glm::dvec3& worldMeters
) const
{
    const std::string& active = m_objectOverlayState.activeObjectId();
    if (active.empty() ||
        (m_modeState.current() != Mode::System && m_modeState.current() != Mode::Detail))
        return false;
    const auto& overlay = m_modeState.current() == Mode::Detail
        ? m_detailPresentation.frame.objectOverlay
        : m_systemSceneFrame.interaction.objectOverlay;
    for (const auto& item : overlay.items)
    {
        if (item.objectId != active)
            continue;
        if (item.infoKind != game::system_map::MapObjectInfoKind::Tactical || !item.hasTrackingWorldPosition)
            return false;
        hubId = item.navigationHubId;
        // System items carry their system-local AU (their tracking position
        // includes the galactic offset); Detail items track system meters.
        worldMeters = item.hasNavigationSystemPositionAu
            ? item.navigationSystemPositionAu * world::celestial::MetersPerAu
            : world::coordinates::fullMeters(item.trackingWorldPosition);
        return true;
    }
    return false;
}
