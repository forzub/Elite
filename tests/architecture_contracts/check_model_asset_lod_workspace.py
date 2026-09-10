#!/usr/bin/env python3
"""v0.10.64: LOD tab is a per-mesh PREPARE workspace with read-only ANALYZE."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def text(rel: str) -> str:
    return (ROOT / rel).read_text(encoding="utf-8", errors="replace")


def js_function(data: str, name: str) -> str:
    needle = f"function {name}("
    start = data.find(needle)
    if start < 0:
        raise AssertionError(f"missing JS function {name}")
    brace = data.find("{", start)
    depth = 0
    quote = None
    escaped = False
    for i in range(brace, len(data)):
        ch = data[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"', "`"):
            quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return data[start:i + 1]
    raise AssertionError(f"unterminated JS function {name}")


def cpp_function(data: str, signature: str) -> str:
    start = data.find(signature)
    if start < 0:
        raise AssertionError(f"missing C++ function {signature}")
    brace = data.find("{", start)
    depth = 0
    quote = None
    escaped = False
    for i in range(brace, len(data)):
        ch = data[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == quote:
                quote = None
            continue
        if ch in ("'", '"'):
            quote = ch
        elif ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return data[start:i + 1]
    raise AssertionError(f"unterminated C++ function {signature}")


web = text("src/assets/webui/model_asset_editor.html")
session = text("tools/model_asset_editor/ModelAssetEditorSession.cpp")
session_h = text("tools/model_asset_editor/ModelAssetEditorSession.h")
version = text("tools/model_asset_editor/EditorVersion.h")
contract = text("tools/model_asset_editor/PATCH_CONTRACT.md")

# LOD panel no longer has the old workset mode switch. Scope the assertion to
# the LODS branch so shared maintenance helpers may remain for other stages.
lods_start = web.index("if(stage==='lods'){")
lods_end = web.index("if(stage==='geometry')", lods_start)
lods_branch = web[lods_start:lods_end]
lods_impl = "\n".join((
    lods_branch,
    js_function(web, "wizardLodsStageModel"),
    js_function(web, "wizardLodsStageHtml"),
    js_function(web, "renderWizardLodsStage"),
))
for forbidden in (
    "maintenanceWorksetBarHtml",
    "data-maint-view",
    "bindMaintenanceWorkset",
    "bindMaintenanceLodWork",
    "ВСЯ МОДЕЛЬ",
    "ИЗМЕНЕНИЯ",
):
    if forbidden in lods_impl:
        raise AssertionError(f"old LOD working-set UI survived: {forbidden!r}")

for required in (
    "pendingPrepare",
    "untrackedPrepare",
    "modelPreflightPrepareBtn",
    "ПОДГОТОВИТЬ МЕШИ",
    "modelPreflightCheckBtn",
    "АНАЛИЗИРОВАТЬ",
    "analyze_model_preflight",
    "lodGeneratorAnalyzeBtn",
    "analyze_lod_requirements",
):
    if required not in lods_impl:
        raise AssertionError(f"LOD workspace missing {required!r}")

# LODS uses the same explicit active-render-LOD selector pattern as GEOMETRY.
# The selector is a distinct semantic block; switching it goes through the
# canonical EditorViewState LOD path instead of a stage-local assignment.
for required in (
    "wizardLodSelector(lods,'data-lods-lod'",
    "root.querySelectorAll('[data-lods-lod]')",
    "switchEditorLod(Number(btn.dataset.lodsLod),'lods-selector')",
):
    if required not in lods_impl:
        raise AssertionError(f"LOD active-render selector missing {required!r}")

# The table is deliberately taller, uses the existing per-mesh graph colors,
# and exposes compact viewport-only visibility controls.
for token in (
    ".preflightTable{max-height:min(64vh,640px)",
    ".preflightMeshToolbar{display:flex;flex-direction:column",
    ".preflightMeshSelectionLine{display:flex",
    ".preflightMeshActions{display:flex;gap:10px;align-items:center;flex-wrap:nowrap",
    ".preflightVisibilityActions,.preflightMeshEditActions{display:flex",
    ".preflightMeshToolbar button,.lodTechButton{min-width:0;height:25px",
    ".preflightRow.meshStagePassed.blocker",
    ".preflightRow.meshValidationPending.blocker",
    "lodPreflightVisibleByLod:{get:()=>editorViewState.lodPreflightVisibility}",
    "function lodPreflightVisibilitySet(",
    "function setLodPreflightGeometryVisible(",
    "if(existing===null){set=new Set([id]);}",
    "preflightShowAll",
    "preflightHideAll",
    "● ПОКАЗАТЬ ВСЕ",
    "○ СПРЯТАТЬ ВСЕ",
    "data-lod-preflight-visible",
):
    if token not in web:
        raise AssertionError(f"LOD table/visibility contract missing {token!r}")

inventory = js_function(web, "renderModelPreflightInventory")
analysis_panel = js_function(web, "renderModelPreflightPanel")
if ".filter(r=>Number(r.lodIndex)===Number(state.activeLod))" not in analysis_panel:
    raise AssertionError("post-ANALYZE LOD mesh table is not scoped to the active render LOD")
for body, label in ((inventory, "pre-ANALYZE inventory"), (analysis_panel, "post-ANALYZE table")):
    for token in (
        "data-preflight-select",
        "data-preflight-logical-id",
        "data-lod-preflight-visible",
        "selectPreflightLogicalGeometry",
    ):
        if token not in body:
            raise AssertionError(f"{label} missing {token!r}")
    if "meshStageVisualClass(logical,'lods')" not in body and "meshStageVisualClass(logical||g,'lods')" not in body:
        raise AssertionError(f"{label} is not using canonical/instance-aware LODS stage coloring")
    if "meshStageCheckValue(logical,'lods')" not in body and "meshStageCheckValue(logical||g,'lods')" not in body:
        raise AssertionError(f"{label} is not using canonical/instance-aware LODS stage checks")

if "✓ ПОДГОТОВЛЕН" not in inventory or "! НЕ ПОДГОТОВЛЕН" not in inventory:
    raise AssertionError("LOD inventory does not expose prepared/pending state")

# Table -> 3D and 3D -> table share the canonical RenderNode selection path.
for token in (
    "function selectPreflightGeometry(",
    "selectRenderNode(ri,{scrollPreflight:false});",
    "syncPreflightSelectionUi(!!options.scrollPreflight)",
    "selectRenderNode(ri,{scrollPreflight:state.wizardStage==='lods',scrollGeometry:state.wizardStage==='geometry',focusTable:['source','lods','geometry'].includes(state.wizardStage)});",
    "scrollPreflightRowIntoView",
):
    if token not in web:
        raise AssertionError(f"LOD table/viewport selection sync missing {token!r}")

# PREPARE is now the per-mesh LODS certification boundary. Certified source
# meshes must be skipped before canonical fingerprint/canonicalization work.
for token in (
    "bool onlyUncheckedLodMeshes = false",
    "skippedCertifiedGeometries",
    'lodStageValue(li, geometry.id) == "passed"',
    'setLodStageValue(li, geometry, "failed")',
    'setLodStageValue(li, geometry, "passed")',
    "if (authoringStateChanged) markEditorStateDirty();",
):
    if token not in session and token not in session_h:
        raise AssertionError(f"per-mesh LODS certification contract missing {token!r}")

canonical = cpp_function(session, "bool ModelAssetEditorSession::canonicalizeLoadedWorkingSet(")
skip_at = canonical.find('lodStageValue(li, geometry.id) == "passed"')
fingerprint_at = canonical.find("canonicalMeshFingerprint(geometry.mesh)")
if skip_at < 0 or fingerprint_at < 0 or skip_at > fingerprint_at:
    raise AssertionError("certified LOD mesh is not skipped before fingerprint/canonical PREPARE work")

prepare_cmd_start = session.index('if (command == "prepare_model_meshes")')
prepare_cmd_end = session.index('if (command == "analyze_model_preflight")', prepare_cmd_start)
prepare_cmd = session[prepare_cmd_start:prepare_cmd_end]
for token in (
    "ensureAllLodsLoaded()",
    "synchronizeMeshSourceRecords(true)",
    'canonicalizeLoadedWorkingSet(\n                "lods", true, &payloadChanged, &changedLods,\n                std::size_t(-1), {}, true)',
):
    if token not in prepare_cmd:
        raise AssertionError(f"global LOD PREPARE pending-only route missing {token!r}")

# SOURCE add/replace already resets every per-mesh stage; that is the authority
# that makes only new/changed meshes red again after SOURCE maintenance.
if "resetMeshStageChecks(lodIndex, resident.id);" not in session and "resetMeshStageChecks(lodIndex, geometry.id);" not in session:
    raise AssertionError("SOURCE add/replace no longer resets per-mesh validation evidence")

# ANALYZE is deliberately left as the existing read-only audit. It must not
# route through PREPARE/canonicalization as a side effect of this LOD UI patch.
if 'if (command == "analyze_model_preflight") { analyzeModelPreflight(); return; }' not in session:
    raise AssertionError("ANALYZE command route changed")
analyze = cpp_function(session, "bool ModelAssetEditorSession::analyzeModelPreflight()")
for forbidden in ("canonicalizeLoadedWorkingSet(", "prepare_model_meshes", "setLodStageValue"):
    if forbidden in analyze:
        raise AssertionError(f"ANALYZE is no longer read-only: found {forbidden!r}")

for token in (
    "LOD workspace is per-mesh",
    "ANALYZE remains read-only",
    "already `lods=passed` meshes",
):
    if token not in contract:
        raise AssertionError(f"PATCH_CONTRACT missing LOD protection {token!r}")

if 'ModelAssetEditorVersion = "0.10.66"' not in version:
    raise AssertionError("editor version is not 0.10.66")

print("[PASS] model asset editor v0.10.66 LOD per-mesh PREPARE workspace / selection / visibility / ANALYZE fence")
