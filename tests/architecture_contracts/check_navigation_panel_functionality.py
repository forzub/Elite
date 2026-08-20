#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(message):
    print(f'[FAIL] native navigation panel/drill contract: {message}', file=sys.stderr)
    raise SystemExit(1)

app = read('src/core/Application.cpp') + read('src/core/Application.h')
space = read('src/game/SpaceState.cpp') + read('src/game/SpaceState.h')
renderer = read('src/ui/presentation/InSessionPresentationRenderer.cpp') + read('src/ui/presentation/InSessionPresentationRenderer.h')
presentation = read('src/game/presentation/SystemMapPanelPresentation.h')
map_renderer = read('src/game/system_map/SystemMapRenderer.cpp') + read('src/game/system_map/SystemMapRenderer.h')

# The single-surface panel exposes semantic layer destinations. Button slots
# must not encode toggle/current-layer/close behavior.
for token in (
    'SystemMapPanelActionType',
    'SelectSystem',
    'OpenGalaxy',
    'OpenSystem',
    'OpenDetail',
    'OpenHub',
    'SystemMapPanelNavigationActions',
    'buildSystemMapPanelNavigationActions',
):
    if token not in presentation:
        fail(f'typed panel action disappeared: {token}')

for token in (
    'handleSystemMapPanelInput(',
    'm_systemDropdownOpen',
    'm_systemDropdownFirstRow',
    'buildSystemMapPanelNavigationActions(panel)',
    'Action::OpenGalaxy',
    'Action::OpenSystem',
    'Action::OpenDetail',
    'Action::OpenHub',
):
    if token not in renderer:
        fail(f'native interactive STAR ATLAS control missing: {token}')

# Freeze the semantic destination routes. Parent layers return to the loaded
# map context; they must not re-resolve from the player's current system.
for token in (
    'openSelectedGalaxyMapTarget()',
    'selectSystemMapSystem(command.systemId)',
    'setSystemMapGalaxyMode()',
    'setSystemMapLoadedSystemMode()',
    'setSystemMapLoadedDetailMode()',
    'setSystemMapDetailMode()',
    'setSystemMapHubMode()',
):
    if token not in space:
        fail(f'native panel no longer reaches map action: {token}')

for token in (
    'selectedGalaxyEntryIntent(',
    'MapIntentType::EnterKnownSystem',
    'MapIntentType::EnterEmptySector',
):
    if token not in space + map_renderer:
        fail(f'Galaxy selected cube/system entry path missing: {token}')

# A committed F9 target must not be replayed every frame after an internal
# Galaxy->System/Detail/Hub drill. Preparation is only for a pending F-key
# request; the coordinator is then synchronized to the map mode actually shown.
if '!m_gameUi.requestPending()' not in app:
    fail('committed F9-F12 target is still re-prepared every frame')
if 'void Application::adoptNavigationView' not in app:
    fail('internal map drill cannot synchronize the global presentation target')
for view in ('Galaxy', 'System', 'Detail', 'Local'):
    if f'NavigationPresentationView::{view}' not in space:
        fail(f'internal map transition does not adopt {view} presentation target')

for forbidden in (
    'system_map_open_selected:',
    'system_map_select:',
    'system_map_current_system',
    'system_map_planet',
):
    if forbidden in app + space + renderer:
        fail(f'old browser command transport returned: {forbidden}')

for forbidden in ('ToggleMode', 'SystemMapPanelActionType::Close', 'layout.closeButton'):
    if forbidden in presentation + renderer:
        fail(f'fixed-slot/toggle/close map-panel behavior returned: {forbidden}')

for token in (
    'systemLayerIsSpace',
    'm_loadedSystemMapId',
    'setSystemMapLoadedSystemMode()',
):
    if token not in space + presentation + renderer:
        fail(f'loaded System/Space context contract missing: {token}')

print('[PASS] native STAR ATLAS semantic layer actions + selected-space drill remain functional')
