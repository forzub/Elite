#!/usr/bin/env python3
"""v0.10.61: SOURCE scan results are unresolved work, not scan history."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
WEB = (ROOT / "src/assets/webui/model_asset_editor.html").read_text(encoding="utf-8", errors="replace")
CONTRACT = (ROOT / "tools/model_asset_editor/PATCH_CONTRACT.md").read_text(encoding="utf-8", errors="replace")
VERSION = (ROOT / "tools/model_asset_editor/EditorVersion.h").read_text(encoding="utf-8", errors="replace")

for token in (
    "function sourceChangeRowAcceptedByStageCheck",
    "function settleSourceChangeScanAfterCheck",
    "meshStageCheckValue(geometry,stage)==='passed'",
    "acceptedAdded",
    "acceptedReplaced",
    "unchanged:Number(scan.unchanged||0)+accepted.length",
    "new:Math.max(0,Number(scan.new||0)-acceptedAdded)",
    "replaced:Math.max(0,Number(scan.replaced||0)-acceptedReplaced)",
    "settleSourceChangeScanAfterCheck(msg.stage)",
):
    if token not in WEB:
        raise AssertionError(f"SOURCE acceptance queue contract missing: {token!r}")

# The helper must not blindly clear the whole scan. Missing/failure rows have to
# survive and the mesh's actual SOURCE-stage evidence must authorize acceptance.
start = WEB.index("function sourceChangeRowAcceptedByStageCheck")
end = WEB.index("function maintenanceRowStatus", start)
queue_helpers = WEB[start:end]
ms = WEB.index("function meshStageCheckValue")
me = WEB.index("function meshStageVisualClass", ms)
check_helper = WEB[ms:me]

script = f"""
const state={{wizardStage:'source',asset:{{renderLods:[{{geometries:[
 {{id:'A',sourcePath:'A.obj',stageChecks:{{source:'passed'}}}},
 {{id:'B',sourcePath:'B.obj',stageChecks:{{source:'passed'}}}},
 {{id:'C',sourcePath:'C.obj',stageChecks:{{source:'not_checked'}}}},
 {{id:'M',sourcePath:'M.obj',sourceMissing:true,stageChecks:{{source:'passed'}}}}
]}}]}},sourceChangeScan:{{unchanged:32,new:2,replaced:1,missingSource:1,failed:1,rows:[
 {{kind:'added',lodIndex:0,geometryId:'A'}},
 {{kind:'replaced',lodIndex:0,geometryId:'B'}},
 {{kind:'added',lodIndex:0,geometryId:'C'}},
 {{kind:'missing_source',lodIndex:0,geometryId:'M'}},
 {{kind:'hash_error',lodIndex:0,geometryId:'H'}}
]}}}};
{check_helper}
{queue_helpers}
function assert(v,m){{if(!v)throw new Error(m);}}
assert(settleSourceChangeScanAfterCheck('geometry')===0,'non-SOURCE CHECK consumed rows');
assert(state.sourceChangeScan.rows.length===5,'non-SOURCE CHECK mutated queue');
const accepted=settleSourceChangeScanAfterCheck('source');
assert(accepted===2,'expected exactly passed ADDED/REPLACED rows accepted');
assert(state.sourceChangeScan.unchanged===34,'accepted rows not folded into current count');
assert(state.sourceChangeScan.new===1,'uncertified ADDED counter was incorrectly cleared');
assert(state.sourceChangeScan.replaced===0,'accepted REPLACED counter was not cleared');
const kinds=state.sourceChangeScan.rows.map(r=>r.kind+':'+r.geometryId).sort();
assert(kinds.includes('added:C'),'uncertified new mesh disappeared');
assert(kinds.includes('missing_source:M'),'missing SOURCE row disappeared');
assert(kinds.includes('hash_error:H'),'failure row disappeared');
assert(!kinds.includes('added:A')&&!kinds.includes('replaced:B'),'certified SOURCE rows remained pending');
console.log('[PASS] SOURCE acceptance queue runtime behavior');
"""
node = shutil.which("node") or shutil.which("node.exe")
if node:
    with tempfile.NamedTemporaryFile("w", suffix=".js", delete=False, encoding="utf-8") as f:
        f.write(script)
        js_path = f.name
    try:
        proc = subprocess.run([node, js_path], text=True, capture_output=True)
    finally:
        Path(js_path).unlink(missing_ok=True)
    if proc.returncode != 0:
        raise AssertionError((proc.stdout + proc.stderr).strip())
    print("[PASS] SOURCE acceptance queue runtime behavior")
else:
    print("[SKIP] SOURCE acceptance queue runtime behavior: Node.js is not on PATH")

for token in (
    "SOURCE change panel is an unresolved-work queue",
    "ADDED` / `REPLACED` rows",
    "missing_source",
    "CHECK still does not SAVE",
):
    if token not in CONTRACT:
        raise AssertionError(f"PATCH_CONTRACT missing acceptance queue rule: {token!r}")

if 'ModelAssetEditorVersion = "0.10.66"' not in VERSION:
    raise AssertionError("editor version is not 0.10.66")

print("[PASS] model asset editor v0.10.66 SOURCE checked-change acceptance queue")
