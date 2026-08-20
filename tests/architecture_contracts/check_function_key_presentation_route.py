#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(message):
    print(f'[FAIL] F1-F12 presentation route: {message}', file=sys.stderr)
    raise SystemExit(1)

window = read('src/window/Window.cpp') + read('src/window/Window.h')
app = read('src/core/Application.cpp') + read('src/core/Application.h')
router = read('src/ui/presentation/PresentationFunctionKeyRouter.cpp')
coord = read('src/ui/presentation/GamePresentationCoordinator.cpp') + read('src/ui/presentation/GamePresentationCoordinator.h')
space = read('src/game/SpaceState.cpp') + read('src/game/SpaceState.h')
contract = read('tests/architecture_contracts/PresentationCoordinatorContractTests.cpp')

for token in ('struct FunctionKeyPress', 'std::deque<FunctionKeyPress>', 'WM_KEYDOWN', 'WM_SYSKEYDOWN',
              'm_functionKeyPresses.push_back(press)', 'pressedSincePoll', '0x0001', 'pollFunctionKeyPress'):
    if token not in window:
        fail(f'physical message-backed key edge missing: {token}')

expected = (
    (1, 'forFlight(FlightPresentationView::Front)'),
    (2, 'forFlight(FlightPresentationView::Rear)'),
    (3, 'forFlight(FlightPresentationView::FrontDrone)'),
    (4, 'forFlight(FlightPresentationView::Drone)'),
    (9, 'forNavigation(NavigationPresentationView::Galaxy)'),
    (10, 'forNavigation(NavigationPresentationView::System)'),
    (11, 'forNavigation(NavigationPresentationView::Detail)'),
    (12, 'forNavigation(NavigationPresentationView::Local)'),
)
for key, target in expected:
    if f'case {key}: return GameUiTarget::{target}' not in router:
        fail(f'F{key} direct mapping missing')
    if f'directTargetForFunctionKey({key})' not in contract:
        fail(f'F{key} executable assertion missing')

for key in (5,6,7,8):
    if f'directTargetForFunctionKey({key})' not in contract:
        fail(f'F{key} executable service assertion missing')

for token in ('requestedTarget()', 'sceneTarget()', 'committedTarget()', 'requestedSerial()',
              'requestTarget(', 'armSceneTarget(', 'commitRequested('):
    if token not in coord + app:
        fail(f'coordinator primitive missing: {token}')

if 'target.mode == GameUiMode::ServicePanel' not in app or 'm_gameUi.armSceneTarget(target);' not in app:
    fail('F5-F8 are not armed as native scene targets')
if 'preparePlayerNavigationMapLevel(level)' not in app or 'm_gameUi.armSceneTarget(requested)' not in app:
    fail('F9-F12 are not prepared/armed through the common scene path')

for forbidden in ('m_mapPanelWebView', 'system_map_panel_state_prepared|', 'service_panel_ready|', 'service_panel_prepared|'):
    if forbidden in app + space:
        fail(f'legacy WebView/session-specific key dependency survived: {forbidden}')

# Freeze latest-request-wins and no-intermediate-commit behavior in executable C++.
for phrase in ('Latest request wins', 'Service -> pending Map -> Flight', 'assert(ui.committedTarget() == trade)',
               'assert(ui.requestedTarget() == local)', 'assert(ui.committedTarget() == front)'):
    if phrase not in contract:
        fail(f'executable key-transition invariant missing: {phrase}')

print('[PASS] F1-F12 message-backed routing + one-surface prepared-to-commit chain')
