#!/usr/bin/env python3
"""v0.10.73 regression: LOD runtime/UI effect owner is physically extracted and explicit."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
HTML = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
VERSION = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
CONTRACT = json.loads((ROOT / 'tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json').read_text(encoding='utf-8'))
MODULE = CONTRACT['modules']['lod_runtime']
REL = 'src/assets/webui/model_asset_editor/effects/lod_runtime.js'
SRC = (ROOT / REL).read_text(encoding='utf-8')

if 'ModelAssetEditorVersion = "0.10.73"' not in VERSION:
    raise AssertionError('editor version is not 0.10.73')
effect_modules=CONTRACT['physical_split'].get('effect_extracted_modules') or []
if effect_modules[:2] != ['i18n', 'lod_runtime'] or 'lod_runtime' not in effect_modules:
    raise AssertionError('wave7E effect extraction must retain i18n + lod_runtime as the first accepted effect modules')
if MODULE.get('physical_source') != REL or MODULE.get('physical_factory') != 'createLodRuntimeEffects':
    raise AssertionError('lod_runtime physical source/factory contract missing')

owned = [name for role in ('core','presentation','adapters','infrastructure') for name in MODULE.get(role, [])]
if len(owned) != 36:
    raise AssertionError(f'unexpected lod_runtime owned function count: {len(owned)}')
for name in owned:
    marker = f'function {name}('
    if marker not in SRC:
        raise AssertionError(f'lod_runtime function missing from physical module: {name}')
    if marker in HTML:
        raise AssertionError(f'lod_runtime function still inline after move: {name}')

for needle in (
    "import {createLodRuntimeEffects} from './model_asset_editor/effects/lod_runtime.js';",
    'createLodRuntimeEffects({state,editorViewState,$,document,confirm:window.confirm.bind(window)',
    "import * as THREE from 'three';",
    "from '../core/geometry.js';",
    "from '../core/shared.js';",
    "from '../core/source_maintenance.js';",
    'export {createLodRuntimeEffects};',
):
    if needle not in (HTML + '\n' + SRC):
        raise AssertionError(f'missing v0.10.73 LOD runtime extraction contract: {needle}')

factory = re.search(r'const\s+createLodRuntimeEffects\s*=\s*\(\s*\{([^}]*)\}\s*\)\s*=>', SRC, re.S)
if not factory:
    raise AssertionError('cannot resolve createLodRuntimeEffects factory ports')
ports = {part.strip().split(':',1)[-1].split('=',1)[0].strip() for part in factory.group(1).split(',') if part.strip()}
required_ports = {
    'state','editorViewState','$','document','confirm','localStatus','send','fitView','lodHasGeometryPayload',
    'rebuildScene','renderNodeVisibilityKey','renderActiveLodDetails','renderLodFiles','renderRenderNodeInspector',
    'selectRenderNode','updateVisibility','attachDynamicToolTip','tr','renderVariantAssignment',
    'updateVariantPreviewControls','captureUiScroll','renderWizardPanel',
}
missing = required_ports - ports
if missing:
    raise AssertionError(f'lod_runtime factory missing explicit ports: {sorted(missing)}')

# Diagnostics are intentionally outside the moved owner so relocation remains source-identical.
for needle in (
    "reportEditorDiagnostic('lod_transition','LOD switch deferred: load required'",
    "reportEditorDiagnostic('lod_transition','LOD switch begin'",
    "reportEditorDiagnostic('lod_transition','LOD switch committed'",
    "reportEditorDiagnostic('lod_io','LOD load requested'",
    "reportEditorDiagnostic('lod_io','LOD reload requested'",
    "reportEditorDiagnostic('lod_io','LOD unload requested'",
):
    if needle not in HTML:
        raise AssertionError(f'missing targeted LOD transition diagnostic: {needle}')

print('[PASS] Model Asset Editor v0.10.73 LOD runtime/UI effect extraction: 36 owned functions moved; explicit factory/import closure; targeted LOD transition/I-O diagnostics wired')
