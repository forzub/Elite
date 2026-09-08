#!/usr/bin/env python3
"""Frozen acceptance contract for SOURCE + LODS + GEOMETRY editor tabs.

SOURCE keeps its v0.10.62 fingerprint. LODS and GEOMETRY are frozen at the
v0.10.64 acceptance baseline after authoritative EditorViewState, matched
workspace controls, 3D->table focus, and stronger viewport selection feedback.
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

LOD_TAB_SHA256 = "9fb545df9b62581c58da922925630c903d4e157df1b5b6821ff049aad0080f16"
GEOMETRY_TAB_SHA256 = "7e7fd0b7f8b4c0dbf7272395c81a401b3518a9e992412049d21871584ed20365"

LOD_FUNCTIONS = [
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
            "GEOMETRY TAB IS FROZEN at v0.10.64: protected GEOMETRY behavior/layout changed. "
            f"expected {GEOMETRY_TAB_SHA256}, got {actual_geometry}. "
            "Update only for an explicitly approved GEOMETRY change documented in PATCH_CONTRACT/CHANGELOG."
        )

    # Shared UX path is intentionally token-guarded rather than whole-function
    # hashed so later SEMANTICS/PHYSICS work can evolve shared functions without
    # reopening the three accepted tabs.
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
            raise AssertionError(f"accepted SOURCE/LODS/GEOMETRY shared UX contract missing {token!r}")
