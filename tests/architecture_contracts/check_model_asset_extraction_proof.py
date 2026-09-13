#!/usr/bin/env python3
"""Standalone extraction proof for Model Asset Editor portable modules.

This is the final logical-decomposition gate before physical source splitting.
It does not execute functions inside the original monolithic module. Instead it:
  * regenerates temporary ES modules strictly from MODULE_OWNERSHIP_CONTRACT;
  * wires only declared cross-module imports plus declared external libraries;
  * creates one facade per PORTABLE_BLOCK_CONTRACT block;
  * imports those real .mjs files with Node; and
  * replays every frozen certified PURE behavioural fixture through the extracted
    module graph.

If this passes, the portable core is mechanically movable: hidden editor state,
DOM/backend/scene wiring cannot be required for the certified calculation path.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import check_model_asset_function_purity as purity
from model_asset_editor_source_bundle import load_source_bundle

ROOT = Path(__file__).resolve().parents[2]
WEB_PATH = ROOT / "src/assets/webui/model_asset_editor.html"
OWNERSHIP_PATH = ROOT / "tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json"
BLOCKS_PATH = ROOT / "tools/model_asset_editor/PORTABLE_BLOCK_CONTRACT.json"
PURITY_PATH = ROOT / "tools/model_asset_editor/FUNCTION_PURITY_CONTRACT.json"
THREE_PATH = ROOT / "src/assets/webui/vendor/three/three.module.js"

WEB = load_source_bundle(ROOT)
OWNERSHIP = json.loads(OWNERSHIP_PATH.read_text(encoding="utf-8"))
BLOCKS = json.loads(BLOCKS_PATH.read_text(encoding="utf-8"))
PURITY = json.loads(PURITY_PATH.read_text(encoding="utf-8"))
GRAPH = purity.analyze_function_graph(WEB)
PORTABLE_ROLES = {"core", "presentation"}


def fail(message: str) -> None:
    raise AssertionError(f"Model Asset Editor extraction proof: {message}")


def rel_import(source_dir: Path, target: Path) -> str:
    rel = os.path.relpath(target, source_dir).replace("\\", "/")
    if not rel.startswith("."):
        rel = "./" + rel
    return rel


if OWNERSHIP.get("schema") != 1:
    fail("unsupported MODULE_OWNERSHIP_CONTRACT schema")
if BLOCKS.get("schema") != 1:
    fail("unsupported PORTABLE_BLOCK_CONTRACT schema")
if not OWNERSHIP.get("policy", {}).get("portable_modules_must_pass_standalone_extraction"):
    fail("ownership policy does not require standalone extraction")
if BLOCKS.get("extraction_proof") != "tests/architecture_contracts/check_model_asset_extraction_proof.py":
    fail("portable contract does not point at this extraction proof")
if not THREE_PATH.is_file():
    fail("vendored THREE module is missing")

modules = OWNERSHIP.get("modules", {})
blocks = BLOCKS.get("blocks", {})
owner: dict[str, str] = {}
role: dict[str, str] = {}
for module_name, spec in modules.items():
    for role_name in ("core", "presentation", "adapters", "infrastructure"):
        for name in spec.get(role_name, []):
            if name in owner:
                fail(f"duplicate owner for {name}")
            owner[name] = module_name
            role[name] = role_name

certified_entries = {entry["name"]: entry for entry in PURITY.get("pure", [])}
certified = set(certified_entries)
if len(certified) != 259:
    fail(f"unexpected certified PURE inventory: {len(certified)} (expected 259 for wave6D baseline)")

portable_modules = {
    module_name
    for module_name, spec in modules.items()
    if spec.get("core") or spec.get("presentation")
}
for name in certified:
    if name not in owner:
        fail(f"certified function {name} has no module owner")
    if owner[name] not in portable_modules or role[name] not in PORTABLE_ROLES:
        fail(f"certified function {name} is not owned by a portable module")

# Every block is now promoted from foundation to proved portable, and every public
# root must live in a module explicitly assigned to that block by the ownership map.
ownership_blocks = OWNERSHIP.get("blocks", {})
for block_name, block in blocks.items():
    if block.get("status") != "portable":
        fail(f"{block_name} is not promoted to portable")
    owned_block = ownership_blocks.get(block_name)
    if not owned_block or owned_block.get("status") != "portable":
        fail(f"ownership block {block_name} is not portable")
    allowed_modules = set(owned_block.get("modules", []))
    public = list(block.get("api", [])) + list(block.get("presentation", []))
    if not public:
        fail(f"portable block {block_name} has no public API")
    for name in public:
        if name not in certified:
            fail(f"public function {block_name}.{name} is not behaviourally certified")
        if owner[name] not in allowed_modules:
            fail(
                f"public function {block_name}.{name} belongs to {owner[name]}, "
                f"outside declared block modules {sorted(allowed_modules)}"
            )

# Frozen behavioural oracle itself must remain immutable before replaying it through
# the extracted modules.
purity.verify_entry_oracle_digests(PURITY)
purity.verify_baseline_digest(PURITY)

# Constants used by certified functions are explicit owned module constants. Currently
# these are the immutable axis dictionaries; the rule is generic for future additions.
constant_users: dict[str, set[str]] = {}
for entry in PURITY.get("pure", []):
    for constant in entry.get("constants", []) or []:
        constant_users.setdefault(constant, set()).add(entry["name"])

constant_specs = OWNERSHIP.get("module_constants", {})
for constant, users in constant_users.items():
    if constant not in constant_specs:
        fail(f"fixture/extraction constant {constant} has no owned module constant")
    modules_using = {owner[name] for name in users}
    if modules_using != {constant_specs[constant].get("module")}:
        fail(
            f"constant {constant} crosses module ownership implicitly: "
            f"owner={constant_specs[constant].get('module')}, users={sorted(modules_using)}"
        )
    rel = constant_specs[constant].get("physical_source")
    if not rel or not (ROOT / rel).is_file():
        fail(f"fixture/extraction constant {constant} has no physical module source")
    physical = (ROOT / rel).read_text(encoding="utf-8")
    if not re.search(rf"\bconst\s+{re.escape(constant)}\s*=", physical):
        fail(f"fixture/extraction constant {constant} is not module-local in {rel}")

# Build the exact portable cross-module dependency graph from the current source and
# assert every generated import is already declared by wave6D ownership contract.
portable_imports: dict[str, dict[str, set[str]]] = {module: {} for module in portable_modules}
for caller, row in GRAPH.items():
    if caller not in owner or role[caller] not in PORTABLE_ROLES:
        continue
    caller_module = owner[caller]
    for callee in row["calls"]:
        if callee not in owner:
            continue
        target_module = owner[callee]
        if target_module == caller_module:
            continue
        if role[callee] not in PORTABLE_ROLES:
            fail(f"portable extraction crosses into effects: {caller} -> {callee}")
        if target_module not in portable_modules:
            fail(f"portable extraction references non-portable module {target_module}")
        declared = set(modules[caller_module].get("imports", {}).get(target_module, []))
        if callee not in declared:
            fail(f"undeclared portable import: {caller_module}.{caller} -> {target_module}.{callee}")
        portable_imports[caller_module].setdefault(target_module, set()).add(callee)

fixture_count = sum(len(entry.get("fixtures", [])) for entry in PURITY.get("pure", []))

with tempfile.TemporaryDirectory(prefix="model_asset_editor_extract_", dir=ROOT) as tmp_raw:
    tmp = Path(tmp_raw)
    module_files: dict[str, Path] = {
        module_name: tmp / f"{module_name}.mjs" for module_name in portable_modules
    }

    # Materialize each logical portable module. Nothing from adapter/infrastructure
    # ownership is copied into these files.
    for module_name in sorted(portable_modules):
        spec = modules[module_name]
        names = list(spec.get("core", [])) + list(spec.get("presentation", []))
        names.sort(key=lambda name: purity._function_span(WEB, name)[0])
        parts: list[str] = [
            "// Generated by check_model_asset_extraction_proof.py; do not edit.",
        ]
        for target_module, imported in sorted(portable_imports[module_name].items()):
            imports = ", ".join(sorted(imported))
            parts.append(f"import {{ {imports} }} from './{target_module}.mjs';")

        external = set(spec.get("external_imports", []))
        if external - {"three:THREE"}:
            fail(f"unsupported external portable imports in {module_name}: {sorted(external)}")
        if "three:THREE" in external:
            parts.append(f"import * as THREE from {json.dumps(rel_import(tmp, THREE_PATH))};")

        constants = sorted(
            constant
            for constant, users in constant_users.items()
            if any(owner[user] == module_name for user in users)
        )
        for constant in constants:
            parts.append(purity.extract_const(WEB, constant))

        for name in names:
            parts.append(purity.extract_function(WEB, name))
        if names:
            parts.append(f"export {{ {', '.join(names)} }};")
        module_files[module_name].write_text("\n".join(parts) + "\n", encoding="utf-8")

    # Block-facing facades are also real ES modules. Their public surface is exactly
    # API + presentation from PORTABLE_BLOCK_CONTRACT: no private helper leaks out.
    facade_files: dict[str, Path] = {}
    for block_name, block in sorted(blocks.items()):
        public = list(block.get("api", [])) + list(block.get("presentation", []))
        grouped: dict[str, list[str]] = {}
        for name in public:
            grouped.setdefault(owner[name], []).append(name)
        lines = ["// Generated block facade; public API only."]
        for module_name, names in sorted(grouped.items()):
            lines.append(
                f"export {{ {', '.join(sorted(names))} }} from './{module_name}.mjs';"
            )
        facade = tmp / f"block_{block_name}.mjs"
        facade.write_text("\n".join(lines) + "\n", encoding="utf-8")
        facade_files[block_name] = facade

    # Import actual extracted module exports into one Node oracle runner. Registry
    # calls always use the extracted export. `tr` and the legacy no-arg active-LOD
    # fixture form remain harness-only adapters used solely to construct explicit
    # arguments for old frozen fixtures.
    harness_lines: list[str] = [
        f"import * as THREE from {json.dumps(rel_import(tmp, THREE_PATH))};"
    ]
    module_aliases: dict[str, str] = {}
    for idx, module_name in enumerate(sorted(portable_modules)):
        alias = f"M{idx}"
        module_aliases[module_name] = alias
        harness_lines.append(f"import * as {alias} from './{module_name}.mjs';")
    block_aliases: dict[str, str] = {}
    for idx, block_name in enumerate(sorted(blocks)):
        alias = f"B{idx}"
        block_aliases[block_name] = alias
        harness_lines.append(f"import * as {alias} from './block_{block_name}.mjs';")

    registry_rows = [
        f"{json.dumps(name)}: {module_aliases[owner[name]]}.{name}"
        for name in sorted(certified)
    ]
    harness_lines.append("const registry = {\n  " + ",\n  ".join(registry_rows) + "\n};")

    block_expectations = {
        block_name: sorted(list(block.get("api", [])) + list(block.get("presentation", [])))
        for block_name, block in blocks.items()
    }
    harness_lines.append(f"const blockExpected = {json.dumps(block_expectations, ensure_ascii=False)};")
    harness_lines.append(
        "const blockActual = {\n  "
        + ",\n  ".join(
            f"{json.dumps(block)}: Object.keys({block_aliases[block]}).sort()"
            for block in sorted(blocks)
        )
        + "\n};"
    )
    harness_lines.append(
        "for (const [block,names] of Object.entries(blockExpected)) {\n"
        "  if (JSON.stringify(names) !== JSON.stringify(blockActual[block])) "
        "throw new Error(`block facade drift ${block}: expected ${JSON.stringify(names)}, got ${JSON.stringify(blockActual[block])}`);\n"
        "}"
    )

    # Lexical aliases are needed only because some frozen invoke adapters compose
    # helper calls in an expression. The actual callable comes from the imported module.
    for name in sorted(certified):
        if name == "activeRenderLod":
            continue
        harness_lines.append(f"const {name} = registry[{json.dumps(name)}];")
    harness_lines.append("const extractedActiveRenderLod = registry['activeRenderLod'];")
    harness_lines.append("let state = {};")
    harness_lines.append(
        "function activeRenderLod(renderLods,activeLod){return extractedActiveRenderLod(renderLods,activeLod);}"
    )
    harness_lines.append(
        "function tr(key,fallback=key,vars={}){"
        "const map=state.i18n?.strings?.[key];let value=fallback;"
        "if(map&&typeof map==='object')value=map[state.locale]||map[baseLocale(state.locale)]||map.en||fallback;"
        "return String(value).replace(/\\{([A-Za-z0-9_]+)\\}/g,(m,k)=>vars[k]??m);}"
    )

    harness_lines.append(f"const contract = {json.dumps(PURITY, ensure_ascii=False)};")
    harness_lines.append(
        r"""
function revive(value) {
  if (Array.isArray(value)) return value.map(revive);
  if (value && typeof value === 'object') {
    if (Object.prototype.hasOwnProperty.call(value, '__set__')) return new Set(value.__set__.map(revive));
    if (Object.prototype.hasOwnProperty.call(value, '__map__')) return new Map(value.__map__.map(([k,v]) => [revive(k), revive(v)]));
    if (Object.prototype.hasOwnProperty.call(value, '__matrix4__')) return new THREE.Matrix4().fromArray(value.__matrix4__.map(Number));
    if (Object.prototype.hasOwnProperty.call(value, '__vector3__')) return new THREE.Vector3(...value.__vector3__.map(Number));
    const out = {};
    for (const [k,v] of Object.entries(value)) out[k] = revive(v);
    return out;
  }
  return value;
}
function canonical(value) {
  if (value === undefined) return {__undefined__: true};
  if (typeof value === 'number' && Number.isNaN(value)) return {__number__: 'NaN'};
  if (value === Infinity) return {__number__: 'Infinity'};
  if (value === -Infinity) return {__number__: '-Infinity'};
  if (value instanceof Set) return {__set__: [...value].map(canonical)};
  if (value instanceof Map) return {__map__: [...value.entries()].map(([k,v]) => [canonical(k), canonical(v)])};
  if (value?.isMatrix4) return {__matrix4__: value.elements.map(Number)};
  if (value?.isVector3) return {__vector3__: [Number(value.x),Number(value.y),Number(value.z)]};
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') {
    const out = {};
    for (const k of Object.keys(value).sort()) out[k] = canonical(value[k]);
    return out;
  }
  return value;
}
function snapshot(value){return JSON.stringify(canonical(value));}
function deepFreeze(value, seen=new Set()) {
  if (!value || typeof value !== 'object' || seen.has(value)) return value;
  seen.add(value);
  if (value instanceof Map) { for (const [k,v] of value) { deepFreeze(k,seen); deepFreeze(v,seen); } return value; }
  if (value instanceof Set) { for (const v of value) deepFreeze(v,seen); return value; }
  for (const v of Object.values(value)) deepFreeze(v,seen);
  Object.freeze(value);
  return value;
}
function adaptedInvoke(source){
  return String(source).replace(/\bactiveRenderLod\(\)/g,'activeRenderLod(state.asset?.renderLods,state.activeLod)');
}
let fixtureCount=0;
for (const entry of contract.pure) {
  if (typeof registry[entry.name] !== 'function') throw new Error(`missing extracted export ${entry.name}`);
  for (let i=0;i<entry.fixtures.length;i++) {
    const fixture=entry.fixtures[i];
    state=revive(fixture.state||{});
    const args=revive(fixture.args||[]);
    const stateBefore=snapshot(state), argsBefore=snapshot(args);
    deepFreeze(state); deepFreeze(args);
    let value;
    if (fixture.invoke) value=eval(adaptedInvoke(fixture.invoke));
    else value=registry[entry.name](...args);
    const actual=canonical(value);
    const expected=canonical(revive(fixture.expected));
    if (JSON.stringify(actual)!==JSON.stringify(expected)) {
      throw new Error(`${entry.name} fixture ${i} drift: expected ${JSON.stringify(expected)}, got ${JSON.stringify(actual)}`);
    }
    if (stateBefore!==snapshot(state)) throw new Error(`${entry.name} mutated fixture state after extraction`);
    if (argsBefore!==snapshot(args)) throw new Error(`${entry.name} mutated fixture args after extraction`);
    let again;
    if (fixture.invoke) again=eval(adaptedInvoke(fixture.invoke));
    else again=registry[entry.name](...args);
    if (JSON.stringify(actual)!==JSON.stringify(canonical(again))) throw new Error(`${entry.name} lost determinism after extraction`);
    fixtureCount++;
  }
}
process.stdout.write(JSON.stringify({ok:true,functions:contract.pure.length,fixtures:fixtureCount,blocks:Object.keys(blockExpected).length}));
"""
    )

    harness = tmp / "extraction_harness.mjs"
    harness.write_text("\n".join(harness_lines) + "\n", encoding="utf-8")
    node = purity.resolve_node_executable()
    result = subprocess.run(
        [node, str(harness)],
        cwd=ROOT,
        encoding="utf-8",
        errors="strict",
        capture_output=True,
    )
    if result.returncode != 0:
        fail("Node standalone-module harness failed:\n" + result.stderr.strip())
    try:
        outcome = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        fail(f"invalid Node extraction result: {exc}: {result.stdout[:500]!r}")
    expected = {
        "ok": True,
        "functions": len(certified),
        "fixtures": fixture_count,
        "blocks": len(blocks),
    }
    if outcome != expected:
        fail(f"unexpected extraction result: expected {expected}, got {outcome}")

print(
    "[PASS] Model Asset Editor standalone extraction proof: "
    f"{len(portable_modules)} portable ES modules + {len(blocks)} block facades imported; "
    f"{len(certified)} certified functions / {fixture_count} frozen fixtures PASS"
)
