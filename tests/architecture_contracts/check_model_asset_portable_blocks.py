#!/usr/bin/env python3
"""Hard boundary contract for portable Model Asset Editor blocks.

A portable API may call other certified PURE helpers, but it must not reach editor
state, DOM, backend/status/prompt I/O, timers/storage/network or scene adapters
through hidden dependencies. The function-purity contract provides the transitive
closure guarantee; this contract names the stable block-facing API and adapters.
"""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[2]
WEB = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8")
BLOCKS = json.loads((ROOT / "tools/model_asset_editor/PORTABLE_BLOCK_CONTRACT.json").read_text(encoding="utf-8"))
PURITY = json.loads((ROOT / "tools/model_asset_editor/FUNCTION_PURITY_CONTRACT.json").read_text(encoding="utf-8"))
CERTIFIED = {entry["name"] for entry in PURITY.get("pure", [])}


def function_source(name: str) -> str:
    marker = f"function {name}("
    start = WEB.find(marker)
    if start < 0:
        raise AssertionError(f"portable blocks: missing function {name}")
    brace = WEB.index("{", start)
    depth = 0
    quote = None
    escaped = False
    for i in range(brace, len(WEB)):
        c = WEB[i]
        if quote is not None:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                quote = None
            continue
        if c in ("'", '"', "`"):
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return WEB[start:i + 1]
    raise AssertionError(f"portable blocks: unterminated function {name}")


if BLOCKS.get("schema") != 1:
    raise AssertionError("portable blocks: unsupported schema")
if BLOCKS.get("ownership_contract") != "tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json":
    raise AssertionError("portable blocks: complete module ownership contract is not linked")

for block_name, block in BLOCKS.get("blocks", {}).items():
    status = block.get("status")
    if status not in {"portable", "foundation", "pending"}:
        raise AssertionError(f"portable blocks: {block_name} has invalid status {status!r}")
    portable = list(block.get("api", [])) + list(block.get("presentation", []))
    adapters = list(block.get("adapters", []))
    if status != "pending" and not portable:
        raise AssertionError(f"portable blocks: {block_name} must expose an explicit API")
    overlap = sorted(set(portable) & set(adapters))
    if overlap:
        raise AssertionError(f"portable blocks: {block_name} API/adapter overlap: {overlap}")
    for name in portable:
        if name not in CERTIFIED:
            raise AssertionError(f"portable blocks: {block_name}.{name} is not behaviourally certified PURE")
        body = function_source(name)
        for forbidden in (
            r"\bstate\b", r"\beditorViewState\b", r"\bdocument\b", r"\bwindow\b",
            r"\bsend\s*\(", r"\btr\s*\(", r"\blocalStatus\s*\(",
            r"\bprompt\s*\(", r"\bconfirm\s*\(", r"\bfetch\s*\(",
            r"\blocalStorage\b", r"\bsessionStorage\b", r"\$\s*\(",
            r"\.innerHTML\s*=", r"\.onclick\s*=", r"\.addEventListener\s*\(",
        ):
            if re.search(forbidden, body):
                raise AssertionError(
                    f"portable blocks: {block_name}.{name} regained hidden editor wiring: {forbidden}"
                )
    for name in adapters:
        function_source(name)

# PHYSICS, Hit Volumes and DAMAGE are post-SEMANTICS blocks migrated under this
# contract. Keep their dispatch/adapters narrow so implementation cannot leak back
# into the common wizard/inspector mega-functions.
shell = function_source("renderWizardPanelContents")
if "if(stage==='physics'){renderWizardPhysicsStage(root,state.asset);return;}" not in shell:
    raise AssertionError("portable blocks: PHYSICS wizard branch must remain dispatch-only")
if "if(stage==='damage'){renderWizardDamageStage(root,state.asset,lods);return;}" not in shell:
    raise AssertionError("portable blocks: DAMAGE wizard branch must remain dispatch-only")
inspector = function_source("renderNodeInspector")
if "renderPhysicsNodeInspector(root,state.selectedNode,n,actions);" not in inspector:
    raise AssertionError("portable blocks: PHYSICS node inspector must delegate to its adapter")
for forbidden in ("massMode", "densityKgM3:Number($('density').value)", "estimate_physics"):
    if forbidden in inspector:
        raise AssertionError(f"portable blocks: PHYSICS implementation leaked into renderNodeInspector: {forbidden}")
if "renderDamageNodeInspector(root,state.selectedNode,n,actions);" not in inspector:
    raise AssertionError("portable blocks: DAMAGE semantic-node inspector must delegate to its adapter")
for forbidden in ("svTransform", "add_state_variant", "set_state_variant"):
    if forbidden in inspector:
        raise AssertionError(f"portable blocks: DAMAGE implementation leaked into renderNodeInspector: {forbidden}")
render_inspector = function_source("renderRenderNodeInspector")
if "renderDamageRenderNodeInspector(root,info,n);" not in render_inspector:
    raise AssertionError("portable blocks: DAMAGE RenderNode inspector must delegate to its adapter")
for forbidden in ("rnDamageStates", "set_render_node_states"):
    if forbidden in render_inspector:
        raise AssertionError(f"portable blocks: DAMAGE RenderNode implementation leaked into renderRenderNodeInspector: {forbidden}")

# FINAL ASSEMBLY wave6C: frontend owns portable report/readiness/build presentation and
# explicit stage payloads; backend remains the validation/build execution port.
shell = function_source("renderWizardPanelContents")
if "if(stage==='validate'){renderWizardValidateStage(root,state.wizardValidationReport);return;}" not in shell:
    raise AssertionError("portable blocks: VALIDATE branch must remain dispatch-only")
if "if(stage==='build'){renderWizardBuildStage(root,state.asset,lods,state.dirty);return;}" not in shell:
    raise AssertionError("portable blocks: BUILD branch must remain dispatch-only")
for forbidden in ("wizardValidationReport,rows=", "st=state.asset.storage", "lodPayloads||[]).filter"):
    if forbidden in shell:
        raise AssertionError(f"portable blocks: FINAL ASSEMBLY implementation leaked into renderWizardPanelContents: {forbidden}")

print("[PASS] Model Asset Editor portable block API: SOURCE/LODS/GEOMETRY/SURFACES/SEMANTICS portable; PHYSICS/HIT-VOLUMES/DAMAGE/FINAL-ASSEMBLY foundation isolated")
