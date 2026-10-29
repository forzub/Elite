// SpaceState: DEV HTML panels: attachment editor, structure / ship-core / frustum / debug-control state, debug HTML commands, volume viewer.

#include "src/game/SpaceStateInternal.h"




void SpaceState::pushAttachmentEditorState()
{
    json payload;
    payload["shipTypes"] = json::array();
    payload["selectedShipTypeId"] = m_attachmentEditorSelectedShipTypeId;

    // =========================================================
    // 1. List of editable ship types
    // =========================================================
    // Здесь добавь реальные типы из своего registry/descriptors.
    // Ниже пример структуры.
    //
    // payload["shipTypes"].push_back({
    //     {"typeId", "cobra_mk1"},
    //     {"displayName", "Cobra Mk1"}
    // });
    //
    // payload["shipTypes"].push_back({
    //     {"typeId", "station01"},
    //     {"displayName", "Station 01"}
    // });

    // ======= ПРИМЕР: если у тебя пока только Cobra =======
    payload["shipTypes"].push_back({
        {"typeId", "cobra_mk1"},
        {"displayName", "Cobra Mk1"}
    });

    // =========================================================
    // 2. Preview ship for selected type
    // =========================================================
    //
    // Здесь надо собрать exactly тот же JSON, который сейчас
    // attachment_editor уже умеет рисовать:
    // {
    //   shipId,
    //   displayName,
    //   meshParts,
    //   attachments,
    //   visualBasisRotationDeg, ...
    // }
    //
    // Но НЕ из runtime ship, а из descriptor/type preview.
    //
    // Ниже логика:
    //
    // json preview = buildAttachmentEditorPreviewForType(m_attachmentEditorSelectedShipTypeId);
    // payload["selectedShip"] = preview;

    // Пока назови поле именно selectedShip
    // чтобы потом HTML не таскал массив objects[0].
    payload["selectedShip"] = buildAttachmentEditorPreviewForType(*this, m_attachmentEditorSelectedShipTypeId);

    context().htmlUi().broadcastState(HtmlUiPanelId::AttachmentEditor, payload);
}





void SpaceState::processHtmlCommands()
{





    auto cmds = context().htmlUi().popCommands();

    for (const auto& msg : cmds)
    {

        // ------------------------------
        // SYSTEM MAP
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::SystemMap)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                if (context().htmlUi().state().activePanel != HtmlUiPanelId::SystemMap)
                {
                    context().htmlUi().setActivePanel(HtmlUiPanelId::SystemMap);
                }

                pushSystemMapState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {



                if (msg.command == "request_snapshot")
                {
                    pushSystemMapState();
                    continue;
                }


                if (msg.command == "close")
                {
                    if (Application* app = context().app)
                    {
                        // Legacy HtmlUi SystemMap close follows the same
                        // explicit presentation-router contract as the
                        // persistent map panel: return to the last selected
                        // F1-F4 flight view, never to an implicit UI-none
                        // gameplay fallback.
                        app->requestLastFlightView();
                    }

                    continue;
                }
            }
        }





        // ------------------------------
        // ATTACHMENT EDITOR
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::AttachmentEditor)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushAttachmentEditorState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushAttachmentEditorState();
                    continue;
                }

                if (msg.command == "select_ship_type")
                {
                    m_attachmentEditorSelectedShipTypeId =
                        msg.payload.value("shipTypeId", m_attachmentEditorSelectedShipTypeId);

                    pushAttachmentEditorState();
                    continue;
                }

                if (msg.command == "update_attachment")
                {
                    const std::string attachmentId = msg.payload.value("attachmentId", "");
                    if (attachmentId.empty())
                        continue;

                    const auto posArr = msg.payload.value("localPosition", std::vector<float>{0,0,0});
                    const auto rotArr = msg.payload.value("localRotationDeg", std::vector<float>{0,0,0});
                    const bool enabled = msg.payload.value("enabled", true);

                    if (posArr.size() < 3 || rotArr.size() < 3)
                        continue;

                    auto& ov = m_attachmentEditorOverrides[attachmentId];
                    ov.localPosition = glm::vec3(posArr[0], posArr[1], posArr[2]);
                    ov.localRotationDeg = glm::vec3(rotArr[0], rotArr[1], rotArr[2]);
                    ov.enabled = enabled;

                    std::cerr
                        << "[AttachmentEditor APPLY] id=" << attachmentId
                        << " pos=("
                        << ov.localPosition.x << ", "
                        << ov.localPosition.y << ", "
                        << ov.localPosition.z << ") rot=("
                        << ov.localRotationDeg.x << ", "
                        << ov.localRotationDeg.y << ", "
                        << ov.localRotationDeg.z << ") enabled="
                        << (ov.enabled ? "true" : "false")
                        << " overridesCount=" << m_attachmentEditorOverrides.size()
                        << "\n";

                    pushAttachmentEditorState();
                    continue;
                }
            }
        }
                // ------------------------------
        // VOLUME VIEWER
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::VolumeViewer)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushVolumeViewerState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushVolumeViewerState();
                    continue;
                }

                if (msg.command == "select_ship_type")
                {
                    m_attachmentEditorSelectedShipTypeId =
                        msg.payload.value("shipTypeId", m_attachmentEditorSelectedShipTypeId);

                    pushVolumeViewerState();
                    continue;
                }
            }
        }

        // ------------------------------
        // DEBUG CONTROL
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::DebugControl)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushDebugControlState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushDebugControlState();
                    continue;
                }

                if (msg.command == "apply_settings")
                {
                    applyDebugControlPayload(msg.payload);
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }

                if (msg.command == "save_defaults")
                {
                    applyDebugControlPayload(msg.payload);
                    saveDebugControlDefaults(msg.payload);
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }

                if (msg.command == "reset_settings")
                {
                    resetDebugControlSettings();
                    ++m_debugControlSettingsRevision;
                    deferDebugControlStatePush();
                    continue;
                }
            }
        }



                // ------------------------------
        // STRUCTURE DEBUG
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::StructureDebug)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                requestStructureDebugStateRefresh();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    requestStructureDebugStateRefresh();
                    continue;
                }

                if (msg.command == "select_ship_entity")
                {
                    m_structureDebugSelectedShipEntityId =
                        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

                    pushStructureDebugState();
                    continue;
                }

if (msg.command == "destroy_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->destroyShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "detach_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->detachShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "hang_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->hangShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "reevaluate_structure")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->reevaluateShipStructure(id);
    requestStructureDebugStateRefresh();
    continue;
}



if (msg.command == "restore_module")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    const std::string moduleId =
        msg.payload.value("moduleId", std::string{});

    if (!moduleId.empty())
    {
        EntityId id{ static_cast<uint32_t>(entityId) };

        m_debugSession->restoreShipModule(id, moduleId);
    }

    requestStructureDebugStateRefresh();
    continue;
}


                if (msg.command == "set_link_health")
                    {
                        const uint64_t entityId =
                            msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

                        const std::string linkId =
                            msg.payload.value("linkId", std::string{});

                        const float health =
                            msg.payload.value("health", 0.0f);

                        const bool destroyed =
                            msg.payload.value("destroyed", health <= 0.0f);

                        if (!linkId.empty())
                        {
                            EntityId id{ static_cast<uint32_t>(entityId) };
                            m_debugSession->setShipStructuralLinkHealth(
                                id,
                                linkId,
                                health,
                                destroyed
                            );
                        }

                        requestStructureDebugStateRefresh();
                        continue;
                    }

if (msg.command == "reset_ship")
{
    const uint64_t entityId =
        msg.payload.value("entityId", m_structureDebugSelectedShipEntityId);

    EntityId id{ static_cast<uint32_t>(entityId) };

    m_debugSession->resetShipStructure(id);
    requestStructureDebugStateRefresh();
    continue;
}

if (msg.command == "reset_all_ships")
{
    m_debugSession->resetAllShipStructures();
    requestStructureDebugStateRefresh();
    continue;
}
            }
        }


        // ------------------------------
        // SHIP CORE
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::ShipCore)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                pushShipCoreState();
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command)
            {
                if (msg.command == "request_snapshot")
                {
                    pushShipCoreState();
                    continue;
                }

                if (msg.command == "select_ship_entity")
                {
                    m_shipCoreSelectedShipEntityId =
                        msg.payload.value("entityId", m_shipCoreSelectedShipEntityId);

                    pushShipCoreState();
                    continue;
                }

                if (msg.command == "repair_all_panels")
                {
                    if (m_shipCoreSelectedShipEntityId != m_playerId.value)
                    {
                        pushShipCoreState();
                        continue;
                    }

                    ClientShipCommand cmd;
                    cmd.type = ClientShipCommand::RepairAllPanels;

                    std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
                    m_debugCommands.push_back(cmd);
                    continue;
                }

                if (msg.command == "damage_radiator")
                {
                    // ClientShipCommand is player-owned on the authoritative
                    // server. Do not let a browser selection make this command
                    // appear to target an NPC while actually damaging the player.
                    if (m_shipCoreSelectedShipEntityId != m_playerId.value)
                    {
                        pushShipCoreState();
                        continue;
                    }

                    ClientShipCommand cmd;
                    cmd.type = ClientShipCommand::DamageRadiator;
                    cmd.index = msg.payload.value("panel_index", 0);
                    cmd.amount = msg.payload.value("amount", 0.2);

                    std::lock_guard<std::mutex> lock(m_debugCommandsMutex);
                    m_debugCommands.push_back(cmd);
                    continue;
                }
            }
        }

        // ------------------------------
        // FRUSTUM DEBUG
        // ------------------------------
        if (msg.panel == HtmlUiPanelId::FrustumDebug)
        {
            if (msg.type == HtmlUiMessageType::Subscribe)
            {
                continue;
            }

            if (msg.type == HtmlUiMessageType::Command &&
                msg.command == "request_snapshot")
            {
                continue;
            }
        }
    }
}


void SpaceState::requestStructureDebugStateRefresh()
{
    if (!m_debugSession)
        return;

    m_structureDebugAwaitingSnapshotRevision =
        m_debugSession->snapshotRevision();
    m_structureDebugRefreshPending = true;
    m_debugSession->refreshStructureSnapshot();
}

void SpaceState::deferDebugControlStatePush()
{
    if (!m_debugSession)
    {
        pushDebugControlState();
        return;
    }

    m_debugControlAwaitingStateRevision =
        m_debugSession->stateRevision();
    m_debugControlStatePushPending = true;
}

void SpaceState::flushPendingDebugUiState()
{
    if (m_structureDebugRefreshPending && m_debugSession &&
        m_debugSession->snapshotRevision() >
            m_structureDebugAwaitingSnapshotRevision)
    {
        m_structureDebugRefreshPending = false;
        pushStructureDebugState();
    }

    if (m_debugControlStatePushPending && m_debugSession &&
        m_debugSession->stateRevision() >
            m_debugControlAwaitingStateRevision)
    {
        m_debugControlStatePushPending = false;
        pushDebugControlState();
    }
}


void SpaceState::pushStructureDebugState()
{

    json payload;
    payload["ships"] = json::array();
    payload["selectedShipEntityId"] = 0;
    payload["modules"] = json::array();
    payload["links"] = json::array();
    payload["hasData"] = false;
    payload["reason"] = "no_server_ships";

    // snapshot() is a copied diagnostic value. A refresh request is issued by
    // the UI command path before the server advances; never retain a reference
    // into authoritative runtime memory here.
    const auto snapshot = m_debugSession->snapshot();

    if (snapshot.ships.empty())
    {
        context().htmlUi().broadcastState(HtmlUiPanelId::StructureDebug, payload);
        return;
    }

    payload["hasData"] = true;
    payload["reason"] = "ok";

    const ShipSnapshot* selectedShip = nullptr;

    for (const auto& s : snapshot.ships)
    {
        if (s.id.value == m_structureDebugSelectedShipEntityId)
        {
            selectedShip = &s;
            break;
        }
    }

    if (!selectedShip)
    {
        for (const auto& s : snapshot.ships)
        {
            if (s.id.value == m_playerId.value)
            {
                selectedShip = &s;
                m_structureDebugSelectedShipEntityId = s.id.value;
                break;
            }
        }
    }

    if (!selectedShip)
    {
        selectedShip = &snapshot.ships.front();
        m_structureDebugSelectedShipEntityId = selectedShip->id.value;
    }

    const auto& ship = *selectedShip;

    payload["selectedShipEntityId"] = m_structureDebugSelectedShipEntityId;

    for (const auto& s : snapshot.ships)
    {
        const uint64_t id = s.id.value;

        json item;
        item["entityId"] = id;
        item["displayName"] = std::string("Ship #") + std::to_string(id);
        item["role"] = (s.role == ShipRole::Player) ? "Player" : "Npc";
        item["isPlayer"] = (s.role == ShipRole::Player);

        payload["ships"].push_back(std::move(item));
    }

    if (ship.graph.hasModules)
    {
        // The debug page needs a rich view, but static ship/module definitions
        // are deliberately not replicated. Rehydrate them from the same local
        // descriptor catalog used by the normal client presentation.
        const auto& descriptor = ObjectDescriptorRegistry::get(ship.typeId);
        const auto moduleViews = game::client::buildModuleViews(
            descriptor.moduleDescriptors(),
            ship.graph.modules
        );

        for (const auto& mod : moduleViews)
        {
            json m;

            m["moduleId"] = mod.moduleId;
            m["parentModuleId"] = mod.parentModuleId;
            m["subsystemId"] = mod.subsystemId;

            m["state"] = mod.state;
            m["health"] = mod.health;
            m["maxHealth"] = mod.maxHealth;

            m["destructible"] = mod.destructible;
            m["detachable"] = mod.detachable;
            m["hangable"] = mod.hangable;

            m["destroyPolicy"] = mod.destroyPolicy;
            m["detachPolicy"] = mod.detachPolicy;
            m["attachmentType"] = mod.attachmentType;

            m["meshPartIds"] = mod.meshPartIds;
            m["supportModuleIds"] = mod.supportModuleIds;

            m["minSupportsForAttached"] = mod.minSupportsForAttached;
            m["minSupportsForStable"] = mod.minSupportsForStable;
            m["aliveSupportCount"] = mod.aliveSupportCount;

            m["localPosition"] = json::array({ 0.0, 0.0, 0.0 });
            m["localRotationDeg"] = json::array({ 0.0, 0.0, 0.0 });

            payload["modules"].push_back(std::move(m));
        }
    }

    if (ship.graph.hasStructuralLinks)
    {
        for (const auto& link : ship.graph.structuralLinks)
        {
            json l;

            l["id"] = link.id;
            l["ownerModuleId"] = link.ownerModuleId;
            l["moduleAId"] = link.moduleAId;
            l["moduleBId"] = link.moduleBId;
            l["kind"] = link.kind;

            l["health"] = link.health;
            l["maxHealth"] = link.maxHealth;
            l["impulseTolerance"] = link.impulseTolerance;

            l["loadBearing"] = link.loadBearing;
            l["destroyed"] = link.destroyed;
            l["autoGenerated"] = link.autoGenerated;

            l["center"] = json::array({
                double(link.center.x),
                double(link.center.y),
                double(link.center.z)
            });

            l["halfSize"] = json::array({
                double(link.halfSize.x),
                double(link.halfSize.y),
                double(link.halfSize.z)
            });

            json orientation = json::array();
            orientation.push_back(json::array({
                double(link.orientation[0].x),
                double(link.orientation[0].y),
                double(link.orientation[0].z)
            }));
            orientation.push_back(json::array({
                double(link.orientation[1].x),
                double(link.orientation[1].y),
                double(link.orientation[1].z)
            }));
            orientation.push_back(json::array({
                double(link.orientation[2].x),
                double(link.orientation[2].y),
                double(link.orientation[2].z)
            }));
            l["orientation"] = std::move(orientation);

            payload["links"].push_back(std::move(l));
        }
    }

    context().htmlUi().broadcastState(HtmlUiPanelId::StructureDebug, payload);
}


void SpaceState::pushShipCoreState()
{
    const auto& ships = m_client->world().ships();
    if (ships.empty())
        return;

    if (m_shipCoreSelectedShipEntityId == 0 || ships.find(m_shipCoreSelectedShipEntityId) == ships.end())
    {
        auto itPlayer = ships.find(m_playerId.value);
        if (itPlayer != ships.end())
            m_shipCoreSelectedShipEntityId = itPlayer->first;
        else
            m_shipCoreSelectedShipEntityId = ships.begin()->first;
    }

    auto it = ships.find(m_shipCoreSelectedShipEntityId);
    if (it == ships.end())
        return;

    const auto& ship = it->second;

    // The player's own engineering terminal reads the same motherboard
    // as cockpit HUD and docking status. Selecting a different NPC remains
    // an external inspector, not a claim to own that NPC's ship computer.
    const auto computerSystems = m_motherComputer.cockpitSnapshot(
        static_cast<std::uint64_t>(ship.instanceId),
        m_client->lastSimulationMetadata().serverTick
    );
    const bool ownShip = it->first == m_playerId.value;
    if (ownShip && !computerSystems.valid)
        return; // do not silently invent a healthy status
    json payload = shipCoreStatusToJson(
        ownShip ? computerSystems.systems : ship.shipCoreStatus
    );

    // список доступных кораблей для будущего selector в HTML
    payload["ships"] = json::array();
    for (const auto& [id, s] : ships)
    {
        json item;
        item["entityId"] = id;
        item["displayName"] = std::string("Ship #") + std::to_string(id);
        item["role"] = (s.role == ShipRole::Player) ? "Player" : "Npc";
        item["isPlayer"] = (s.role == ShipRole::Player);
        payload["ships"].push_back(item);
    }

    payload["selectedShipEntityId"] = m_shipCoreSelectedShipEntityId;

    context().htmlUi().broadcastState(HtmlUiPanelId::ShipCore, payload);
}


void SpaceState::pushFrustumDebugState(const json& payload)
{
    // Frustum telemetry is an external diagnostic stream. It must not steal
    // the global in-game activePanel from other browser tools.
    context().htmlUi().broadcastState(HtmlUiPanelId::FrustumDebug, payload);
}


void SpaceState::pushDebugControlState()
{
    const auto& dbg = debug::get().render;

    json payload =
        game::debug_control::encodeRenderSettings(dbg);
    payload["debugSettingsRevision"] = m_debugControlSettingsRevision;

    SceneRenderStats totalStats = m_perfMainStats;
    totalStats.add(m_perfRearStats);

    json perf;
    perf["frameIndex"] = m_perfFrameIndex;

    perf["fps"] = m_perfFps;
    perf["frameMs"] = m_perfFrameMs;

    perf["updateMs"] = m_perfUpdateMs;
    perf["processHtmlMs"] = m_perfProcessHtmlMs;
    perf["fixedSimMs"] = m_perfFixedSimMs;
    perf["serverFixedSteps"] = m_perfServerFixedSteps;
    perf["serverTickDebtMs"] = m_perfServerTickDebtMs;
    perf["serverDiscardedMs"] = m_perfServerDiscardedMs;
    perf["serverTotalDiscardedMs"] = m_perfServerTotalDiscardedMs;
    perf["serverCatchUpLimited"] = m_perfServerCatchUpLimited;
    perf["clientUpdateMs"] = m_perfClientUpdateMs;
    perf["dockingGuidanceMs"] = m_perfDockingGuidanceMs;
    perf["dockingRequestActive"] = m_perfDockingRequestActive;
    perf["scenePrepareMs"] = m_perfScenePrepareMs;
    perf["playerViewMs"] = m_perfPlayerViewMs;
    perf["uiRootUpdateMs"] = m_perfUiRootUpdateMs;

    perf["mainRenderMs"] = m_perfMainRenderMs;
    perf["rearCameraMs"] = m_perfRearCameraMs;
    perf["renderUiMs"] = m_perfRenderUiMs;
    perf["hudMs"] = m_perfHudMs;

    perf["sceneCpuTotalMs"] = m_perfMainStats.cpuTotalMs;
    perf["sceneCpuSetupMs"] = m_perfMainStats.cpuSetupMs;
    perf["sceneCpuStarfieldMs"] = m_perfMainStats.cpuStarfieldMs;
    perf["sceneCpuFarPassesMs"] = m_perfMainStats.cpuFarPassesMs;
    perf["sceneCpuLabelsMs"] = m_perfMainStats.cpuLabelsMs;
    perf["sceneCpuRealShipsMs"] = m_perfMainStats.cpuRealShipsMs;
    perf["sceneCpuVisualShipsMs"] = m_perfMainStats.cpuVisualShipsMs;
    perf["sceneCpuVisualDronesMs"] = m_perfMainStats.cpuVisualDronesMs;
    perf["sceneCpuObjectsMs"] = m_perfMainStats.cpuObjectsMs;
    perf["sceneCpuDebugCallbackMs"] = m_perfMainStats.cpuDebugCallbackMs;

    const auto& hubPerf =
        m_systemMapRenderer
            .hubMapPerformanceStats();

    perf["hubMapActive"] =
        m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Hub;

    perf["hubCpuTotalMs"] =
        hubPerf.cpuTotalMs;

    perf["hubCpuBackgroundMs"] =
        hubPerf.cpuBackgroundMs;

    perf["hubCpuPlanetBackdropMs"] =
        hubPerf.cpuPlanetBackdropMs;

    perf["hubCpuGeometryMs"] =
        hubPerf.cpuGeometryMs;

    perf["hubCpuLabelsMs"] =
        hubPerf.cpuLabelsMs;

    perf["hubGpuValid"] =
        hubPerf.gpuValid;

    perf["hubGpuTotalMs"] =
        hubPerf.gpuTotalMs;

    perf["hubGpuBackgroundMs"] =
        hubPerf.gpuBackgroundMs;

    perf["hubGpuFallbackBodyMs"] =
        hubPerf.gpuFallbackBodyMs;

    perf["hubGpuSurfaceMs"] =
        hubPerf.gpuSurfaceMs;

    perf["hubGpuCloudsMs"] =
        hubPerf.gpuCloudsMs;

    perf["hubGpuAtmosphereMs"] =
        hubPerf.gpuAtmosphereMs;

    perf["hubGpuGeometryMs"] =
        hubPerf.gpuGeometryMs;

    perf["hubGpuLabelsMs"] =
        hubPerf.gpuLabelsMs;

    perf["drawCalls"] = totalStats.drawCalls;
    perf["modulesDrawn"] = totalStats.modulesDrawn;
    perf["modulesCulled"] = totalStats.modulesCulled;
    perf["partsDrawn"] = totalStats.partsDrawn;
    perf["partsCulled"] = totalStats.partsCulled;


    perf["realShipsDrawn"] = totalStats.realShipsDrawn;
    perf["realShipPartsDrawn"] = totalStats.realShipPartsDrawn;

    perf["visualShipsDrawn"] = totalStats.visualShipsDrawn;
    perf["visualShipsCulled"] = totalStats.visualShipsCulled;

    perf["visualProxyShipsDrawn"] = totalStats.visualProxyShipsDrawn;
    perf["visualFullShipsDrawn"] = totalStats.visualFullShipsDrawn;
    perf["visualShipPartsDrawn"] = totalStats.visualShipPartsDrawn;

    payload["performance"] = perf;
    payload["debugFastUniverseTime"] =
        m_debugSession
            ? m_debugSession->fastUniverseTime()
            : false;

    /*
        These values come from the latest copied server-side debug state.
        Apply/reset waits for stateRevision() to advance before this payload is
        pushed, so the HTML page never needs a direct read of ServerRuntime.
    */
    payload["debugUniverseTimeSimulation"] =
        m_debugSession
            ? m_debugSession->universeTimeSimulation()
            : false;

    payload["debugUniverseTimeScale"] =
        m_debugSession
            ? m_debugSession->universeTimeScale()
            : 1.0;

    payload["debugUniverseTimeConfiguredScale"] =
        m_debugSession
            ? m_debugSession->configuredUniverseTimeScale()
            : 10000.0;

    payload["systemMapVisible"] =
        m_systemMapVisible;

    payload["systemMapLiveSnapshotsEnabled"] =
        m_systemMapLiveSnapshotsEnabled;

    payload["liveSystemMapId"] =
        m_liveSystemMapId;

    payload["detailMapSampleMode"] =
        m_authoritativeMapInterpolator.detailSampleModeName();
    payload["detailMapNewestGapMs"] =
        m_authoritativeMapInterpolator.detailNewestGapSeconds() * 1000.0;
    payload["detailMapBufferedSnapshots"] =
        m_authoritativeMapInterpolator.detailBufferedSnapshotCount();

    payload["hubMapSampleMode"] =
        m_authoritativeMapInterpolator.hubSampleModeName();
    payload["hubMapNewestGapMs"] =
        m_authoritativeMapInterpolator.hubNewestGapSeconds() * 1000.0;
    payload["hubMapBufferedSnapshots"] =
        m_authoritativeMapInterpolator.hubBufferedSnapshotCount();

    context().htmlUi().broadcastState(HtmlUiPanelId::DebugControl, payload);
}

void SpaceState::loadDebugControlDefaults()
{
    namespace fs = std::filesystem;

    const fs::path path = debugControlDefaultsPath();
    if (!fs::exists(path))
        return;

    try
    {
        std::ifstream input(path);
        if (!input)
            return;

        json payload;
        input >> payload;
        if (!payload.is_object())
            return;

        /*
            Older Debug Control files may predate the transactional diagnostic
            contract and contain an active simulation flag. Startup preferences
            may configure the scale, but they must never start a debug timeline.
        */
        payload.erase("debugUniverseTimeSimulation");
        payload.erase("debugFastUniverseTime");

        applyDebugControlPayload(payload);

        std::cout
            << "[DebugControl] loaded startup defaults from "
            << path.string()
            << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[DebugControl] failed to load startup defaults: "
            << e.what()
            << "\n";
    }
}

bool SpaceState::saveDebugControlDefaults(const json& payload)
{
    namespace fs = std::filesystem;

    const fs::path path = debugControlDefaultsPath();

    try
    {
        if (!path.parent_path().empty())
            fs::create_directories(path.parent_path());

        std::ofstream output(path, std::ios::trunc);
        if (!output)
            return false;

        json persistentPayload = payload;

        // Runtime session state is transactional, not a startup preference.
        // Persist the configured scale if desired, but never boot directly
        // into an accelerated diagnostic branch.
        persistentPayload.erase("debugUniverseTimeSimulation");
        persistentPayload.erase("debugFastUniverseTime");

        output << std::setw(2) << persistentPayload << '\n';

        std::cout
            << "[DebugControl] saved startup defaults to "
            << path.string()
            << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr
            << "[DebugControl] failed to save startup defaults: "
            << e.what()
            << "\n";
        return false;
    }
}

void SpaceState::resetDebugControlSettings()
{
    namespace fs = std::filesystem;

    debug::get().render = debug::DebugRenderSettings{};

    if (m_debugSession)
        m_debugSession->setUniverseTimeSimulation(false, 1.0);

    const fs::path path = debugControlDefaultsPath();
    std::error_code ec;
    fs::remove(path, ec);

    if (ec)
    {
        std::cerr
            << "[DebugControl] failed to remove startup defaults: "
            << ec.message()
            << "\n";
    }
}

void SpaceState::applyDebugControlPayload(const json& payload)
{
    auto& dbg = debug::get().render;
    const std::string previousSceneMode = dbg.sceneMode;

    game::debug_control::applyRenderSettings(dbg, payload);

    if (dbg.sceneMode != previousSceneMode)
    {
        if (m_client)
            m_promoSceneScenario.reset(m_client->world());
    }

    if (m_debugSession)
    {
        const bool simulationEnabled =
            payload.value(
                "debugUniverseTimeSimulation",
                m_debugSession->universeTimeSimulation()
            );

        const double simulationScale =
            payload.value(
                "debugUniverseTimeScale",
                m_debugSession->configuredUniverseTimeScale()
            );

        m_debugSession->setUniverseTimeSimulation(
            simulationEnabled,
            simulationScale
        );
    }


}



void SpaceState::pushVolumeViewerState()
{
    json payload;

    if (m_attachmentEditorSelectedShipTypeId.empty())
        m_attachmentEditorSelectedShipTypeId = "cobra_mk1";

    payload["shipTypes"] = json::array({
        {
            {"typeId", "cobra_mk1"},
            {"displayName", "Cobra Mk1"}
        },
        {
            {"typeId", "station_01"},
            {"displayName", "Station 01"}
        }
    });

    payload["selectedShipTypeId"] = m_attachmentEditorSelectedShipTypeId;
    payload["selectedShip"] =
        buildVolumeViewerPreviewForType(*this, m_attachmentEditorSelectedShipTypeId);

    context().htmlUi().broadcastState(HtmlUiPanelId::VolumeViewer, payload);
}
