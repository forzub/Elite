#!/usr/bin/env python3
"""v0.10.73: extract the complete SEMANTICS effect owner and keep tab entry view-state neutral."""
from pathlib import Path
import json
import re

import check_model_asset_function_purity as purity
from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]
HTML = (ROOT / 'src/assets/webui/model_asset_editor.html').read_text(encoding='utf-8')
VERSION = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
CONTRACT = json.loads((ROOT / 'tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json').read_text(encoding='utf-8'))
MODULE = CONTRACT['modules']['semantics']
REL = 'src/assets/webui/model_asset_editor/effects/semantics.js'
SRC = (ROOT / REL).read_text(encoding='utf-8')
BUNDLE = load_source_bundle(ROOT)

if 'ModelAssetEditorVersion = "0.10.73"' not in VERSION:
    raise AssertionError('editor version is not 0.10.73')
if CONTRACT['physical_split'].get('effect_extracted_modules') != ['i18n','lod_runtime','semantics']:
    raise AssertionError('wave7E effect extraction set must include i18n + lod_runtime + semantics')
if MODULE.get('physical_source') != REL or MODULE.get('physical_factory') != 'createSemanticsEffects':
    raise AssertionError('semantics physical source/factory contract missing')

owned=[name for role in ('core','presentation','adapters','infrastructure') for name in MODULE.get(role,[])]
if len(owned) != 51:
    raise AssertionError(f'unexpected semantics owned function count: {len(owned)}')
for name in owned:
    marker=f'function {name}('
    if marker not in SRC:
        raise AssertionError(f'semantics function missing from physical module: {name}')
    if marker in HTML:
        raise AssertionError(f'semantics function still inline after move: {name}')

for needle in (
    "import {createSemanticsEffects} from './model_asset_editor/effects/semantics.js';",
    'createSemanticsEffects({state,editorViewState,$,readVec,localStatus,reportEditorDiagnostic,send',
    "import * as THREE from 'three';",
    "from '../semantics/workspace.js';",
    "from '../semantics/tree.js';",
    "from '../semantics/motion.js';",
    "from '../semantics/structural.js';",
    "from '../semantics/world_graph.js';",
    'export {createSemanticsEffects};',
):
    if needle not in (HTML+'\n'+SRC):
        raise AssertionError(f'missing v0.10.73 SEMANTICS extraction contract: {needle}')

factory=re.search(r'const\s+createSemanticsEffects\s*=\s*\(\s*\{([^}]*)\}\s*\)\s*=>',SRC,re.S)
if not factory:
    raise AssertionError('cannot resolve createSemanticsEffects factory ports')
ports={part.strip().split(':',1)[-1].split('=',1)[0].strip() for part in factory.group(1).split(',') if part.strip()}
required_ports={
    'state','editorViewState','$','readVec','localStatus','reportEditorDiagnostic','send','lodHasGeometryPayload','rebuildScene',
    'highlightSelection','selectRenderNode','assertEditorViewInvariant','bindWizardStageCheckControls',
    'rebuildStructuralProxies','updateSemanticCollisionTransforms','tr','activeRenderLod','clearGroup',
    'updateSemanticSocketTransforms','renderSharedStageMeshPanel','renderWizardPanel','switchEditorLod',
    'wizardLodSelector','wizardStageCheckControls','wizardStageLabel',
}
missing=required_ports-ports
if missing:
    raise AssertionError(f'semantics factory missing explicit ports: {sorted(missing)}')

stage=purity.extract_function(BUNDLE,'renderWizardSemanticsStage')
for forbidden in ('state.selectedNode=selection.selectedNode','state.semanticSelectedNodes.clear();for(const i of selection.selectedNodeIndices)'):
    if forbidden in stage:
        raise AssertionError(f'SEMANTICS tab render still persists normalization: {forbidden}')
for required in (
    'stageSelectedNode=selection.selectedNode',
    'stageSelectedNodeIndices=selection.selectedNodeIndices',
    'selectedNode:stageSelectedNode,semanticSelectedNodes:stageSelectedNodeIndices',
    'selectedNode:stageSelectedNode,selectedNodeIndices:stageSelectedNodeIndices',
):
    if required not in stage:
        raise AssertionError(f'SEMANTICS render projection missing {required!r}')

transition=purity.extract_function(BUNDLE,'assertEditorViewTransitionPreserved')
for token in ('const changed=Object.keys(after).filter', "{context,changed,before,after}"):
    if token not in transition:
        raise AssertionError(f'EditorViewState diagnostic does not report changed fields: {token!r}')

inline_count=len(re.findall(r'\bfunction\s+[A-Za-z_$][\w$]*\s*\(',HTML))
if inline_count != 212:
    raise AssertionError(f'expected 212 inline named functions after SEMANTICS move, got {inline_count}')

print('[PASS] Model Asset Editor v0.10.73 wave7E SEMANTICS effect extraction: 51 owned functions moved; initial tab render is projection-only; inline named functions=212')
