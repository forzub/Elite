#!/usr/bin/env python3
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'src/assets/webui/model_asset_editor'
HTML = ROOT / 'src/assets/webui/model_asset_editor.html'
VERSION = ROOT / 'tools/model_asset_editor/EditorVersion.h'


def fail(message: str) -> None:
    print(f'MODEL ASSET EDITOR VIEWPORT ADAPTERS: FAIL\n - {message}')
    sys.exit(1)


required = [WEB / 'viewport/overlays.js', WEB / 'viewport/attachments.js', WEB / 'viewport/picking.js']
for path in required:
    if not path.is_file():fail(f'missing {path.relative_to(ROOT)}')

read=lambda path:path.read_text(encoding='utf-8')
html=read(HTML)
overlays=read(WEB / 'viewport/overlays.js')
attachments=read(WEB / 'viewport/attachments.js')
picking=read(WEB / 'viewport/picking.js')
version=read(VERSION)

for token in [
    "./model_asset_editor/viewport/overlays.js",
    "./model_asset_editor/viewport/attachments.js",
    "./model_asset_editor/viewport/picking.js",
    'createViewportOverlayEffects({','createViewportAttachmentEffects({','createViewportPickingEffects({'
]:
    if token not in html:fail(f'HTML shell is missing viewport adapter composition token {token!r}')

for forbidden in [
    'function clearEdgeOverlay(', 'function clearNormalOverlay(', 'function rebuildNormals(', 'function rebuildEdgeOverlay(',
    'function toggleEdge(', 'function collisionPrimitive(', 'function rebuildStructuralProxies(', 'function rebuildCollisions(',
    'function updateSemanticCollisionTransforms(', 'function socketKindColor(', 'function viewFromSocket(',
    'function updateSemanticSocketTransforms(', 'function socketIndexFromObject(', 'function rebuildSockets(', 'function pick(ev)'
]:
    if forbidden in html:fail(f'HTML shell still owns viewport adapter implementation: {forbidden}')

for token in ['createViewportOverlayEffects','rebuildNormals','rebuildEdgeOverlay','toggleEdge','new THREE.LineSegments']:
    if token not in overlays:fail(f'overlay adapter lost {token}')
for forbidden in ['WebSocket','request_catalog','workingSaveRevision','send(']:
    if forbidden in overlays:fail(f'overlay adapter absorbed unrelated transport/session dependency {forbidden!r}')

for token in ['createViewportAttachmentEffects','rebuildStructuralProxies','rebuildCollisions','viewFromSocket','rebuildSockets','new THREE.ArrowHelper']:
    if token not in attachments:fail(f'attachment adapter lost {token}')
for forbidden in ['document.','window.','WebSocket','send(','request_catalog','workingSaveRevision']:
    if forbidden in attachments:fail(f'attachment adapter absorbed unrelated dependency {forbidden!r}')

for token in ['createViewportPickingEffects','function pick(event)','raycaster.setFromCamera','wizardSemanticsViewportGizmoPickDecision','wizardSemanticsViewportMeshPickDecision']:
    if token not in picking:fail(f'picking adapter lost {token}')
for forbidden in ['new THREE','WebSocket','send(','request_catalog','workingSaveRevision']:
    if forbidden in picking:fail(f'picking adapter owns forbidden construction/transport {forbidden!r}')

if not re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.82"',version):fail('editor version must be 0.10.82')

print('MODEL ASSET EDITOR VIEWPORT ADAPTERS: PASS')
print(' - edge and normal overlay rendering is outside the HTML shell')
print(' - collision, structural proxy and socket materialization is isolated')
print(' - socket camera preview is owned by the viewport attachment adapter')
print(' - raycast picking orchestration is isolated from the shell')
print(' - renderer adapters call feature/command boundaries through injected callbacks')
