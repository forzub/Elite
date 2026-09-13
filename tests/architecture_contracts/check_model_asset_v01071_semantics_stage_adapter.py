#!/usr/bin/env python3
"""v0.10.73 regression: initial SEMANTICS render must use the explicit selected-panel API."""
from pathlib import Path
import re

import check_model_asset_function_purity as purity
from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]
BUNDLE = load_source_bundle(ROOT)
VERSION = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
WORKSPACE = (ROOT / 'src/assets/webui/model_asset_editor/semantics/workspace.js').read_text(encoding='utf-8')

if 'ModelAssetEditorVersion = "0.10.73"' not in VERSION:
    raise AssertionError('editor version is not 0.10.73')

if not re.search(r'function\s+semanticSelectedPanels\s*\(\s*selected\s*,\s*model\s*,\s*text\s*,\s*relationText\s*\)', WORKSPACE):
    raise AssertionError('semanticSelectedPanels explicit 4-argument API changed unexpectedly')

stage = purity.extract_function(BUNDLE, 'renderWizardSemanticsStage')
refresh = purity.extract_function(BUNDLE, 'semanticRefreshSelectionUi')

stale = 'semanticSelectedPanels(selected,nodes,desc)'
if stale in stage or stale in refresh:
    raise AssertionError('stale pre-wave5M semanticSelectedPanels(selected,nodes,desc) adapter call returned')

required_stage = (
    'const panelModel=wizardSemanticsSelectedPanelsModel({selected,nodes',
    'if(panelModel.hasParent)state.semanticPreviewAngleDeg=panelModel.previewAngleDeg',
    "const panelText={selectHelp:tr('model_editor.semantics.node.select_help'",
    "const relationText={root:tr('model_editor.semantics.relation.root'",
    'semanticSelectedPanels(selected,panelModel,panelText,relationText)',
    "reportEditorDiagnostic('semantic_stage',error,{operation:'renderWizardSemanticsStage',phase:'selected_panels'",
)
for token in required_stage:
    if token not in stage:
        raise AssertionError(f'initial SEMANTICS selected-panel adapter missing {token!r}')

# Selection refresh is the accepted reference adapter.  Initial stage render and
# refresh must both call the same explicit pure API shape.
for token in (
    'const panelModel=wizardSemanticsSelectedPanelsModel({selected,nodes',
    "const relationText={root:tr('model_editor.semantics.relation.root'",
    'semanticSelectedPanels(selected,panelModel,panelText,relationText)',
):
    if token not in refresh:
        raise AssertionError(f'SEMANTICS refresh reference adapter missing {token!r}')

# Guard the exact runtime failure class seen in v0.10.70: relationText must be
# constructed before every semanticSelectedPanels call in the stage renderer.
call_pos = stage.find('semanticSelectedPanels(selected,panelModel,panelText,relationText)')
relation_pos = stage.find("const relationText={root:tr('model_editor.semantics.relation.root'")
if relation_pos < 0 or call_pos < 0 or relation_pos > call_pos:
    raise AssertionError('relationText is not constructed before semanticSelectedPanels in initial stage render')

print('[PASS] Model Asset Editor v0.10.73 SEMANTICS initial selected-panel adapter: explicit model/text/relation inputs + failure diagnostic; stale 3-argument call forbidden')
