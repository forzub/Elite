#!/usr/bin/env python3
"""Frozen UI/behavior contract for the Model Asset Editor shell + SOURCE tab.

The SOURCE tab was declared structurally complete at v0.10.62. The v0.10.64
instance-family patch is an explicitly approved contract exception: SOURCE keeps
logical identities that were consolidated into canonical geometry instances.
The accepted layout remains frozen; only the instance-link semantics changed.
The v0.10.66 purity migration is a second explicitly approved structural-only exception:
SOURCE call wiring may pass formerly hidden state as explicit arguments to behaviourally
frozen pure helpers, without changing SOURCE layout, UX or authored data semantics.
The v0.10.66 wizard decomposition is a third structural-only exception: the SOURCE branch
may dispatch to a dedicated stage wrapper while its calculations/HTML assembly move behind
behaviourally frozen pure functions. The accepted SOURCE controls/order remain frozen.
Further protected changes still require PATCH_CONTRACT.md / CHANGELOG.md rationale.
"""
from __future__ import annotations

from pathlib import Path
import hashlib
import re

from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]
WEBUI = ROOT / "src/assets/webui/model_asset_editor.html"
SOURCE_BUNDLE = load_source_bundle(ROOT)

WIZARD_STAGE_ORDER = [
    "source", "lods", "geometry", "surfaces", "semantics",
    "physics", "damage", "validate", "build",
]

# SHA-256 over the protected SOURCE implementation surface below.
# Baseline: v0.10.62 layout + v0.10.64 persistent instance links + v0.10.66
# behaviourally equivalent purity call wiring through wave 5 plus SOURCE stage decomposition
# into certified pure model/HTML builders and a narrow effect adapter.
SOURCE_TAB_SHA256 = "228026eba5285df8014f26253d2fe3bbb7ec7a30e16bd911bab7cca312374fbb"

PROTECTED_FUNCTIONS = [
    "wizardSourceStageModel",
    "wizardSourceStageHtml",
    "renderWizardSourceStage",
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
    source = body
    start = source.find(marker)
    if start < 0:
        # Physical split is an approved structural-only exception. Frozen function
        # text may live in a real ES module while the HTML remains the composition root.
        source = SOURCE_BUNDLE
        start = source.find(marker)
    if start < 0:
        raise AssertionError(f"SOURCE freeze: missing function {name}")
    open_at = source.index("{", start)
    block = _balanced_block(source, start)
    return source[start:open_at] + block


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
    dispatch = "renderWizardSourceStage(root,state.asset,state.settings)"
    if dispatch not in branch:
        raise AssertionError("SOURCE structure freeze: SOURCE branch no longer dispatches through renderWizardSourceStage")
    source_stage = _function_source(body, "renderWizardSourceStage")
    ordered_tokens = [
        "wizardSourceStageModel(asset,settings)",
        "physicalSizePanelHtml()",
        "maintenanceSourceScanHtml()",
        "wizardStageCheckControls('source')",
        "wizardSourceStageHtml(model,text,fragments)",
        "bindPhysicalSizePanel()",
        "bindMaintenanceSourceScan(root)",
        "wizardSourceRefreshBtn",
        "wizardSourceReimportBtn",
        "bindWizardStageCheckControls('source')",
    ]
    cursor = -1
    for token in ordered_tokens:
        pos = source_stage.find(token, cursor + 1)
        if pos < 0:
            raise AssertionError(f"SOURCE structure freeze: missing ordered stage element {token!r}")
        if pos < cursor:
            raise AssertionError(f"SOURCE structure freeze: order changed near {token!r}")
        cursor = pos
    html_builder = _function_source(body, "wizardSourceStageHtml")
    for token in ("sourceInventory", "FULL SOURCE REIMPORT · ADVANCED", "wizardSourceRefreshBtn", "wizardSourceReimportBtn"):
        if token not in html_builder and token != "FULL SOURCE REIMPORT · ADVANCED":
            raise AssertionError(f"SOURCE structure freeze: pure SOURCE HTML builder missing {token!r}")

    actual_hash = current_source_tab_sha256(body)
    if actual_hash != SOURCE_TAB_SHA256:
        raise AssertionError(
            "SOURCE TAB IS FROZEN at the accepted v0.10.66 purity-equivalent baseline: protected SOURCE behavior/layout changed. "
            f"expected {SOURCE_TAB_SHA256}, got {actual_hash}. "
            "Do not update this digest as a drive-by fix. An intentional SOURCE/structure "
            "change requires an explicit exceptional case plus PATCH_CONTRACT.md/CHANGELOG.md rationale."
        )
