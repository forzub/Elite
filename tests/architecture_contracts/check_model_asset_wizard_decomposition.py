#!/usr/bin/env python3
"""Structural contract for incremental wizard-stage decomposition.

A stage may leave DOM/event/backend work in a narrow adapter, but calculations and
HTML assembly extracted from renderWizardPanelContents must be pure and behaviourally
certified before the branch is reduced to dispatch-only wiring.
"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
WEB = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8")
CONTRACT = json.loads((ROOT / "tools/model_asset_editor/FUNCTION_PURITY_CONTRACT.json").read_text(encoding="utf-8"))


def function_source(name: str) -> str:
    marker = f"function {name}("
    start = WEB.find(marker)
    if start < 0:
        raise AssertionError(f"wizard decomposition: missing {name}")
    brace = WEB.index("{", start)
    depth = 0
    quote = None
    escaped = False
    i = brace
    while i < len(WEB):
        c = WEB[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                quote = None
            i += 1
            continue
        if c in ("'", '"', "`"):
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return WEB[start:i + 1]
        i += 1
    raise AssertionError(f"wizard decomposition: unterminated {name}")


shell = function_source("renderWizardPanelContents")
source_model = function_source("wizardSourceStageModel")
source_html = function_source("wizardSourceStageHtml")
source_adapter = function_source("renderWizardSourceStage")
lods_model = function_source("wizardLodsStageModel")
lods_html = function_source("wizardLodsStageHtml")
lods_adapter = function_source("renderWizardLodsStage")
geometry_model = function_source("wizardGeometryStageModel")
geometry_html = function_source("wizardGeometryStageHtml")
geometry_adapter = function_source("renderWizardGeometryStage")

certified = {entry["name"]: entry for entry in CONTRACT.get("pure", [])}


def assert_pure_block(name: str, body: str) -> None:
    entry = certified.get(name)
    if not entry or not entry.get("oracle_sha256") or not entry.get("fixtures"):
        raise AssertionError(f"wizard decomposition: {name} must have a frozen behavioural oracle")
    for forbidden in (r"\bstate\b", r"\beditorViewState\b", r"\bdocument\b", r"\bsend\s*\(", r"\.innerHTML\s*=", r"\.onclick\s*="):
        if re.search(forbidden, body):
            raise AssertionError(f"wizard decomposition: pure block {name} regained side effect/global dependency: {forbidden}")


# SOURCE: dispatch-only shell + pure model/view + narrow effect adapter.
if "if(stage==='source'){renderWizardSourceStage(root,state.asset,state.settings);return;}" not in shell:
    raise AssertionError("wizard decomposition: SOURCE branch must remain dispatch-only")
for forbidden in ("wizardSourceRefreshBtn", "wizardSourceReimportBtn", "maintenanceSourceScanHtml()"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: SOURCE implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardSourceStageModel", source_model), ("wizardSourceStageHtml", source_html)):
    assert_pure_block(name, body)
for token in (
    "wizardSourceStageModel(asset,settings)",
    "wizardSourceStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "bindPhysicalSizePanel()",
    "bindMaintenanceSourceScan(root)",
    "send('refresh_source_variants')",
    "send('reimport_asset')",
    "bindWizardStageCheckControls('source')",
):
    if token not in source_adapter:
        raise AssertionError(f"wizard decomposition: SOURCE effect adapter missing {token!r}")
if "if(stage==='" in source_adapter:
    raise AssertionError("wizard decomposition: SOURCE adapter must not own cross-stage dispatch")

# LODS: same boundary. Calculations and deterministic markup are pure; DOM/events/backend
# remain isolated in the adapter until the whole tab is moved into its own module.
if "if(stage==='lods'){renderWizardLodsStage(root,lods,payloads);return;}" not in shell:
    raise AssertionError("wizard decomposition: LODS branch must remain dispatch-only")
for forbidden in ("modelPreflightPrepareBtn", "lodGeneratorAnalyzeBtn", "prepare_model_meshes", "analyze_lod_requirements"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: LODS implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardLodsStageModel", lods_model), ("wizardLodsStageHtml", lods_html)):
    assert_pure_block(name, body)
for token in (
    "wizardLodsStageModel(lods,payloads)",
    "wizardLodsStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "switchEditorLod(Number(btn.dataset.lodsLod),'lods-selector')",
    "send('prepare_model_meshes',{})",
    "send('analyze_model_preflight',{})",
    "send('analyze_lod_requirements',{lodIndex:0})",
    "renderModelPreflightPanel()",
    "renderLodGeneratorPanel()",
    "bindWizardStageCheckControls('lods')",
):
    if token not in lods_adapter:
        raise AssertionError(f"wizard decomposition: LODS effect adapter missing {token!r}")
if "if(stage==='" in lods_adapter:
    raise AssertionError("wizard decomposition: LODS adapter must not own cross-stage dispatch")

# GEOMETRY: calculations/markup are isolated from event/backend wiring. The adapter owns
# visibility synchronization, DOM binding and commands until the tab moves to its own file.
if "if(stage==='geometry'){renderWizardGeometryStage(root,lods);return;}" not in shell:
    raise AssertionError("wizard decomposition: GEOMETRY branch must remain dispatch-only")
for forbidden in ("wizardGeometryScanBtn", "wizardGeometryConsolidateBtn", "geometryCompareAllBtn", "scan_render_duplicates", "consolidate_render_duplicates"):
    if forbidden in shell:
        raise AssertionError(f"wizard decomposition: GEOMETRY implementation leaked back into mega-function: {forbidden}")
for name, body in (("wizardGeometryStageModel", geometry_model), ("wizardGeometryStageHtml", geometry_html)):
    assert_pure_block(name, body)
for token in (
    "wizardGeometryStageModel(state.activeLod,unused,rows,geometryStageVisibleCount())",
    "wizardGeometryStageHtml(model,text,fragments)",
    "root.innerHTML=",
    "switchGeometryLod(Number(btn.dataset.geometryLod))",
    "showAllGeometryStageMeshes",
    "hideAllGeometryStageMeshes",
    "send('scan_render_duplicates'",
    "send('consolidate_render_duplicates'",
    "renderGeometryCandidates()",
    "renderVariantAssignment()",
    "renderGeometryEditor()",
    "bindWizardStageCheckControls('geometry')",
):
    if token not in geometry_adapter:
        raise AssertionError(f"wizard decomposition: GEOMETRY effect adapter missing {token!r}")
if "if(stage==='" in geometry_adapter:
    raise AssertionError("wizard decomposition: GEOMETRY adapter must not own cross-stage dispatch")

print("[PASS] Model Asset Editor wizard decomposition: SOURCE + LODS + GEOMETRY = pure model + pure HTML + narrow effect adapters")
