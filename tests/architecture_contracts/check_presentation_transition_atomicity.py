#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]

def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

def fail(message):
    print(f'[FAIL] presentation atomicity: {message}', file=sys.stderr)
    raise SystemExit(1)

app = read('src/core/Application.cpp')
app_h = read('src/core/Application.h')
coord = read('src/ui/presentation/GamePresentationCoordinator.cpp')
coord_h = read('src/ui/presentation/GamePresentationCoordinator.h')
space = read('src/game/SpaceState.cpp')
renderer = read('src/render/Renderer.cpp')

# Visible and staged state stay separate until the destination frame swaps.
for token in ('committedTarget()', 'requestedTarget()', 'sceneTarget()', 'armSceneTarget', 'commitRequested'):
    if token not in app + coord + coord_h:
        fail(f'presentation transaction primitive missing: {token}')
if 'm_window->swapBuffers();' not in app:
    fail('frame swap boundary disappeared')
swap = app.find('m_window->swapBuffers();')
commit = app.find('commitPreparedPresentationAfterSwap', swap)
if commit < swap:
    fail('scene presentation can commit before framebuffer swap')

# F1-F12 are all scene-backed; browser front/back is reserved for explicit
# non-game-session documents only.
web_pred = coord_h[coord_h.find('isFullScreenWebDocument'):coord_h.find('\n    }', coord_h.find('isFullScreenWebDocument'))]
for forbidden in ('GameUiMode::Flight', 'GameUiMode::SystemMap', 'GameUiMode::ServicePanel'):
    if forbidden in web_pred:
        fail(f'{forbidden} is again a browser document')
for required in ('GameUiMode::MainMenu', 'GameUiMode::Loading', 'GameUiMode::SessionMenu'):
    if required not in web_pred:
        fail(f'expected browser document missing: {required}')

for forbidden in ('m_mapPanelWebView', 'system_map_panel.html', 'service_shell.html', 'service_panel.js'):
    if forbidden in app + app_h + space:
        fail(f'multi-surface in-session presentation survived: {forbidden}')

# The OpenGL backing is frame-owned. No parked browser presentation may composite
# stale scene pixels, and each post-process frame clears its own FBO.
if 'const bool scene3dActive = flightMode || systemMapMode;' not in app:
    fail('post-process is not restricted to real 3D session scenes')
begin_pp = renderer[renderer.find('bool Renderer::beginPostProcess('):renderer.find('void Renderer::endPostProcess')]
for token in ('glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFramebuffer)', 'glClear(', 'GL_STENCIL_BUFFER_BIT'):
    if token not in begin_pp:
        fail(f'post-process FBO frame ownership missing: {token}')

# Fullscreen document double buffering remains for MainMenu/Loading/ESC only.
for token in ('m_documentWebViews[2]', 'beginDocumentPreparation', 'acknowledgeDocumentPrepared',
              'incoming.setVisible(true)', 'incoming.bringToFront()'):
    if token not in app + app_h + coord + coord_h:
        fail(f'non-session document double buffer missing: {token}')

print('[PASS] F1-F12 presentation commits atomically on one OpenGL surface')
