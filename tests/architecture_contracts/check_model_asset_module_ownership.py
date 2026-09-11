#!/usr/bin/env python3
"""Complete ownership/API-boundary contract for Model Asset Editor.

This is the final logical-boundary gate before physical JS extraction. It does not
require every editor function to be PURE. Instead it proves that every named
function/class/module binding has one owner, that portable core/presentation code
is transitively PURE, and that every cross-module named dependency or runtime
binding is explicit in MODULE_OWNERSHIP_CONTRACT.json.
"""
from __future__ import annotations

from pathlib import Path
import json
import re

import check_model_asset_function_purity as purity

ROOT = Path(__file__).resolve().parents[2]
WEB_PATH = ROOT / "src/assets/webui/model_asset_editor.html"
OWNERSHIP_PATH = ROOT / "tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json"
PORTABLE_PATH = ROOT / "tools/model_asset_editor/PORTABLE_BLOCK_CONTRACT.json"

WEB = WEB_PATH.read_text(encoding="utf-8")
CONTRACT = json.loads(OWNERSHIP_PATH.read_text(encoding="utf-8"))
PORTABLE = json.loads(PORTABLE_PATH.read_text(encoding="utf-8"))
GRAPH = purity.analyze_function_graph(WEB)

ROLE_KEYS = ("core", "presentation", "adapters", "infrastructure")
PORTABLE_ROLES = {"core", "presentation"}
EFFECT_ROLES = {"adapters", "infrastructure"}


def fail(message: str) -> None:
    raise AssertionError(f"module ownership: {message}")


def class_source(name: str) -> str:
    match = re.search(rf"\bclass\s+{re.escape(name)}(?:\s+extends\s+[^{{]+)?\s*{{", WEB)
    if not match:
        fail(f"missing class {name}")
    opening = WEB.find("{", match.start(), match.end())
    closing = purity._scan_balanced_js(WEB, opening, "{", "}")
    return WEB[match.start():closing + 1]


def class_methods(source: str) -> list[str]:
    """Return depth-0 class method names, including constructor/get/set/async."""
    opening = source.find("{")
    closing = purity._scan_balanced_js(source, opening, "{", "}")
    body = source[opening + 1:closing]
    result: list[str] = []
    i = 0
    depth = 0
    while i < len(body):
        if body.startswith("//", i):
            newline = body.find("\n", i + 2)
            i = len(body) if newline < 0 else newline + 1
            continue
        if body.startswith("/*", i):
            end = body.find("*/", i + 2)
            i = len(body) if end < 0 else end + 2
            continue
        c = body[i]
        if c in "'\"":
            i = purity._skip_js_string(body, i, c)
            continue
        if c == "`":
            i = purity._skip_js_template(body, i)
            continue
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth -= 1
            i += 1
            continue
        if depth == 0:
            match = re.match(r"\s*(?:(?:get|set|async)\s+)?([A-Za-z_$][\w$]*)\s*\(", body[i:])
            if match:
                result.append(match.group(1))
                i += match.end()
                continue
        i += 1
    return result


def direct_external_imports(source: str) -> set[str]:
    result: set[str] = set()
    if re.search(r"\bTHREE\b", source):
        result.add("three:THREE")
    if re.search(r"\bOrbitControls\b", source):
        result.add("three/addons/controls/OrbitControls.js:OrbitControls")
    return result


if CONTRACT.get("schema") != 1:
    fail("unsupported contract schema")
if CONTRACT.get("source") != str(WEB_PATH.relative_to(ROOT)).replace("\\", "/"):
    fail("contract source does not match editor source")

modules = CONTRACT.get("modules", {})
if not modules:
    fail("no ownership modules declared")

# 1. Every named function has exactly one module + role owner.
function_owner: dict[str, str] = {}
function_role: dict[str, str] = {}
duplicates: list[str] = []
for module_name, module in modules.items():
    for role_key in ROLE_KEYS:
        values = module.get(role_key, [])
        if values != sorted(set(values)):
            fail(f"{module_name}.{role_key} must be sorted and unique")
        for name in values:
            if name in function_owner:
                duplicates.append(name)
            function_owner[name] = module_name
            function_role[name] = role_key
if duplicates:
    fail(f"functions have multiple owners: {sorted(set(duplicates))}")
current_names = set(GRAPH)
owned_names = set(function_owner)
if current_names != owned_names:
    fail(
        f"named-function ownership mismatch; missing={sorted(current_names-owned_names)}, "
        f"stale={sorted(owned_names-current_names)}"
    )

# 2. Role boundary: every statically PURE function is core/presentation and only
#    statically PURE functions may live there. This intentionally leaves EASY /
#    TRANSITIVE / hard orchestration in adapter/infrastructure ownership.
for name, row in GRAPH.items():
    role = function_role[name]
    if row["category"] == "PURE":
        if role not in PORTABLE_ROLES:
            fail(f"PURE function {name} is owned as effect role {role}")
    elif role in PORTABLE_ROLES:
        fail(f"portable {function_owner[name]}.{name} is no longer statically PURE: {row['category']}")

# 3. Explicit named imports/exports. Actual cross-module calls must match the
#    frozen import map exactly, including free-function calls from class methods.
actual_imports: dict[str, dict[str, set[str]]] = {
    module: {} for module in modules
}
actual_exports: dict[str, set[str]] = {module: set() for module in modules}
for caller, row in GRAPH.items():
    caller_module = function_owner[caller]
    for callee in row["calls"]:
        target_module = function_owner[callee]
        if target_module == caller_module:
            continue
        actual_imports[caller_module].setdefault(target_module, set()).add(callee)
        actual_exports[target_module].add(callee)

classes = CONTRACT.get("classes", {})
class_names_in_source = set(re.findall(r"\bclass\s+([A-Za-z_$][\w$]*)\b", WEB))
if set(classes) != class_names_in_source:
    fail(
        f"class ownership mismatch; missing={sorted(class_names_in_source-set(classes))}, "
        f"stale={sorted(set(classes)-class_names_in_source)}"
    )
method_count = 0
for class_name, spec in classes.items():
    module = spec.get("module")
    role = spec.get("role")
    if module not in modules:
        fail(f"class {class_name} references unknown module {module}")
    if role not in {"adapter", "infrastructure"}:
        fail(f"class {class_name} must remain an adapter/infrastructure owner, got {role}")
    source = class_source(class_name)
    actual_methods = class_methods(source)
    expected_methods = spec.get("methods", [])
    if actual_methods != expected_methods:
        fail(f"class {class_name} method ownership changed: expected {expected_methods}, got {actual_methods}")
    method_count += len(actual_methods)
    calls = purity._internal_calls(purity.strip_strings_and_comments(source), current_names, class_name)
    for callee in calls:
        target_module = function_owner[callee]
        if target_module == module:
            continue
        actual_imports[module].setdefault(target_module, set()).add(callee)
        actual_exports[target_module].add(callee)

# Public portable roots and declared adapters are intentional exports even when
# no named function currently calls them across a module boundary.
intentional_exports: set[str] = set()
for block in PORTABLE.get("blocks", {}).values():
    intentional_exports.update(block.get("api", []))
    intentional_exports.update(block.get("presentation", []))
    intentional_exports.update(block.get("adapters", []))
intentional_exports.update({
    "loadI18n", "initScene", "resize", "loop", "connect", "pick",
    "openSettings", "closeSettings", "cycleLocale", "setWizardStage",
})
for name in intentional_exports:
    if name not in function_owner:
        fail(f"intentional export {name} has no function owner")
    actual_exports[function_owner[name]].add(name)

# Composition-root callbacks are a real dependency surface too. They must not
# remain invisible just because they execute at module scope rather than inside a
# named function.
composition = CONTRACT.get("composition_root", {})
if composition.get("module") != "app_shell" or not composition.get("owns_module_scope_execution"):
    fail("composition root must explicitly own module-scope browser wiring")
start = WEB.find("$('assetSelect').onchange")
end = WEB.find("</script>", start)
if start < 0 or end < 0:
    fail("cannot locate module-scope browser wiring region")
composition_source = purity.strip_strings_and_comments(WEB[start:end])
actual_composition_imports: dict[str, set[str]] = {}
for name, module in function_owner.items():
    if module == "app_shell":
        continue
    if re.search(rf"(?<![\w$]){re.escape(name)}(?![\w$])", composition_source):
        actual_composition_imports.setdefault(module, set()).add(name)
        actual_exports[module].add(name)
expected_composition_imports = {
    module: set(names) for module, names in composition.get("imports", {}).items()
}
if actual_composition_imports != expected_composition_imports:
    fail(
        f"composition imports changed; expected={{{', '.join(f'{k}:{sorted(v)}' for k,v in expected_composition_imports.items())}}}, "
        f"actual={{{', '.join(f'{k}:{sorted(v)}' for k,v in actual_composition_imports.items())}}}"
    )

for module_name, module in modules.items():
    expected = {target: set(names) for target, names in module.get("imports", {}).items()}
    actual = actual_imports[module_name]
    if expected != actual:
        fail(
            f"{module_name} named imports changed; expected={{{', '.join(f'{k}:{sorted(v)}' for k,v in expected.items())}}}, "
            f"actual={{{', '.join(f'{k}:{sorted(v)}' for k,v in actual.items())}}}"
        )
    declared_exports = set(module.get("exports", []))
    if declared_exports != actual_exports[module_name]:
        fail(
            f"{module_name} exports changed; missing={sorted(actual_exports[module_name]-declared_exports)}, "
            f"stale={sorted(declared_exports-actual_exports[module_name])}"
        )
    for target, names in expected.items():
        if target not in modules:
            fail(f"{module_name} imports unknown module {target}")
        target_exports = set(modules[target].get("exports", []))
        if not names <= target_exports:
            fail(f"{module_name} imports non-exported symbols from {target}: {sorted(names-target_exports)}")

# 4. Portable callers may cross module boundaries only to other portable
#    functions. TRANSITIVE calls are fine; hidden editor wiring is not.
for caller, row in GRAPH.items():
    if function_role[caller] not in PORTABLE_ROLES:
        continue
    for callee in row["calls"]:
        if function_owner[caller] == function_owner[callee]:
            continue
        if function_role[callee] not in PORTABLE_ROLES:
            fail(
                f"portable call crosses into effects: {function_owner[caller]}.{caller} -> "
                f"{function_owner[callee]}.{callee} ({function_role[callee]})"
            )

# 5. The portable module dependency graph itself must remain acyclic. Function-level
#    purity already gives a DAG, but module grouping could accidentally create a
#    circular import even without a function recursion cycle.
portable_module_edges: dict[str, set[str]] = {module: set() for module in modules}
for caller, row in GRAPH.items():
    if function_role[caller] not in PORTABLE_ROLES:
        continue
    caller_module = function_owner[caller]
    for callee in row["calls"]:
        target_module = function_owner[callee]
        if target_module != caller_module:
            portable_module_edges[caller_module].add(target_module)

visiting: set[str] = set()
visited: set[str] = set()

def visit_portable_module(module: str, path: list[str]) -> None:
    if module in visiting:
        cycle_start = path.index(module) if module in path else 0
        fail(f"portable module import cycle: {' -> '.join(path[cycle_start:] + [module])}")
    if module in visited:
        return
    visiting.add(module)
    for target in sorted(portable_module_edges[module]):
        visit_portable_module(target, path + [module])
    visiting.remove(module)
    visited.add(module)

for module_name in sorted(modules):
    if modules[module_name].get("core") or modules[module_name].get("presentation"):
        visit_portable_module(module_name, [])

# 6. All top-level bindings are owned and every cross-owner binding read is
#    declared. This is the hidden-wire half of the future module API.
actual_bindings = purity.discover_module_bindings(WEB)
declared_bindings = CONTRACT.get("top_level_bindings", {})
if set(actual_bindings) != set(declared_bindings):
    fail(
        f"top-level binding ownership mismatch; missing={sorted(set(actual_bindings)-set(declared_bindings))}, "
        f"stale={sorted(set(declared_bindings)-set(actual_bindings))}"
    )
for name, spec in declared_bindings.items():
    if spec.get("module") not in modules:
        fail(f"binding {name} references unknown owner module {spec.get('module')}")

binding_owner = {name: spec["module"] for name, spec in declared_bindings.items()}
actual_binding_imports: dict[str, set[str]] = {module: set() for module in modules}
for name, module in function_owner.items():
    source = purity.strip_strings_and_comments(purity.extract_function(WEB, name))
    for binding, owner in binding_owner.items():
        if owner != module and re.search(rf"(?<![\w$]){re.escape(binding)}(?![\w$])", source):
            actual_binding_imports[module].add(binding)
for class_name, spec in classes.items():
    module = spec["module"]
    source = purity.strip_strings_and_comments(class_source(class_name))
    for binding, owner in binding_owner.items():
        if owner != module and re.search(rf"(?<![\w$]){re.escape(binding)}(?![\w$])", source):
            actual_binding_imports[module].add(binding)
for module_name, module in modules.items():
    expected = set(module.get("binding_imports", []))
    if expected != actual_binding_imports[module_name]:
        fail(
            f"{module_name} runtime binding imports changed; "
            f"missing={sorted(actual_binding_imports[module_name]-expected)}, stale={sorted(expected-actual_binding_imports[module_name])}"
        )
    # Portable code cannot have hidden cross-owner runtime bindings.
    portable_functions = module.get("core", []) + module.get("presentation", [])
    for name in portable_functions:
        source = purity.strip_strings_and_comments(purity.extract_function(WEB, name))
        for binding, owner in binding_owner.items():
            if owner != module_name and re.search(rf"(?<![\w$]){re.escape(binding)}(?![\w$])", source):
                fail(f"portable {module_name}.{name} reads external runtime binding {binding} from {owner}")

actual_comp_bindings = {
    binding for binding, owner in binding_owner.items()
    if owner != "app_shell" and re.search(rf"(?<![\w$]){re.escape(binding)}(?![\w$])", composition_source)
}
if actual_comp_bindings != set(composition.get("binding_imports", [])):
    fail(
        f"composition runtime binding imports changed; expected={sorted(composition.get('binding_imports', []))}, "
        f"actual={sorted(actual_comp_bindings)}"
    )

# 7. External JS library dependencies are explicit at module level.
actual_external: dict[str, set[str]] = {module: set() for module in modules}
for name, module in function_owner.items():
    actual_external[module].update(direct_external_imports(purity.extract_function(WEB, name)))
for class_name, spec in classes.items():
    actual_external[spec["module"]].update(direct_external_imports(class_source(class_name)))
# Initializers for the composition state and raycaster/mouse use THREE, while
# scene_runtime owns the OrbitControls construction path.
actual_external["app_shell"].add("three:THREE")
actual_external["viewport"].add("three:THREE")
actual_external["scene_runtime"].add("three/addons/controls/OrbitControls.js:OrbitControls")
for module_name, module in modules.items():
    expected = set(module.get("external_imports", []))
    if expected != actual_external[module_name]:
        fail(
            f"{module_name} external imports changed; missing={sorted(actual_external[module_name]-expected)}, "
            f"stale={sorted(expected-actual_external[module_name])}"
        )
for external in set().union(*actual_external.values()):
    if external not in CONTRACT.get("external_libraries", {}):
        fail(f"undeclared external library dependency {external}")

# 8. Effect-port inventory is frozen for adapters/infrastructure. These ports are
#    not required to disappear; they define the wires a physical adapter module
#    must connect.
for module_name, module in modules.items():
    actual_ports: set[str] = set()
    for name in module.get("adapters", []) + module.get("infrastructure", []):
        actual_ports.update(GRAPH[name]["direct_effects"])
        actual_ports.update("READ_" + value for value in GRAPH[name]["hidden_reads"])
    if actual_ports != set(module.get("ports", [])):
        fail(
            f"{module_name} effect ports changed; missing={sorted(actual_ports-set(module.get('ports', [])))}, "
            f"stale={sorted(set(module.get('ports', []))-actual_ports)}"
        )

# 9. Existing block-facing API contract is a subset of this complete ownership
#    map. Core/presentation roots must be owned by declared block modules.
ownership_blocks = CONTRACT.get("blocks", {})
if set(ownership_blocks) != set(PORTABLE.get("blocks", {})):
    fail("ownership block set diverged from PORTABLE_BLOCK_CONTRACT")
for block_name, portable_block in PORTABLE["blocks"].items():
    block = ownership_blocks[block_name]
    for key in ("status", "api", "presentation", "adapters"):
        if block.get(key) != portable_block.get(key):
            fail(f"{block_name}.{key} diverged from portable block contract")
    declared_modules = set(block.get("modules", []))
    for name in block.get("api", []) + block.get("presentation", []):
        if function_owner[name] not in declared_modules:
            fail(f"{block_name} public portable function {name} owned outside declared modules")

pure_count = sum(1 for row in GRAPH.values() if row["category"] == "PURE")
effect_count = len(GRAPH) - pure_count
print(
    "[PASS] Model Asset Editor complete module ownership / explicit API wiring: "
    f"{len(GRAPH)}/{len(GRAPH)} named functions owned; "
    f"{len(classes)} classes / {method_count} methods owned; "
    f"{len(declared_bindings)} top-level bindings owned; "
    f"{len(modules)} logical modules; portable={pure_count}, adapters/infrastructure={effect_count}"
)
