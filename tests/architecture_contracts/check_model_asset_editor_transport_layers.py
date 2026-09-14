#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'src/assets/webui/model_asset_editor'
HTML = ROOT / 'src/assets/webui/model_asset_editor.html'
VERSION = ROOT / 'tools/model_asset_editor/EditorVersion.h'


def fail(message: str) -> None:
    print(f'MODEL ASSET EDITOR TRANSPORT LAYERS: FAIL\n - {message}')
    sys.exit(1)


required = [
    WEB / 'transport/diagnostics.js',
    WEB / 'transport/commands.js',
    WEB / 'transport/binary_wire.js',
    WEB / 'transport/binary_transfers.js',
    WEB / 'transport/websocket.js',
    WEB / 'transport/runtime.js',
]
for path in required:
    if not path.is_file():
        fail(f'missing {path.relative_to(ROOT)}')

read = lambda p: p.read_text(encoding='utf-8')
html = read(HTML)
diagnostics = read(WEB / 'transport/diagnostics.js')
commands = read(WEB / 'transport/commands.js')
wire = read(WEB / 'transport/binary_wire.js')
transfers = read(WEB / 'transport/binary_transfers.js')
websocket = read(WEB / 'transport/websocket.js')
runtime = read(WEB / 'transport/runtime.js')
version = read(VERSION)

if "createEditorTransport" not in html or "./model_asset_editor/transport/runtime.js" not in html:
    fail('HTML shell does not compose the transport runtime')
for forbidden in [
    'new WebSocket(', 'class EditorWireReader', 'editorWireTransfers',
    'function send(', 'function connect()', 'state.ws', "new TextDecoder('utf-8')"
]:
    if forbidden in html:
        fail(f'HTML shell still owns transport implementation: {forbidden}')

for required_token in ['ELWIR001', 'class EditorWireReader', 'decodeEditorLodGeometry']:
    if required_token not in wire:
        fail(f'binary wire codec lost {required_token}')
for forbidden in ['document.', 'window.', 'THREE.', 'WebSocket', 'state.', 'fetch(', '.send(']:
    if forbidden in wire:
        fail(f'binary wire codec contains effect/domain dependency {forbidden!r}')

if "import {decodeEditorLodGeometry} from './binary_wire.js';" not in transfers:
    fail('binary transfer manager does not depend on the isolated wire codec')
for token in ['createBinaryTransferManager', 'const transfers=new Map()', 'beginAsset', 'beginLod', 'handleBinary', 'clear']:
    if token not in transfers:
        fail(f'binary transfer manager lost {token}')
for forbidden in ['document.', 'window.', 'THREE.', 'WebSocket', 'state.']:
    if forbidden in transfers:
        fail(f'binary transfer manager leaked shell/runtime dependency {forbidden!r}')

if 'createCommandTransport' not in commands or 'socket.send(JSON.stringify({command,...payload}))' not in commands:
    fail('command transport is not isolated')
for forbidden in ['document.', 'window.', 'THREE.', 'state.']:
    if forbidden in commands:
        fail(f'command transport leaked editor runtime dependency {forbidden!r}')

for token in ['createWebSocketLifecycle', 'new WebSocketClass', "binaryType='arraybuffer'", 'reconnectDelay']:
    if token not in websocket:
        fail(f'WebSocket lifecycle lost {token}')
for forbidden in ['document.', 'THREE.', 'state.', 'asset.', 'renderWizard']:
    if forbidden in websocket:
        fail(f'WebSocket lifecycle absorbed editor/domain responsibility {forbidden!r}')

if 'createDiagnosticTransport' not in diagnostics or 'installGlobalHandlers' not in diagnostics:
    fail('diagnostic transport is not isolated')
if 'createEditorTransport' not in runtime:
    fail('transport runtime composition root is missing')
for token in ['request_catalog', 'request_settings', 'transfers.handleBinary', 'diagnostics.report']:
    if token not in runtime:
        fail(f'transport runtime lost integration {token}')

if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.86"', version):
    fail('editor version must be 0.10.86')

print('MODEL ASSET EDITOR TRANSPORT LAYERS: PASS')
print(' - WebSocket lifecycle and reconnect are outside the HTML shell')
print(' - command and diagnostic dispatch are isolated transport adapters')
print(' - binary wire decoding is independent from editor/domain state')
print(' - binary transfer bookkeeping is isolated from DOM/THREE')
print(' - transport terminates at the isolated session message boundary')
