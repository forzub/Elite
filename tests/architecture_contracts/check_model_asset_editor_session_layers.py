#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'src/assets/webui/model_asset_editor'
HTML = ROOT / 'src/assets/webui/model_asset_editor.html'
VERSION = ROOT / 'tools/model_asset_editor/EditorVersion.h'


def fail(message: str) -> None:
    print(f'MODEL ASSET EDITOR SESSION LAYERS: FAIL\n - {message}')
    sys.exit(1)


required = [
    WEB / 'session/router.js',
    WEB / 'session/asset_acceptance.js',
    WEB / 'session/handlers.js',
    WEB / 'persistence/working_asset.js',
    WEB / 'persistence/settings.js',
]
for path in required:
    if not path.is_file():
        fail(f'missing {path.relative_to(ROOT)}')

read = lambda path: path.read_text(encoding='utf-8')
html = read(HTML)
router = read(WEB / 'session/router.js')
asset = read(WEB / 'session/asset_acceptance.js')
handlers = read(WEB / 'session/handlers.js')
working = read(WEB / 'persistence/working_asset.js')
settings = read(WEB / 'persistence/settings.js')
version = read(VERSION)

for token in [
    "./model_asset_editor/session/router.js",
    "./model_asset_editor/session/asset_acceptance.js",
    "./model_asset_editor/session/handlers.js",
    "./model_asset_editor/persistence/working_asset.js",
    "./model_asset_editor/persistence/settings.js",
    'createBackendMessageRouter(backendMessageHandlers)',
]:
    if token not in html:
        fail(f'HTML shell is missing session composition token {token!r}')

for forbidden in [
    'function handle(msg)',
    'if(msg.type===',
    'function acceptAssetState(msg,fullPayload)',
    'function mergeAssetMetadata(next,msg)',
    'function retainGeometryPayload(target,source)',
]:
    if forbidden in html:
        fail(f'HTML shell still owns session implementation: {forbidden}')

if 'const state={ws:null,' in html or 'state.ws' in html:
    fail('legacy socket state leaked back into the editor state object')

for token in ['createBackendMessageRouter', 'registry[type]', 'handler(message)']:
    if token not in router:
        fail(f'declarative message router lost {token}')
for forbidden in ['document.', 'window.', 'THREE.', 'WebSocket', 'state.', '.send(']:
    if forbidden in router:
        fail(f'message router contains effect dependency {forbidden!r}')

for token in ['retainGeometryPayload', 'mergeAssetMetadata', 'acceptAssetState', 'resetAssetScopedSession', 'resetFullPayloadSession']:
    if token not in asset:
        fail(f'asset acceptance adapter lost {token}')
for forbidden in ['WebSocket', '.send(', 'new THREE']:
    if forbidden in asset:
        fail(f'asset acceptance absorbed transport/renderer construction via {forbidden!r}')

expected_types = [
    'source_directory_updated', 'working_saved', 'source_change_scan_result',
    'source_mesh_deletion_confirmed', 'geometry_preflight_result', 'wizard_validation_report',
    'semantic_tree_patch', 'semantic_binding_patch', 'lod_runtime_metadata_patch', 'surface_metadata_patch',
    'asset_binary_begin', 'lod_payload_binary_begin', 'asset_metadata', 'model_preflight_result',
    'lod_analysis_result', 'lod_generator_preview_result', 'lod_generator_apply_result', 'lod_payload',
    'geometry_scan_result', 'wizard_state_patch', 'wizard_stage_checked', 'settings', 'settings_saved',
    'catalog', 'progress', 'status', 'asset'
]
for message_type in expected_types:
    if not re.search(rf'\b{re.escape(message_type)}\s*:', handlers):
        fail(f'session handler registry is missing backend type {message_type!r}')
for forbidden in ['WebSocket', 'new THREE', '.send(']:
    if forbidden in handlers:
        fail(f'session handlers absorbed transport/renderer construction via {forbidden!r}')

for token in ['createWorkingAssetPersistence', 'handleWorkingSaved', 'workingSaveRevision', 'workingPackageBytes']:
    if token not in working:
        fail(f'WORKING persistence adapter lost {token}')
for token in ['createSettingsPersistence', 'handleSettings', 'handleSettingsSaved', 'handleStatusError']:
    if token not in settings:
        fail(f'settings persistence adapter lost {token}')
for name, text in [('working persistence', working), ('settings persistence', settings)]:
    for forbidden in ['WebSocket', 'THREE.', '.send(']:
        if forbidden in text:
            fail(f'{name} contains transport/render dependency {forbidden!r}')

if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.84"', version):
    fail('editor version must be 0.10.84')

print('MODEL ASSET EDITOR SESSION LAYERS: PASS')
print(' - backend JSON dispatch uses a declarative type -> handler registry')
print(' - asset metadata/full-payload acceptance is isolated from the HTML shell')
print(' - WORKING SAVE bookkeeping is isolated in persistence effects')
print(' - settings load/save/error acknowledgement is isolated in persistence effects')
print(' - transport remains below the session/domain effect boundary')
