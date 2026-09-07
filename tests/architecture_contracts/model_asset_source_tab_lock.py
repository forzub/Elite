#!/usr/bin/env python3
"""Frozen UI/behavior contract for the Model Asset Editor shell + SOURCE tab.

The SOURCE tab was declared structurally complete at v0.10.62.  This helper is
imported by the general editor architecture test and by a dedicated lock test.
Changing the protected surface is intentionally noisy: update the lock only for
an explicitly approved SOURCE/structure change and document the reason in
PATCH_CONTRACT.md / CHANGELOG.md.
"""
from __future__ import annotations

from pathlib import Path
import hashlib
import re

ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "src/assets/webui/model_asset_editor.html"

WIZARD_STAGE_ORDER = [
    "source", "lods", "geometry", "surfaces", "semantics",
    "physics", "damage", "validate", "build",
]

# SHA-256 over the protected SOURCE implementation surface below.
# Baseline: Model Asset Editor v0.10.62.
SOURCE_TAB_SHA256 = "28e33bb8d5fcc3d2966d9a2d83aff052e541edf652009cc11878ace5d1c65771"

PROTECTED_FUNCTIONS = [
    "maintenanceSourceScanHtml",
    "bindMaintenanceSourceScan",
    "confirmMissingSourceMesh",
    "physicalSizePanelHtml",
    "bindPhysicalSizePanel",
    "geometryInventoryVisibilitySet",
    "geometryInventoryGeometryVisible",
    "geometryInventoryNodeVisible",
    "geometryInventoryVisibleCount",
    "syncGeometryInventorySelectionUi",
    "renderGeometryInventoryToolbar",
    "switchGeometryInventoryLod",
    "selectGeometryInventoryDefinition",
    "renderGeometries",
    "settleSourceChangeScanAfterCheck",
]

PROTECTED_CSS_SELECTORS = [
    ".sourceInventory",
    ".sourceInventoryHeader,.sourceInventoryRow",
    ".sourceInventoryHeader",
    ".sourceInventoryRow",
    ".sourceInventoryRow .ok",
    ".sourceInventoryRow .bad",
    ".geometryInventoryToolbar",
    ".geometryInventoryLods,.geometryInventoryVisibility",
    ".geometryInventoryVisibility",
    ".geometryInventoryMini",
    ".geometryInventoryMini:hover",
    ".geometryInventoryMini.active",
    ".geometryInventoryMini.unloaded",
    ".geometryInventoryMini.visibilityAction",
    ".geometryInventoryCount",
    ".geometryVisibilityCheck",
    ".geometryVisibilityCheck input",
    ".geometryInventoryRow",
    ".geometryInventoryRow.selectedGeometry",
    ".geometryInventoryRow.meshValidationPending.selectedGeometry",
    ".geometryInventoryRow.visibilityOff",
    ".meshValidationPending",
    ".meshStagePassed",
    ".meshSourceMissing",
    ".sourceDeleteConfirm",
    ".physicalSizeControls",
    ".physicalSizeControl",
    ".physicalSizeCurrent",
    ".physicalSizeActions",
    ".physicalSizeTargetWrap",
    ".physicalSizeUnit",
]


def _balanced_block(body: str, start: int) -> str:
    """Return block beginning at the first '{' at/after start, including braces."""
    open_at = body.index("{", start)
    depth = 0
    quote: str | None = None
    escaped = False
    line_comment = False
    block_comment = False
    i = open_at
    while i < len(body):
        c = body[i]
        n = body[i + 1] if i + 1 < len(body) else ""
        if line_comment:
            if c == "\n":
                line_comment = False
            i += 1
            continue
        if block_comment:
            if c == "*" and n == "/":
                block_comment = False
                i += 2
                continue
            i += 1
            continue
        if quote is not None:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                quote = None
            i += 1
            continue
        if c == "/" and n == "/":
            line_comment = True
            i += 2
            continue
        if c == "/" and n == "*":
            block_comment = True
            i += 2
            continue
        if c in ("'", '"', "`"):
            quote = c
            i += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return body[open_at : i + 1]
        i += 1
    raise AssertionError("unterminated JavaScript block while checking SOURCE freeze")


def _function_source(body: str, name: str) -> str:
    marker = f"function {name}("
    start = body.find(marker)
    if start < 0:
        raise AssertionError(f"SOURCE freeze: missing function {name}")
    open_at = body.index("{", start)
    block = _balanced_block(body, start)
    return body[start:open_at] + block


def _source_stage_branch(body: str) -> str:
    marker = "if(stage==='source'){"
    start = body.find(marker)
    if start < 0:
        raise AssertionError("SOURCE freeze: renderWizardPanelContents SOURCE branch missing")
    return "if(stage==='source')" + _balanced_block(body, start)


def _css_rule(body: str, selector: str) -> str:
    marker = selector + "{"
    start = body.find(marker)
    if start < 0:
        raise AssertionError(f"SOURCE freeze: missing CSS rule {selector}")
    end = body.find("}", start + len(marker))
    if end < 0:
        raise AssertionError(f"SOURCE freeze: unterminated CSS rule {selector}")
    return body[start : end + 1]


def _protected_payload(body: str) -> str:
    parts = ["SOURCE_STAGE_BRANCH\n" + _source_stage_branch(body)]
    for name in PROTECTED_FUNCTIONS:
        parts.append(f"FUNCTION:{name}\n" + _function_source(body, name))
    for selector in PROTECTED_CSS_SELECTORS:
        parts.append(f"CSS:{selector}\n" + _css_rule(body, selector))
    return "\n\n---SOURCE-FREEZE-SEGMENT---\n\n".join(parts).replace("\r\n", "\n").strip()


def current_source_tab_sha256(body: str | None = None) -> str:
    if body is None:
        body = WEBUI.read_text(encoding="utf-8", errors="replace")
    return hashlib.sha256(_protected_payload(body).encode("utf-8")).hexdigest()


def validate_source_tab_lock() -> None:
    body = WEBUI.read_text(encoding="utf-8", errors="replace")

    # Whole editor workflow structure is frozen: SOURCE is first/default and the
    # nine stage buttons keep their established order.
    stage_match = re.search(r"const wizardStageIds=\[([^\]]+)\]", body)
    if not stage_match:
        raise AssertionError("editor structure freeze: wizardStageIds missing")
    actual_stages = re.findall(r"'([^']+)'", stage_match.group(1))
    if actual_stages != WIZARD_STAGE_ORDER:
        raise AssertionError(
            f"editor structure freeze: wizard stage order changed: {actual_stages!r}"
        )
    if "wizardStage:'source'" not in body:
        raise AssertionError("editor structure freeze: SOURCE is no longer the default stage")

    button_stages = re.findall(r'data-wizard-stage="([^"]+)"', body)
    if button_stages[: len(WIZARD_STAGE_ORDER)] != WIZARD_STAGE_ORDER:
        raise AssertionError(
            f"editor structure freeze: wizard button order changed: {button_stages!r}"
        )

    # SOURCE owns the established active-LOD geometry browser in the side pane.
    required_shell = (
        'id="wizardPanel" class="section wizardPanel"',
        'id="geometrySection" class="section" data-wizard-groups="source"',
        'id="geometryInventoryToolbar" class="geometryInventoryToolbar"',
        'id="geometries"',
    )
    for token in required_shell:
        if token not in body:
            raise AssertionError(f"SOURCE structure freeze: missing shell token {token!r}")

    # The established SOURCE panel order is part of the contract, not just the
    # existence of controls.
    branch = _source_stage_branch(body)
    ordered_tokens = [
        "sourceInventory",
        "physicalSizePanelHtml()",
        "maintenanceSourceScanHtml()",
        "FULL SOURCE REIMPORT · ADVANCED",
        "wizardSourceRefreshBtn",
        "wizardSourceReimportBtn",
        "wizardStageCheckControls('source')",
    ]
    cursor = -1
    for token in ordered_tokens:
        pos = branch.find(token, cursor + 1)
        if pos < 0:
            raise AssertionError(f"SOURCE structure freeze: missing ordered element {token!r}")
        if pos < cursor:
            raise AssertionError(f"SOURCE structure freeze: order changed near {token!r}")
        cursor = pos

    actual_hash = current_source_tab_sha256(body)
    if actual_hash != SOURCE_TAB_SHA256:
        raise AssertionError(
            "SOURCE TAB IS FROZEN at v0.10.62: protected SOURCE behavior/layout changed. "
            f"expected {SOURCE_TAB_SHA256}, got {actual_hash}. "
            "Do not update this digest as a drive-by fix. An intentional SOURCE/structure "
            "change requires an explicit exceptional case plus PATCH_CONTRACT.md/CHANGELOG.md rationale."
        )
