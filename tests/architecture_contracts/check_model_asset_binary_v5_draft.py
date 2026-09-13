#!/usr/bin/env python3
"""Static contract guard for the Model Asset Binary v5 draft.

This intentionally does not enable v5. It prevents the draft container work from
silently changing the current production v4 serializer before the migration gate
is complete.
"""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
MODEL_ASSET_H = ROOT / "src/model_asset/ModelAsset.h"
DRAFT_H = ROOT / "src/model_asset/ModelAssetBinaryV5Draft.h"
DRAFT_MD = ROOT / "src/model_asset/MODEL_ASSET_BINARY_V5_DRAFT.md"
SEMANTICS_WORKFLOW = ROOT / "src/assets/webui/model_asset_editor/ui/semantics_workflow_model.js"


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    model_asset = MODEL_ASSET_H.read_text(encoding="utf-8")
    header = DRAFT_H.read_text(encoding="utf-8")
    spec = DRAFT_MD.read_text(encoding="utf-8")
    workflow = SEMANTICS_WORKFLOW.read_text(encoding="utf-8")

    # Production remains v4 until the explicit v5 acceptance gate is met.
    require(
        re.search(r"ModelAssetFormatVersion\s*=\s*4\s*;", model_asset) is not None,
        "production ModelAssetFormatVersion must remain 4 while v5 is DRAFT",
    )

    for token in (
        "FormatMajor = 5",
        "HeaderBytes = 64",
        "DirectoryEntryBytes = 48",
        "LittleEndianTag = 0x01020304u",
        "'E','L','M','D','L','0','0','5'",
        "'E','L','M','S','H','0','0','5'",
        "packageId",
        "ChunkFlagRequired",
        "ChunkFlagStreamable",
        'ManifestStringTable = "STRS"',
        'ManifestSemantics = "SEMN"',
        'ManifestStructuralLinks = "STRL"',
        'LodRenderGraph = "RGRF"',
        'LodMesh = "MESH"',
    ):
        require(token in header, f"missing v5 draft header contract token: {token}")

    for phrase in (
        "Production format remains v4",
        "explicit little-endian",
        "Unknown **optional** chunks are skipped",
        "Unknown **required** chunks fail",
        "manifest is the commit point",
        "must remain `4`",
    ):
        require(phrase in spec, f"missing v5 draft specification rule: {phrase}")

    # The accepted SEMANTICS direction is a five-step user workflow with
    # diagnostics moved behind help rather than shown as the main workspace.
    for step in ("assembly", "bindings", "motion", "graph", "check"):
        require(f"'{step}'" in workflow, f"missing SEMANTICS workflow step: {step}")
    require("semanticWorkflowDiagnostics" in workflow, "missing SEMANTICS diagnostics registry")
    require("доступны через ?" in workflow or "available through ?" in workflow,
            "SEMANTICS workflow must state that diagnostics belong under help")

    print("MODEL ASSET BINARY V5 DRAFT CONTRACT: PASS")
    print(" - production format is still v4")
    print(" - v5 draft header/directory/package identity contract is present")
    print(" - SEMANTICS five-step workflow/help direction is frozen")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as exc:
        print(f"MODEL ASSET BINARY V5 DRAFT CONTRACT: FAIL: {exc}", file=sys.stderr)
        raise SystemExit(1)
