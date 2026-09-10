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

from model_asset_source_tab_lock import (
    WEBUI,
    _balanced_block,
    _css_rule,
    _function_source,
    validate_source_tab_lock,
)

LOD_TAB_SHA256 = "a1d2590219a408837c251577ffd7518aef5b3f497e86b7085382f55623cf41cb"
GEOMETRY_TAB_SHA256 = "d9ff18f3bc8e9f2487eccd57787168dc6b2e284e04c3c0f7a02115c43514208b"
SURFACES_TAB_SHA256 = "0c8423af9a5d8944d5f837e10239e6f324d91951062b5c98a2d2f2e7c8a2c7b2"

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
    marker = f"if(stage==='{stage}'){{"
    start = body.find(marker)
    if start < 0:
        raise AssertionError(f"{stage.upper()} freeze: stage branch missing")
    return f"if(stage==='{stage}')" + _balanced_block(body, start)


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
            "LODS TAB IS FROZEN at v0.10.64: protected LODS behavior/layout changed. "
            f"expected {LOD_TAB_SHA256}, got {actual_lod}. "
            "Update only for an explicitly approved LODS change documented in PATCH_CONTRACT/CHANGELOG."
        )

    actual_geometry = current_geometry_tab_sha256(body)
    if actual_geometry != GEOMETRY_TAB_SHA256:
        raise AssertionError(
            "GEOMETRY TAB IS FROZEN at the v0.10.66 purity-equivalent baseline: protected GEOMETRY behavior/layout changed. "
            f"expected {GEOMETRY_TAB_SHA256}, got {actual_geometry}. "
            "Update only for an explicitly approved GEOMETRY change documented in PATCH_CONTRACT/CHANGELOG."
        )

    actual_surfaces = current_surfaces_tab_sha256(body)
    if actual_surfaces != SURFACES_TAB_SHA256:
        raise AssertionError(
            "SURFACES TAB IS FROZEN at the v0.10.66 purity-equivalent baseline: protected SURFACES behavior/layout changed. "
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
    for token in shared_required:
        if token not in body:
            raise AssertionError(f"accepted SOURCE/LODS/GEOMETRY/SURFACES shared UX contract missing {token!r}")
