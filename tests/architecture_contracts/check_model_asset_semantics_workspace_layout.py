#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
WORKSPACE = ROOT / "src/assets/webui/model_asset_editor/semantics/workspace.js"
STRUCTURAL = ROOT / "src/assets/webui/model_asset_editor/semantics/structural.js"
EFFECTS = ROOT / "src/assets/webui/model_asset_editor/effects/semantics.js"
VERSION = ROOT / "tools/model_asset_editor/EditorVersion.h"

workspace = WORKSPACE.read_text(encoding="utf-8")
structural = STRUCTURAL.read_text(encoding="utf-8")
effects = EFFECTS.read_text(encoding="utf-8")
version = VERSION.read_text(encoding="utf-8")

def function_body(text: str, name: str) -> str:
    start = text.find(f"function {name}(")
    assert start >= 0, f"missing function: {name}"
    next_fn = text.find("\nfunction ", start + 1)
    if next_fn < 0:
        next_fn = text.find("\nexport ", start + 1)
    return text[start:next_fn if next_fn >= 0 else len(text)]

tree = function_body(workspace, "wizardSemanticsWorkspaceHtml")
graph = function_body(structural, "structuralGraphPanelHtml")
mode = function_body(structural, "semanticStructureModeHtml")

workflow = '<div class="semanticWorkflowBar">${fragments.structureMode}${fragments.stageCheck}</div>'
assert workflow in tree, "TREE workspace must place mode selection and CHECK in one workflow bar"
assert workflow in graph, "GRAPH workspace must place mode selection and CHECK in one workflow bar"

for noisy in [
    'class="wizardLead"',
    'class="semanticSummary',
    'class="semanticPreviewNote',
    '<div class="semanticHint">${text.previewHelp}</div>',
    '<span class="grow">${text.cleanupHelp}</span>',
    '<span>${text.selectionHelp}</span>',
]:
    assert noisy not in tree, f"TREE workspace exposes retired explanatory UI: {noisy}"

for noisy in [
    'class="wizardLead"',
    '<span class="grow">${text.cleanupHelp}</span>',
    '<div class="structGraphHint">${text.newLinkHelp}</div>',
]:
    assert noisy not in graph, f"GRAPH workspace exposes retired explanatory UI: {noisy}"

for required in [
    'id="semanticCleanLegacyTree"',
    'id="semanticGraphEnabled"',
    'id="semanticGraphExplode"',
    'id="semanticGraphExplodeValue"',
    'id="semanticGraphReset"',
    'id="semanticSelectedCount"',
    'id="wizardSemanticAddChild"',
    'id="semanticBindingHealthHost"',
    'id="semanticBindingRepairHost"',
]:
    assert required in tree, f"TREE workspace lost control: {required}"

for required in [
    'id="semanticCleanLegacyGraph"',
    'id="semanticGraphExplode"',
    'id="semanticGraphExplodeValue"',
    'id="structGraphReset"',
    'id="structMakeRoot"',
    'id="structSetA"',
    'id="structSetB"',
    'id="structCreateLink"',
]:
    assert required in graph, f"GRAPH workspace lost control: {required}"

for required in [
    'name="semanticStructureMode"',
    'value="tree"',
    'value="graph"',
    'semanticModeChoice',
    'semanticModeArrow',
]:
    assert required in mode, f"structure mode selector lost contract: {required}"

assert 'data-semantic-workspace-style' in structural, "compact SEMANTICS workspace chrome is missing"
assert 'class="semanticHelp"' in tree and 'class="semanticHelp"' in graph, "long help text must be moved behind compact ? affordances"
assert "bindWizardStageCheckControls('semantics')" in effects, "SEMANTICS CHECK button binding is missing"
assert re.search(r'ModelAssetEditorVersion\s*=\s*"0\.10\.84"', version), "editor version must be 0.10.84"

print("MODEL ASSET SEMANTICS WORKSPACE LAYOUT: PASS")
print(" - TREE and GRAPH workspaces use the compact workflow bar")
print(" - CHECK is the final workflow action")
print(" - explanatory banners are removed from the primary workspace")
print(" - existing semantic control IDs and bindings are preserved")
