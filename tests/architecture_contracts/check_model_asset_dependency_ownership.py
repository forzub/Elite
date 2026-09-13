#!/usr/bin/env python3
"""Dependency-ownership / lexical-closure gate for Model Asset Editor.

This gate complements purity and module ownership.  It answers a different question:

    Does every named function use only dependencies that are explicit and reachable?

For every named function we classify bare function calls, owned runtime bindings and
writes.  Named calls are resolved through MODULE_OWNERSHIP_CONTRACT.json.  For code
already extracted into a physical ES module, cross-file dependencies must be reachable
through an ES import, a module-local declaration, or an explicit effect-factory port.
Unresolved bare calls are forbidden.

The purpose is to catch physical-split defects such as the historic
`axisDirectionTokens is not defined` and `lodIcon is not defined` before runtime.
"""
from __future__ import annotations

from pathlib import Path
import json
import re

import check_model_asset_function_purity as purity
from model_asset_editor_source_bundle import load_source_bundle, source_paths

ROOT = Path(__file__).resolve().parents[2]
OWNERSHIP_PATH = ROOT / "tools/model_asset_editor/MODULE_OWNERSHIP_CONTRACT.json"
CONTRACT = json.loads(OWNERSHIP_PATH.read_text(encoding="utf-8"))
BUNDLE = load_source_bundle(ROOT)
GRAPH = purity.analyze_function_graph(BUNDLE)
MODULES = CONTRACT["modules"]
CLASSES = CONTRACT.get("classes", {})
TOP_BINDINGS = CONTRACT.get("top_level_bindings", {})
MODULE_CONSTANTS = CONTRACT.get("module_constants", {})
POLICY = CONTRACT.get("dependency_policy", {})


def fail(message: str) -> None:
    raise AssertionError(f"dependency ownership: {message}")


if POLICY.get("unresolved_bare_calls") != "forbidden":
    fail("dependency_policy.unresolved_bare_calls must be forbidden")
if POLICY.get("physical_hidden_dependencies") != "forbidden":
    fail("dependency_policy.physical_hidden_dependencies must be forbidden")
if POLICY.get("cross_owner_named_calls") != "declared_import":
    fail("dependency_policy.cross_owner_named_calls must require declared_import")

PLATFORM_GLOBALS = set(POLICY.get("platform_globals", []))
if not PLATFORM_GLOBALS:
    fail("dependency_policy.platform_globals must be declared")

# Logical ownership maps.
function_owner: dict[str, str] = {}
function_role: dict[str, str] = {}
for module_name, module in MODULES.items():
    for role in ("core", "presentation", "adapters", "infrastructure"):
        for name in module.get(role, []):
            if name in function_owner:
                fail(f"duplicate function owner for {name}")
            function_owner[name] = module_name
            function_role[name] = role
if set(function_owner) != set(GRAPH):
    fail("function owner inventory diverges from named-function inventory")

# Source-unit inventory.  A named function is unique across the aggregate bundle, so
# physical source ownership is deterministic.
SOURCES: dict[str, str] = {}
for path in source_paths(ROOT):
    SOURCES[path.relative_to(ROOT).as_posix()] = path.read_text(encoding="utf-8", errors="replace")

function_source: dict[str, str] = {}
for name in GRAPH:
    marker = re.compile(rf"\bfunction\s+{re.escape(name)}\s*\(")
    matches = [rel for rel, source in SOURCES.items() if marker.search(source)]
    if len(matches) != 1:
        fail(f"function {name} must have exactly one physical declaration, got {matches}")
    function_source[name] = matches[0]


def parse_imports(source: str) -> set[str]:
    """Return local names introduced by static ES imports."""
    out: set[str] = set()
    for match in re.finditer(r"\bimport\s+(.+?)\s+from\s+['\"][^'\"]+['\"]\s*;", source, re.S):
        clause = match.group(1).strip()
        if clause.startswith("* as "):
            out.add(clause[5:].strip())
            continue
        if clause.startswith("{"):
            inside = clause[1:clause.rfind("}")]
            for part in inside.split(","):
                part = part.strip()
                if not part:
                    continue
                out.add(part.split(" as ")[-1].strip())
            continue
        # Default import, optionally followed by a named/namespace clause.
        first = clause.split(",", 1)[0].strip()
        if first:
            out.add(first)
    return out


def module_level_bindings(source: str) -> set[str]:
    """Conservatively collect module-scope function/class/const/let/var names."""
    code = purity.mask_js_for_analysis(source)
    out: set[str] = set(re.findall(r"\bfunction\s+([A-Za-z_$][\w$]*)\s*\(", code))
    out.update(re.findall(r"\bclass\s+([A-Za-z_$][\w$]*)\b", code))
    depth = 0
    i = 0
    while i < len(code):
        c = code[i]
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth == 0:
            match = re.match(r"\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_$][\w$]*)", code[i:])
            if match:
                out.add(match.group(1))
                i += match.end()
                continue
        i += 1
    out.update(parse_imports(source))
    return out


SOURCE_IMPORTS = {rel: parse_imports(source) for rel, source in SOURCES.items()}
SOURCE_BINDINGS = {}
for rel, source in SOURCES.items():
    if rel == "src/assets/webui/model_asset_editor.html":
        SOURCE_BINDINGS[rel] = set(purity.discover_module_bindings(source)) | SOURCE_IMPORTS[rel]
    else:
        SOURCE_BINDINGS[rel] = module_level_bindings(source)


def effect_factory_ports(module_name: str, source: str) -> set[str]:
    factory = MODULES[module_name].get("physical_factory")
    if not factory:
        return set()
    # Effect factories currently use an object-destructuring arrow parameter.  Keep
    # this deliberately strict: changing the factory shape requires explicit review.
    match = re.search(rf"\bconst\s+{re.escape(factory)}\s*=\s*\(\s*\{{([^}}]*)\}}\s*\)\s*=>", source)
    if not match:
        fail(f"cannot resolve explicit ports for physical effect factory {module_name}.{factory}")
    ports: set[str] = set()
    for part in match.group(1).split(","):
        part = part.strip()
        if not part:
            continue
        local = part.split(":", 1)[-1].split("=", 1)[0].strip()
        if re.fullmatch(r"[A-Za-z_$][\w$]*", local):
            ports.add(local)
    return ports


# Immutable module-owned data follows the same physical-closure rule as functions.
for constant, spec in MODULE_CONSTANTS.items():
    module_name = spec.get("module")
    rel = spec.get("physical_source")
    if module_name not in MODULES:
        fail(f"module constant {constant} references unknown owner {module_name}")
    if not rel or rel != MODULES[module_name].get("physical_source"):
        fail(f"module constant {constant} physical source does not match owner {module_name}")
    if rel not in SOURCES or not re.search(rf"\bconst\s+{re.escape(constant)}\s*=", SOURCES[rel]):
        fail(f"module constant {constant} is not declared in {rel}")

SOURCE_FACTORY_PORTS: dict[str, set[str]] = {rel: set() for rel in SOURCES}
for module_name, spec in MODULES.items():
    rel = spec.get("physical_source")
    if rel and spec.get("physical_factory"):
        SOURCE_FACTORY_PORTS[rel] = effect_factory_ports(module_name, SOURCES[rel])


# Exact logical cross-owner function calls must already be represented by the owner
# import graph.  This duplicates a small part of the ownership checker intentionally:
# here it becomes the first stage of dependency resolution before physical reachability.
for caller, row in GRAPH.items():
    caller_module = function_owner[caller]
    declared_imports = MODULES[caller_module].get("imports", {})
    for callee in row["calls"]:
        target_module = function_owner[callee]
        if target_module == caller_module:
            continue
        if callee not in declared_imports.get(target_module, []):
            fail(
                f"undeclared cross-owner call {caller_module}.{caller} -> "
                f"{target_module}.{callee}"
            )
        # Portable -> effect remains illegal irrespective of physical placement.
        if function_role[caller] in {"core", "presentation"} and function_role[callee] in {"adapters", "infrastructure"}:
            fail(
                f"portable caller crosses into effect owner: {caller_module}.{caller} -> "
                f"{target_module}.{callee}"
            )


# Resolve bare calls that are not in the named-function graph.  A call is acceptable
# only when it is a local variable/parameter (the purity analyzer already excludes
# those), a declared class constructor, an explicit platform global, or a physical
# module import/factory port.  Everything else is a likely runtime ReferenceError.
unresolved_calls: list[str] = []
for caller, row in GRAPH.items():
    rel = function_source[caller]
    allowed = PLATFORM_GLOBALS | set(CLASSES) | SOURCE_IMPORTS[rel] | SOURCE_FACTORY_PORTS[rel]
    for symbol in sorted(row["unknown_calls"]):
        if symbol == "of":  # lexer artefact from `for (... of (...))`; not a call.
            continue
        if symbol not in allowed:
            unresolved_calls.append(f"{function_owner[caller]}.{caller}: CALL {symbol} [{rel}]")
if unresolved_calls:
    fail("unresolved bare calls:\n  " + "\n  ".join(unresolved_calls))


# Known runtime bindings must respect ownership logically and must also be physically
# reachable once the caller has moved out of the composition-root HTML.
for caller in GRAPH:
    rel = function_source[caller]
    source = purity.strip_strings_and_comments(purity.extract_function(BUNDLE, caller))
    caller_module = function_owner[caller]
    for binding, spec in TOP_BINDINGS.items():
        if not re.search(rf"(?<![\w$]){re.escape(binding)}(?![\w$])", source):
            continue
        owner = spec["module"]
        if owner != caller_module and binding not in MODULES[caller_module].get("binding_imports", []):
            fail(f"{caller_module}.{caller} reads {owner}.{binding} without declared binding_import")
        if rel != "src/assets/webui/model_asset_editor.html":
            physically_reachable = (
                binding in SOURCE_BINDINGS[rel]
                or binding in SOURCE_IMPORTS[rel]
                or binding in SOURCE_FACTORY_PORTS[rel]
            )
            if not physically_reachable:
                fail(
                    f"physical hidden binding: {caller_module}.{caller} in {rel} reads {binding} "
                    f"owned by {owner}, but it is neither module-local, imported nor a factory port"
                )


# Named callees must also be physically reachable in extracted modules.  Same physical
# source is local; otherwise the callee must be imported or injected as a factory port.
for caller, row in GRAPH.items():
    rel = function_source[caller]
    if rel == "src/assets/webui/model_asset_editor.html":
        continue
    for callee in row["calls"]:
        callee_rel = function_source[callee]
        if callee_rel == rel:
            continue
        if callee not in SOURCE_IMPORTS[rel] and callee not in SOURCE_FACTORY_PORTS[rel]:
            fail(
                f"physical hidden call: {function_owner[caller]}.{caller} in {rel} -> "
                f"{function_owner[callee]}.{callee} in {callee_rel}; missing ES import/factory port"
            )



def function_local_names(name: str) -> tuple[set[str], str]:
    """Conservative lexical locals for free-identifier closure checks."""
    _, params_text, body_text = purity.extract_function_parts(BUNDLE, name)
    code = purity.strip_strings_and_comments(body_text)
    params, _ = purity._simple_parameter_names(params_text)
    local = set(params)
    if "{" in params_text or "[" in params_text:
        local.update(re.findall(r"[A-Za-z_$][\w$]*", purity.strip_strings_and_comments(params_text)))
    local.update(re.findall(r"\b(?:const|let|var|function|class)\s+([A-Za-z_$][\w$]*)\b", code))
    local.update(re.findall(r"(?:\b(?:const|let|var)\s+|,)\s*([A-Za-z_$][\w$]*)\s*=", code))
    for destructure in re.finditer(r"\b(?:const|let|var)\s*([\{\[])(.*?)([\}\]])\s*=", code):
        local.update(re.findall(r"[A-Za-z_$][\w$]*", destructure.group(2)))
    for destructure in re.finditer(r"\bfor\s*\(\s*(?:const|let|var)\s*([\{\[])(.*?)([\}\]])\s+(?:of|in)\b", code):
        local.update(re.findall(r"[A-Za-z_$][\w$]*", destructure.group(2)))
    local.update(re.findall(r"(?<![\w$])([A-Za-z_$][\w$]*)\s*=>", code))
    for arrow in re.finditer(r"\(([^()]*)\)\s*=>", code):
        local.update(re.findall(r"[A-Za-z_$][\w$]*", arrow.group(1)))
    local.update(re.findall(r"\bcatch\s*\(\s*([A-Za-z_$][\w$]*)", code))
    for nested in re.finditer(r"\bfunction(?:\s+[A-Za-z_$][\w$]*)?\s*\(", code):
        opening = code.find("(", nested.start())
        closing = purity._scan_balanced_js(code, opening, "(", ")")
        local.update(re.findall(r"[A-Za-z_$][\w$]*", code[opening + 1 : closing]))
    return local, code


JS_KEYWORDS = {
    "break", "case", "catch", "class", "const", "continue", "debugger", "default",
    "delete", "do", "else", "export", "extends", "finally", "for", "function", "if",
    "import", "in", "instanceof", "let", "new", "return", "super", "switch", "this",
    "throw", "try", "typeof", "var", "void", "while", "with", "yield", "async", "await",
    "of", "static", "get", "set", "true", "false", "null", "undefined",
}


def bare_identifier_references(code: str) -> set[str]:
    """Approximate lexical references while excluding member names/object keys."""
    out: set[str] = set()
    for match in re.finditer(r"(?<![\w$])([A-Za-z_$][\w$]*)(?![\w$])", code):
        name = match.group(1)
        if name in JS_KEYWORDS:
            continue
        before = match.start() - 1
        while before >= 0 and code[before].isspace():
            before -= 1
        if before >= 0 and code[before] == ".":
            continue
        after = match.end()
        while after < len(code) and code[after].isspace():
            after += 1
        if after < len(code) and code[after] == ":":
            # Static object-literal key or statement label, not a variable read.
            continue
        out.add(name)
    return out


# Module constants are private by default. A different logical owner may use one only
# through an explicit future export/import contract; no such cross-owner constants are
# currently accepted.
for caller in GRAPH:
    source = purity.strip_strings_and_comments(purity.extract_function(BUNDLE, caller))
    for constant, spec in MODULE_CONSTANTS.items():
        if re.search(rf"(?<![\w$]){re.escape(constant)}(?![\w$])", source) and function_owner[caller] != spec["module"]:
            fail(f"cross-owner module constant read: {function_owner[caller]}.{caller} -> {spec['module']}.{constant}")


# A physically extracted function must be lexically closed: every bare identifier is
# local, module-local/imported, an explicit effect-factory port, or a declared platform
# global.  This is the general read-side counterpart to the write/call checks below.
free_identifier_errors: list[str] = []
for caller, rel in function_source.items():
    if rel == "src/assets/webui/model_asset_editor.html":
        continue
    local_names, code = function_local_names(caller)
    allowed = local_names | SOURCE_BINDINGS[rel] | SOURCE_FACTORY_PORTS[rel] | PLATFORM_GLOBALS
    free = sorted(bare_identifier_references(code) - allowed)
    if free:
        free_identifier_errors.append(
            f"{function_owner[caller]}.{caller} [{rel}]: FREE {', '.join(free)}"
        )
if free_identifier_errors:
    fail("physical free identifiers:\n  " + "\n  ".join(free_identifier_errors))


# External writes are especially dangerous during decomposition.  Detect writes whose
# root is not a function parameter/local variable.  Known top-level bindings are legal
# only through the same ownership + physical reachability rules above; unknown roots
# are rejected outright.
write_errors: list[str] = []
for caller in GRAPH:
    rel = function_source[caller]
    local_names, code = function_local_names(caller)
    for match in re.finditer(
        r"(?<![\w$.])([A-Za-z_$][\w$]*)(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\s*(?:=|\+=|-=|\*=|/=|\+\+|--)(?!=)",
        code,
    ):
        root = match.group(1)
        if root in local_names:
            continue
        if root in TOP_BINDINGS:
            owner = TOP_BINDINGS[root]["module"]
            caller_module = function_owner[caller]
            if owner != caller_module and root not in MODULES[caller_module].get("binding_imports", []):
                write_errors.append(f"{caller_module}.{caller}: WRITE {root} owned by {owner} without binding_import")
            if rel != "src/assets/webui/model_asset_editor.html" and root not in SOURCE_BINDINGS[rel] | SOURCE_IMPORTS[rel] | SOURCE_FACTORY_PORTS[rel]:
                write_errors.append(f"{caller_module}.{caller}: WRITE {root} not physically reachable in {rel}")
            continue
        if root in {"this"}:
            continue
        if root in SOURCE_FACTORY_PORTS[rel] or root in SOURCE_IMPORTS[rel]:
            continue
        if rel == "src/assets/webui/model_asset_editor.html" and root in PLATFORM_GLOBALS:
            continue
        # Assigning a completely unresolved root would create/mutate accidental global
        # state in sloppy JS or throw in module strict mode.
        write_errors.append(f"{function_owner[caller]}.{caller}: unresolved WRITE root {root} [{rel}]")
if write_errors:
    fail("external write violations:\n  " + "\n  ".join(sorted(set(write_errors))))

physical_count = sum(1 for rel in function_source.values() if rel != "src/assets/webui/model_asset_editor.html")
print(
    "[PASS] Model Asset Editor dependency ownership / lexical closure: "
    f"{len(GRAPH)} named functions resolved; {physical_count} physically extracted function declarations checked; "
    "cross-owner calls declared; physical free identifiers=0; unresolved bare calls=0; hidden physical bindings/calls=0; external writes resolved"
)
