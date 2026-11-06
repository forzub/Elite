// SpaceState: Galaxy / System Map glue: map snapshots, map modes and transitions, the native map panel and the map UI state.

#include <map>
#include "src/game/economy/EconomyModel.h"
#include "src/game/navigation/orbital/OrbitalTransferPlanner.h"
#include "src/game/presentation/SystemMapInfoBuilder.h"
#include "src/game/SpaceStateInternal.h"



bool SpaceState::buildPlayerDetailTarget(
    world::celestial::DetailTarget& outTarget,
    bool preferReferenceContext
) const
{
    using namespace world::celestial;

    outTarget = {};

    if (!m_client)
        return false;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId < 0)
        return false;

    outTarget.systemId = nav.currentSystemId;

    // Galactocentric placement is static endpoint-local catalog data. Detail
    // composition must not depend on the Galaxy overlay RPC having run first.
    if (const auto* atlas = m_client->starAtlas())
    {
        if (const auto* summary = atlas->findSystemSummary(nav.currentSystemId))
            outTarget.systemPositionLy = summary->positionLy;
    }

    const auto& ships = m_client->world().ships();
    const auto playerIt = ships.find(m_playerId.value);

    const game::navigation::DynamicMotionState* motion = nullptr;
    if (playerIt != ships.end())
        motion = &playerIt->second.transform.motion;

    if (preferReferenceContext && motion)
    {
        const std::string bodyId =
            !motion->parentBodyId.empty()
                ? motion->parentBodyId
                : motion->primaryGravityBodyId;

        const bool activeHubReference =
            motion->matchedToReferenceFrame && !motion->hubId.empty();

        if (activeHubReference && !bodyId.empty())
        {
            outTarget.sceneKind = DetailSceneKind::CelestialBody;
            outTarget.focusClass = DetailObjectClass::Hub;
            outTarget.anchorId = bodyId;
            outTarget.focusId = motion->hubId;
            return outTarget.valid();
        }

        if (!bodyId.empty())
        {
            outTarget.sceneKind = DetailSceneKind::CelestialBody;
            outTarget.focusClass = DetailObjectClass::CelestialBody;
            outTarget.anchorId = bodyId;
            outTarget.focusId = bodyId;
            return outTarget.valid();
        }

        if (activeHubReference)
        {
            outTarget.sceneKind = DetailSceneKind::LocalObject;
            outTarget.focusClass = DetailObjectClass::Hub;
            outTarget.anchorId = motion->hubId;
            outTarget.focusId = motion->hubId;
            return outTarget.valid();
        }
    }

    // No semantic body/hub target (or F12 explicitly requested local space):
    // address the exact terminal system-navigation cube that contains the
    // player. The cube is the PLAYER's local space (focus = player): it is
    // never promoted to a body scene (in low orbit the 2500 km cube centre
    // often lies inside the planet, and F12 then showed the whole planet's
    // Details) and it follows the player into the neighbour cube.
    return buildLocalCubeTarget(
        nav.currentSystemId, nav.systemLocalAu, DetailObjectClass::Ship, "player", outTarget);
}

bool SpaceState::buildLocalCubeTarget(
    int systemId,
    const glm::dvec3& systemLocalAu,
    world::celestial::DetailObjectClass focusClass,
    const std::string& focusId,
    world::celestial::DetailTarget& outTarget
) const
{
    using namespace world::celestial;
    outTarget = {};
    if (!m_client || systemId < 0)
        return false;
    outTarget.systemId = systemId;
    if (const auto* atlas = m_client->starAtlas())
        if (const auto* summary = atlas->findSystemSummary(systemId))
            outTarget.systemPositionLy = summary->positionLy;

    game::navigation::SystemNavigationGrid grid;
    grid.activateSystem(systemId);
    const int level = grid.maximumLevel();
    const auto cell = grid.cell(grid.nearestIndexForPosition(systemLocalAu, level), level);

    outTarget.sceneKind = DetailSceneKind::SpatialVolume;
    outTarget.focusClass = focusClass;
    outTarget.focusId = focusId;
    outTarget.spatialCell.level = cell.level;
    outTarget.spatialCell.maximumLevel = grid.maximumLevel();
    outTarget.spatialCell.x = cell.index.x;
    outTarget.spatialCell.y = cell.index.y;
    outTarget.spatialCell.z = cell.index.z;
    outTarget.spatialCell.centerAu = cell.center;
    outTarget.spatialCell.edgeAu = cell.size;
    return outTarget.valid();
}

bool SpaceState::resolveLocalCubeFocusAu(
    const std::string& focusId,
    int systemId,
    glm::dvec3& outAu
) const
{
    if (!m_client || focusId.empty())
        return false;
    if (focusId == "player")
    {
        const auto& nav = m_client->playerNavigation();
        if (nav.currentSystemId != systemId)
            return false;
        outAu = nav.systemLocalAu;
        return true;
    }
    if (focusId.rfind("entity:", 0) == 0)
    {
        const auto& ships = m_client->world().ships();
        const auto it = ships.find(static_cast<std::uint32_t>(std::strtoul(focusId.c_str() + 7, nullptr, 10)));
        if (it == ships.end())
            return false;
        outAu = world::coordinates::fullMeters(it->second.transform.worldPosition) / world::celestial::MetersPerAu;
        return true;
    }
    return false;
}

void SpaceState::followLocalCubeFocus()
{
    using namespace world::celestial;
    const DetailTarget loaded = m_loadedDetailTarget;
    if (loaded.sceneKind != DetailSceneKind::SpatialVolume || loaded.focusClass == DetailObjectClass::None ||
        loaded.focusId.empty() || loaded.systemId < 0)
        return;
    glm::dvec3 au(0.0);
    if (!resolveLocalCubeFocusAu(loaded.focusId, loaded.systemId, au))
        return;
    DetailTarget next;
    if (!buildLocalCubeTarget(loaded.systemId, au, loaded.focusClass, loaded.focusId, next) ||
        next.spatialCell == loaded.spatialCell)
        return;
    if (composeDetailMapSnapshot(next))
        std::cerr << "[LocalMap] " << next.focusId << " left cube " << loaded.spatialCell.x << ','
                  << loaded.spatialCell.y << ',' << loaded.spatialCell.z << " -> shown in cube "
                  << next.spatialCell.x << ',' << next.spatialCell.y << ',' << next.spatialCell.z << std::endl;
}

bool SpaceState::prepareSelectedObjectLocalMap()
{
    using namespace world::celestial;
    if (!m_client || !context().app || context().app->gameUiMode() != GameUiMode::SystemMap)
        return false;
    const auto mode = m_systemMapRenderer.mode();
    if (mode != SystemMapRenderer::Mode::System && mode != SystemMapRenderer::Mode::Detail)
        return false;

    std::string tacticalHubId;
    glm::dvec3 tacticalMeters(0.0);
    const bool tactical = m_systemMapRenderer.activeTacticalObjectContext(tacticalHubId, tacticalMeters);
    const std::string activeId = m_systemMapRenderer.activeObjectId();

    std::string hubId = m_systemMapRenderer.selectedHubId();
    if (hubId.empty())
        hubId = tacticalHubId;
    if (hubId.empty() && m_client->world().hubs().count(activeId))
        hubId = activeId;

    const auto hub = m_client->world().hubs().find(hubId);
    if (!hubId.empty() && hub != m_client->world().hubs().end() && hub->second.systemId >= 0)
    {
        const int systemId = hub->second.systemId;
        if (!composeSystemMapSnapshot(systemId))
            return false;
        DetailTarget parent;
        if (mode == SystemMapRenderer::Mode::Detail && m_loadedDetailTarget.valid() &&
            m_loadedDetailTarget.systemId == systemId)
        {
            parent = m_loadedDetailTarget;
        }
        else
        {
            parent.systemId = systemId;
            if (const auto* atlas = m_client->starAtlas())
                if (const auto* summary = atlas->findSystemSummary(systemId))
                    parent.systemPositionLy = summary->positionLy;
            parent.focusClass = DetailObjectClass::Hub;
            parent.focusId = hubId;
            parent.anchorId = hub->second.parentBodyId.empty() ? hubId : hub->second.parentBodyId;
            parent.sceneKind = hub->second.parentBodyId.empty() ? DetailSceneKind::LocalObject
                                                                : DetailSceneKind::CelestialBody;
        }
        if (!composeDetailMapSnapshot(parent) || !composeHubMapSnapshot(systemId, hubId))
            return false;
        std::cerr << "[LocalMap] F12: the selected object's hub " << hubId << std::endl;
        m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Hub);
        return true;
    }

    if (!tactical || activeId.empty())
        return false;
    const int systemId = mode == SystemMapRenderer::Mode::Detail && m_loadedDetailTarget.valid()
        ? m_loadedDetailTarget.systemId
        : m_loadedSystemMapId;
    const bool ship = activeId == "player" || activeId.rfind("entity:", 0) == 0;
    DetailTarget cube;
    if (!buildLocalCubeTarget(systemId, tacticalMeters / MetersPerAu,
                              ship ? DetailObjectClass::Ship : DetailObjectClass::Hub, activeId, cube))
        return false;
    if (!composeSystemMapSnapshot(systemId) || !composeDetailMapSnapshot(cube))
        return false;
    std::cerr << "[LocalMap] F12: the cube of the selected object " << activeId << " (" << cube.spatialCell.x << ','
              << cube.spatialCell.y << ',' << cube.spatialCell.z << ")" << std::endl;
    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
    return true;
}


void SpaceState::requestGalaxyMapSnapshotOnce()
{
    if (!m_client)
        return;

    // Galaxy presentation must never wait for the jurisdiction overlay RPC.
    // Build the deterministic catalog layer immediately from the endpoint-local
    // StarAtlas, then merge the authoritative overlay whenever it arrives.
    if (!m_hasGalaxyMapSnapshot)
    {
        if (const auto* atlas = m_client->starAtlas())
        {
            world::celestial::GalaxyMapSnapshot local;
            local.universeTimeSeconds = m_client->universeTimeSeconds();
            local.universeDate = m_client->sessionSnapshot().universeDate;
            game::client::rebuildGalaxyMapCatalogLayer(local, *atlas);
            m_galaxyMapSnapshot = std::move(local);
            m_hasGalaxyMapSnapshot = true;
        }
    }

    if (m_hasGalaxyMapOverlay)
        return;

    // The request is an asynchronous overlay refresh, not an opening barrier.
    m_client->requestGalaxyMapSnapshot();
    const auto* snapshot = m_client->galaxyMapSnapshot();
    if (!snapshot)
        return;

    m_galaxyMapSnapshot = *snapshot;
    m_hasGalaxyMapSnapshot = true;
    m_hasGalaxyMapOverlay = true;
}




bool SpaceState::composeSystemMapSnapshot(int systemId)
{
    if (!m_client)
        return false;

    m_systemMapShowsEmptySector = false;
    const bool composed = m_client->composeSystemMapSnapshot(systemId);
    const auto* snapshot = m_client->systemMapSnapshot(systemId);
    const auto& metadata = m_client->systemMapMetadata();

    const bool sameTarget =
        m_hasSystemMapSnapshot && m_loadedSystemMapId == systemId;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedSystemMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_systemMapSnapshot = *snapshot;
        m_loadedSystemMapId = systemId;
        m_hasSystemMapSnapshot = true;
        m_appliedSystemMapServerTick = metadata.serverTick;
    }

    return m_hasSystemMapSnapshot && m_loadedSystemMapId == systemId;
}





bool SpaceState::composeDetailMapSnapshot(
    const world::celestial::DetailTarget& target)
{
    if (!m_client || !target.valid())
        return false;

    const bool composed = m_client->composeDetailMapSnapshot(target);
    const auto* snapshot = m_client->detailMapSnapshot(target);
    const auto& metadata = m_client->detailMapMetadata();

    const bool sameTarget =
        m_hasDetailMapSnapshot && m_loadedDetailTarget == target;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedDetailMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_authoritativeMapInterpolator.acceptDetail(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );
        m_detailMapSnapshot = m_authoritativeMapInterpolator.detail();
        m_loadedDetailTarget = target;
        m_hasDetailMapSnapshot = m_detailMapSnapshot.valid;
        m_appliedDetailMapServerTick = metadata.serverTick;
    }

    return m_hasDetailMapSnapshot && m_loadedDetailTarget == target;
}



bool SpaceState::composeHubMapSnapshot(
    int systemId,
    const std::string& hubId)
{
    if (!m_client || systemId < 0 || hubId.empty())
        return false;

    const bool composed = m_client->composeHubMapSnapshot(systemId, hubId);
    const auto* snapshot = m_client->hubMapSnapshot(systemId, hubId);
    const auto& metadata = m_client->hubMapMetadata();

    const bool sameTarget =
        m_hasHubMapSnapshot &&
        m_loadedHubMapSystemId == systemId &&
        m_loadedHubMapHubId == hubId;
    const bool newerSnapshot =
        snapshot != nullptr && metadata.serverTick > m_appliedHubMapServerTick;

    if (newerSnapshot || !sameTarget)
    {
        if (!snapshot)
            return composed && sameTarget;

        m_authoritativeMapInterpolator.acceptHub(
            *snapshot,
            metadata.serverTimeSeconds,
            metadata.universeTimelineRevision
        );
        m_hubMapSnapshot = m_authoritativeMapInterpolator.hub();
        m_loadedHubMapSystemId = systemId;
        m_loadedHubMapHubId = hubId;
        m_hasHubMapSnapshot = m_hubMapSnapshot.valid;
        m_appliedHubMapServerTick = metadata.serverTick;
    }

    return m_hasHubMapSnapshot &&
           m_loadedHubMapSystemId == systemId &&
           m_loadedHubMapHubId == hubId;
}





void SpaceState::beginSystemMapHubTransition(
    int systemId,
    const std::string& hubId
)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, systemId, hubId]()
        {
            if (!m_client ||
                !m_hasHubMapSnapshot ||
                m_loadedHubMapSystemId != systemId ||
                m_loadedHubMapHubId != hubId)
            {
                return;
            }

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Hub
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Local);
        }
    );
}


void SpaceState::setSystemMapHubMode()
{
    const auto mode = m_systemMapRenderer.mode();
    if (!m_client ||
        (mode != SystemMapRenderer::Mode::System &&
         mode != SystemMapRenderer::Mode::Detail))
    {
        return;
    }

    // A local-map drill belongs to the map context that is already loaded,
    // never to the player's unrelated current navigation membership. On
    // Details that is the loaded Details target: F11 opens Details without
    // composing a System map, so m_loadedSystemMapId may still be -1 there
    // (the Hub drill from such a Details map silently did nothing).
    const int selectedId =
        mode == SystemMapRenderer::Mode::Detail && m_loadedDetailTarget.valid() &&
                m_loadedDetailTarget.systemId >= 0
            ? m_loadedDetailTarget.systemId
            : m_loadedSystemMapId;
    const std::string hubId = m_systemMapRenderer.selectedHubId();

    // For a free-space tactical target, HUB means "open this object's local
    // neighborhood".  The renderer has already resolved the object's current
    // position to a terminal System cube, so the correct local view is Details
    // rather than a fictitious Hub snapshot.
    if (hubId.empty())
    {
        if (mode == SystemMapRenderer::Mode::System &&
            m_systemMapRenderer.canOpenSelectedLocalContext() &&
            m_systemMapRenderer.selectedTerminalDetailCell().has_value())
        {
            setSystemMapDetailMode();
        }
        return;
    }

    // Physical Hub composition requires a real system. Synthetic negative
    // empty-sector ids remain valid for the terminal-cube branch above.
    if (selectedId < 0)
        return;

    // System -> Hub is a shortcut, not a hierarchy bypass. Prepare the exact
    // parent Details scene as well, so the Hub panel can always navigate back
    // to Details even when the user never opened that layer first.
    if (mode == SystemMapRenderer::Mode::System)
    {
        world::celestial::DetailTarget detailTarget;
        if (!buildSelectedMapDetailTarget(detailTarget) ||
            !composeDetailMapSnapshot(detailTarget))
        {
            return;
        }
    }

    if (!composeHubMapSnapshot(selectedId, hubId))
        return;

    beginSystemMapHubTransition(selectedId, hubId);
}



void SpaceState::updateLiveMapSnapshots(float dt)
{
    if (m_client && !m_hasGalaxyMapOverlay)
        requestGalaxyMapSnapshotOnce();

    if (!m_client || !m_systemMapVisible)
    {
        m_systemMapLiveRefreshTimer = 0.0;
        m_detailMapLiveRefreshTimer = 0.0;
        m_hubMapLiveRefreshTimer = 0.0;
        return;
    }

    const double refreshSeconds = std::max(
        0.02,
        static_cast<double>(debug::get().render.systemMapLiveRefreshSec)
    );

    using Mode = SystemMapRenderer::Mode;
    switch (m_systemMapRenderer.mode())
    {
        case Mode::System:
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            if (!shouldRefreshSystemMapSnapshot())
            {
                m_systemMapLiveRefreshTimer = 0.0;
                return;
            }
            m_systemMapLiveRefreshTimer += dt;
            if (m_systemMapLiveRefreshTimer >= refreshSeconds)
            {
                m_systemMapLiveRefreshTimer = 0.0;
                composeSystemMapSnapshot(m_liveSystemMapId);
            }
            return;

        case Mode::Detail:
            m_systemMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            if (!m_loadedDetailTarget.valid())
            {
                m_detailMapLiveRefreshTimer = 0.0;
                return;
            }
            m_detailMapLiveRefreshTimer += dt;
            if (m_detailMapLiveRefreshTimer >= refreshSeconds)
            {
                m_detailMapLiveRefreshTimer = 0.0;
                followLocalCubeFocus();
                composeDetailMapSnapshot(m_loadedDetailTarget);
            }
            return;

        case Mode::Hub:
        {
            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;
            const int systemId = m_loadedHubMapSystemId;
            if (systemId < 0 || m_loadedHubMapHubId.empty())
            {
                m_hubMapLiveRefreshTimer = 0.0;
                return;
            }
            m_hubMapLiveRefreshTimer += dt;
            if (m_hubMapLiveRefreshTimer >= refreshSeconds)
            {
                m_hubMapLiveRefreshTimer = 0.0;
                composeHubMapSnapshot(systemId, m_loadedHubMapHubId);
            }
            return;
        }

        default:
            m_systemMapLiveRefreshTimer = 0.0;
            m_detailMapLiveRefreshTimer = 0.0;
            m_hubMapLiveRefreshTimer = 0.0;
            return;
    }
}



void SpaceState::updateLocalMapPresentationSnapshots(float /*dt*/)
{
    if (m_client)
    {
        m_authoritativeMapInterpolator.update(
            m_client->renderServerTimeSeconds(),
            m_client->renderUniverseTimeSeconds()
        );
    }

    if (m_hasDetailMapSnapshot &&
        m_authoritativeMapInterpolator.hasDetail())
    {
        m_detailMapSnapshot =
            m_authoritativeMapInterpolator.detail();
    }

    if (m_hasHubMapSnapshot &&
        m_authoritativeMapInterpolator.hasHub())
    {
        m_hubMapSnapshot =
            m_authoritativeMapInterpolator.hub();
    }

    /*
        Celestial rotation is predictable state. It is reconstructed every
        client frame from the synchronized universe clock, instead of being
        held/interpolated from a low-rate map response.
    */
    if (m_client)
    {
        const auto* celestial =
            m_client->celestialSnapshot();

        if (celestial)
        {
            if (m_hasDetailMapSnapshot)
            {
                game::client::applyClientCelestialPresentation(
                    m_detailMapSnapshot,
                    *celestial
                );
            }

            if (m_hasHubMapSnapshot)
            {
                game::client::applyClientCelestialPresentation(
                    m_hubMapSnapshot,
                    *celestial
                );
            }
        }
    }
}


void SpaceState::updateSystemMapLiveFlags()
{
    const bool wasSystemMapVisible =
        m_systemMapVisible;

    m_systemMapVisible =
        context().app &&
        context().app->gameUiMode() == GameUiMode::SystemMap;

    /*
        Первый вход в карту либо вход после прыжка.

        Сам SystemMapRenderer решает:
        - сохранить старую камеру;
        - или центрировать её на кубе игрока.
    */
    if (m_systemMapVisible &&
        !wasSystemMapVisible &&
        m_client &&
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        // Galaxy camera entry belongs only to an actual Galaxy presentation.
        // F10-F12 may now prepare another mode before the map is revealed.
        m_systemMapRenderer.onGalaxyMapEntered(
            m_galaxyMapSnapshot,
            m_client->playerNavigation()
        );
    }

    if (!m_systemMapVisible && wasSystemMapVisible)
    {
        // A hidden map must not retain a half-finished crossfade or an
        // asynchronous user-navigation request that can mutate its mode later.
        m_systemMapRenderer.cancelMapTransition();
    }

    m_systemMapLiveSnapshotsEnabled =
        m_systemMapVisible &&
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System;

    if (!m_systemMapLiveSnapshotsEnabled)
    {
        m_liveSystemMapId = -1;
        m_systemMapLiveRefreshTimer = 0.0;
        return;
    }

    if (m_systemMapShowsEmptySector)
    {
        /*
            У пустого сектора нет серверного systemId.
            Нельзя подменять его live snapshot текущей системы
            игрока на следующем update().
        */
        m_liveSystemMapId = -1;
    }
    else if (m_systemMapRenderer.focusedSystemId() >= 0)
    {
        m_liveSystemMapId =
            m_systemMapRenderer.focusedSystemId();
    }
    else if (m_client)
    {
        m_liveSystemMapId =
            m_client->playerNavigation().currentSystemId;
    }
    else
    {
        m_liveSystemMapId = -1;
    }
}

bool SpaceState::shouldRefreshSystemMapSnapshot() const
{
    return
        m_systemMapLiveSnapshotsEnabled &&
        m_liveSystemMapId >= 0 &&
        m_client != nullptr;
}


game::presentation::SystemMapPanelPresentation
SpaceState::buildNativeSystemMapPanelPresentation()
{
    game::presentation::SystemMapPanelPresentation empty;
    if (!context().app || !m_client)
        return empty;

    const auto& nav = m_client->playerNavigation();
    const auto& loc = context().app->localization();
    std::string currentSystemName = loc.text(
        "map.interstellar",
        "INTERSTELLAR"
    );

    if (nav.currentSystemId >= 0 && m_client->resolveCelestialSnapshot())
    {
        if (const auto* celestial = m_client->celestialSnapshot())
        {
            currentSystemName = loc.catalogName(
                "systems",
                std::to_string(nav.currentSystemId),
                celestial->systemName
            );
        }
    }

    int selectedId = -1;
    if (!m_systemMapShowsEmptySector)
    {
        selectedId = m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : nav.currentSystemId;
    }

    game::presentation::SystemMapPanelPresentationInput input;
    input.universeTimeSeconds = m_client->universeTimeSeconds();
    input.universeDate = m_client->sessionSnapshot().universeDate;
    input.universeTimeScale = m_client->sessionSnapshot().universeTimeScale;
    input.mode = m_systemMapRenderer.mode();
    input.galaxy = m_hasGalaxyMapSnapshot ? &m_galaxyMapSnapshot : nullptr;
    input.system = &m_systemMapSnapshot;
    input.navigation = &nav;
    input.currentSystemName = currentSystemName;
    input.selectedEmptySector = m_systemMapShowsEmptySector;
    input.selectedSystemId = selectedId;
    input.selectedBodyId = m_systemMapRenderer.selectedBodyId();
    input.selectedHubId = m_systemMapRenderer.selectedHubId();

    input.systemLayerIsSpace = m_systemMapShowsEmptySector;
    if (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Galaxy)
    {
        const auto entryIntent =
            m_systemMapRenderer.selectedGalaxyEntryIntent(m_galaxyMapSnapshot);
        if (entryIntent.has_value())
        {
            input.systemLayerIsSpace =
                entryIntent->type == game::system_map::MapIntentType::EnterEmptySector;
        }
        else
        {
            input.systemLayerIsSpace = nav.currentSystemId < 0;
        }
    }

    input.canOpenDetail =
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System &&
        m_systemMapRenderer.canOpenSelectedDetail();
    input.selectedDetailCell = m_systemMapRenderer.selectedTerminalDetailCell();
    input.canOpenHub =
        (m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System ||
         m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Detail) &&
        m_systemMapRenderer.canOpenSelectedLocalContext();

    auto panel = game::presentation::buildSystemMapPanelPresentation(input);
    m_systemMapRenderer.setWorldSitesCatalog(&m_client->catalogs().worldSites());
    // System / Detail: the information block (world model facts; never a hand-written status)
    const auto infoMode = m_systemMapRenderer.mode();
    const int infoSystemId =
        infoMode == SystemMapRenderer::Mode::Hub ? m_loadedHubMapSystemId
        : infoMode == SystemMapRenderer::Mode::Detail && m_loadedDetailTarget.valid()
            ? m_loadedDetailTarget.systemId
            : m_systemMapSnapshot.systemId;
    if ((infoMode == SystemMapRenderer::Mode::System || infoMode == SystemMapRenderer::Mode::Detail ||
         (infoMode == SystemMapRenderer::Mode::Hub && !m_loadedHubMapHubId.empty())) &&
        infoSystemId >= 0 && !m_systemMapShowsEmptySector)
    {
        game::presentation::SystemMapInfoInput info;
        info.systemId = infoSystemId;
        info.catalog = &m_client->catalogs().worldSites();
        info.system = &m_systemMapSnapshot;
        info.selectedObjectId = m_systemMapRenderer.activeObjectId();
        info.selectedBodyId = m_systemMapRenderer.selectedBodyId();
        // A hub selected on the map (its marker or one of its module
        // markers): the hub's facts + its composition (modules by block,
        // economy hub_modules data) in the panel.
        // (the Hub map: the hub it shows)
        std::string hubId = infoMode == SystemMapRenderer::Mode::Hub ? m_loadedHubMapHubId
                                                                     : m_systemMapRenderer.selectedHubId();
        if (hubId.empty())
            hubId = info.selectedObjectId;
        const auto hubIt = m_client->world().hubs().find(hubId);
        if (hubIt != m_client->world().hubs().end())
            info.selectedObjectId = hubId;
        const std::string economyId =
            info.selectedObjectId.rfind("site:", 0) == 0 ? info.selectedObjectId.substr(5) : info.selectedObjectId;
        if (const auto* economy = economyId.empty() ? nullptr : game::economy::EconomyModel::instance().hub(economyId))
            info.selectedHubModules = economy->composition;
        if (hubIt != m_client->world().hubs().end())
        {
            if (info.selectedHubModules.empty())
            {
                // no economy record: the modules attached to it in the replicated world
                std::map<std::string, int> modules;
                for (const auto& [id, object] : m_client->world().objects())
                {
                    (void)id;
                    if (object.hubAttachment.valid && object.hubAttachment.hubId == hubId)
                        ++modules[object.displayName.empty() ? std::string("модуль") : object.displayName];
                }
                for (const auto& [name, count] : modules)
                    info.selectedHubModules.push_back(count > 1 ? name + " ×" + std::to_string(count) : name);
            }
            if (!info.catalog->findSite(info.systemId, hubId))
                info.selectedHubName = hubIt->second.name.empty() ? hubId : hubIt->second.name;
        }
        game::presentation::buildSystemMapInfo(info, panel.infoTitle, panel.info);
        // the star-system list belongs to the Galaxy map; here: every hub and beacon of this system
        if (infoMode != SystemMapRenderer::Mode::Hub)
            panel.objects = game::presentation::buildSystemMapObjectList(info);
    }
    return panel;
}

void SpaceState::openSelectedGalaxyMapTarget()
{
    if (!m_client)
        return;

    const auto intent = m_systemMapRenderer.selectedGalaxyEntryIntent(
        m_galaxyMapSnapshot);

    if (intent.has_value())
    {
        using game::system_map::MapIntentType;
        if (intent->type == MapIntentType::EnterKnownSystem)
        {
            setSystemMapKnownSystemMode(intent->systemId);
            return;
        }
        if (intent->type == MapIntentType::EnterEmptySector)
        {
            setSystemMapEmptySectorMode(intent->positionLy);
            return;
        }
    }

    // No explicit Galaxy target: preserve the old panel behavior and open the
    // player's current system (or current interstellar sector).
    setSystemMapPlayerSystemMode();
}

void SpaceState::applyNativeSystemMapPanelAction(
    const game::presentation::SystemMapPanelAction& action)
{
    using game::presentation::SystemMapPanelCommandType;

    const auto command = game::presentation::resolveSystemMapPanelAction(
        action,
        m_systemMapRenderer.mode());

    switch (command.type)
    {
        case SystemMapPanelCommandType::None:
            return;

        case SystemMapPanelCommandType::OpenSelectedGalaxyTarget:
            openSelectedGalaxyMapTarget();
            return;

        case SystemMapPanelCommandType::SelectSystem:
            if (command.systemId >= 0)
                selectSystemMapSystem(command.systemId);
            return;

        case SystemMapPanelCommandType::Galaxy:
            setSystemMapGalaxyMode();
            return;

        case SystemMapPanelCommandType::LoadedSystem:
            setSystemMapLoadedSystemMode();
            return;

        case SystemMapPanelCommandType::LoadedDetail:
            setSystemMapLoadedDetailMode();
            return;

        case SystemMapPanelCommandType::SelectedDetail:
            setSystemMapDetailMode();
            return;

        case SystemMapPanelCommandType::Hub:
            setSystemMapHubMode();
            return;

        case SystemMapPanelCommandType::SelectObject:
            // a hub / beacon picked from the list: back to the System map if needed, then the
            // same selection a click on its marker makes (highlighted marker + facts in the panel)
            if (m_systemMapRenderer.mode() != SystemMapRenderer::Mode::System)
                setSystemMapLoadedSystemMode();
            m_systemMapRenderer.focusObjectFromList(command.objectId);
            return;
    }
}

bool SpaceState::handleNativeSystemMapPanelInput(const Viewport& viewport)
{
    if (!m_client)
        return false;

    GLFWwindow* window = glfwGetCurrentContext();
    if (!window)
        return false;

    double mouseX = 0.0;
    double mouseY = 0.0;
    glfwGetCursorPos(window, &mouseX, &mouseY);

    const bool insidePanel =
        m_inSessionPresentationRenderer.systemMapPanelContains(
            viewport,
            mouseX,
            mouseY);

    const bool leftDown =
        glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    // Scroll belongs to exactly one owner. The old WebView panel naturally
    // swallowed wheel input; the native panel must preserve that boundary.
    const double scrollY = insidePanel
        ? Input::instance().consumeScrollY()
        : 0.0;

    const auto panel = buildNativeSystemMapPanelPresentation();
    const auto action =
        m_inSessionPresentationRenderer.handleSystemMapPanelInput(
            viewport,
            panel,
            mouseX,
            mouseY,
            leftDown,
            scrollY);

    if (action.has_value())
        applyNativeSystemMapPanelAction(*action);

    return insidePanel;
}




void SpaceState::pushSystemMapState()
{
    if (!m_client)
        return;

    if (!m_client->resolveCelestialSnapshot())
        return;

    const auto* atlasPtr = m_client->starAtlas();
    const auto* celestialPtr = m_client->celestialSnapshot();
    if (!atlasPtr || !celestialPtr)
        return;

    const auto& atlas = *atlasPtr;
    const auto& celestial = *celestialPtr;
    const auto& nav = m_client->playerNavigation();

    json payload;

    payload["universeTimeSeconds"] =
        m_client->universeTimeSeconds();

    payload["universeDate"] =
        m_client->sessionSnapshot().universeDate;

    payload["universeTimeScale"] =
        m_client->sessionSnapshot().universeTimeScale;

    payload["currentSystemId"] = nav.currentSystemId;
    payload["system"]["id"] = celestial.systemId;
    payload["system"]["name"] = celestial.systemName;
    payload["simTimeSeconds"] = celestial.simTimeSeconds;

    payload["player"]["positionAu"] = {
        {"x", nav.systemLocalAu.x},
        {"y", nav.systemLocalAu.y},
        {"z", nav.systemLocalAu.z}
    };

    payload["player"]["forward"] = {
        {"x", nav.forward.x},
        {"y", nav.forward.y},
        {"z", nav.forward.z}
    };

    payload["systems"] = json::array();

    const world::celestial::GalaxyMapSystem* currentSystem = nullptr;



for (const auto& s : m_galaxyMapSnapshot.systems)
{
    if (s.id == nav.currentSystemId)
    {
        currentSystem = &s;
        break;
    }
}

auto distanceFromPlayerLy =
    [&](const world::celestial::GalaxyMapSystem& s) -> double
{
    if (!currentSystem)
        return 0.0;

    const double dx = s.positionLy.x - currentSystem->positionLy.x;
    const double dy = s.positionLy.y - currentSystem->positionLy.y;
    const double dz = s.positionLy.z - currentSystem->positionLy.z;

    return std::sqrt(dx * dx + dy * dy + dz * dz);
};


const int selectedId =
    m_systemMapRenderer.selectedSystemId() >= 0
        ? m_systemMapRenderer.selectedSystemId()
        : nav.currentSystemId;

for (const auto& s : m_galaxyMapSnapshot.systems)
{
    json item;
    item["id"] = s.id;
    item["name"] = s.name;
    item["starType"] = s.starType;
    item["starsCount"] = s.starsCount;
    item["xLy"] = s.positionLy.x;
    item["yLy"] = s.positionLy.y;
    item["zLy"] = s.positionLy.z;
    item["current"] = (s.id == nav.currentSystemId);
    item["selected"] = (s.id == selectedId);

    item["distanceFromPlayerLy"] = distanceFromPlayerLy(s);
    item["jurisdiction"] = s.jurisdiction.empty() ? "Unregistered" : s.jurisdiction;

    payload["systems"].push_back(std::move(item));
}

    payload["systemBodiesById"] = json::object();

    for (const auto& s : m_galaxyMapSnapshot.systems)
    {
        const auto* def = atlas.findSystem(s.id);
        if (!def)
            continue;

        payload["systemBodiesById"][std::to_string(s.id)] =
            celestialDefinitionToJson(*def);
    }

    payload["bodies"] = json::array();

    for (const auto& b : celestial.bodies)
    {
        json item;
        item["id"] = b.id;
        item["name"] = b.name;
        item["type"] = world::celestial::toString(b.type);
        item["parentId"] = b.parentId;

        item["positionAu"] = {
            {"x", b.positionAu.x},
            {"y", b.positionAu.y},
            {"z", b.positionAu.z}
        };

        item["radiusKm"] = b.radiusKm;
        item["orbitalPhaseRad"] = b.orbitalPhaseRad;
        item["rotationPhaseRad"] = b.rotationPhaseRad;

        item["rings"] = json::array();

        for (const auto& r : b.rings)
        {
            item["rings"].push_back({
                {"name", r.name},
                {"innerRadiusKm", r.innerRadiusKm},
                {"outerRadiusKm", r.outerRadiusKm},
                {"composition", r.composition}
            });
        }

        payload["bodies"].push_back(std::move(item));
    }

    context().htmlUi().broadcastState(HtmlUiPanelId::SystemMap, payload);
}





void SpaceState::selectSystemMapSystem(
    int systemId
)
{
    if (!m_client)
        return;

    /*
        В Galaxy выбор системы является движением внутри
        одной сцены. Здесь используется плавный перелёт камеры,
        без fade.
    */
    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Galaxy)
    {
        m_systemMapShowsEmptySector =
            false;

        m_systemMapRenderer.focusGalaxySystem(
            systemId,
            m_galaxyMapSnapshot
        );

        return;
    }

    /*
        System-system switching and Galaxy-system entry use the same
        atomic transition path.
    */
    setSystemMapKnownSystemMode(
        systemId
    );
}








void SpaceState::setSystemMapGalaxyMode()
{
    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::Galaxy)
    {
        return;
    }

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),

        [this]()
        {
            if (!m_client)
                return;

            requestGalaxyMapSnapshotOnce();

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Galaxy
            );

            m_systemMapRenderer.onGalaxyMapEntered(
                m_galaxyMapSnapshot,
                m_client->playerNavigation()
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Galaxy);

        }
    );
}










void SpaceState::setSystemMapEmptySectorMode(
    const glm::dvec3& positionLy
)
{
    if (!m_client)
        return;

    world::celestial::SystemMapSnapshot
        emptySector;

    /*
        Каждый пустой сектор получает отдельный отрицательный
        runtime-id. Благодаря этому SystemNavigationGrid и камера
        сбрасываются даже при переходе из одного пустого сектора
        в другой.
    */
    emptySector.systemId =
        m_nextEmptySystemMapId--;

    emptySector.systemName =
        "Deep Space Sector";

    emptySector.universeTimeSeconds =
        m_client->universeTimeSeconds();

    emptySector.universeTimeScale =
        m_client->sessionSnapshot().universeTimeScale;

    emptySector.universeDate =
        m_client->sessionSnapshot().universeDate;

    emptySector.systemPositionLy =
        positionLy;

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),

        [this, emptySector]()
        {
            if (!m_client)
                return;

            m_systemMapSnapshot =
                emptySector;

            m_loadedSystemMapId =
                emptySector.systemId;

            m_hasSystemMapSnapshot =
                true;

            m_systemMapShowsEmptySector =
                true;

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::System
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);

        }
    );
}





void SpaceState::beginSystemMapSystemTransition(int systemId)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, systemId]()
        {
            if (!m_client ||
                !m_hasSystemMapSnapshot ||
                m_loadedSystemMapId != systemId)
            {
                return;
            }

            m_systemMapShowsEmptySector = false;
            m_systemMapRenderer.focusGalaxySystem(
                systemId,
                m_galaxyMapSnapshot
            );
            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::System
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);
        }
    );
}


void SpaceState::setSystemMapKnownSystemMode(int systemId)
{
    if (!m_client || systemId < 0)
        return;

    if (!composeSystemMapSnapshot(systemId))
        return;

    beginSystemMapSystemTransition(systemId);
}





void SpaceState::setSystemMapLoadedSystemMode()
{
    if (!m_client ||
        m_systemMapRenderer.mode() == SystemMapRenderer::Mode::System)
    {
        return;
    }

    if (!m_hasSystemMapSnapshot)
    {
        setSystemMapPlayerSystemMode();
        return;
    }

    const int loadedSystemMapId = m_loadedSystemMapId;
    const bool loadedMapIsEmptySector = m_systemMapShowsEmptySector;

    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, loadedSystemMapId, loadedMapIsEmptySector]()
        {
            if (!m_client ||
                !m_hasSystemMapSnapshot ||
                m_loadedSystemMapId != loadedSystemMapId)
            {
                return;
            }

            m_systemMapShowsEmptySector = loadedMapIsEmptySector;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
            if (context().app)
            {
                context().app->adoptNavigationView(
                    NavigationPresentationView::System);
            }
        }
    );
}


void SpaceState::setSystemMapCurrentSystemMode()
{
    if (!m_client)
        return;

    if (m_systemMapRenderer.mode() ==
        SystemMapRenderer::Mode::System)
    {
        return;
    }

    const int selectedId =
        m_systemMapRenderer.selectedSystemId() >= 0
            ? m_systemMapRenderer.selectedSystemId()
            : m_client->playerNavigation().currentSystemId;

    setSystemMapKnownSystemMode(
        selectedId
    );
}








bool SpaceState::preparePlayerNavigationMapLevel(PlayerNavigationMapLevel level)
{
    if (!m_client)
        return false;

    // Direct F9-F12 selection is presentation preparation, not an internal
    // map animation. The global coordinator keeps the committed owner until
    // this concrete Navigation scene is ready. Cancel any renderer-internal
    // drill/crossfade so an outgoing map snapshot can never be painted over a
    // direct function-key destination.
    m_systemMapRenderer.cancelMapTransition();

    switch (level)
    {
        case PlayerNavigationMapLevel::Galaxy:
        {
            requestGalaxyMapSnapshotOnce();
            if (!m_hasGalaxyMapSnapshot)
                return false;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Galaxy);
            m_systemMapRenderer.onGalaxyMapEntered(
                m_galaxyMapSnapshot,
                m_client->playerNavigation()
            );
            return true;
        }

        case PlayerNavigationMapLevel::System:
        {
            const auto& nav = m_client->playerNavigation();
            if (nav.currentSystemId >= 0)
            {
                if (!composeSystemMapSnapshot(nav.currentSystemId))
                    return false;
                m_systemMapShowsEmptySector = false;
                m_systemMapRenderer.focusGalaxySystem(
                    nav.currentSystemId,
                    m_galaxyMapSnapshot
                );
                m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
                return true;
            }

            glm::dvec3 playerGalacticPositionLy {0.0};
            if (!resolvePlayerGalacticPositionLy(playerGalacticPositionLy))
                return false;

            world::celestial::SystemMapSnapshot emptySector;
            emptySector.systemId = m_nextEmptySystemMapId--;
            emptySector.systemName = "Deep Space Sector";
            emptySector.universeTimeSeconds = m_client->universeTimeSeconds();
            emptySector.universeTimeScale = m_client->sessionSnapshot().universeTimeScale;
            emptySector.universeDate = m_client->sessionSnapshot().universeDate;
            emptySector.systemPositionLy = playerGalacticPositionLy;
            m_systemMapSnapshot = std::move(emptySector);
            m_loadedSystemMapId = m_systemMapSnapshot.systemId;
            m_hasSystemMapSnapshot = true;
            m_systemMapShowsEmptySector = true;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::System);
            return true;
        }

        case PlayerNavigationMapLevel::Detail:
        {
            world::celestial::DetailTarget target;
            if (!buildPlayerDetailTarget(target, true))
                return preparePlayerNavigationMapLevel(PlayerNavigationMapLevel::System);
            // The parent System context first (the right panel facts, the
            // SYSTEM button and the Hub drill read the loaded System map).
            if (!composeSystemMapSnapshot(target.systemId))
                return false;
            if (!composeDetailMapSnapshot(target))
                return false;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
            return true;
        }

        case PlayerNavigationMapLevel::Local:
        {
            // An object selected on the open System / Detail map: its hub or
            // its cube. Otherwise (and always from flight): the player's.
            if (prepareSelectedObjectLocalMap())
                return true;

            const auto& nav = m_client->playerNavigation();
            if (nav.currentSystemId < 0)
                return preparePlayerNavigationMapLevel(PlayerNavigationMapLevel::System);

            const auto& ships = m_client->world().ships();
            const auto playerIt = ships.find(m_playerId.value);
            if (playerIt != ships.end())
            {
                const auto& motion = playerIt->second.transform.motion;
                if (motion.matchedToReferenceFrame && !motion.hubId.empty())
                {
                    if (!composeSystemMapSnapshot(nav.currentSystemId))
                        return false;

                    world::celestial::DetailTarget parentTarget;
                    if (!buildPlayerDetailTarget(parentTarget, true) ||
                        !composeDetailMapSnapshot(parentTarget) ||
                        !composeHubMapSnapshot(nav.currentSystemId, motion.hubId))
                    {
                        return false;
                    }
                    m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Hub);
                    return true;
                }
            }

            world::celestial::DetailTarget target;
            if (!buildPlayerDetailTarget(target, false) ||
                !composeSystemMapSnapshot(target.systemId) ||
                !composeDetailMapSnapshot(target))
            {
                return false;
            }
            std::cerr << "[LocalMap] F12: the player's cube " << target.spatialCell.x << ','
                      << target.spatialCell.y << ',' << target.spatialCell.z << std::endl;
            m_systemMapRenderer.setMode(SystemMapRenderer::Mode::Detail);
            return true;
        }
    }

    return false;
}


void SpaceState::setSystemMapPlayerSystemMode()
{
    if (!m_client)
        return;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId >= 0)
    {
        setSystemMapKnownSystemMode(nav.currentSystemId);
        return;
    }

    glm::dvec3 playerGalacticPositionLy {0.0};
    if (resolvePlayerGalacticPositionLy(playerGalacticPositionLy))
    {
        setSystemMapEmptySectorMode(playerGalacticPositionLy);
        return;
    }

}


void SpaceState::setSystemMapPlayerDetailMode()
{
    if (!m_client)
        return;

    world::celestial::DetailTarget target;
    if (!buildPlayerDetailTarget(target, true))
    {
        // Interstellar space has no system-local Details address. F10 remains
        // the highest meaningful navigation level there.
        setSystemMapPlayerSystemMode();
        return;
    }

    if (!composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}



void SpaceState::setSystemMapPlayerLocalMode()
{
    if (!m_client)
        return;

    const auto& nav = m_client->playerNavigation();
    if (nav.currentSystemId < 0)
    {
        setSystemMapPlayerSystemMode();
        return;
    }

    const auto& ships = m_client->world().ships();
    const auto playerIt = ships.find(m_playerId.value);
    if (playerIt != ships.end())
    {
        const auto& motion = playerIt->second.transform.motion;
        if (motion.matchedToReferenceFrame && !motion.hubId.empty())
        {
            if (!composeSystemMapSnapshot(nav.currentSystemId))
                return;

            world::celestial::DetailTarget parentTarget;
            if (!buildPlayerDetailTarget(parentTarget, true) ||
                !composeDetailMapSnapshot(parentTarget) ||
                !composeHubMapSnapshot(nav.currentSystemId, motion.hubId))
            {
                return;
            }
            beginSystemMapHubTransition(nav.currentSystemId, motion.hubId);
            return;
        }
    }

    world::celestial::DetailTarget target;
    if (!buildPlayerDetailTarget(target, false))
        return;

    if (!composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}




bool SpaceState::buildSelectedMapDetailTarget(
    world::celestial::DetailTarget& target
) const
{
    using namespace world::celestial;

    target = {};
    if (!m_client || !m_hasSystemMapSnapshot)
        return false;

    // Details is a refinement of the currently displayed System/Space map.
    // The loaded map context is authoritative for this drill; the player's
    // current physical membership may point at a completely different system.
    target.systemId = m_loadedSystemMapId;
    target.systemPositionLy = m_systemMapSnapshot.systemPositionLy;

    const std::string selectedBodyId = m_systemMapRenderer.selectedBodyId();
    const std::string selectedHubId = m_systemMapRenderer.selectedHubId();
    const std::string selectedHubParentBodyId =
        m_systemMapRenderer.selectedHubParentBodyId();

    if (!selectedBodyId.empty())
    {
        target.sceneKind = DetailSceneKind::CelestialBody;
        target.focusClass = DetailObjectClass::CelestialBody;
        target.anchorId = selectedBodyId;
        target.focusId = selectedBodyId;
    }
    else if (!selectedHubId.empty() && !selectedHubParentBodyId.empty())
    {
        target.sceneKind = DetailSceneKind::CelestialBody;
        target.focusClass = DetailObjectClass::Hub;
        target.anchorId = selectedHubParentBodyId;
        target.focusId = selectedHubId;
    }
    else if (!selectedHubId.empty())
    {
        target.sceneKind = DetailSceneKind::LocalObject;
        target.focusClass = DetailObjectClass::Hub;
        target.anchorId = selectedHubId;
        target.focusId = selectedHubId;
    }
    else
    {
        const auto selectedCell =
            m_systemMapRenderer.selectedTerminalDetailCell();
        if (!selectedCell)
            return false;

        target.sceneKind = DetailSceneKind::SpatialVolume;
        target.focusClass = DetailObjectClass::None;
        target.spatialCell = *selectedCell;
    }

    return target.valid();
}


void SpaceState::setSystemMapDetailMode()
{
    if (!m_client ||
        m_systemMapRenderer.mode() != SystemMapRenderer::Mode::System)
    {
        return;
    }

    world::celestial::DetailTarget target;
    if (!buildSelectedMapDetailTarget(target) ||
        !composeDetailMapSnapshot(target))
    {
        return;
    }

    beginSystemMapDetailTransition(target);
}



void SpaceState::beginSystemMapDetailTransition(
    const world::celestial::DetailTarget& target
)
{
    m_systemMapRenderer.beginMapTransition(
        MapTransitionPresets::modeChange(),
        [this, target]()
        {
            if (!m_client ||
                !m_hasDetailMapSnapshot ||
                m_loadedDetailTarget != target)
            {
                return;
            }

            m_systemMapRenderer.setMode(
                SystemMapRenderer::Mode::Detail
            );
            if (context().app)
                context().app->adoptNavigationView(
                    NavigationPresentationView::Detail);
        }
    );
}




void SpaceState::setSystemMapLoadedHubMode()
{
    if (!m_client || m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Hub)
        return;

    const int systemId = m_loadedHubMapSystemId;
    const std::string hubId = m_loadedHubMapHubId;
    if (systemId < 0 || hubId.empty() || !composeHubMapSnapshot(systemId, hubId))
        return;

    beginSystemMapHubTransition(systemId, hubId);
}

void SpaceState::restorePreviousSystemMapMode(
    const game::system_map::MapIntent& intent
)
{
    using game::system_map::MapMode;
    switch (intent.requestedMapMode)
    {
        case MapMode::Galaxy:
            setSystemMapGalaxyMode();
            return;
        case MapMode::System:
            // The system the map showed (an empty sector: the loaded one).
            if (intent.systemId == m_loadedSystemMapId || intent.systemId < 0)
                setSystemMapLoadedSystemMode();
            else
                setSystemMapKnownSystemMode(intent.systemId);
            return;
        case MapMode::Detail:
            setSystemMapLoadedDetailMode();
            return;
        case MapMode::Hub:
            setSystemMapLoadedHubMode();
            return;
    }
}

void SpaceState::setSystemMapLoadedDetailMode()
{
    if (!m_client || m_systemMapRenderer.mode() == SystemMapRenderer::Mode::Detail)
        return;

    const world::celestial::DetailTarget target = m_loadedDetailTarget;
    if (!target.valid())
    {
        setSystemMapPlayerDetailMode();
        return;
    }

    if (!m_hasDetailMapSnapshot && !composeDetailMapSnapshot(target))
        return;

    beginSystemMapDetailTransition(target);
}









void SpaceState::applyClientCatalogLocalization()
{
    if (!context().app)
        return;

    const auto& loc = context().app->localization();

    for (auto& system : m_galaxyMapSnapshot.systems)
    {
        system.name = loc.catalogName(
            "systems",
            std::to_string(system.id),
            system.name
        );
    }

    for (auto& object : m_galaxyMapSnapshot.objects)
    {
        object.name = loc.catalogName(
            "galaxy_objects",
            object.id,
            object.name
        );
    }

    std::unordered_map<int, std::string> systemDisplayNames;
    systemDisplayNames.reserve(m_galaxyMapSnapshot.systems.size());
    for (const auto& system : m_galaxyMapSnapshot.systems)
        systemDisplayNames[system.id] = system.name;
    m_sceneRenderer.setGameSystemDisplayNames(systemDisplayNames);

    if (m_systemMapSnapshot.systemId >= 0)
    {
        m_systemMapSnapshot.systemName = loc.catalogName(
            "systems",
            std::to_string(m_systemMapSnapshot.systemId),
            m_systemMapSnapshot.systemName
        );

        for (auto& body : m_systemMapSnapshot.bodies)
        {
            body.name = loc.catalogName(
                "bodies",
                std::to_string(m_systemMapSnapshot.systemId) + ":" + body.id,
                body.name
            );
        }

        for (auto& object : m_systemMapSnapshot.objects)
        {
            if (!object.stableId.empty())
            {
                object.name = loc.catalogName(
                    "hubs",
                    object.stableId,
                    object.name
                );
            }
        }
    }

    const auto localizeSceneObject =
        [&](world::celestial::LocalSceneObject& object, int systemId)
        {
            if (object.stableId.empty())
                return;

            switch (object.objectClass)
            {
                case world::celestial::DetailObjectClass::CelestialBody:
                    if (systemId >= 0)
                    {
                        object.name = loc.catalogName(
                            "bodies",
                            std::to_string(systemId) + ":" + object.stableId,
                            object.name
                        );
                    }
                    break;

                case world::celestial::DetailObjectClass::Hub:
                    object.name = loc.catalogName(
                        "hubs",
                        object.stableId,
                        object.name
                    );
                    break;

                default:
                    break;
            }
        };

    if (m_hasDetailMapSnapshot && m_detailMapSnapshot.valid)
    {
        if (m_detailMapSnapshot.systemId >= 0 &&
            !m_detailMapSnapshot.planetBodyId.empty())
        {
            m_detailMapSnapshot.planetName = loc.catalogName(
                "bodies",
                std::to_string(m_detailMapSnapshot.systemId) + ":" +
                    m_detailMapSnapshot.planetBodyId,
                m_detailMapSnapshot.planetName
            );
        }

        for (auto& orbit : m_detailMapSnapshot.hubOrbits)
        {
            orbit.name = loc.catalogName(
                "hubs",
                orbit.id,
                orbit.name
            );
        }

        for (auto& object : m_detailMapSnapshot.scene.objects)
        {
            localizeSceneObject(object, m_detailMapSnapshot.systemId);
            // the player's marker reads "player" (golden badge on the Detail map)
            if (object.player)
                object.name = loc.text("overlay.player_marker", "player");
        }
    }

    if (m_hasHubMapSnapshot && m_hubMapSnapshot.valid)
    {
        m_hubMapSnapshot.displayName = loc.catalogName(
            "hubs",
            m_hubMapSnapshot.hubId,
            m_hubMapSnapshot.displayName
        );

        for (auto& object : m_hubMapSnapshot.scene.objects)
            localizeSceneObject(object, m_hubMapSnapshot.systemId);
    }

    render::navigation::NavigationOverlayTextProfile overlayText;
    overlayText.player = loc.text("overlay.player", "PLAYER");
    overlayText.selected = loc.text("overlay.selected", "SELECTED");
    overlayText.cursor = loc.text("overlay.cursor", "CURSOR");
    overlayText.galaxy = loc.text("overlay.galaxy", "GALAXY");
    overlayText.system = loc.text("overlay.system", "SYSTEM");
    overlayText.edge = loc.text("overlay.edge", "EDGE");
    overlayText.format = loc.text("overlay.format", "FORMAT");
    overlayText.level = loc.text("overlay.level", "LEVEL");
    overlayText.track = loc.text("overlay.track", "TRACK");
    overlayText.trackOn = loc.text("overlay.track_on", "TRACK ON");
    overlayText.hierarchicalFormat =
        loc.text("nav.format.hierarchical", "STRAIGHT THERE");
    overlayText.axisFormat =
        loc.text("nav.format.axis", "THREE AXES");
    overlayText.packedFormat =
        loc.text("nav.format.packed", "VERY SECRET CODE");
    m_systemMapRenderer.setNavigationOverlayTextProfile(overlayText);

    game::system_map::NavigationMapTextProfile mapText;
    mapText.type = loc.text("map.object_info.type");
    mapText.name = loc.text("map.object_info.name");
    mapText.localSpeed = loc.text("map.object_info.local_speed");
    mapText.globalSpeed = loc.text("map.object_info.global_speed");
    mapText.azimuth = loc.text("map.object_info.azimuth");
    mapText.elevation = loc.text("map.object_info.elevation");
    mapText.owner = loc.text("map.object_info.owner");
    mapText.radius = loc.text("map.object_info.radius");
    mapText.address = loc.text("map.object_info.address");
    mapText.setWaypoint = loc.text("map.object_info.set_waypoint");
    mapText.setRendezvous = loc.text("map.object_info.set_rendezvous");
    mapText.cancelWaypoint = loc.text("map.object_info.cancel_waypoint");
    mapText.setFinish = loc.text("map.object_info.set_finish");
    mapText.cancelFinish = loc.text("map.object_info.cancel_finish");
    mapText.setIntermediate = loc.text("map.object_info.set_intermediate");
    mapText.cancelIntermediate = loc.text("map.object_info.cancel_intermediate");
    mapText.spaceTarget = loc.text("map.object_info.space_target");
    mapText.contextInfo = loc.text("map.context.info", "INFO");
    mapText.contextDetails = loc.text("map.context.details", "SHOW DETAILS");
    mapText.contextSurroundings = loc.text("map.context.surroundings", "VIEW SURROUNDINGS");
    mapText.starSystem = loc.text("map.context.star_system", "Star system");
    mapText.finishTarget = loc.text("map.object_info.finish_target");
    mapText.intermediateTarget = loc.text("map.object_info.intermediate_target");
    mapText.navigationPoint = loc.text("map.object_info.navigation_point");
    mapText.dockingPorts = loc.text("map.object_info.docking_ports");
    mapText.dockOpening = loc.text("map.object_info.dock_opening");
    mapText.shipEnvelope = loc.text("map.object_info.ship_envelope");
    mapText.dockFit = loc.text("map.object_info.dock_fit");
    mapText.dockStatus = loc.text("map.object_info.dock_status");
    mapText.dockAccess = loc.text("map.object_info.dock_access");
    mapText.dockOperational = loc.text("map.object_info.dock_operational");
    mapText.dockClearance = loc.text("map.object_info.dock_clearance");
    mapText.dockMaxEntrySpeed = loc.text("map.object_info.dock_max_entry_speed");
    mapText.calculateRoute = loc.text("map.object_info.calculate_route");
    mapText.showDockingRoute = loc.text("map.object_info.show_docking_route");
    mapText.startDocking = loc.text("map.object_info.start_docking");
    mapText.statusAvailable = loc.text("map.status.available");
    mapText.statusUnavailable = loc.text("map.status.unavailable");
    mapText.statusFree = loc.text("map.status.free");
    mapText.statusOccupied = loc.text("map.status.occupied");
    mapText.statusReserved = loc.text("map.status.reserved");
    mapText.statusAllowed = loc.text("map.status.allowed");
    mapText.statusDenied = loc.text("map.status.denied");
    mapText.statusClearanceRequired = loc.text("map.status.clearance_required");
    mapText.statusOnline = loc.text("map.status.online");
    mapText.statusOffline = loc.text("map.status.offline");
    mapText.statusDamaged = loc.text("map.status.damaged");
    mapText.statusUnknown = loc.text("map.status.unknown");
    mapText.routeTitle = loc.text("map.route.title");
    mapText.showOnHud = loc.text("map.route.show_on_hud");
    mapText.start = loc.text("map.route.start");
    mapText.waypoint = loc.text("map.route.waypoint");
    mapText.finish = loc.text("map.route.finish");
    mapText.dragWaypoints = loc.text("map.route.drag_waypoints");
    mapText.deleteRoute = loc.text("map.route.delete_route");
    mapText.deleteWaypoint = loc.text("map.route.delete_waypoint");
    mapText.yes = loc.text("confirm.yes");
    mapText.no = loc.text("confirm.no");
    mapText.arrivalSafeZone = loc.text("map.route.arrival.safe_zone");
    mapText.arrivalFollow = loc.text("map.route.arrival.follow");
    mapText.arrivalFormation = loc.text("map.route.arrival.formation");
    mapText.arrivalParade = loc.text("map.route.arrival.parade");
    m_systemMapRenderer.setNavigationMapTextProfile(mapText);
    m_systemMapRenderer.setNavigationNamingLocale(loc.locale());
}


void SpaceState::devToggleTrafficLanes()
{
    m_systemMapRenderer.toggleTrafficLanes();
}

void SpaceState::devSelectTrafficShip(const std::string& callsign)
{
    if (!m_client)
        return;
    for (const auto& [id, ship] : m_client->world().ships())
        if (ship.traffic.active && ship.traffic.callsign == callsign)
        {
            m_systemMapRenderer.devActivateObject("entity:" + std::to_string(id));
            std::cerr << "[MapScript] selected " << callsign << " entity:" << id << std::endl;
            return;
        }
    std::cerr << "[MapScript] traffic ship " << callsign << " not replicated" << std::endl;
}


// Orbital Transfer from the map: the client previews the tunnel with the same planner the
// server uses (Earth's state is recovered from the source hub's replicated ephemeris), then
// asks the server, which plans and flies the authoritative transfer.
void SpaceState::requestFreeFlightAutopilot(const std::string& bodyId, const std::string& landingSpec)
{
    if (!m_client)
        return;
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::FreeFlightAutopilot;
    command.index = 1;
    command.target = bodyId;
    command.detail = landingSpec;   // "pad:<lat>,<lon>" or empty = wild site search
    msg.payload = command;
    m_client->sendMessage(msg);
    std::cerr << "[Autopilot] requested -> " << (bodyId.empty() ? std::string("the dominant body") : bodyId) << std::endl;
}

void SpaceState::requestOrbitalTransfer(const std::string& targetHubId)
{
    namespace orb = game::navigation::orbital;
    if (!m_client)
        return;
    const auto& world = m_client->world();
    const auto shipIt = world.ships().find(m_playerId.value);
    const auto targetIt = world.hubs().find(targetHubId);
    if (shipIt == world.ships().end() || targetIt == world.hubs().end())
        return;
    const auto& motion = shipIt->second.transform.motion;
    const auto sourceIt = world.hubs().find(motion.hubId);
    m_systemMapRenderer.clearTransferTunnel();
    if (sourceIt != world.hubs().end() && sourceIt->second.parentBodyId == targetIt->second.parentBodyId && sourceIt->first != targetHubId)
    {
        const double t = m_client->universeTimeSeconds();
        const auto& src = sourceIt->second;
        const glm::dvec3 bodyPos = world::coordinates::fullMeters(src.worldPosition) - orb::targetPositionAt(src.motion, t);
        const glm::dvec3 bodyVel = src.worldVelocityMps - orb::targetVelocityAt(src.motion, t);
        const glm::dvec3 shipPos = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);
        orb::CentralBody body;
        body.radiusMeters = src.motion.parentRadiusMeters > 0.0 ? src.motion.parentRadiusMeters : body.radiusMeters;
        orb::TransferLimits limits;
        const orb::OrbitalTransferPlan plan =
            orb::planOrbitalTransfer(body, shipPos - bodyPos, motion.worldVelocityMps - bodyVel, t, targetIt->second.motion, limits);
        if (plan.feasible)
        {
            m_systemMapRenderer.setTransferTunnel(targetIt->second.parentBodyId, plan.tunnelMeters);
            std::cerr << "[OrbitalTransfer] preview -> " << targetIt->second.name << ": ETA " << plan.durationSeconds() << " s, path "
                      << plan.pathLengthMeters / 1000.0 << " km, peak " << plan.peakSpeedMps / 1000.0 << " km/s\n";
        }
        else
        {
            std::cerr << "[OrbitalTransfer] preview -> " << targetIt->second.name << ": " << plan.reason << "\n";
        }
    }
    else if (sourceIt != world.hubs().end() && sourceIt->first != targetHubId)
    {
        // other parent (Earth -> Ceres): the same compressed-time plan as the server, star-relative
        const double t = m_client->universeTimeSeconds();
        const auto& dst = targetIt->second;
        const glm::dvec3 starPos = world::coordinates::fullMeters(dst.worldPosition) - orb::targetPositionAt(dst.motion, t);
        const glm::dvec3 starVel = dst.worldVelocityMps - orb::targetVelocityAt(dst.motion, t);
        const glm::dvec3 shipPos = world::coordinates::fullMeters(shipIt->second.transform.worldPosition);
        const orb::OrbitalTransferPlan plan = orb::planCompressedTransit(shipPos - starPos, motion.worldVelocityMps - starVel, t, dst.motion, orb::TransferLimits {}, 90.0);
        m_systemMapRenderer.setTransferTunnel(dst.parentBodyId, plan.tunnelMeters);
        std::cerr << "[OrbitalTransfer] preview -> " << dst.name << ": compressed-time tunnel " << plan.pathLengthMeters / 1.495978707e11 << " AU\n";
    }
    game::network::ClientMessage msg;
    ClientShipCommand command;
    command.type = ClientShipCommand::RequestOrbitalTransfer;
    command.target = targetHubId;
    msg.payload = command;
    m_client->sendMessage(msg);
}
