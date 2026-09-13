#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[2]
HTML=ROOT/'src/assets/webui/model_asset_editor.html'
VERSION=ROOT/'tools/model_asset_editor/EditorVersion.h'
html=HTML.read_text(encoding='utf-8');version=VERSION.read_text(encoding='utf-8')

def fail(message):
 print(f'MODEL ASSET EDITOR SHELL ARCHITECTURE: FAIL\n - {message}')
 sys.exit(1)

# These responsibilities have dedicated modules and must never drift back into the shell.
for forbidden in [
 'class EditorViewState','class EditorVisibilityMapAdapter','function setWizardStage(','function handle(msg)',
 'new WebSocket(','class EditorWireReader','function acceptAssetState(msg,fullPayload)','function rebuildScene(',
 'function initScene(','new THREE.WebGLRenderer(','function threeGeometry(','function pick(ev)',
 'function rebuildCollisions(','function rebuildSockets(','function rebuildEdgeOverlay('
]:
 if forbidden in html:fail(f'extracted responsibility returned to shell: {forbidden}')

required_import_fragments=['/app/','/effects/','/transport/','/session/','/persistence/','/viewport/']
for fragment in required_import_fragments:
 if fragment not in html:fail(f'shell no longer composes required layer {fragment}')

if 'createEditorTransportBridge' not in html or 'editorTransportBridge.bind(createEditorTransport({' not in html:
 fail('transport composition bridge is missing')
if 'createEditorViewState' not in html or 'installEditorViewProjection' not in html:
 fail('view-state composition is missing')
if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.85"',version):fail('editor version must be 0.10.85')

print('MODEL ASSET EDITOR SHELL ARCHITECTURE: PASS')
print(' - extracted application/session/transport/viewport implementations stay outside the shell')
print(' - shell composes explicit architectural layers')
print(' - view-state and transport bootstrap ordering is explicit')
