#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(message):
    print(f'[FAIL] direct Navigation presentation: {message}', file=sys.stderr)
    raise SystemExit(1)

app = read('src/core/Application.cpp')
space = read('src/game/SpaceState.cpp')
renderer = read('src/game/system_map/SystemMapRenderer.cpp')

# SystemMapRenderer may keep its outgoing-snapshot crossfade for in-map drill
# actions. Direct F9-F12 selectors may not enter it: otherwise the previous map
# is deliberately drawn over the new target and appears as a background image.
for token in ('drawMapTransitionSnapshot(', 'm_mapTransition.outgoingAlpha()'):
    if token not in renderer:
        fail(f'expected in-map transition implementation changed: {token}')

for forbidden in (
    'space->setSystemMapGalaxyMode()',
    'space->setSystemMapPlayerSystemMode()',
    'space->setSystemMapPlayerDetailMode()',
    'space->setSystemMapPlayerLocalMode()',
):
    if forbidden in app:
        fail(f'F9-F12 direct selector invokes map crossfade route: {forbidden}')

for token in (
    'space.preparePlayerNavigationMapLevel(level)',
    'm_systemMapRenderer.cancelMapTransition();',
    'm_gameUi.armSceneTarget(requested)',
):
    if token not in app + space:
        fail(f'direct selector preparation/commit seam missing: {token}')

for forbidden in (
    'm_mapPanelExpectedStateSerial',
    'm_mapPanelPreparedStateSerial',
    'system_map_panel_state_prepared|',
):
    if forbidden in app + space:
        fail(f'obsolete WebView readiness seam returned: {forbidden}')

print('[PASS] F9-F12 direct Navigation targets never blend an outgoing map snapshot over the destination')
