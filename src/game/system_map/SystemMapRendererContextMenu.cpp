// SystemMapRenderer: map buttons (one row, bottom right), map back history
// and the object context menu - the same in every map.

#include "src/game/system_map/SystemMapRendererInternal.h"

namespace
{
    constexpr std::size_t MapHistoryLimit = 24;

    bool cubeIsChildOf(
        const game::system_map::GalaxyHierarchyBox& box,
        const game::navigation::CubicNavigationCell& cell
    )
    {
        return game::system_map::galaxyHierarchyIsChild(box, cell);
    }
}

// ============================================================================
// Map buttons: back (previous map), up (one level up), TRACK, LEVEL 0.
// A button exists only where its command does: no up arrow at the top
// level, no back arrow without a previous map.
// ============================================================================

std::vector<SystemMapRenderer::MapButton> SystemMapRenderer::mapButtons(
    const Viewport& viewport
) const
{
    using render::navigation::NavigationCoordinateOverlay;
    std::vector<MapButton> buttons;
    const auto& ui = m_navigationCoordinateOverlay.textProfile();
    const Mode mode = m_modeState.current();
    const auto add = [&](const char* id, std::string label,
                         const render::navigation::NavigationOverlayButtonBounds& bounds, bool enabled = true,
                         bool active = false) {
        MapButton button;
        button.id = id;
        button.view.label = std::move(label);
        button.view.bounds = bounds;
        button.view.enabled = enabled;
        button.view.active = active;
        button.view.hovered = enabled && m_hoveredMapButton == id;
        buttons.push_back(std::move(button));
    };

    std::optional<render::navigation::NavigationOverlayButtonBounds> leftmost;
    if (mode == Mode::Galaxy || mode == Mode::System)
    {
        add("level0", ui.level + " 0", NavigationCoordinateOverlay::levelZeroButtonBounds(viewport));
        leftmost = buttons.back().view.bounds;
        if (mode == Mode::System)
        {
            const auto& state = m_systemView.state();
            add("track", state.selectedBodyTrackingEnabled ? ui.trackOn : ui.track,
                NavigationCoordinateOverlay::trackButtonBounds(viewport), !state.selectedBodyId.empty(),
                state.selectedBodyTrackingEnabled);
            leftmost = buttons.back().view.bounds;
        }
        const bool upAvailable =
            mode == Mode::Galaxy
                ? !m_galaxyView.state().hierarchyParent.overview()
                : m_systemView.state().navigationGrid.enabled() && m_systemView.state().hierarchyParent.level > 0;
        const auto upBounds = NavigationCoordinateOverlay::upButtonBounds(viewport, mode == Mode::System);
        if (upAvailable)
            add("up", "↑", upBounds);
        // The back slot stays left of the up slot (stable positions).
        leftmost = upBounds;
    }
    if (!m_mapHistory.empty())
        add("back", "←",
            NavigationCoordinateOverlay::backButtonBounds(viewport, leftmost ? &*leftmost : nullptr));
    return buttons;
}

// ============================================================================
// Back history
// ============================================================================

SystemMapRenderer::MapHistoryEntry SystemMapRenderer::captureHistoryEntry(
    Mode mode
) const
{
    MapHistoryEntry entry;
    entry.mode = mode;
    entry.galaxy = m_galaxyView.bookmark();
    entry.system = m_systemView.bookmark(m_systemView.state().lastCameraFitSystemId);
    entry.detailCamera = m_detailView.camera();
    entry.detailHubId = m_detailView.state().selectedHubId;
    entry.detailHubParentBodyId = m_detailView.state().selectedHubParentBodyId;
    entry.hubCamera = m_hubView.camera();
    return entry;
}

void SystemMapRenderer::recordMapHistory(
    Mode previousMode,
    Mode mode
)
{
    const bool restoring = m_pendingHistoryRestore && m_pendingHistoryRestore->mode == mode;
    if (restoring)
        return;
    m_pendingHistoryRestore.reset();
    m_mapHistory.push_back(captureHistoryEntry(previousMode));
    if (m_mapHistory.size() > MapHistoryLimit)
        m_mapHistory.erase(m_mapHistory.begin());
}

void SystemMapRenderer::applyMapHistoryRestore(
    Mode mode
)
{
    if (!m_pendingHistoryRestore || m_pendingHistoryRestore->mode != mode)
        return;
    const MapHistoryEntry entry = *m_pendingHistoryRestore;
    m_pendingHistoryRestore.reset();
    switch (mode)
    {
        case Mode::Galaxy:
            // Applied after onGalaxyMapEntered (which frames the entry).
            m_pendingGalaxyBookmark = entry.galaxy;
            break;
        case Mode::System:
            // Applied by the presentation builder once the system is shown.
            m_systemView.state().pendingBookmark = entry.system;
            break;
        case Mode::Detail:
            m_detailView.camera() = entry.detailCamera;
            m_detailView.camera().rotating = false;
            m_detailView.camera().panning = false;
            m_detailView.selectHub(entry.detailHubId, entry.detailHubParentBodyId);
            break;
        case Mode::Hub:
            m_hubView.camera() = entry.hubCamera;
            m_hubView.camera().rotating = false;
            m_hubView.camera().panning = false;
            break;
    }
}

std::optional<game::system_map::MapIntent> SystemMapRenderer::navigateBack()
{
    if (m_mapHistory.empty())
        return std::nullopt;
    m_pendingHistoryRestore = m_mapHistory.back();
    m_mapHistory.pop_back();
    m_contextMenu.close();
    return game::system_map::MapIntent::restoreMap(m_pendingHistoryRestore->mode,
                                                   m_pendingHistoryRestore->system.systemId);
}

// ============================================================================
// Context menu
// ============================================================================

game::system_map::MapObjectOverlayItem SystemMapRenderer::makeWaypointCandidate(
    char prefix,
    const game::navigation::CubicNavigationCell& cell,
    const glm::dvec3& worldPositionMeters,
    const glm::dvec2& screenPx
) const
{
    game::system_map::MapObjectOverlayItem candidate;
    candidate.objectId = waypointCandidateId(prefix, cell);
    candidate.infoKind = game::system_map::MapObjectInfoKind::WaypointCandidate;
    candidate.typeName = m_navigationMapTextProfile.navigationPoint;
    candidate.name = m_navigationMapTextProfile.spaceTarget;
    candidate.drawGlyph = true;
    candidate.pointerInteractive = true;
    candidate.screenAffordance = true;
    candidate.glyphScale = 0.78;
    candidate.hitRadiusPx = 12.0;
    candidate.facingScreenDirection = glm::dvec2(0.0, -1.0);
    candidate.factionColor = glm::vec4(0.20f, 0.66f, 1.00f, 0.78f);
    candidate.extraFields.push_back({"address", navigationCellAddress(prefix, cell), ""});
    candidate.trackingWorldPosition = world::coordinates::makeWorldPositionFromMeters(worldPositionMeters);
    candidate.hasTrackingWorldPosition = true;
    candidate.screenPx = screenPx;
    candidate.visible = true;
    return candidate;
}

// The route commands of an overlay item: its own panel actions (route,
// docking), else the route roles it can take.
void SystemMapRenderer::appendContextActionRows(
    const game::system_map::MapObjectOverlayItem& item
)
{
    std::vector<game::system_map::MapObjectPanelAction> actions = item.panelActions;
    if (actions.empty() && item.hasTrackingWorldPosition && item.objectId != "player")
    {
        const auto target = routeTargetRefForOverlayItem(item);
        if (target.valid())
        {
            const auto& routePlan = m_navigationWorkspace.routePlan();
            if (const auto* waypoint = routePlan.findByTarget(target))
            {
                actions = waypointPanelActions(routePlan, *waypoint);
            }
            else
            {
                game::navigation::NavigationWaypoint fresh;
                actions = waypointPanelActions(routePlan, fresh);
            }
        }
    }
    for (const auto& action : actions)
    {
        game::system_map::MapContextMenu::Row row;
        row.label = game::system_map::MapObjectOverlayRenderer::text(m_navigationMapTextProfile, action.labelKey);
        if (row.label.empty())
            row.label = action.labelKey;
        row.command = "action:" + action.key;
        row.enabled = action.enabled;
        m_contextMenu.rows.push_back(std::move(row));
    }
}

bool SystemMapRenderer::openContextMenu(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::HubMapSnapshot& hub,
    const glm::vec2& at,
    double nowSeconds
)
{
    using Menu = game::system_map::MapContextMenu;
    const auto& text = m_navigationMapTextProfile;
    Menu menu;
    menu.mode = m_modeState.current();

    // 1. An overlay object glyph (ship, hub, dock, waypoint ...).
    const game::system_map::MapObjectOverlayFrame* frame = nullptr;
    if (menu.mode == Mode::Galaxy)
        frame = &m_galaxyInfoOverlayFrame;
    else if (menu.mode == Mode::System)
        frame = &m_systemSceneFrame.interaction.objectOverlay;
    else if (menu.mode == Mode::Detail)
        frame = &m_detailPresentation.frame.objectOverlay;
    else
        frame = &m_hubPresentation.frame.objectOverlay;

    const float margin =
        static_cast<float>(game::presentation::pointerInteractionSettings().glyphHitMarginPx);
    const game::system_map::MapObjectOverlayItem* glyph = nullptr;
    float glyphDistance = 1.0e9f;
    for (const auto& item : frame->items)
    {
        if (!item.visible || !item.pointerInteractive || !item.drawGlyph || item.objectId.empty())
            continue;
        const float d = glm::length(glm::vec2(item.screenPx) - at);
        if (d <= static_cast<float>(item.hitRadiusPx) + margin && d < glyphDistance)
        {
            glyph = &item;
            glyphDistance = d;
        }
    }

    // A waypoint-candidate glyph presents its cube: the cube's menu.
    std::optional<game::navigation::CubicNavigationCell> glyphCell;
    if (glyph && glyph->infoKind == game::system_map::MapObjectInfoKind::WaypointCandidate)
    {
        if (menu.mode == Mode::Galaxy && m_galaxyView.state().navigationGrid.hasSelectedCell() &&
            glyph->objectId == waypointCandidateId('G', m_galaxyView.state().navigationGrid.selectedCell()))
            glyphCell = m_galaxyView.state().navigationGrid.selectedCell();
        if (menu.mode == Mode::System && m_systemView.state().navigationGrid.hasSelectedCell() &&
            glyph->objectId == waypointCandidateId('S', m_systemView.state().navigationGrid.selectedCell()))
            glyphCell = m_systemView.state().navigationGrid.selectedCell();
    }

    if (glyph && !glyphCell)
    {
        menu.targetKind = Menu::TargetKind::OverlayItem;
        menu.objectId = glyph->objectId;
        menu.title = glyph->name.empty() ? glyph->typeName : glyph->name;
        if (menu.title.empty())
            menu.title = text.spaceTarget;
        m_objectOverlayState.activate(glyph->objectId);
        menu.hubId = glyph->navigationHubId;
        menu.hubParentBodyId = glyph->navigationHubParentBodyId;
        m_contextMenu = menu;
        m_contextMenu.rows.push_back({text.contextInfo, "info", true});
        appendContextActionRows(*glyph);
        const bool detailHub = menu.mode == Mode::Detail && !glyph->navigationHubId.empty();
        const bool systemHub = menu.mode == Mode::System && glyph->kind == game::system_map::MapObjectGlyphKind::Hub;
        if (detailHub || systemHub)
            m_contextMenu.rows.push_back({text.contextDetails, "details", true});
        m_contextMenu.open = true;
        m_contextMenu.placeAt(at, vp.width, vp.height);
        return true;
    }

    // 2. The map's own resolver.
    if (menu.mode == Mode::Galaxy)
    {
        using Kind = game::system_map::GalaxyPointerTargetKind;
        game::system_map::GalaxyPointerTarget target;
        if (glyphCell)
        {
            target.kind = Kind::NavigationCube;
            target.cell = glyphCell;
        }
        else
        {
            game::system_map::GalaxyMapInputFrame inputFrame;
            inputFrame.viewport = vp;
            inputFrame.localMouseX = at.x;
            inputFrame.localMouseY = at.y;
            inputFrame.inside = true;
            target = game::system_map::resolveGalaxyPointer(
                         m_galaxyInteraction.pointerScene(m_galaxyView, galaxy, inputFrame), at)
                         .winner;
        }
        if (target.kind != Kind::Star && target.kind != Kind::Medium && target.kind != Kind::NavigationCube)
            return false;

        // The menu target is selected (a single click without click tracking).
        auto& state = m_galaxyView.state();
        state.cubeClickTracker.reset();
        state.starClickTracker.reset();
        state.mediumClickTracker.reset();
        const auto chosen = m_galaxyInteraction.applyChosenTarget(m_galaxyView, galaxy, vp, target, at.x, at.y, nowSeconds);
        state.cubeClickTracker.reset();
        state.starClickTracker.reset();
        state.mediumClickTracker.reset();

        if (target.kind == Kind::Star)
        {
            const auto star = std::find_if(galaxy.systems.begin(), galaxy.systems.end(),
                                           [&](const auto& s) { return s.id == target.systemId; });
            if (star == galaxy.systems.end())
                return false;
            menu.targetKind = Menu::TargetKind::Star;
            menu.systemId = star->id;
            menu.pointLy = star->positionLy;
            menu.title = star->name;
            menu.objectId = "galaxy-system:" + std::to_string(star->id);

            // The star's card item (route target: the system position).
            auto& grid = m_galaxyView.state().navigationGrid;
            const auto terminal = grid.cell(grid.nearestIndexForPositionLy(star->positionLy, grid.maximumLevel()),
                                            grid.maximumLevel());
            game::system_map::MapObjectOverlayItem item =
                makeWaypointCandidate('G', terminal, star->positionLy * world::coordinates::MetersPerLightYear,
                                      glm::dvec2(at));
            item.objectId = menu.objectId;
            item.typeName = text.starSystem;
            item.name = star->name;
            item.drawGlyph = false;
            item.pointerInteractive = false;
            m_galaxySystemCard = item;
            m_galaxySystemCardSystemId = star->id;

            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", true});
            appendContextActionRows(item);
            m_contextMenu.rows.push_back({text.contextDetails, "details", true});
        }
        else if (target.kind == Kind::Medium)
        {
            if (target.mediumObject < 0 || target.mediumObject >= static_cast<int>(galaxy.objects.size()))
                return false;
            menu.targetKind = Menu::TargetKind::Medium;
            menu.mediumObject = target.mediumObject;
            menu.pointLy = target.hitLy;
            menu.title = galaxy.objects[static_cast<std::size_t>(target.mediumObject)].name;
            menu.objectId = "medium:" + galaxy.objects[static_cast<std::size_t>(target.mediumObject)].id;
            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", true});
            m_contextMenu.rows.push_back({text.contextSurroundings, "surroundings", true});
        }
        else
        {
            if (!target.cell)
                return false;
            const auto cell = *target.cell;
            menu.targetKind = Menu::TargetKind::Cube;
            menu.cell = cell;
            menu.pointLy = cell.center;
            menu.title = navigationCellAddress('G', cell);
            auto candidate =
                makeWaypointCandidate('G', cell, cell.center * world::coordinates::MetersPerLightYear, glm::dvec2(at));
            menu.objectId = candidate.objectId;
            m_galaxyWaypointCandidate = candidate;
            refreshGalaxyWaypointCandidate(vp, galaxy);
            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", true});
            appendContextActionRows(m_galaxyWaypointCandidate ? *m_galaxyWaypointCandidate : candidate);
            m_contextMenu.rows.push_back({text.contextSurroundings, "surroundings", true});
        }
        (void)chosen;
        m_contextMenu.open = true;
        m_contextMenu.placeAt(at, vp.width, vp.height);
        return true;
    }

    if (menu.mode == Mode::System)
    {
        using Kind = game::system_map::SystemPointerTarget::Kind;
        const game::system_map::SystemMapFrameInteractionContext context(
            m_systemSceneFrame.interaction, m_systemView.controls());
        game::system_map::SystemPointerTarget target;
        if (glyphCell)
        {
            target.kind = Kind::Cube;
            target.cell = glyphCell;
            target.pointAu = glyphCell->center;
        }
        else
        {
            target = m_systemInteraction.resolvePointer(m_systemView, context, vp, at.x, at.y);
        }
        if (target.kind == Kind::Empty)
            return false;

        game::system_map::SystemMapInputResult selection;
        m_systemInteraction.selectTarget(m_systemView, context, target, selection);
        m_systemView.state().bodyClickTracker.reset();
        m_systemView.state().cubeClickTracker.reset();

        const auto& items = m_systemSceneFrame.interaction.objectOverlay.items;
        const auto findItem = [&](const std::string& id) -> const game::system_map::MapObjectOverlayItem* {
            const auto it = std::find_if(items.begin(), items.end(), [&](const auto& item) { return item.objectId == id; });
            return it == items.end() ? nullptr : &*it;
        };

        if (target.kind == Kind::Body)
        {
            menu.targetKind = Menu::TargetKind::Body;
            menu.bodyId = target.bodyId;
            menu.objectId = "body:" + std::to_string(system.systemId) + ":" + target.bodyId;
            const auto* item = findItem(menu.objectId);
            menu.title = item && !item->name.empty() ? item->name : target.bodyId;
            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", item != nullptr});
            if (item)
                appendContextActionRows(*item);
            m_contextMenu.rows.push_back({text.contextDetails, "details", true});
        }
        else if (target.kind == Kind::Hub)
        {
            menu.targetKind = Menu::TargetKind::Hub;
            menu.hubId = target.hubId;
            menu.hubParentBodyId = target.hubParentBodyId;
            menu.objectId = target.hubId;
            const auto* item = findItem(menu.objectId);
            menu.title = item && !item->name.empty() ? item->name : target.hubId;
            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", item != nullptr});
            if (item)
                appendContextActionRows(*item);
            m_contextMenu.rows.push_back({text.contextDetails, "details", true});
        }
        else
        {
            if (!target.cell)
                return false;
            const auto cell = *target.cell;
            menu.targetKind = Menu::TargetKind::Cube;
            menu.cell = cell;
            menu.pointLy = cell.center;
            menu.title = navigationCellAddress('S', cell);
            auto candidate = makeWaypointCandidate(
                'S', cell,
                system.systemPositionLy * world::coordinates::MetersPerLightYear +
                    cell.center * world::celestial::MetersPerAu,
                glm::dvec2(at));
            menu.objectId = candidate.objectId;
            m_systemWaypointCandidate = candidate;
            refreshSystemWaypointCandidate(vp, system);
            m_contextMenu = menu;
            m_contextMenu.rows.push_back({text.contextInfo, "info", true});
            appendContextActionRows(m_systemWaypointCandidate ? *m_systemWaypointCandidate : candidate);
            const bool terminal = cell.level >= m_systemView.state().navigationGrid.maximumLevel();
            m_contextMenu.rows.push_back(
                terminal ? Menu::Row {text.contextDetails, "details", true}
                         : Menu::Row {text.contextSurroundings, "surroundings", true});
        }
        m_contextMenu.open = true;
        m_contextMenu.placeAt(at, vp.width, vp.height);
        return true;
    }

    // 3. Hub map: an infrastructure module under the pointer.
    if (menu.mode == Mode::Hub)
    {
        const auto* body = pickHubInfrastructureBody(m_hubPresentation.camera, hub, glm::dvec2(at));
        if (!body || body->stableId.empty())
            return false;
        const std::string objectId = std::string(HubModuleOverlayPrefix) + body->stableId;
        const auto& items = m_hubPresentation.frame.objectOverlay.items;
        const auto item = std::find_if(items.begin(), items.end(), [&](const auto& c) { return c.objectId == objectId; });
        if (item == items.end())
            return false;
        menu.targetKind = Menu::TargetKind::OverlayItem;
        menu.objectId = objectId;
        menu.title = item->name.empty() ? item->typeName : item->name;
        m_objectOverlayState.activate(objectId);
        m_contextMenu = menu;
        m_contextMenu.rows.push_back({text.contextInfo, "info", true});
        appendContextActionRows(*item);
        m_contextMenu.open = true;
        m_contextMenu.placeAt(at, vp.width, vp.height);
        return true;
    }
    return false;
}

std::optional<game::system_map::MapIntent> SystemMapRenderer::executeContextCommand(
    const game::system_map::MapContextMenu& menu,
    const std::string& command,
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    double nowSeconds
)
{
    using Menu = game::system_map::MapContextMenu;
    using game::system_map::MapIntent;
    const glm::dvec2 viewportSize(vp.width, vp.height);

    if (command == "info")
    {
        if (menu.targetKind == Menu::TargetKind::Medium)
            return std::nullopt; // the cloud card follows its selection
        const auto* item = currentOverlayItem(menu.objectId);
        if (!item && menu.targetKind == Menu::TargetKind::Star && m_galaxySystemCard)
            item = &*m_galaxySystemCard;
        if (!item)
            return std::nullopt;
        m_objectOverlayState.activate(item->objectId);
        m_objectOverlayState.ensureOpen(*item, viewportSize);
        return std::nullopt;
    }

    if (command.rfind("action:", 0) == 0)
    {
        const std::string key = command.substr(7);
        if (key == "show_docking_route" || key == "start_docking")
            applyDockingAction(menu.objectId, key);
        else
            applyWaypointAction(menu.objectId, key);
        return std::nullopt;
    }

    if (command == "details")
    {
        switch (menu.targetKind)
        {
            case Menu::TargetKind::Star:
                return MapIntent::enterKnownSystem(menu.systemId, menu.pointLy);
            case Menu::TargetKind::Body:
                return MapIntent::openBody(menu.bodyId);
            case Menu::TargetKind::Hub:
                return MapIntent::openHub(menu.hubId, menu.hubParentBodyId);
            case Menu::TargetKind::Cube:
                // A terminal System cube: its Details (the cube is selected).
                m_systemView.state().navigationCellExplicitlySelected = true;
                return MapIntent::openBody(std::string());
            case Menu::TargetKind::OverlayItem:
                if (menu.mode == Mode::Detail && !menu.hubId.empty())
                {
                    m_detailView.selectHub(menu.hubId, menu.hubParentBodyId);
                    return MapIntent::openHub(menu.hubId, menu.hubParentBodyId);
                }
                if (menu.mode == Mode::System && !menu.objectId.empty())
                {
                    auto& state = m_systemView.state();
                    state.selectedBodyId.clear();
                    state.selectedHubId = menu.hubId.empty() ? menu.objectId : menu.hubId;
                    state.selectedHubParentBodyId = menu.hubParentBodyId;
                    return MapIntent::openHub(state.selectedHubId, menu.hubParentBodyId);
                }
                return std::nullopt;
            default:
                return std::nullopt;
        }
    }

    if (command == "surroundings")
    {
        if (menu.mode == Mode::Galaxy)
        {
            auto& grid = m_galaxyView.state().navigationGrid;
            std::optional<game::navigation::CubicNavigationCell> cell = menu.cell;
            if (!cell)
            {
                // A cloud point: the child cube holding it, approached.
                cell = m_galaxyView.hierarchyChildAt(menu.pointLy);
                if (cell && m_galaxyView.enterHierarchyCell(*cell, vp, nowSeconds, false))
                {
                    m_galaxyView.approachHierarchyPoint(menu.pointLy, vp, nowSeconds);
                    announceNavigationLevel('G', m_galaxyView.hierarchyBox().childLevel);
                    return std::nullopt;
                }
            }
            if (!cell)
                return std::nullopt;
            if (cell->level >= grid.maximumLevel())
            {
                const MapIntent intent = m_galaxyView.entryIntentForPosition(galaxy, cell->center, -1);
                if (intent.valid())
                    return intent;
                return std::nullopt;
            }
            if (cubeIsChildOf(m_galaxyView.hierarchyBox(), *cell) &&
                m_galaxyView.enterHierarchyCell(*cell, vp, nowSeconds))
                announceNavigationLevel('G', m_galaxyView.hierarchyBox().childLevel);
            return std::nullopt;
        }
        if (menu.mode == Mode::System && menu.cell)
        {
            const game::system_map::SystemMapFrameInteractionContext context(
                m_systemSceneFrame.interaction, m_systemView.controls());
            game::system_map::SystemPointerTarget target;
            target.kind = game::system_map::SystemPointerTarget::Kind::Cube;
            target.cell = menu.cell;
            target.pointAu = menu.cell->center;
            game::system_map::SystemMapInputResult result;
            if (m_systemInteraction.descendTarget(m_systemView, context, vp, target, nowSeconds, result) &&
                result.systemLevelChanged)
                announceNavigationLevel('S', *result.systemLevelChanged);
        }
        (void)system;
        return std::nullopt;
    }
    return std::nullopt;
}

/*
    Pointer for the context menu, before anything else of the map: an open
    menu owns the pointer (row press = command, any other press closes it);
    right click (press + release without a drag) or Ctrl + left press opens
    it on the object under the pointer. Returns true when the frame's
    pointer was consumed here.
*/
bool SystemMapRenderer::handleContextMenuPointer(
    const Viewport& vp,
    const world::celestial::GalaxyMapSnapshot& galaxy,
    const world::celestial::SystemMapSnapshot& system,
    const world::celestial::HubMapSnapshot& hub,
    GLFWwindow* window,
    double mx,
    double my,
    double localMx,
    double localMy,
    bool inside,
    bool leftDown,
    bool rightDown,
    double nowSeconds,
    std::optional<game::system_map::MapIntent>& intent
)
{
    const bool leftPress = leftDown && !m_contextLeftWasDown;
    const bool rightPress = rightDown && !m_contextRightWasDown;
    const bool rightRelease = !rightDown && m_contextRightWasDown;
    m_contextLeftWasDown = leftDown;
    m_contextRightWasDown = rightDown;
    const glm::vec2 local(static_cast<float>(localMx), static_cast<float>(localMy));

    const auto swallow = [&]() {
        m_pendingScrollY = 0.0;
        m_galaxyView.suppressCameraGesture(leftDown, rightDown, mx, my);
        m_systemView.suppressCameraGesture(leftDown, rightDown, mx, my);
        for (auto* camera : {&m_detailView.camera(), &m_hubView.camera()})
        {
            camera->rotating = false;
            camera->panning = false;
            camera->lastMouseX = mx;
            camera->lastMouseY = my;
        }
        m_navigationOverlayLeftWasDown = leftDown;
    };

    // A left press the menu took stays the menu's until it is released.
    if (m_contextSwallowLeft)
    {
        if (!leftDown)
            m_contextSwallowLeft = false;
        swallow();
        return true;
    }

    if (rightPress && inside)
    {
        m_contextRightPressPx = local;
        m_contextRightCandidate = true;
    }
    if (rightDown && m_contextRightCandidate &&
        glm::length(local - m_contextRightPressPx) > static_cast<float>(m_systemView.controls().clickMoveThresholdPx))
    {
        m_contextRightCandidate = false; // a pan
    }

    if (m_contextMenu.open && m_contextMenu.mode != m_modeState.current())
        m_contextMenu.close();

    if (m_contextMenu.open)
    {
        m_contextMenu.hovered = m_contextMenu.rowAt(localMx, localMy);
        const bool escape = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (leftPress)
        {
            const int row = m_contextMenu.hovered;
            const auto menu = m_contextMenu;
            m_contextMenu.close();
            if (row >= 0 && menu.rows[static_cast<std::size_t>(row)].enabled)
                intent = executeContextCommand(menu, menu.rows[static_cast<std::size_t>(row)].command, vp, galaxy,
                                               system, nowSeconds);
            m_contextSwallowLeft = true;
        }
        else if (escape || (rightPress && !m_contextMenu.contains(localMx, localMy)) || m_pendingScrollY != 0.0)
        {
            m_contextMenu.close();
            m_contextRightCandidate = false;
        }
        swallow();
        return true;
    }

    const bool ctrlDown =
        glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    const bool ctrlClick = inside && leftPress && ctrlDown;
    const bool rightClick = rightRelease && m_contextRightCandidate;
    if (rightRelease)
        m_contextRightCandidate = false;
    if (!ctrlClick && !rightClick)
        return false;

    const glm::vec2 at = ctrlClick ? local : m_contextRightPressPx;
    // Map chrome and cards keep their own clicks.
    if (m_objectOverlayState.panelUnderCursor(glm::dvec2(at)))
        return false;
    for (const auto& button : mapButtons(vp))
        if (button.view.bounds.contains(at.x, at.y))
            return false;

    const bool opened = openContextMenu(vp, galaxy, system, hub, at, nowSeconds);
    if (ctrlClick)
    {
        m_contextSwallowLeft = true;
        swallow();
        return true;
    }
    if (opened)
        swallow();
    return opened;
}

void SystemMapRenderer::drawContextMenu(
    const Viewport& vp
)
{
    if (!m_contextMenu.open || m_contextMenu.mode != m_modeState.current())
        return;
    render::navigation::NavigationContextMenuView view;
    const glm::vec4 b = m_contextMenu.bounds();
    view.bounds = {b.x, b.y, b.z, b.w};
    view.headerHeightPx = game::system_map::MapContextMenu::HeaderHeightPx;
    view.rowHeightPx = game::system_map::MapContextMenu::RowHeightPx;
    view.title = m_contextMenu.title;
    for (const auto& row : m_contextMenu.rows)
    {
        view.rows.push_back(row.label);
        view.enabled.push_back(row.enabled);
    }
    view.hovered = m_contextMenu.hovered;
    m_navigationCoordinateOverlay.drawContextMenu(vp, view);
}
