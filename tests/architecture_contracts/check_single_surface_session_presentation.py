#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(msg):
    print(f'[FAIL] single-surface F1-F12 presentation: {msg}', file=sys.stderr)
    raise SystemExit(1)

app = read('src/core/Application.cpp')
app_h = read('src/core/Application.h')
coord_h = read('src/ui/presentation/GamePresentationCoordinator.h')
space = read('src/game/SpaceState.cpp')
native = read('src/ui/presentation/InSessionPresentationRenderer.cpp')

# F1-F12 session surfaces are all OpenGL scene-backed. Browser documents are
# deliberately restricted to MainMenu/Loading/SessionMenu.
web_pred = coord_h[coord_h.find('constexpr bool isFullScreenWebDocument'):coord_h.find('\n    }', coord_h.find('constexpr bool isFullScreenWebDocument'))]
for forbidden in ('GameUiMode::Flight', 'GameUiMode::SystemMap', 'GameUiMode::ServicePanel'):
    if forbidden in web_pred:
        fail(f'{forbidden} leaked back into WebView document ownership')
for required in ('GameUiMode::MainMenu', 'GameUiMode::Loading', 'GameUiMode::SessionMenu'):
    if required not in web_pred:
        fail(f'non-session WebView document classification missing: {required}')

for forbidden in (
    'm_mapPanelWebView', 'evalMapPanelUiScript', 'processMapPanelWebViewCommands',
    'system_map_panel_state_prepared|', 'service_panel_ready|', 'service_panel_prepared|',
):
    if forbidden in app + app_h + space:
        fail(f'legacy multi-HWND in-session presentation survived: {forbidden}')

if (ROOT / 'src/assets/webui/system_map_panel.html').exists():
    fail('system_map_panel.html survived even though navigation panel is native')
if (ROOT / 'src/assets/webui/service_shell.html').exists():
    fail('service_shell.html survived even though F5-F8 are native')

for token in (
    'renderInSessionPresentationOverlay',
    'renderSystemMapPanel',
    'renderServicePanel',
    'const bool scene3dActive = flightMode || systemMapMode',
    'if ((systemMapMode || serviceMode))',
):
    if token not in app + space + native:
        fail(f'one-framebuffer in-session render seam missing: {token}')

# Native browser visibility is allowed only at explicit document boundaries.
request_start = app.find('void Application::requestPresentationTarget')
request_end = app.find('void Application::requestFlightView', request_start)
request_body = app[request_start:request_end]
service_start = request_body.find('if (target.mode == GameUiMode::ServicePanel)')
service_end = request_body.find('if (target.mode == GameUiMode::SystemMap)', service_start)
service_body = request_body[service_start:service_end]
for forbidden in ('m_documentWebViews', 'setVisible', 'bringToFront', 'navigate(', 'DwmFlush', 'flushDesktopCompositor'):
    if forbidden in service_body:
        fail(f'F5-F8 request path still touches native browser compositor: {forbidden}')

map_start = request_body.find('if (target.mode == GameUiMode::SystemMap)')
map_body = request_body[map_start:]
for forbidden in ('m_documentWebViews', 'setVisible', 'bringToFront', 'navigate(', 'm_mapPanel'):
    if forbidden in map_body:
        fail(f'F9-F12 request path still touches WebView: {forbidden}')

print('[PASS] active-session F1-F12 use one GLFW/OpenGL presentation surface')
