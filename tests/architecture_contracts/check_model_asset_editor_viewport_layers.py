#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'src/assets/webui/model_asset_editor'
HTML = ROOT / 'src/assets/webui/model_asset_editor.html'
VERSION = ROOT / 'tools/model_asset_editor/EditorVersion.h'


def fail(message: str) -> None:
    print(f'MODEL ASSET EDITOR VIEWPORT LAYERS: FAIL\n - {message}')
    sys.exit(1)


required = [
    WEB / 'viewport/runtime.js',
    WEB / 'viewport/geometry.js',
    WEB / 'viewport/scene_bridge.js',
    WEB / 'viewport/scene.js',
]
for path in required:
    if not path.is_file():
        fail(f'missing {path.relative_to(ROOT)}')

read = lambda path: path.read_text(encoding='utf-8')
html = read(HTML)
runtime = read(WEB / 'viewport/runtime.js')
geometry = read(WEB / 'viewport/geometry.js')
bridge = read(WEB / 'viewport/scene_bridge.js')
scene = read(WEB / 'viewport/scene.js')
version = read(VERSION)

for token in [
    "./model_asset_editor/viewport/runtime.js",
    "./model_asset_editor/viewport/geometry.js",
    "./model_asset_editor/viewport/scene_bridge.js",
    "./model_asset_editor/viewport/scene.js",
    'createViewportRuntime({',
    'createViewportGeometryRuntime({',
    'createViewportSceneBridge()',
    'viewportSceneBridge.bind(createViewportSceneEffects({',
]:
    if token not in html:
        fail(f'HTML shell is missing viewport composition token {token!r}')

for forbidden in [
    'function initScene(', 'function initWorldAxes(', 'function resize()', 'function loop(',
    'function fitView(', 'function threeGeometry(', 'function configureSurfacePreviewGroups(',
    'function makeSurfacePreviewMaterials(', 'function defaultPreviewMaterial(',
    'function rebuildScene(', 'function updateVisibility(', 'new THREE.Scene(',
    'new THREE.WebGLRenderer(', 'new THREE.PerspectiveCamera(', 'new THREE.Raycaster()'
]:
    if forbidden in html:
        fail(f'HTML shell still owns viewport implementation: {forbidden}')

for token in ['createViewportRuntime', 'new THREE.Scene()', 'new THREE.WebGLRenderer', 'new THREE.PerspectiveCamera', 'new THREE.Raycaster', 'fitView', 'ResizeObserverClass']:
    if token not in runtime:
        fail(f'viewport runtime lost {token}')
for forbidden in ['WebSocket', '.send(', 'renderWizard', 'workingSaveRevision']:
    if forbidden in runtime:
        fail(f'viewport runtime absorbed non-render responsibility {forbidden!r}')

for token in ['lodHasGeometryPayload', 'createViewportGeometryRuntime', 'threeGeometry', 'pruneGeometryCache', 'makeSurfacePreviewMaterials', 'defaultPreviewMaterial']:
    if token not in geometry:
        fail(f'viewport geometry runtime lost {token}')
for forbidden in ['document.', 'window.', 'WebSocket', '.send(', 'renderWizard']:
    if forbidden in geometry:
        fail(f'viewport geometry runtime contains unrelated effect dependency {forbidden!r}')

for token in ['createViewportSceneBridge', 'rebuildScene', 'updateVisibility', 'bind(next)']:
    if token not in bridge:
        fail(f'viewport scene bridge lost {token}')
for forbidden in ['THREE.', 'document.', 'window.', 'WebSocket', '.send(']:
    if forbidden in bridge:
        fail(f'viewport scene bridge is not effect-neutral: {forbidden!r}')

for token in ['createViewportSceneEffects', 'function rebuildScene(', 'function updateVisibility(', 'new THREE.Group()', 'new THREE.Mesh(']:
    if token not in scene:
        fail(f'viewport scene effects lost {token}')
for forbidden in ['document.', 'window.', 'WebSocket', '.send(', 'request_catalog', 'workingSaveRevision']:
    if forbidden in scene:
        fail(f'viewport scene effects absorbed non-viewport responsibility {forbidden!r}')

if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.80"', version):
    fail('editor version must be 0.10.80')

print('MODEL ASSET EDITOR VIEWPORT LAYERS: PASS')
print(' - scene/bootstrap lifecycle is outside the HTML shell')
print(' - raycaster and camera-fit ownership are isolated in viewport runtime')
print(' - geometry cache and THREE materialization are isolated')
print(' - rebuildScene and visibility orchestration are isolated behind a late-bound bridge')
print(' - transport/session/application layers remain outside the viewport boundary')
