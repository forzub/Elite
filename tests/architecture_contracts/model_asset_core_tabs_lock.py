#!/usr/bin/env python3
"""Frozen acceptance contract for SOURCE + LODS + GEOMETRY + SURFACES editor tabs.

SOURCE/LODS/GEOMETRY/SURFACES remain frozen at the v0.10.64 acceptance baseline.
The approved instance-family contract extends their table semantics so a source
mesh consolidated into another geometry remains visible as an INSTANCE link
whose effective mesh properties come from the canonical family payload.
The v0.10.66 function-purity / wizard-decomposition migration is an explicitly approved
structural-only exception for SOURCE / LODS / GEOMETRY / SURFACES: formerly hidden state
reads are passed as explicit pure-helper arguments and accepted stage branches may be
reduced to dispatch-only adapters while frozen behavioural outputs remain identical.
"""
from __future__ import annotations

from pathlib import Path
import hashlib
import re

from model_asset_source_tab_lock import (
    SOURCE_BUNDLE,
    WEBUI,
    _balanced_block,
    _css_rule,
    _function_source,
    validate_source_tab_lock,
)

# Re-baselined at v0.10.87 (2026-11-06 gate triage). Deliberate committed changes
# since the v0.10.64/66 baseline: application-state / renderer-registry split
# (089f824f..b553908b), unified stage validation footer (aab146aa), generated LODs
# preview / apply / report UI in LODS (6e3f9add), catalog model list (25436bed).
LOD_TAB_SHA256 = "4f95c6e10e3ef59b19a49fd2ac376600958264883f7809fba2efa014aab4bc22"
GEOMETRY_TAB_SHA256 = "04aec010715f5b2ef0ab134b8618eef5bf5e1ea5b7932b169668ccda9f07bf6d"
SURFACES_TAB_SHA256 = "eded1ad0a40b4e78aac679e12e80cb7eee86b9220afb9618ba52d763ac480f8c"

LOD_FUNCTIONS = [
    "wizardLodsStageModel",
    "wizardLodsStageHtml",
    "renderWizardLodsStage",
    "selectedRenderMeshInfo",
    "lodPreflightVisibilitySet",
    "lodPreflightGeometryVisible",
    "lodPreflightNodeVisible",
    "lodPreflightVisibleCount",
    "setLodPreflightGeometryVisible",
    "showAllLodPreflightMeshes",
    "hideAllLodPreflightMeshes",
    "renderNodeForPreflightGeometry",
    "selectPreflightGeometry",
    "selectedMeshOrientationAction",
    "preflightMeshToolbarHtml",
    "updatePreflightMeshToolbarUi",
    "scrollPreflightRowIntoView",
    "syncPreflightSelectionUi",
    "bindPreflightMeshToolbar",
    "renderModelPreflightInventory",
    "renderModelPreflightPanel",
]

LOD_CSS = [
    ".preflightTable",
    ".preflightHeader,.preflightRow",
    ".preflightRow.selected",
    ".preflightMeshToolbar",
    ".preflightMeshSelectionLine",
    ".preflightMeshActions",
    ".preflightVisibilityActions,.preflightMeshEditActions",
    ".preflightVisibilityCount",
    ".geometryStickyToolbar",
]

GEOMETRY_FUNCTIONS = [
    "wizardGeometryStageModel",
    "wizardGeometryStageHtml",
    "renderWizardGeometryStage",
    "geometryComparableNodes",
    "geometryWorkspaceChangedGeometries",
    "geometryWorkspaceChangeCount",
    "geometryWorkspaceRows",
    "ensureGeometryCompareState",
    "geometryResultMap",
    "matchingCheckedTargets",
    "compareTargets",
    "geometryReadableSource",
    "applyGeometryVisibilitySelection",
    "geometryStageVisibilitySet",
    "geometryStageNodeVisible",
    "geometryStageVisibleCount",
    "syncGeometryCompareCheckedToVisibility",
    "setGeometryStageNodeVisible",
    "showAllGeometryStageMeshes",
    "hideAllGeometryStageMeshes",
    "syncGeometryWorkspaceSelectionUi",
    "updateGeometryCompareButtons",
    "renderGeometryCandidates",
    "renderGeometryCandidatesContents",
    "renderGeometryCompareSummary",
    "switchGeometryLod",
]

GEOMETRY_CSS = [
    ".geometryWorkspace",
    ".geometryStageTable",
    ".geometryVisibilityTools",
    ".geometryTechButton",
    ".compareHeader,.compareRow",
    ".compareRow",
    ".compareRow.selected",
]

SURFACES_FUNCTIONS = [
    "wizardSurfacesStageModel",
    "wizardSurfacesStageHtml",
    "renderWizardSurfacesStage",
    "surfaceStageVisualClass",
    "surfaceStageGlyph",
    "surfaceSelectedGeometry",
    "surfaceSelectionSet",
    "surfaceSelectionIds",
    "surfaceSetSelectionAnchor",
    "surfaceSelectionAnchor",
    "surfaceGeometryVisibility",
    "setSurfaceGeometryVisible",
    "surfaceSelectGeometry",
]

SURFACES_CSS = [
    ".surfaceWorkspace",
    ".surfaceBrowser,.surfaceToolBlock",
    ".surfaceGeometryTable",
    ".surfaceGeometryRow",
    ".surfaceGeometryRow.selected",
    ".surfaceGeometryRow.meshStagePassed.selected,.surfaceGeometryRow.meshStagePassed.primarySelection",
    ".surfaceGeometryRow.meshValidationPending.selected,.surfaceGeometryRow.meshValidationPending.primarySelection",
    ".surfaceGeometryRow.surfaceStageUnchecked",
    ".surfaceGeometryRow.surfaceStageUnchecked.selected,.surfaceGeometryRow.surfaceStageUnchecked.primarySelection",
]


def _stage_branch(body: str, stage: str) -> str:
    """Stage dispatch: the wizard panel dispatches through
    createWizardStageRendererRegistry({<stage>:<renderer>, ...}) since the
    application-state split; the frozen part is the registry entry."""
    marker = "createWizardStageRendererRegistry({"
    start = body.find(marker)
    if start < 0:
        raise AssertionError(f"{stage.upper()} freeze: wizard stage renderer registry missing")
    entry = re.search(rf"(?<![\w$]){stage}:([A-Za-z_$][\w$]*)", _balanced_block(body, start))
    if not entry:
        raise AssertionError(f"{stage.upper()} freeze: renderer registry has no {stage} entry")
    return f"{stage}:" + entry.group(1)


def _payload(body: str, stage: str, functions: list[str], css: list[str]) -> str:
    parts = [f"STAGE:{stage}\n" + _stage_branch(body, stage)]
    for name in functions:
        parts.append(f"FUNCTION:{name}\n" + _function_source(body, name))
    for selector in css:
        parts.append(f"CSS:{selector}\n" + _css_rule(body, selector))
    return "\n\n---CORE-TAB-FREEZE---\n\n".join(parts).replace("\r\n", "\n").strip()


def current_lod_tab_sha256(body: str | None = None) -> str:
    body = body if body is not None else WEBUI.read_text(encoding="utf-8", errors="replace")
    return hashlib.sha256(_payload(body, "lods", LOD_FUNCTIONS, LOD_CSS).encode("utf-8")).hexdigest()


def current_geometry_tab_sha256(body: str | None = None) -> str:
    body = body if body is not None else WEBUI.read_text(encoding="utf-8", errors="replace")
    return hashlib.sha256(_payload(body, "geometry", GEOMETRY_FUNCTIONS, GEOMETRY_CSS).encode("utf-8")).hexdigest()


def current_surfaces_tab_sha256(body: str | None = None) -> str:
    body = body if body is not None else WEBUI.read_text(encoding="utf-8", errors="replace")
    return hashlib.sha256(_payload(body, "surfaces", SURFACES_FUNCTIONS, SURFACES_CSS).encode("utf-8")).hexdigest()


def validate_core_tabs_lock() -> None:
    validate_source_tab_lock()
    body = WEBUI.read_text(encoding="utf-8", errors="replace")

    actual_lod = current_lod_tab_sha256(body)
    if actual_lod != LOD_TAB_SHA256:
        raise AssertionError(
            "LODS TAB IS FROZEN at v0.10.87: protected LODS behavior/layout changed. "
            f"expected {LOD_TAB_SHA256}, got {actual_lod}. "
            "Update only for an explicitly approved LODS change documented in PATCH_CONTRACT/CHANGELOG."
        )

    actual_geometry = current_geometry_tab_sha256(body)
    if actual_geometry != GEOMETRY_TAB_SHA256:
        raise AssertionError(
            "GEOMETRY TAB IS FROZEN at the v0.10.87 baseline: protected GEOMETRY behavior/layout changed. "
            f"expected {GEOMETRY_TAB_SHA256}, got {actual_geometry}. "
            "Update only for an explicitly approved GEOMETRY change documented in PATCH_CONTRACT/CHANGELOG."
        )

    actual_surfaces = current_surfaces_tab_sha256(body)
    if actual_surfaces != SURFACES_TAB_SHA256:
        raise AssertionError(
            "SURFACES TAB IS FROZEN at the v0.10.87 baseline: protected SURFACES behavior/layout changed. "
            f"expected {SURFACES_TAB_SHA256}, got {actual_surfaces}. "
            "Update only for an explicitly approved SURFACES change documented in PATCH_CONTRACT/CHANGELOG."
        )

    # Shared UX path is intentionally token-guarded rather than whole-function
    # hashed so later SEMANTICS/PHYSICS work can evolve shared functions without
    # reopening the four accepted tabs.
    shared_required = (
        ".editorMeshTableFocus:focus",
        "function focusEditorMeshTableRow(row)",
        "function focusActiveMeshTableSelection()",
        "if(stage==='source')",
        "if(stage==='lods')",
        "if(stage==='geometry')",
        "row.focus({preventScroll:true})",
        "row.scrollIntoView({block:'nearest',inline:'nearest'})",
        "if(options.focusTable)focusActiveMeshTableSelection()",
        "focusTable:['source','lods','geometry'].includes(state.wizardStage)",
        "m.color?.setHex(renderPrimary?selectionColor:0x7d91a7)",
        "m.emissive?.setHex(renderPrimary?selectionEmissive:0x000000)",
        "selectionColor=0x00a84f",
        "selectionEmissive=0x00ff70",
    )
    # The viewport picking path moved into viewport/picking.js; search the bundle.
    for token in shared_required:
        if token not in SOURCE_BUNDLE:
            raise AssertionError(f"accepted SOURCE/LODS/GEOMETRY/SURFACES shared UX contract missing {token!r}")
