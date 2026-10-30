// SystemMapRenderer: navigation overlays: hub docking cards / actions, active guidance trajectory, navigation tracking, galaxy / system waypoints and route focus.

#include "src/game/system_map/SystemMapRendererInternal.h"


void SystemMapRenderer::decorateHubDockingOverlay(
    const world::celestial::HubMapSnapshot& hub
)
{
    if (!hub.valid || !m_hubSemanticAnchors)
        return;

    auto& frame = m_hubPresentation.frame.objectOverlay;

    // Module cards expose how many authored docking ports they own. The module
    // itself remains selectable even when it has zero ports.
    for (auto& item : frame.items)
    {
        if (item.infoKind != game::system_map::MapObjectInfoKind::Infrastructure ||
            item.semanticTargetId.empty())
        {
            continue;
        }

        int dockingPortCount = 0;
        for (const auto& anchor :
             m_hubSemanticAnchors->anchorsForModule(item.semanticTargetId))
        {
            if (anchor.enabled &&
                anchor.kind == game::navigation::HubSemanticAnchorKind::DockingPort)
            {
                ++dockingPortCount;
            }
        }

        game::system_map::MapObjectInfoField docks;
        docks.labelKey = "docking_ports";
        docks.value = std::to_string(dockingPortCount);
        item.extraFields.push_back(std::move(docks));
    }

    // Resolve the local player's logical docking envelope once per ship type.
    // LogicalDimensions use the canonical ship basis:
    // width=left/right, height=belly/top, length=nose/tail.
    for (const auto& object : hub.scene.objects)
    {
        if (!object.valid ||
            object.objectClass != world::celestial::DetailObjectClass::Ship ||
            !object.player)
        {
            continue;
        }

        if (object.typeId != m_cachedDockingEnvelopeShipType)
        {
            m_cachedDockingEnvelopeShipType = object.typeId;
            m_cachedDockingEnvelope = {};
            try
            {
                const auto& descriptor = ShipDescriptorRegistry::get(object.typeId);
                const auto& dimensions = descriptor.logicalDimensions();
                if (dimensions.enabled)
                {
                    m_cachedDockingEnvelope.lengthMeters = dimensions.length;
                    m_cachedDockingEnvelope.widthMeters = dimensions.width;
                    m_cachedDockingEnvelope.heightMeters = dimensions.height;
                    m_cachedDockingEnvelope.valid =
                        dimensions.length > 0.0f &&
                        dimensions.width > 0.0f &&
                        dimensions.height > 0.0f;
                }
            }
            catch (...)
            {
                m_cachedDockingEnvelope = {};
            }
        }
        break;
    }

    const double finalScale =
        m_hubPresentation.scale * m_hubPresentation.camera.state.zoom;

    const glm::vec4 good(0.42f, 1.00f, 0.58f, 1.00f);
    const glm::vec4 warn(1.00f, 0.78f, 0.30f, 1.00f);
    const glm::vec4 bad(1.00f, 0.34f, 0.30f, 1.00f);

    const auto runtimeText = [&](game::navigation::DockingOperationalState state)
    {
        switch (state)
        {
            case game::navigation::DockingOperationalState::Online:
                return m_navigationMapTextProfile.statusOnline;
            case game::navigation::DockingOperationalState::Offline:
                return m_navigationMapTextProfile.statusOffline;
            case game::navigation::DockingOperationalState::Damaged:
                return m_navigationMapTextProfile.statusDamaged;
            case game::navigation::DockingOperationalState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };
    const auto occupancyText = [&](game::navigation::DockingOccupancyState state)
    {
        switch (state)
        {
            case game::navigation::DockingOccupancyState::Free:
                return m_navigationMapTextProfile.statusFree;
            case game::navigation::DockingOccupancyState::Occupied:
                return m_navigationMapTextProfile.statusOccupied;
            case game::navigation::DockingOccupancyState::Reserved:
                return m_navigationMapTextProfile.statusReserved;
            case game::navigation::DockingOccupancyState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };
    const auto accessText = [&](game::navigation::DockingAccessState state)
    {
        switch (state)
        {
            case game::navigation::DockingAccessState::Allowed:
                return m_navigationMapTextProfile.statusAllowed;
            case game::navigation::DockingAccessState::ClearanceRequired:
                return m_navigationMapTextProfile.statusClearanceRequired;
            case game::navigation::DockingAccessState::Denied:
                return m_navigationMapTextProfile.statusDenied;
            case game::navigation::DockingAccessState::Unknown:
                return m_navigationMapTextProfile.statusUnknown;
        }
        return m_navigationMapTextProfile.statusUnknown;
    };

    // Docking ports are presentation/interaction objects in their own right.
    // Do not make the player select the much larger parent module first: every
    // authored enabled port is projected each Hub frame and receives first
    // refusal inside the exact projected opening rectangle.
    for (const auto& module : hub.scene.objects)
    {
        if (!module.valid ||
            module.objectClass != world::celestial::DetailObjectClass::Hub ||
            module.stableId.empty())
        {
            continue;
        }

        const auto& anchors =
            m_hubSemanticAnchors->anchorsForModule(module.stableId);
        if (anchors.empty())
            continue;

        const glm::dmat3 moduleBasis(
            module.axes.x,
            module.axes.y,
            module.axes.z
        );

        for (const auto& anchor : anchors)
        {
            if (!anchor.enabled ||
                anchor.kind != game::navigation::HubSemanticAnchorKind::DockingPort)
            {
                continue;
            }

            game::navigation::DockingPortRuntimeState unavailableState;
            unavailableState.hubModuleId = module.stableId;
            unavailableState.anchorId = anchor.id;
            const auto* runtime = m_dockingPortRuntimeStates
                ? m_dockingPortRuntimeStates->find(module.stableId, anchor.id)
                : nullptr;
            const auto& effectiveRuntime = runtime ? *runtime : unavailableState;

            const auto compatibility = game::navigation::evaluateDockingCompatibility(
                m_cachedDockingEnvelope,
                anchor,
                effectiveRuntime
            );

            const glm::dvec3 localPosition =
                module.positionMeters + moduleBasis * anchor.localPositionMeters;

            game::system_map::MapObjectOverlayItem item;
            item.objectId = hubDockOverlayId(module.stableId, anchor.id);
            item.semanticTargetId = module.stableId;
            item.semanticAnchorId = anchor.id;
            item.trackingSystemId = hub.systemId;
            item.name = anchor.displayName.empty() ? anchor.id : anchor.displayName;
            item.typeName = "Docking port";
            item.kind = game::system_map::MapObjectGlyphKind::DockingPort;
            item.infoKind = game::system_map::MapObjectInfoKind::DockingPort;
            item.navigationHubId = hub.hubId;
            item.navigationHubParentBodyId = hub.parentBodyId;
            item.trackingWorldPosition = world::coordinates::makeWorldPositionFromMeters(
                hubLocalToWorldMetersForOverlay(hub, localPosition)
            );
            item.hasTrackingWorldPosition = true;
            item.factionColor = compatibility.routeAvailable
                ? glm::vec4(0.42f, 0.96f, 0.72f, 0.94f)
                : glm::vec4(0.96f, 0.68f, 0.30f, 0.92f);
            item.screenPx = m_hubPresentation.camera.project(localPosition);
            const glm::dvec2 hubViewportSize =
                m_hubPresentation.centerPx * 2.0;
            item.visible =
                item.screenPx.x >= -20.0 &&
                item.screenPx.y >= -20.0 &&
                item.screenPx.x <= hubViewportSize.x + 20.0 &&
                item.screenPx.y <= hubViewportSize.y + 20.0;
            item.physicalSizeMeters = std::max(
                1.0,
                std::max(anchor.extentMeters.x, anchor.extentMeters.y)
            );
            item.glyphScale = game::system_map::mapObjectGlyphScale(
                item.physicalSizeMeters,
                finalScale
            );
            // Docking ports are actionable semantic subtargets and must win
            // over their parent module/assembly even when the physical opening
            // is sub-pixel at distant zoom.
            item.hitRadiusPx = 22.0;
            item.pickPriority = 1000;
            item.pointerInteractive = true;
            item.drawGlyph = true;

            const glm::dvec3 forwardModule = glm::normalize(
                moduleBasis * anchor.localForward
            );
            const glm::dvec3 upModule = glm::normalize(
                moduleBasis * anchor.localUp
            );
            glm::dvec3 rightModule = glm::cross(forwardModule, upModule);
            if (glm::length(rightModule) > 1.0e-9)
            {
                rightModule = glm::normalize(rightModule);
                const double halfW = std::max(0.5, anchor.extentMeters.x * 0.5);
                const double halfH = std::max(0.5, anchor.extentMeters.y * 0.5);
                const glm::dvec3 corners[4] = {
                    localPosition - rightModule * halfW - upModule * halfH,
                    localPosition + rightModule * halfW - upModule * halfH,
                    localPosition + rightModule * halfW + upModule * halfH,
                    localPosition - rightModule * halfW + upModule * halfH
                };
                item.hitPolygonPx.reserve(4);
                for (const auto& corner : corners)
                    item.hitPolygonPx.push_back(m_hubPresentation.camera.project(corner));
            }

            auto addField = [&](
                const std::string& key,
                std::string value,
                const glm::vec4* color = nullptr,
                std::string unit = {})
            {
                game::system_map::MapObjectInfoField field;
                field.labelKey = key;
                field.value = std::move(value);
                field.unit = std::move(unit);
                if (color)
                {
                    field.valueColor = *color;
                    field.hasValueColor = true;
                }
                item.extraFields.push_back(std::move(field));
            };

            addField(
                "dock_opening",
                formatDockPair(
                    compatibility.openingWidthMeters,
                    compatibility.openingHeightMeters
                ),
                nullptr,
                "m"
            );
            addField(
                "ship_envelope",
                m_cachedDockingEnvelope.valid
                    ? formatDockPair(
                        m_cachedDockingEnvelope.widthMeters,
                        m_cachedDockingEnvelope.heightMeters
                      )
                    : "—",
                nullptr,
                m_cachedDockingEnvelope.valid ? "m" : ""
            );
            addField(
                "dock_clearance",
                formatDockScalar(compatibility.requiredClearanceMeters),
                nullptr,
                "m"
            );
            addField(
                "dock_fit",
                compatibility.geometryFits
                    ? m_navigationMapTextProfile.statusAvailable
                    : m_navigationMapTextProfile.statusUnavailable,
                compatibility.geometryFits ? &good : &bad
            );
            addField(
                "dock_operational",
                runtimeText(effectiveRuntime.operational),
                compatibility.operational ? &good : &bad
            );
            addField(
                "dock_status",
                occupancyText(effectiveRuntime.occupancy),
                compatibility.free
                    ? &good
                    : effectiveRuntime.occupancy ==
                        game::navigation::DockingOccupancyState::Reserved
                        ? &warn
                        : &bad
            );
            addField(
                "dock_access",
                accessText(effectiveRuntime.access),
                compatibility.accessAllowed
                    ? &good
                    : effectiveRuntime.access ==
                        game::navigation::DockingAccessState::ClearanceRequired
                        ? &warn
                        : &bad
            );
            addField(
                "dock_max_entry_speed",
                formatDockScalar(anchor.maxEntrySpeedMps),
                nullptr,
                "m/s"
            );

            game::system_map::MapObjectPanelAction calculate;
            calculate.key = "show_docking_route";
            calculate.labelKey = "show_docking_route";
            calculate.enabled = compatibility.routeAvailable;
            const auto& pending =
                m_navigationWorkspace.dockingRouteRequests().pending();
            const auto target = routeTargetRefForOverlayItem(item);
            calculate.active = pending.valid() &&
                pending.mode == game::navigation::DockingRouteRequest::Mode::Guidance &&
                game::navigation::sameRouteTarget(pending.target, target);
            item.panelActions.push_back(std::move(calculate));
            game::system_map::MapObjectPanelAction automatic;
            automatic.key = "start_docking";
            automatic.labelKey = "start_docking";
            automatic.enabled = compatibility.routeAvailable;
            automatic.active = pending.valid() &&
                pending.mode ==
                    game::navigation::DockingRouteRequest::Mode::Automatic &&
                game::navigation::sameRouteTarget(
                    pending.target,
                    target
                );
            item.panelActions.push_back(std::move(automatic));

            frame.items.push_back(std::move(item));
        }
    }
}

void SystemMapRenderer::applyDockingAction(
    const std::string& objectId,
    const std::string& actionKey
)
{
    if (actionKey != "show_docking_route" &&
        actionKey != "start_docking")
    {
        return;
    }

    std::cerr << "[DockRequest] ui-action=" << actionKey
              << " object=" << objectId << std::endl;

    const auto* item = currentOverlayItem(objectId);
    if (!item ||
        item->infoKind != game::system_map::MapObjectInfoKind::DockingPort)
    {
        std::cerr << "[DockRequest] ui-reject reason=port-unavailable"
                  << std::endl;
        return;
    }

    const auto target = routeTargetRefForOverlayItem(*item);
    if (!target.valid() ||
        target.kind != game::navigation::NavigationRouteAnchorKind::SemanticAnchor)
    {
        std::cerr << "[DockRequest] ui-reject reason=invalid-target"
                  << std::endl;
        return;
    }

    const auto requestMode =
        actionKey == "start_docking"
            ? game::navigation::DockingRouteRequest::Mode::Automatic
            : game::navigation::DockingRouteRequest::Mode::Guidance;

    const auto serial =
        m_navigationWorkspace.dockingRouteRequests().request(
            target,
            requestMode
        );
    if (serial != 0)
    {
        std::cerr << "[DockRequest] ui serial=" << serial
                  << " mode="
                  << (requestMode == game::navigation::DockingRouteRequest::Mode::Automatic
                          ? "automatic" : "guidance")
                  << " system=" << target.systemId
                  << " module=" << target.stableObjectId
                  << " anchor=" << target.semanticAnchorId
                  << std::endl;
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::RoutePlanning,
            true
        );

        // Route presentation and execution are both client-owned. Guidance
        // and Automatic use the same local RoutePlan and visible corridor;
        // the server receives only ordinary ShipControlState samples.
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::LocalGuidance,
            true
        );
        m_navigationWorkspace.modules().setEnabled(
            game::navigation::NavigationModuleId::HudGuidanceCorridor,
            true
        );
    }
}


void SystemMapRenderer::cancelDockingTaskForClosedCard(
    const std::string& objectId
)
{
    std::string moduleId;
    std::string anchorId;
    if (!parseHubDockOverlayId(objectId, moduleId, anchorId))
        return;

    const auto& pending =
        m_navigationWorkspace.dockingRouteRequests().pending();
    if (!pending.valid())
        return;

    if (pending.target.stableObjectId == moduleId &&
        pending.target.semanticAnchorId == anchorId)
    {
        std::cerr << "[DockRequest] ui-clear serial=" << pending.serial
                  << " reason=card-closed" << std::endl;
        // Docking guidance is intentionally card-scoped. Closing the dock
        // information card means the pilot has cancelled that advisory task;
        // SpaceState drops the corridor on the next update and tracking is
        // reconciled from the remaining open cards immediately.
        m_navigationWorkspace.dockingRouteRequests().clear();
        const auto id = "dock:" + moduleId + ":" + anchorId;
        m_navigationWorkspace.guidance().erase(id);
        m_navigationWorkspace.guidance().erase(id + ":frames");
    }
}

void SystemMapRenderer::decorateActiveGuidanceTrajectory(
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::DetailMapSnapshot& detail,
    const world::celestial::HubMapSnapshot& hub
)
{
    game::system_map::MapObjectOverlayFrame* overlay = nullptr;
    int systemId = -1;
    double universeTimeSeconds = 0.0;

    switch (m_modeState.current())
    {
        case Mode::System:
            overlay = &m_systemSceneFrame.interaction.objectOverlay;
            systemId = system.systemId;
            universeTimeSeconds = system.universeTimeSeconds;
            break;
        case Mode::Detail:
            overlay = &m_detailPresentation.frame.objectOverlay;
            systemId = detail.systemId;
            universeTimeSeconds = detail.universeTimeSeconds;
            break;
        case Mode::Hub:
            overlay = &m_hubPresentation.frame.objectOverlay;
            systemId = hub.systemId;
            universeTimeSeconds = hub.universeTimeSeconds;
            break;
        case Mode::Galaxy:
        default:
            return;
    }

    if (!overlay)
        return;

    // Rebuild only this local docking producer's rows. Other history,
    // prediction or long-range producers own their own trajectory entries and
    // must survive this presentation pass.
    overlay->trajectories.erase(
        std::remove_if(
            overlay->trajectories.begin(),
            overlay->trajectories.end(),
            [](const game::system_map::MapObjectTrajectory& trajectory)
            {
                return trajectory.objectId.rfind("dock:", 0) == 0;
            }
        ),
        overlay->trajectories.end()
    );

    const auto& guidance = m_navigationWorkspace.guidance();
    const auto* corridor = guidance.activePredictive(
        systemId,
        universeTimeSeconds,
        &m_navigationWorkspace.modules()
    );
    const auto& dockRequest = m_navigationWorkspace.dockingRouteRequests().pending();
    if (dockRequest.valid() &&
        dockRequest.target.systemId == systemId)
    {
        const std::string dockId = "dock:" + dockRequest.target.stableObjectId +
            ":" + dockRequest.target.semanticAnchorId;
        const auto it = std::find_if(
            guidance.corridors().begin(),guidance.corridors().end(),
            [&](const auto& candidate)
            {
                return candidate.id == dockId &&
                    candidate.validAt(universeTimeSeconds) &&
                    game::navigation::NavigationGuidanceState::sourceEnabled(
                        candidate.source,m_navigationWorkspace.modules());
            });
        if (it != guidance.corridors().end()) corridor = &*it;
    }
    if (!corridor || corridor->frames.size() < 2)
        return;

    game::system_map::MapObjectTrajectory trajectory;
    trajectory.objectId = corridor->id;
    trajectory.kind = game::system_map::MapTrajectoryKind::Planned;
    trajectory.noSafePrimarySolution = corridor->noSafePrimarySolution;
    trajectory.points.reserve(corridor->frames.size());

    const glm::dvec3 systemOriginMeters =
        system.systemPositionLy * world::coordinates::MetersPerLightYear;

    game::navigation::HubCoMovingFrameSeed hubFrameSeed;
    if (m_modeState.current() == Mode::Hub)
    {
        hubFrameSeed = game::navigation::makeHubCoMovingFrameSeed(
            hub.systemId,
            hub.hubId,
            hub.universeTimeSeconds,
            hub.hubWorldPositionMeters,
            hub.hubWorldVelocityMps,
            hub.parentPlanetWorldPositionMeters,
            hub.parentPlanetWorldVelocityMps,
            hub.hubWorldAxes.x,
            hub.hubWorldAxes.y,
            hub.hubWorldAxes.z
        );
    }

    const auto projectWorldPoint = [&]
    (
        const glm::dvec3& worldMeters,
        double sampleUniverseTimeSeconds,
        glm::dvec2& screenPx
    ) -> bool
    {
        if (m_modeState.current() == Mode::System)
        {
            const glm::dvec3 systemRelativeAu =
                (worldMeters - systemOriginMeters) /
                world::celestial::MetersPerAu;
            const glm::dvec3 absoluteMapUnits =
                systemRelativeAu *
                static_cast<double>(m_systemSceneFrame.systemScale);
            const glm::vec3 renderPosition = glm::vec3(
                absoluteMapUnits - m_systemSceneFrame.cameraOrigin
            );
            const glm::vec4 clip =
                m_systemSceneFrame.mvp * glm::vec4(renderPosition, 1.0f);
            if (clip.w <= 1.0e-6f)
                return false;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) ||
                !std::isfinite(ndc.z) || ndc.z < -1.2f || ndc.z > 1.2f)
            {
                return false;
            }

            screenPx = glm::dvec2(
                (ndc.x * 0.5 + 0.5) * viewport.width,
                (1.0 - (ndc.y * 0.5 + 0.5)) * viewport.height
            );
            return true;
        }

        if (m_modeState.current() == Mode::Detail)
        {
            glm::dvec3 detailPoint = worldMeters;
            if (detail.scene.coordinateSpace ==
                world::celestial::LocalSceneCoordinateSpace::AnchorLocalMeters)
            {
                detailPoint -= detail.scene.originWorldMeters;
            }
            screenPx = m_detailPresentation.camera.project(detailPoint);
            return std::isfinite(screenPx.x) && std::isfinite(screenPx.y);
        }

        if (m_modeState.current() == Mode::Hub)
        {
            if (!hubFrameSeed.valid)
                return false;

            // The Hub map is a co-moving rotating coordinate domain. Project
            // each future sample against the Hub frame at that same future
            // time. Subtracting hub NOW from ship FUTURE is what produced the
            // previous hundreds-of-kilometres tangent/"laser".
            const auto futureFrame =
                game::navigation::predictHubCoMovingFrameAt(
                    hubFrameSeed,
                    sampleUniverseTimeSeconds
                );
            if (!futureFrame.valid)
                return false;

            const glm::dvec3 local =
                futureFrame.worldToLocalPosition(worldMeters);
            screenPx = m_hubPresentation.camera.project(local);
            return std::isfinite(screenPx.x) && std::isfinite(screenPx.y);
        }

        return false;
    };

    // The map draws the full route while the cockpit may omit passed spatial
    // gates. Keep every supplied point, including the route start.
    const bool localDockRoute =
        m_modeState.current() == Mode::Hub &&
        corridor->source == game::navigation::GuidanceSource::DockingComputer &&
        corridor->hubLocalFrameId == hub.hubId &&
        corridor->hubLocalGatePositionsMeters.size() ==
            corridor->frames.size();
    for (std::size_t index = 0; index < corridor->frames.size(); ++index)
    {
        const auto& guidanceFrame = corridor->frames[index];
        game::system_map::MapTrajectoryPoint point;
        point.universeTimeSeconds = guidanceFrame.universeTimeSeconds;
        point.position = guidanceFrame.centerMeters;
        if (localDockRoute)
        {
            // Fixed docking gates and the Hub camera share a tactical local
            // frame. Do not leave it for world space just to re-enter it
            // through a second Hub orbit/epoch predictor.
            point.screenPx = m_hubPresentation.camera.project(
                corridor->hubLocalGatePositionsMeters[index]
            );
            point.screenProjected =
                std::isfinite(point.screenPx.x) &&
                std::isfinite(point.screenPx.y);
        }
        else
        {
            point.screenProjected = projectWorldPoint(
                point.position,
                point.universeTimeSeconds,
                point.screenPx
            );
        }

        if (point.screenProjected)
            trajectory.points.push_back(std::move(point));
    }

    trajectory.terminalPositionErrorMeters =
        corridor->terminalPositionErrorMeters;
    if (corridor->hasTerminalTarget)
    {
        const double terminalTime = corridor->frames.empty()
            ? universeTimeSeconds
            : corridor->frames.back().universeTimeSeconds;
        trajectory.terminalTargetProjected = projectWorldPoint(
            corridor->terminalTargetMeters,
            terminalTime,
            trajectory.terminalTargetScreenPx
        );
    }

    if (trajectory.points.size() >= 2)
        overlay->trajectories.push_back(std::move(trajectory));
}


void SystemMapRenderer::synchronizeNavigationTracking(
    game::system_map::MapObjectOverlayFrame& frame
)
{
    auto& targets = m_navigationWorkspace.targets();
    auto& routePlan = m_navigationWorkspace.routePlan();
    const auto openIds = m_objectOverlayState.openObjectIds();
    targets.reconcileOpenCards(openIds);
    routePlan.pruneTransientCandidates(openIds);

    const auto routeActionsFor =
        [&](const game::system_map::MapObjectOverlayItem& item)
        {
            std::vector<game::system_map::MapObjectPanelAction> actions;
            if (item.objectId == "player" || !item.hasTrackingWorldPosition)
                return actions;

            const auto target = routeTargetRefForOverlayItem(item);
            if (!target.valid())
                return actions;

            const auto* route = routePlan.findByTarget(target);
            const bool isWaypoint =
                route && route->role ==
                    game::navigation::NavigationWaypointRole::Intermediate;
            const bool isFinish =
                route && route->role ==
                    game::navigation::NavigationWaypointRole::Finish;

            if (!isFinish)
            {
                game::system_map::MapObjectPanelAction waypointAction;
                waypointAction.key = "toggle_intermediate";
                waypointAction.labelKey =
                    isWaypoint
                        ? "cancel_waypoint"
                        : item.kind == game::system_map::MapObjectGlyphKind::Ship
                            ? "set_rendezvous"
                            : "set_waypoint";
                waypointAction.active = isWaypoint;
                actions.push_back(std::move(waypointAction));
            }

            if (isFinish || !routePlan.hasFinishWaypoint())
            {
                game::system_map::MapObjectPanelAction finishAction;
                finishAction.key = "toggle_finish";
                finishAction.labelKey =
                    isFinish ? "cancel_finish" : "set_finish";
                finishAction.active = isFinish;
                actions.push_back(std::move(finishAction));
            }
            return actions;
        };

    for (auto& item : frame.items)
    {
        if (item.hasTrackingWorldPosition)
        {
            const auto target = routeTargetRefForOverlayItem(item);
            if (target.valid())
            {
                if (auto* route = routePlan.findByTarget(target);
                    route && route->role !=
                        game::navigation::NavigationWaypointRole::None)
                {
                    routePlan.bindPresentationSource(route->id, item.objectId);
                    route->worldPosition = item.trackingWorldPosition;
                    if (!item.name.empty())
                        route->displayName = item.name;

                    item.routeDisplayIndex =
                        route->role == game::navigation::NavigationWaypointRole::Finish
                            ? static_cast<int>(routePlan.routeSize())
                            : route->sequence;
                }
            }
        }

        if (item.infoKind == game::system_map::MapObjectInfoKind::Tactical ||
            item.infoKind == game::system_map::MapObjectInfoKind::Celestial)
        {
            item.panelActions = routeActionsFor(item);
        }

        if (!m_objectOverlayState.isOpen(item.objectId))
            continue;

        if (item.infoKind == game::system_map::MapObjectInfoKind::Tactical)
        {
            const bool numberedShipTarget =
                item.kind == game::system_map::MapObjectGlyphKind::Ship &&
                item.objectId != "player";
            targets.rememberTacticalObject(
                item.objectId,
                item.typeName,
                item.name,
                item.factionColor,
                numberedShipTarget,
                numberedShipTarget
                    ? m_objectOverlayState.shipTargetNumberFor(item.objectId)
                    : 0
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::Celestial &&
            item.hasTrackingWorldPosition)
        {
            targets.rememberCelestialBody(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.typeName,
                item.name,
                item.trackingWorldPosition,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::Infrastructure &&
            !item.semanticTargetId.empty())
        {
            targets.rememberInfrastructure(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.typeName,
                item.name,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::DockingPort &&
            !item.semanticTargetId.empty() &&
            !item.semanticAnchorId.empty())
        {
            targets.rememberSemanticAnchor(
                item.objectId,
                item.trackingSystemId,
                item.semanticTargetId,
                item.semanticAnchorId,
                item.typeName,
                item.name,
                item.factionColor
            );
        }
        else if (
            item.infoKind == game::system_map::MapObjectInfoKind::WaypointCandidate &&
            item.hasTrackingWorldPosition)
        {
            std::string address;
            for (const auto& field : item.extraFields)
            {
                if (field.labelKey == "address")
                {
                    address = field.value;
                    break;
                }
            }
            const auto target = routeTargetRefForOverlayItem(item);
            routePlan.rememberCandidate(
                target,
                item.objectId,
                item.trackingWorldPosition,
                std::move(address),
                item.name.empty() ? m_navigationMapTextProfile.spaceTarget : item.name
            );
        }
    }
}


void SystemMapRenderer::refreshGalaxyWaypointCandidate(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy
)
{
    m_galaxyInfoOverlayFrame.items.clear();
    m_galaxyInfoOverlayFrame.trajectories.clear();

    const glm::mat4 mvp =
        m_galaxyView.projectionMatrix(viewport) *
        m_galaxyView.viewMatrix();

    for (const auto& waypoint : m_navigationWorkspace.routePlan().waypoints())
    {
        if (waypoint.role == game::navigation::NavigationWaypointRole::None ||
            !waypointHasPrefix(waypoint, 'G'))
            continue;

        game::system_map::MapObjectOverlayItem item;
        item.objectId = waypoint.sourceObjectId;
        item.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
        item.typeName = waypointRoleTypeName(waypoint.role, m_navigationMapTextProfile);
        item.name = waypoint.address.empty() ? waypoint.displayName : waypoint.address;
        item.drawGlyph = true;
        item.pointerInteractive = true;
        item.screenAffordance = true;
        item.glyphScale = 0.78;
        item.hitRadiusPx = 12.0;
        item.facingScreenDirection = glm::dvec2(0.0, -1.0);
        item.factionColor = waypointRoleColor(waypoint.role);
        item.routeDisplayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint.sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), waypoint);
        item.extraFields.push_back({"address", waypoint.address, ""});
        item.trackingWorldPosition = waypoint.worldPosition;
        item.hasTrackingWorldPosition = true;

        const glm::vec3 renderPosition =
            m_galaxyView.positionLyToRender(
                world::coordinates::toGalacticLy(waypoint.worldPosition)
            );
        bool visible = false;
        float depth = 1.0f;
        item.screenPx = glm::dvec2(
            projectToScreen(renderPosition, mvp, viewport, visible, depth)
        ) + glm::dvec2(20.0, -2.0);
        item.visible = visible;
        m_galaxyInfoOverlayFrame.items.push_back(std::move(item));
    }

    // Selected interstellar cloud / icy field: its info card (one card,
    // closed as soon as the selection is dropped - pointer contract).
    {
        const int selected = m_galaxyView.state().selectedMediumObject;
        std::string cardId;
        if (selected >= 0 && selected < static_cast<int>(galaxy.objects.size()))
        {
            const auto& object = galaxy.objects[static_cast<std::size_t>(selected)];
            game::system_map::MapObjectOverlayItem item;
            item.objectId = "medium:" + object.id;
            item.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
            item.name = object.name;
            item.typeName = object.objectType == "icy_debris_field" ? "Icy debris field" : "Interstellar cloud";
            for (const auto& [label, value] : object.mediumInfo)
                item.extraFields.push_back({label, value, ""});
            item.drawGlyph = false;
            item.pointerInteractive = false;
            bool visible = false;
            float depth = 1.0f;
            item.screenPx = glm::dvec2(projectToScreen(
                m_galaxyView.positionLyToRender(m_galaxyView.state().selectedMediumHitLy), mvp, viewport, visible,
                depth));
            item.visible = true;
            cardId = item.objectId;
            if (m_galaxyMediumCardId != cardId)
            {
                if (!m_galaxyMediumCardId.empty())
                    m_objectOverlayState.close(m_galaxyMediumCardId);
                m_objectOverlayState.ensureOpen(item, glm::dvec2(viewport.width, viewport.height));
            }
            m_galaxyInfoOverlayFrame.items.push_back(std::move(item));
        }
        if (cardId.empty() && !m_galaxyMediumCardId.empty())
            m_objectOverlayState.close(m_galaxyMediumCardId);
        m_galaxyMediumCardId = cardId;
    }

    if (!m_galaxyWaypointCandidate.has_value() ||
        m_galaxyView.state().selectedSystemId >= 0)
    {
        return;
    }

    auto item = *m_galaxyWaypointCandidate;
    const auto& grid = m_galaxyView.state().navigationGrid;
    if (!grid.hasSelectedCell())
        return;

    const auto& cell = grid.selectedCell();
    if (item.objectId != waypointCandidateId('G', cell))
        return;

    const glm::vec3 renderPosition =
        m_galaxyView.positionLyToRender(cell.center);
    bool visible = false;
    float depth = 1.0f;
    item.screenPx = glm::dvec2(
        projectToScreen(renderPosition, mvp, viewport, visible, depth)
    ) + glm::dvec2(20.0, -2.0);
    item.visible = visible;
    item.drawGlyph = true;
    item.pointerInteractive = true;
    item.screenAffordance = true;
    item.facingScreenDirection = glm::dvec2(0.0, -1.0);
    item.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    const auto* waypoint = m_navigationWorkspace.routePlan().findByTarget(
        routeTargetRefForOverlayItem(item)
    );
    if (waypoint)
    {
        item.typeName = waypointRoleTypeName(waypoint->role, m_navigationMapTextProfile);
        item.name = waypoint->address.empty() ? waypoint->displayName : waypoint->address;
        item.factionColor = waypointRoleColor(waypoint->role);
        item.routeDisplayIndex =
            waypoint->role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint->sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), *waypoint);
        item.extraFields.clear();
        item.extraFields.push_back({"address", waypoint->address, ""});
    }
    m_galaxyWaypointCandidate = item;

    const auto duplicate = std::find_if(
        m_galaxyInfoOverlayFrame.items.begin(),
        m_galaxyInfoOverlayFrame.items.end(),
        [&](const game::system_map::MapObjectOverlayItem& existing)
        {
            return existing.objectId == item.objectId;
        }
    );
    if (duplicate == m_galaxyInfoOverlayFrame.items.end())
        m_galaxyInfoOverlayFrame.items.push_back(std::move(item));
}

void SystemMapRenderer::refreshSystemWaypointCandidate(
    const Viewport& viewport,
    const world::celestial::SystemMapSnapshot& system
)
{
    auto& items = m_systemSceneFrame.interaction.objectOverlay.items;
    items.erase(
        std::remove_if(
            items.begin(),
            items.end(),
            [](const game::system_map::MapObjectOverlayItem& candidate)
            {
                return candidate.infoKind ==
                    game::system_map::MapObjectInfoKind::WaypointCandidate;
            }
        ),
        items.end()
    );

    for (const auto& waypoint : m_navigationWorkspace.routePlan().waypoints())
    {
        if (waypoint.role == game::navigation::NavigationWaypointRole::None ||
            !waypointHasPrefix(waypoint, 'S'))
            continue;

        game::system_map::MapObjectOverlayItem item;
        item.objectId = waypoint.sourceObjectId;
        item.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
        item.typeName = waypointRoleTypeName(waypoint.role, m_navigationMapTextProfile);
        item.name = waypoint.address.empty() ? waypoint.displayName : waypoint.address;
        item.drawGlyph = true;
        item.pointerInteractive = true;
        item.screenAffordance = true;
        item.glyphScale = 0.78;
        item.hitRadiusPx = 12.0;
        item.facingScreenDirection = glm::dvec2(0.0, -1.0);
        item.factionColor = waypointRoleColor(waypoint.role);
        item.routeDisplayIndex =
            waypoint.role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint.sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), waypoint);
        item.extraFields.push_back({"address", waypoint.address, ""});
        item.trackingWorldPosition = waypoint.worldPosition;
        item.hasTrackingWorldPosition = true;

        const glm::dvec3 relativeMeters =
            world::coordinates::fullMeters(waypoint.worldPosition) -
            (system.systemPositionLy * world::coordinates::MetersPerLightYear);
        const glm::dvec3 relativeAu =
            relativeMeters / world::celestial::MetersPerAu;
        const glm::dvec3 absoluteMap =
            relativeAu * static_cast<double>(m_systemSceneFrame.systemScale);
        const glm::vec3 renderPosition =
            m_systemSceneFrame.camera.relativePosition(absoluteMap);

        bool visible = false;
        float depth = 1.0f;
        item.screenPx = glm::dvec2(
            projectToScreen(
                renderPosition,
                m_systemSceneFrame.mvp,
                viewport,
                visible,
                depth
            )
        ) + glm::dvec2(20.0, -2.0);
        item.visible = visible;
        items.push_back(std::move(item));
    }

    if (!m_systemWaypointCandidate.has_value() ||
        !m_systemView.state().navigationGrid.hasSelectedCell())
    {
        return;
    }

    const auto& cell = m_systemView.state().navigationGrid.selectedCell();
    if (m_systemWaypointCandidate->objectId != waypointCandidateId('S', cell))
        return;

    auto item = *m_systemWaypointCandidate;
    const glm::dvec3 absoluteMap =
        cell.center * static_cast<double>(m_systemSceneFrame.systemScale);
    const glm::vec3 renderPosition =
        m_systemSceneFrame.camera.relativePosition(absoluteMap);

    bool visible = false;
    float depth = 1.0f;
    item.screenPx = glm::dvec2(
        projectToScreen(
            renderPosition,
            m_systemSceneFrame.mvp,
            viewport,
            visible,
            depth
        )
    ) + glm::dvec2(20.0, -2.0);
    item.visible = visible;
    item.drawGlyph = true;
    item.pointerInteractive = true;
    item.screenAffordance = true;
    item.facingScreenDirection = glm::dvec2(0.0, -1.0);
    item.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    const auto* waypoint = m_navigationWorkspace.routePlan().findByTarget(
        routeTargetRefForOverlayItem(item)
    );
    if (waypoint)
    {
        item.typeName = waypointRoleTypeName(waypoint->role, m_navigationMapTextProfile);
        item.name = waypoint->address.empty() ? waypoint->displayName : waypoint->address;
        item.factionColor = waypointRoleColor(waypoint->role);
        item.routeDisplayIndex =
            waypoint->role == game::navigation::NavigationWaypointRole::Finish
                ? static_cast<int>(m_navigationWorkspace.routePlan().routeSize())
                : waypoint->sequence;
        item.panelActions = waypointPanelActions(m_navigationWorkspace.routePlan(), *waypoint);
        item.extraFields.clear();
        item.extraFields.push_back({"address", waypoint->address, ""});
    }
    m_systemWaypointCandidate = item;

    const auto duplicate = std::find_if(
        items.begin(),
        items.end(),
        [&](const game::system_map::MapObjectOverlayItem& existing)
        {
            return existing.objectId == item.objectId;
        }
    );
    if (duplicate == items.end())
        items.push_back(std::move(item));
}

const game::system_map::MapObjectOverlayItem*
SystemMapRenderer::currentOverlayItem(const std::string& objectId) const
{
    const game::system_map::MapObjectOverlayFrame* frame = nullptr;
    if (m_modeState.current() == Mode::Galaxy)
        frame = &m_galaxyInfoOverlayFrame;
    else if (m_modeState.current() == Mode::System)
        frame = &m_systemSceneFrame.interaction.objectOverlay;
    else if (m_modeState.current() == Mode::Detail)
        frame = &m_detailPresentation.frame.objectOverlay;
    else if (m_modeState.current() == Mode::Hub)
        frame = &m_hubPresentation.frame.objectOverlay;

    if (!frame)
        return nullptr;
    const auto found = std::find_if(
        frame->items.begin(),
        frame->items.end(),
        [&](const game::system_map::MapObjectOverlayItem& item)
        {
            return item.objectId == objectId;
        }
    );
    return found == frame->items.end() ? nullptr : &(*found);
}

void SystemMapRenderer::applyWaypointAction(
    const std::string& objectId,
    const std::string& actionKey
)
{
    auto& routePlan = m_navigationWorkspace.routePlan();
    const auto* item = currentOverlayItem(objectId);
    game::navigation::NavigationWaypoint* waypoint = nullptr;

    if (item && item->hasTrackingWorldPosition)
    {
        const auto target = routeTargetRefForOverlayItem(*item);
        if (!target.valid())
            return;

        std::string address;
        for (const auto& field : item->extraFields)
        {
            if (field.labelKey == "address")
            {
                address = field.value;
                break;
            }
        }

        waypoint = &routePlan.rememberCandidate(
            target,
            item->objectId,
            item->trackingWorldPosition,
            std::move(address),
            item->name.empty() ? item->typeName : item->name
        );

        using Context = game::navigation::NavigationRouteMapKind;
        Context context = Context::System;
        int systemId = item->trackingSystemId;
        std::string bodyId;
        std::string hubId = item->navigationHubId;
        if (m_modeState.current() == Mode::Galaxy)
        {
            context = Context::Galaxy;
        }
        else if (m_modeState.current() == Mode::System)
        {
            context = Context::System;
            systemId = m_systemPresentation.systemId;
            bodyId = m_systemView.state().selectedBodyId;
        }
        else if (m_modeState.current() == Mode::Detail)
        {
            context = Context::Detail;
            systemId = m_detailPresentation.systemId;
            bodyId = m_systemView.state().selectedBodyId;
        }
        else if (m_modeState.current() == Mode::Hub)
        {
            context = Context::Hub;
            systemId = m_hubPresentation.systemId;
            bodyId = m_systemView.state().selectedHubParentBodyId;
            if (hubId.empty())
                hubId = m_hubPresentation.hubId;
        }

        waypoint->authoredMap = context;
        waypoint->authoredSystemId = systemId;
        waypoint->authoredBodyId = std::move(bodyId);
        waypoint->authoredHubId = std::move(hubId);
        waypoint->dynamicTarget =
            target.kind == game::navigation::NavigationRouteAnchorKind::Ship ||
            target.kind == game::navigation::NavigationRouteAnchorKind::Hub;
        waypoint->transitKind =
            target.kind == game::navigation::NavigationRouteAnchorKind::Ship
                ? game::navigation::NavigationWaypointTransitKind::Rendezvous
                : game::navigation::NavigationWaypointTransitKind::PassThrough;
    }
    else
    {
        waypoint = routePlan.findBySourceObjectId(objectId);
    }

    if (!waypoint)
        return;

    if (actionKey == "toggle_finish")
    {
        routePlan.toggleRole(
            waypoint->id,
            game::navigation::NavigationWaypointRole::Finish
        );
    }
    else if (actionKey == "toggle_intermediate")
    {
        routePlan.toggleRole(
            waypoint->id,
            game::navigation::NavigationWaypointRole::Intermediate
        );
    }
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::focusRouteWaypoint(
    std::uint64_t routeNodeId,
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    double nowSeconds
)
{
    const auto* waypoint =
        m_navigationWorkspace.routePlan().findById(routeNodeId);
    if (!waypoint ||
        waypoint->role == game::navigation::NavigationWaypointRole::None)
    {
        return std::nullopt;
    }

    m_pendingRouteFocusNodeId = routeNodeId;
    m_pendingRouteFocusContextApplied = false;
    return advancePendingRouteFocus(
        viewport,
        galaxy,
        system,
        nowSeconds
    );
}

std::optional<game::system_map::MapIntent>
SystemMapRenderer::advancePendingRouteFocus(
    const Viewport& viewport,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    double nowSeconds
)
{
    if (m_pendingRouteFocusNodeId == 0)
        return std::nullopt;

    const auto* waypoint = m_navigationWorkspace.routePlan().findById(
        m_pendingRouteFocusNodeId
    );
    if (!waypoint ||
        waypoint->role == game::navigation::NavigationWaypointRole::None)
    {
        m_pendingRouteFocusNodeId = 0;
        m_pendingRouteFocusContextApplied = false;
        return std::nullopt;
    }

    if (m_pendingRouteFocusContextApplied)
        return std::nullopt;

    using Context = game::navigation::NavigationRouteMapKind;
    using game::system_map::MapIntent;

    // The route remembers where the node was authored.  The renderer may
    // prepare selection/camera state, but changing map mode stays a SpaceState
    // responsibility and therefore leaves this class as a MapIntent.
    if (waypoint->authoredMap == Context::Galaxy)
    {
        if (m_modeState.current() != Mode::Galaxy)
            return MapIntent::recallRouteMap(Mode::Galaxy);

        const glm::dvec3 targetLy =
            world::coordinates::toGalacticLy(waypoint->worldPosition);
        // The waypoint's cube inside the current parent (the frame moves
        // to the same-level cube holding it when it lies outside).
        m_galaxyView.focusHierarchyPoint(targetLy, true, nowSeconds);
        m_pendingRouteFocusContextApplied = true;
        (void)viewport;
        return std::nullopt;
    }

    // Known systems retain stable ids.  Empty-space System maps use ephemeral
    // negative ids, so recall them by their spatial anchor instead.
    if (waypoint->authoredSystemId >= 0 &&
        system.systemId != waypoint->authoredSystemId)
    {
        const auto found = std::find_if(
            galaxy.systems.begin(),
            galaxy.systems.end(),
            [&](const auto& candidate)
            {
                return candidate.id == waypoint->authoredSystemId;
            }
        );
        if (found != galaxy.systems.end())
        {
            return MapIntent::enterKnownSystem(
                found->id,
                found->positionLy
            );
        }
        return std::nullopt;
    }

    if (waypoint->authoredSystemId < 0)
    {
        const glm::dvec3 targetLy =
            world::coordinates::toGalacticLy(waypoint->worldPosition);
        const bool wrongEmptyContext =
            system.systemId >= 0 ||
            glm::length(system.systemPositionLy - targetLy) > 1.0e-9;
        if (wrongEmptyContext)
            return MapIntent::enterEmptySector(targetLy);
    }

    if (waypoint->authoredMap == Context::System)
    {
        if (m_modeState.current() != Mode::System)
            return MapIntent::recallRouteMap(Mode::System);

        const glm::dvec3 relativeMeters =
            world::coordinates::fullMeters(waypoint->worldPosition) -
            system.systemPositionLy * world::coordinates::MetersPerLightYear;
        const glm::dvec3 relativeAu =
            relativeMeters / world::celestial::MetersPerAu;
        m_systemView.state().navigationGrid.setAnchorFromPosition(relativeAu);
        const double scale = std::max(
            1.0e-12,
            static_cast<double>(m_systemView.state().lastScale)
        );
        m_systemView.beginCameraFlight(
            relativeAu * scale,
            m_systemView.state().camera.distance,
            nowSeconds
        );
        m_pendingRouteFocusContextApplied = true;
        return std::nullopt;
    }

    if (waypoint->authoredMap == Context::Detail)
    {
        if (m_modeState.current() == Mode::Detail)
        {
            m_pendingRouteFocusContextApplied = true;
            return std::nullopt;
        }

        if (m_modeState.current() != Mode::System)
            return MapIntent::recallRouteMap(Mode::System);

        auto& state = m_systemView.state();
        if (!waypoint->authoredHubId.empty())
        {
            state.selectedBodyId.clear();
            state.selectedHubId = waypoint->authoredHubId;
            state.selectedHubParentBodyId = waypoint->authoredBodyId;
        }
        else if (!waypoint->authoredBodyId.empty())
        {
            state.selectedBodyId = waypoint->authoredBodyId;
            state.selectedHubId.clear();
            state.selectedHubParentBodyId.clear();
        }
        else if (state.navigationGrid.enabled())
        {
            const glm::dvec3 relativeMeters =
                world::coordinates::fullMeters(waypoint->worldPosition) -
                system.systemPositionLy * world::coordinates::MetersPerLightYear;
            const glm::dvec3 relativeAu =
                relativeMeters / world::celestial::MetersPerAu;
            const int maximumLevel = state.navigationGrid.definition().maximumLevel;
            const auto index = state.navigationGrid.nearestIndexForPosition(
                relativeAu,
                maximumLevel
            );
            state.selectedBodyId.clear();
            state.selectedHubId.clear();
            state.selectedHubParentBodyId.clear();
            state.navigationGrid.selectCell(
                state.navigationGrid.cell(index, maximumLevel)
            );
            state.navigationCellExplicitlySelected = true;
        }

        return MapIntent::openBody(waypoint->authoredBodyId);
    }

    if (waypoint->authoredMap == Context::Hub)
    {
        if (m_modeState.current() == Mode::Hub)
        {
            m_pendingRouteFocusContextApplied = true;
            return std::nullopt;
        }

        if (m_modeState.current() != Mode::System && m_modeState.current() != Mode::Detail)
            return MapIntent::recallRouteMap(Mode::System);

        if (waypoint->authoredHubId.empty())
            return std::nullopt;

        auto& state = m_systemView.state();
        state.selectedBodyId.clear();
        state.selectedHubId = waypoint->authoredHubId;
        state.selectedHubParentBodyId = waypoint->authoredBodyId;
        if (m_modeState.current() == Mode::Detail)
        {
            m_detailView.selectHub(
                waypoint->authoredHubId,
                waypoint->authoredBodyId
            );
        }

        return MapIntent::openHub(
            waypoint->authoredHubId,
            waypoint->authoredBodyId
        );
    }

    (void)viewport;
    return std::nullopt;
}

void SystemMapRenderer::revealPendingRouteFocus(
    const game::system_map::MapObjectOverlayFrame& frame,
    const Viewport& viewport
)
{
    if (m_pendingRouteFocusNodeId == 0)
        return;

    const auto* routeNode =
        m_navigationWorkspace.routePlan().findById(m_pendingRouteFocusNodeId);
    if (!routeNode || routeNode->sourceObjectId.empty())
        return;

    const auto found = std::find_if(
        frame.items.begin(),
        frame.items.end(),
        [&](const auto& item)
        {
            return item.objectId == routeNode->sourceObjectId;
        }
    );
    if (found == frame.items.end())
        return;

    if (!found->visible && (m_modeState.current() == Mode::Detail || m_modeState.current() == Mode::Hub))
    {
        // Local maps pan in screen space. Use the already-projected target as
        // a one-frame correction, then let the normal builder reproject it at
        // the center on the next frame.
        auto& camera =
            m_modeState.current() == Mode::Hub ? m_hubView.camera() : m_detailView.camera();
        camera.pan +=
            glm::dvec2(viewport.width * 0.5, viewport.height * 0.5) -
            found->screenPx;
        if (m_modeState.current() == Mode::Hub)
            m_hubFrameDirty = true;
        else
            m_detailFrameDirty = true;
        return;
    }

    if (!found->visible)
        return;

    m_objectOverlayState.activate(found->objectId);
    m_objectOverlayState.ensureOpen(
        *found,
        glm::dvec2(viewport.width, viewport.height)
    );
    m_pendingRouteFocusNodeId = 0;
    m_pendingRouteFocusContextApplied = false;
}

void SystemMapRenderer::updateActiveTacticalLocalContext(
    const game::system_map::MapObjectOverlayItem& item
)
{
    m_activeTacticalLocalTargetObjectId = item.objectId;
    m_activeTacticalDetailCell.reset();

    // A real Hub binding wins. The System/Detail semantic selection carries
    // that identity; no synthetic cubic address is needed.
    if (!item.navigationHubId.empty() ||
        !item.hasNavigationSystemPositionAu ||
        !m_systemView.state().navigationGrid.enabled())
    {
        return;
    }

    const auto& grid = m_systemView.state().navigationGrid;
    const int maximumLevel = grid.definition().maximumLevel;
    const auto index = grid.nearestIndexForPosition(
        item.navigationSystemPositionAu,
        maximumLevel
    );
    const auto cell = grid.cell(index, maximumLevel);

    world::celestial::DetailSpatialCell detailCell;
    detailCell.level = cell.level;
    detailCell.maximumLevel = maximumLevel;
    detailCell.x = cell.index.x;
    detailCell.y = cell.index.y;
    detailCell.z = cell.index.z;
    detailCell.centerAu = cell.center;
    detailCell.edgeAu = cell.size;
    m_activeTacticalDetailCell = detailCell;
}
