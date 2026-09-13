#!/usr/bin/env python3
"""v0.10.75: workflow-first editor chrome + provisional motion-capable v5 package contract."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
VERSION = (ROOT / 'tools/model_asset_editor/EditorVersion.h').read_text(encoding='utf-8')
MODEL = (ROOT / 'src/assets/webui/model_asset_editor/ui/workflow_master_model.js').read_text(encoding='utf-8')
STYLE = (ROOT / 'src/assets/webui/model_asset_editor/ui/workflow_master_style.js').read_text(encoding='utf-8')
EFFECT = (ROOT / 'src/assets/webui/model_asset_editor/effects/workflow_master.js').read_text(encoding='utf-8')
COMPOSITION = (ROOT / 'src/assets/webui/model_asset_editor/effects/ui_chrome.js').read_text(encoding='utf-8')
BASE_CHROME = ROOT / 'src/assets/webui/model_asset_editor/effects/ui_chrome_base.js'
MOTION = (ROOT / 'src/model_asset/ModelAssetMotionV5.h').read_text(encoding='utf-8')
MOTION_DOC = (ROOT / 'src/model_asset/MODEL_ASSET_V5_MOTION_CONTRACT.md').read_text(encoding='utf-8')
ACTIVE_MODEL = (ROOT / 'src/model_asset/ModelAsset.h').read_text(encoding='utf-8')

if 'ModelAssetEditorVersion = "0.10.75"' not in VERSION:
    raise AssertionError('editor version is not 0.10.75')

# New workflow decisions stay in a portable model; browser effects stay in the adapter.
for forbidden in ('document.', 'window.', 'querySelector', 'MutationObserver', 'addEventListener', 'localStorage', 'fetch('):
    if forbidden in MODEL:
        raise AssertionError(f'workflow model is not PURE: {forbidden}')
for token in (
    'workflowDefinitionFor', 'workflowMasterHtml', 'semantics_tree', 'semantics_graph',
    'source:Object.freeze', 'lods:Object.freeze', 'geometry:Object.freeze', 'surfaces:Object.freeze',
    'physics:Object.freeze', 'damage:Object.freeze', 'validate:Object.freeze', 'build:Object.freeze',
):
    if token not in MODEL:
        raise AssertionError(f'workflow model contract missing {token!r}')
for token in (
    "import {WORKFLOW_MASTER_CSS} from '../ui/workflow_master_style.js';",
    'installModelAssetWorkflowMaster', '.wizardStage.current', 'MutationObserver',
    'workflowHelpSource', 'semanticLegacyCleanup', 'semanticTreeSelectionBar',
):
    if token not in EFFECT:
        raise AssertionError(f'workflow effect contract missing {token!r}')
for token in (
    '.wizardStage.current', '.modelAssetWorkflowMaster', '.workflowMasterStep',
    '.workflowHelpButton', '.workflowHelpSource', '.semanticLegacyCleanup', '.semanticStructureSummary',
):
    if token not in STYLE:
        raise AssertionError(f'workflow visual contract missing {token!r}')
if not BASE_CHROME.is_file():
    raise AssertionError('shared v0.10.74 UI chrome implementation was not preserved as ui_chrome_base.js')
if COMPOSITION.strip() != "// Model Asset Editor UI effect composition. Keep visual chrome and workflow guidance as separate adapters.\nimport './ui_chrome_base.js';\nimport './workflow_master.js';":
    raise AssertionError('ui_chrome.js must remain a tiny composition root for the two UI effect adapters')

# Do not perturb the frozen 546 named-function architecture inventory with the new UI layer.
for rel, source in (
    ('workflow_master_model.js', MODEL),
    ('workflow_master_style.js', STYLE),
    ('workflow_master.js', EFFECT),
):
    named = re.findall(r'\bfunction\s+[A-Za-z_$][\w$]*\s*\(', source)
    if named:
        raise AssertionError(f'{rel} introduced inventory-visible named function declarations: {named}')

# v5 is deliberately concrete, but it is NOT the active writer yet.
if 'constexpr std::uint32_t ModelAssetFormatVersion = 4;' not in ACTIVE_MODEL:
    raise AssertionError('v0.10.75 must not prematurely switch the active serializer away from v4')
for token in (
    'namespace elite::model_asset::v5_draft', 'MotionJointType', 'Fixed = 0', 'Revolute = 1', 'Prismatic = 2',
    'MotionChannel', 'SkeletonDefinition', 'SkinInfluence4', 'LodSkinBinding',
    'AnimationClipDescriptor', 'AnimationTrackDescriptor', 'AttachmentTargetKind', 'AttachmentBinding', 'MotionCatalog',
):
    if token not in MOTION:
        raise AssertionError(f'v5 motion draft schema missing {token!r}')
for token in (
    'Status: PROVISIONAL / DESIGN DRAFT', 'ModelAsset v4', 'SemanticNode != RigNode != Bone != RenderNode',
    '<asset>.elanim', '`RIGS`', '`MCTL`', '`SKEL`', '`ACAT`', '`ATCH`', '`SKIN`',
    'Fixed', 'Revolute', 'Prismatic', 'ClipDriver', 'DirectControlDriver', 'AimDriver',
    'v4 remains production until the v5 serializer/reader is implemented and accepted',
):
    if token not in MOTION_DOC:
        raise AssertionError(f'v5 motion binary contract missing {token!r}')

print('[PASS] Model Asset Editor v0.10.75 workflow master + contextual-help cleanup + provisional motion-capable v5 package contract; active binary writer remains v4')
