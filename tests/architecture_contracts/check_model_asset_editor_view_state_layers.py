#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[2]
WEB=ROOT/'src/assets/webui/model_asset_editor'
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
VERSION=ROOT/'tools/model_asset_editor/EditorVersion.h'


def fail(message):
 print(f'MODEL ASSET EDITOR VIEW STATE LAYERS: FAIL\n - {message}')
 sys.exit(1)

required=[WEB/'app/editor_view_state.js',WEB/'app/runtime_state.js',WEB/'app/view_invariants.js',WEB/'transport/bridge.js']
for path in required:
 if not path.is_file():fail(f'missing {path.relative_to(ROOT)}')

read=lambda p:p.read_text(encoding='utf-8')
html=read(HTML);view=read(required[0]);runtime=read(required[1]);invariants=read(required[2]);bridge=read(required[3]);version=read(VERSION)

for token in [
 "./model_asset_editor/app/editor_view_state.js","./model_asset_editor/app/runtime_state.js",
 "./model_asset_editor/app/view_invariants.js","./model_asset_editor/transport/bridge.js",
 'createEditorViewState({lodHasGeometryPayload})','createEditorRuntimeState({THREE,editorViewState})',
 'installEditorViewProjection(state,editorViewState)','editorViewState.setInvariantScheduler(scheduleEditorViewInvariantCheck)',
 'editorTransportBridge.bind(createEditorTransport({'
]:
 if token not in html:fail(f'HTML shell missing final composition token {token!r}')

for forbidden in [
 'class ProjectedVisibilitySet','class EditorVisibilityMapAdapter','class HiddenRenderNodeAdapter','class EditorViewState',
 'function captureEditorViewTransition(','function assertEditorViewTransitionPreserved(','function scheduleEditorViewInvariantCheck(',
 'function assertEditorViewInvariant(','const state={catalog:'
]:
 if forbidden in html:fail(f'HTML shell still owns view-state/runtime implementation: {forbidden}')

for token in ['class ProjectedVisibilitySet','class EditorVisibilityMapAdapter','class HiddenRenderNodeAdapter','class EditorViewState','installEditorViewProjection','bindAssetProvider','setInvariantScheduler']:
 if token not in view:fail(f'view-state module lost {token}')
for forbidden in ['document.','window.','THREE.','WebSocket','send(']:
 if forbidden in view:fail(f'view-state module contains effect dependency {forbidden!r}')

for token in ['createEditorRuntimeState','root:new THREE.Group()','editorView:editorViewState']:
 if token not in runtime:fail(f'runtime-state module lost {token}')
for forbidden in ['document.','window.','WebSocket','send(']:
 if forbidden in runtime:fail(f'runtime-state module contains browser/transport dependency {forbidden!r}')

for token in ['createEditorViewInvariants','captureEditorViewTransition','assertEditorViewTransitionPreserved','assertEditorViewInvariant','scheduleEditorViewInvariantCheck']:
 if token not in invariants:fail(f'view invariant module lost {token}')
for forbidden in ['document.','window.','THREE.','WebSocket','send(']:
 if forbidden in invariants:fail(f'view invariant module contains forbidden dependency {forbidden!r}')

for token in ['createEditorTransportBridge','pendingDiagnostics','bind(next)','send:(...args)','reportEditorDiagnostic:(...args)']:
 if token not in bridge:fail(f'transport bridge lost {token}')
if 'const {send,reportEditorDiagnostic,connect,beginEditorAssetBinary,beginEditorLodBinary}=createEditorTransport({' in html:
 fail('HTML still initializes transport after consumers through a TDZ-prone destructuring')

if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.86"',version):fail('editor version must be 0.10.86')

print('MODEL ASSET EDITOR VIEW STATE LAYERS: PASS')
print(' - EditorViewState and visibility projections are outside the HTML shell')
print(' - runtime state construction is isolated from application reducers')
print(' - view transition/invariant enforcement is a dedicated adapter')
print(' - early consumers use a late-bound transport bridge instead of TDZ-prone transport references')
print(' - HTML owns composition, not view-state/transport implementation')
