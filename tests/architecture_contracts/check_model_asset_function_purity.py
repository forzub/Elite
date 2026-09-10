#!/usr/bin/env python3
"""Function-purity + behavioural-equivalence contract for Model Asset Editor.

The harness has two layers:
  1. a static named-function call graph that protects the complete v0.10.66 low-risk
     purity surface from regressions; and
  2. frozen dynamic fixtures for functions being actively migrated, proving that
     explicit-argument purification preserves v0.10.66 observable results.

Complex orchestration / DOM / state mutation / command / I/O functions are inventoried
but intentionally deferred until they receive a separate migration contract.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
CONTRACT_PATH = ROOT / "tools/model_asset_editor/FUNCTION_PURITY_CONTRACT.json"
STATIC_BASELINE_PATH = ROOT / "tools/model_asset_editor/FUNCTION_PURITY_STATIC_BASELINE.json"


def _regex_can_start(src: str, slash: int) -> bool:
    """Conservative JavaScript lexer heuristic for `/.../` versus division."""
    j = slash - 1
    while j >= 0 and src[j].isspace():
        j -= 1
    if j < 0:
        return True
    if src[j] in "([{=,:;!?&|+-*%^~<>":
        return True
    if src[j] == ">" and j > 0 and src[j - 1] == "=":  # arrow => /re/
        return True
    k = j
    while k >= 0 and (src[k].isalnum() or src[k] in "_$"):
        k -= 1
    word = src[k + 1 : j + 1]
    return word in {
        "return", "case", "throw", "else", "do", "typeof", "instanceof",
        "in", "of", "yield", "await", "void", "delete",
    }


def _skip_js_string(src: str, i: int, quote: str) -> int:
    i += 1
    while i < len(src):
        if src[i] == "\\":
            i += 2
            continue
        if src[i] == quote:
            return i + 1
        i += 1
    return len(src)


def _skip_js_regex(src: str, i: int) -> int:
    i += 1
    in_class = False
    while i < len(src):
        c = src[i]
        if c == "\\":
            i += 2
            continue
        if c == "[":
            in_class = True
        elif c == "]":
            in_class = False
        elif c == "/" and not in_class:
            i += 1
            while i < len(src) and (src[i].isalpha() or src[i].isdigit()):
                i += 1
            return i
        i += 1
    return len(src)


def _skip_js_template(src: str, i: int) -> int:
    """Skip a template literal while correctly skipping nested `${...}` expressions."""
    i += 1
    while i < len(src):
        c = src[i]
        if c == "\\":
            i += 2
            continue
        if c == "`":
            return i + 1
        if c == "$" and i + 1 < len(src) and src[i + 1] == "{":
            close = _scan_balanced_js(src, i + 1, "{", "}")
            i = close + 1
            continue
        i += 1
    return len(src)


def _scan_balanced_js(src: str, opening: int, open_char: str, close_char: str) -> int:
    if opening >= len(src) or src[opening] != open_char:
        raise AssertionError(f"expected {open_char!r} at JS offset {opening}")
    depth = 1
    i = opening + 1
    while i < len(src):
        c = src[i]
        n = src[i + 1] if i + 1 < len(src) else ""
        if c == "/" and n == "/":
            nl = src.find("\n", i + 2)
            i = len(src) if nl < 0 else nl + 1
            continue
        if c == "/" and n == "*":
            end = src.find("*/", i + 2)
            i = len(src) if end < 0 else end + 2
            continue
        if c in "'\"":
            i = _skip_js_string(src, i, c)
            continue
        if c == "`":
            i = _skip_js_template(src, i)
            continue
        if c == "/" and _regex_can_start(src, i):
            i = _skip_js_regex(src, i)
            continue
        if c == open_char:
            depth += 1
        elif c == close_char:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise AssertionError(f"unbalanced JavaScript {open_char}{close_char} starting at offset {opening}")


def _skip_js_trivia(src: str, i: int) -> int:
    while i < len(src):
        if src[i].isspace():
            i += 1
            continue
        if src.startswith("//", i):
            nl = src.find("\n", i + 2)
            i = len(src) if nl < 0 else nl + 1
            continue
        if src.startswith("/*", i):
            end = src.find("*/", i + 2)
            i = len(src) if end < 0 else end + 2
            continue
        break
    return i


def _function_span(src: str, name: str) -> tuple[int, int, int, int, int, int]:
    match = re.search(rf"\bfunction\s+{re.escape(name)}\s*\(", src)
    if not match:
        raise AssertionError(f"purity contract function missing: {name}")
    open_paren = src.find("(", match.start(), match.end())
    close_paren = _scan_balanced_js(src, open_paren, "(", ")")
    body_open = _skip_js_trivia(src, close_paren + 1)
    if body_open >= len(src) or src[body_open] != "{":
        raise AssertionError(f"function {name} body opening brace missing")
    body_close = _scan_balanced_js(src, body_open, "{", "}")
    return match.start(), body_close + 1, open_paren, close_paren, body_open, body_close


def extract_function(src: str, name: str) -> str:
    start, end, *_ = _function_span(src, name)
    return src[start:end]


def extract_function_parts(src: str, name: str) -> tuple[str, str, str]:
    start, end, open_paren, close_paren, body_open, body_close = _function_span(src, name)
    return src[start:end], src[open_paren + 1 : close_paren], src[body_open + 1 : body_close]


def extract_const(src: str, name: str) -> str:
    match = re.search(rf"\bconst\s+{re.escape(name)}\s*=.*?;", src)
    if not match:
        raise AssertionError(f"purity contract constant missing: {name}")
    return match.group(0)


SIDE_EFFECT_PATTERNS = {
    "DOM_UI": (
        r"\b(?:document|window)\b|\$\s*\(|\.innerHTML\b|\.outerHTML\b|\.textContent\b|"
        r"\.classList\b|\.style\b|\.appendChild\b|\.prepend\s*\(|\.replaceChildren\s*\(|"
        r"\.querySelector(?:All)?\b|\.getBoundingClientRect\b|\.scrollIntoView\b|"
        r"\.focus\b|\.blur\b|\.click\b|\.setAttribute\b|\.removeAttribute\b|"
        r"\.addEventListener\b|\.removeEventListener\b|\bResizeObserver\b"
    ),
    "BACKEND_COMMAND": r"\bsend\s*\(",
    "STATUS_PROMPT": r"\b(?:localStatus|status|confirm|prompt|alert|notice)\s*\(",
    "STORAGE_NETWORK": r"\b(?:localStorage|sessionStorage|fetch|XMLHttpRequest|WebSocket)\b",
    "TIMERS_EVENTS": r"\b(?:requestAnimationFrame|cancelAnimationFrame|setTimeout|clearTimeout|setInterval|clearInterval|dispatchEvent)\s*\(",
    "NONDETERMINISTIC": r"\b(?:performance\.now|Date\.now|Math\.random)\s*\(",
    "CONSOLE_IO": r"\bconsole\.(?:log|info|warn|error|debug|trace)\s*\(",
}
GLOBAL_STATE_PATTERN = re.compile(r"\b(?:state|editorViewState)\b")
GLOBAL_MUTATION_PATTERNS = [
    re.compile(r"\b(?:state|editorViewState)(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])+\s*(?:=|\+=|-=|\*=|/=|\+\+|--)(?!=)"),
    re.compile(r"\b(?:state|editorViewState)(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\.(?:set|add|delete|clear|push|pop|shift|unshift|splice|sort|reverse)\s*\("),
]
MUTATING_METHODS = (
    "set", "add", "delete", "clear", "push", "pop", "shift", "unshift", "splice", "sort", "reverse",
    "copy", "setFromMatrixPosition", "setFromPoints", "setFromCamera", "addScaledVector", "multiply", "premultiply",
    "multiplyScalar", "applyMatrix4", "transformDirection", "normalize", "cross", "crossVectors", "sub", "subVectors",
    "compose", "decompose", "invert", "transpose", "setPosition", "lookAt", "updateMatrix", "updateMatrixWorld",
    "clearGroups", "addGroup", "setAttribute", "setIndex", "computeVertexNormals", "computeBoundingBox", "computeBoundingSphere",
)

READONLY_METHODS = {
    "get", "has", "find", "findIndex", "some", "every", "map", "filter", "flatMap", "reduce", "reduceRight",
    "slice", "concat", "includes", "indexOf", "lastIndexOf", "join", "at", "keys", "values", "entries",
    "toString", "toFixed", "toPrecision", "toLocaleString", "toUpperCase", "toLowerCase", "trim", "split",
    "startsWith", "endsWith", "substring", "substr", "charAt", "localeCompare", "replace", "replaceAll",
    "clone", "equals", "length", "lengthSq", "dot", "distanceTo", "distanceToSquared",
}
HARD_EFFECT_ORDER = [
    "STATE_MUTATION", "GLOBAL_MUTATION", "ARG_MUTATION", "DOM_UI", "BACKEND_COMMAND",
    "STATUS_PROMPT", "STORAGE_NETWORK", "TIMERS_EVENTS", "NONDETERMINISTIC", "CONSOLE_IO",
]
SAFE_EXTERNAL_CALLS = {
    "String", "Number", "Boolean", "BigInt", "parseInt", "parseFloat", "isFinite", "isNaN",
    "Object", "Array", "Set", "Map", "Date", "RegExp", "Error", "TypeError", "JSON",
    "encodeURIComponent", "decodeURIComponent", "Uint8Array", "Uint16Array", "Uint32Array", "Int8Array", "Int16Array", "Int32Array",
    "Float32Array", "Float64Array", "DataView", "TextDecoder", "TextEncoder",
}


def _blank(out: list[str], start: int, end: int) -> None:
    for j in range(start, min(end, len(out))):
        if out[j] not in "\r\n":
            out[j] = " "


def mask_js_for_analysis(src: str) -> str:
    """Mask literals/comments/regex while preserving `${ ... }` JavaScript expressions."""
    out = list(src)
    n = len(src)

    def scan_code(i: int, stop_on_template_brace: bool = False) -> int:
        brace_depth = 0
        while i < n:
            c = src[i]
            nn = src[i + 1] if i + 1 < n else ""
            if stop_on_template_brace and c == "}" and brace_depth == 0:
                return i + 1
            if c == "/" and nn == "/":
                end = src.find("\n", i + 2)
                end = n if end < 0 else end
                _blank(out, i, end)
                i = end
                continue
            if c == "/" and nn == "*":
                end = src.find("*/", i + 2)
                end = n if end < 0 else end + 2
                _blank(out, i, end)
                i = end
                continue
            if c in "'\"":
                end = _skip_js_string(src, i, c)
                _blank(out, i, end)
                i = end
                continue
            if c == "/" and _regex_can_start("".join(out), i):
                end = _skip_js_regex(src, i)
                _blank(out, i, end)
                i = end
                continue
            if c == "`":
                i = scan_template(i)
                continue
            if stop_on_template_brace:
                if c == "{":
                    brace_depth += 1
                elif c == "}":
                    brace_depth -= 1
            i += 1
        return i

    def scan_template(i: int) -> int:
        _blank(out, i, i + 1)
        i += 1
        while i < n:
            c = src[i]
            if c == "\\":
                _blank(out, i, min(i + 2, n))
                i += 2
                continue
            if c == "`":
                _blank(out, i, i + 1)
                return i + 1
            if c == "$" and i + 1 < n and src[i + 1] == "{":
                _blank(out, i, i + 1)  # keep the brace and expression code
                i = scan_code(i + 2, stop_on_template_brace=True)
                continue
            _blank(out, i, i + 1)
            i += 1
        return i

    scan_code(0)
    return "".join(out)


def strip_strings_and_comments(src: str) -> str:
    # Kept as the public helper used by the original contract checks. The stronger
    # lexer also masks regex literals and template-literal raw text.
    return mask_js_for_analysis(src)


def _simple_parameter_names(params: str) -> tuple[set[str], bool]:
    masked = mask_js_for_analysis(params)
    pieces: list[str] = []
    start = 0
    depth = 0
    for i, c in enumerate(masked):
        if c in "([{":
            depth += 1
        elif c in ")]}" and depth:
            depth -= 1
        elif c == "," and depth == 0:
            pieces.append(params[start:i])
            start = i + 1
    pieces.append(params[start:])
    names: set[str] = set()
    complex_params = False
    for piece in pieces:
        piece = piece.strip()
        if not piece:
            continue
        match = re.match(r"([A-Za-z_$][\w$]*)\b", piece)
        if match:
            names.add(match.group(1))
        else:
            complex_params = True
    return names, complex_params


def _module_script(web: str) -> str:
    match = re.search(r'<script\s+type=["\']module["\'][^>]*>', web)
    if not match:
        raise AssertionError("Model Asset Editor module script not found")
    end = web.find("</script>", match.end())
    if end < 0:
        raise AssertionError("Model Asset Editor module script closing tag not found")
    return web[match.end() : end]


def discover_module_bindings(web: str) -> dict[str, str]:
    """Return top-level const/let/var bindings from the module script."""
    script = _module_script(web)
    masked = mask_js_for_analysis(script)
    depth = 0
    i = 0
    out: dict[str, str] = {}
    while i < len(masked):
        c = masked[i]
        if c == "{":
            depth += 1
            i += 1
            continue
        if c == "}":
            depth = max(0, depth - 1)
            i += 1
            continue
        if depth == 0 and (i == 0 or not (masked[i - 1].isalnum() or masked[i - 1] in "_$")):
            match = re.match(r"(const|let|var)\s+([A-Za-z_$][\w$]*)", masked[i:])
            if match:
                out[match.group(2)] = match.group(1)
                i += match.end()
                continue
        i += 1
    return out


def discover_mutable_module_bindings(web: str) -> set[str]:
    """Conservatively identify top-level bindings whose value changes in the editor."""
    script = _module_script(web)
    code = mask_js_for_analysis(script)
    bindings = discover_module_bindings(web)
    mutable: set[str] = set()
    generic_mutators = (
        "set", "add", "delete", "clear", "push", "pop", "shift", "unshift", "splice", "sort", "reverse",
        "copy", "setFromCamera", "setFromMatrixPosition", "setFromPoints", "addScaledVector", "multiply", "premultiply",
    )
    methods = "|".join(map(re.escape, generic_mutators))
    for name, kind in bindings.items():
        escaped = re.escape(name)
        # let/var are mutable by declaration even if this particular snapshot has only
        # one assignment; reading them is still a hidden dependency.
        if kind in {"let", "var"}:
            mutable.add(name)
        # Ignore the declaration initializer by requiring either a property path or an
        # assignment occurrence not immediately preceded by const/let/var text.
        if re.search(rf"\b{escaped}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])+\s*(?:=|\+=|-=|\*=|/=|\+\+|--)(?!=)", code):
            mutable.add(name)
        if re.search(rf"\b{escaped}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\.(?:{methods}|set[A-Z][A-Za-z0-9_$]*|clear[A-Z][A-Za-z0-9_$]*|reset[A-Z][A-Za-z0-9_$]*|commit[A-Z][A-Za-z0-9_$]*|accept[A-Z][A-Za-z0-9_$]*)\s*\??\.??\s*\(", code):
            mutable.add(name)
        # Known central mutable objects use methods that cannot be exhaustively inferred
        # from spelling alone.
        if name in {"state", "editorViewState"}:
            mutable.add(name)
    return mutable


def _split_top_level(text: str, delimiter: str = ",") -> list[str]:
    parts: list[str] = []
    start = 0
    paren = bracket = brace = 0
    for i, c in enumerate(text):
        if c == "(": paren += 1
        elif c == ")": paren = max(0, paren - 1)
        elif c == "[": bracket += 1
        elif c == "]": bracket = max(0, bracket - 1)
        elif c == "{": brace += 1
        elif c == "}": brace = max(0, brace - 1)
        elif c == delimiter and paren == bracket == brace == 0:
            parts.append(text[start:i])
            start = i + 1
    parts.append(text[start:])
    return parts


def _local_initializers(code: str) -> list[tuple[str, str]]:
    out: list[tuple[str, str]] = []
    for match in re.finditer(r"\b(?:const|let|var)\b", code):
        # Ignore a keyword nested inside an already-consumed declaration statement.
        i = match.end()
        paren = bracket = brace = 0
        end = i
        while end < len(code):
            c = code[end]
            if c == "(": paren += 1
            elif c == ")": paren = max(0, paren - 1)
            elif c == "[": bracket += 1
            elif c == "]": bracket = max(0, bracket - 1)
            elif c == "{": brace += 1
            elif c == "}": brace = max(0, brace - 1)
            elif c == ";" and paren == bracket == brace == 0:
                break
            end += 1
        declaration = code[i:end]
        for piece in _split_top_level(declaration):
            m = re.match(r"\s*([A-Za-z_$][\w$]*)\s*=\s*(.*)\Z", piece, re.S)
            if m:
                out.append((m.group(1), m.group(2).strip()))
    return out


def _expression_alias_kind(expr: str, direct: set[str], element_containers: set[str]) -> str | None:
    stripped = expr.strip()
    # A direct alias/root property can expose the same external object. Obvious scalar
    # comparisons/arithmetic break alias identity and are not propagated.
    scalar_ops = re.compile(r"(?:===|!==|==|!=|<=|>=|(?<![?])<(?![<=])|(?<![?])>(?![=>])|&&|\+|-|\*|/|%)")
    safe_new_container = {"map", "flatMap"}
    element_preserving_copy = {"filter", "slice", "concat", "toSorted", "toReversed"}
    for root in direct:
        escaped = re.escape(root)
        if re.match(rf"^{escaped}\b", stripped):
            method = re.match(rf"^{escaped}(?:\?\.|\.)([A-Za-z_$][\w$]*)\s*\??\.??\s*\(", stripped)
            if method:
                if method.group(1) in safe_new_container:
                    return None
                if method.group(1) in element_preserving_copy:
                    return "elements"
                if method.group(1) == "clone":
                    return None
                # get/find/at and unknown methods can return an aliased object.
                return "direct"
            if scalar_ops.search(stripped):
                return None
            return "direct"
        # Common wrapper used by preview material code: build a new array containing
        # references from an external array/object.
        if re.match(rf"^Array\.isArray\([^)]*\b{escaped}\b[^)]*\)\s*\?", stripped) and re.search(rf"\b{escaped}\b", stripped):
            return "elements"
    for root in element_containers:
        escaped = re.escape(root)
        if re.match(rf"^{escaped}(?:\[[^\]]+\]|(?:\?\.|\.)(?:at|find)\s*\()", stripped):
            return "direct"
    if re.match(r"^(?:new\s+(?:Set|Map|Array)|Array\.from|Object\.assign)\s*\(", stripped):
        return None
    return None


def _tainted_aliases(code: str, roots: set[str]) -> tuple[set[str], set[str]]:
    direct = set(roots)
    element_containers: set[str] = set()
    initializers = _local_initializers(code)
    changed = True
    while changed:
        changed = False
        for dst, expr in initializers:
            if dst in direct or dst in element_containers:
                continue
            kind = _expression_alias_kind(expr, direct, element_containers)
            if kind == "direct":
                direct.add(dst); changed = True
            elif kind == "elements":
                element_containers.add(dst); changed = True
        # for (const item of container): iterating either a direct external container or
        # a new container holding external elements yields an external element alias.
        for match in re.finditer(r"\bfor\s*\(\s*(?:const|let|var)\s+([A-Za-z_$][\w$]*)\s+of\s+([^\)]+)\)", code):
            dst, expr = match.group(1), match.group(2).strip()
            if dst in direct:
                continue
            if any(re.search(rf"\b{re.escape(root)}\b", expr) for root in direct | element_containers):
                direct.add(dst); changed = True
        # Iteration callbacks receive source elements, including map/filter callbacks;
        # the *resulting container* may be new, but callback parameters still alias the
        # source elements.
        for root in list(direct | element_containers):
            for match in re.finditer(
                rf"\b{re.escape(root)}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\."
                r"(?:map|flatMap|filter|forEach|some|every|find|findIndex|reduce)\s*\(\s*([A-Za-z_$][\w$]*)\s*=>",
                code,
            ):
                dst = match.group(1)
                if dst not in direct:
                    direct.add(dst); changed = True
    return direct, element_containers


def _tainted_mutation(code: str, tainted: set[str]) -> bool:
    for name in tainted:
        escaped = re.escape(name)
        if re.search(rf"\b{escaped}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])+\s*(?:=|\+=|-=|\*=|/=|\+\+|--)(?!=)", code):
            return True
        # Strict rule for external aliases: an object method is assumed mutating unless
        # it is in the reviewed read-only whitelist. This catches cursor-like readers
        # (`r.u32()`) and browser/event methods without pretending arbitrary methods are pure.
        for match in re.finditer(
            rf"\b{escaped}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\.([A-Za-z_$][\w$]*)\s*\??\.??\s*\(",
            code,
        ):
            if match.group(1) not in READONLY_METHODS:
                return True
    return False


def analyze_direct_function_effects(
    signature_code: str,
    body_code: str,
    params: set[str],
    mutable_globals: set[str],
    local_names: set[str],
) -> tuple[set[str], set[str]]:
    combined = signature_code + "\n" + body_code
    effects: set[str] = set()
    hidden_reads = {
        name for name in mutable_globals
        if name not in local_names and re.search(rf"\b{re.escape(name)}\b", combined)
    }
    state_roots = {name for name in hidden_reads if name in {"state", "editorViewState"}}
    other_global_roots = hidden_reads - state_roots
    param_aliases, _ = _tainted_aliases(body_code, params)
    if _tainted_mutation(body_code, param_aliases):
        effects.add("ARG_MUTATION")
    state_aliases, _ = _tainted_aliases(body_code, state_roots)
    if state_roots and _tainted_mutation(body_code, state_aliases):
        effects.add("STATE_MUTATION")
    global_aliases, _ = _tainted_aliases(body_code, other_global_roots)
    if other_global_roots and _tainted_mutation(body_code, global_aliases):
        effects.add("GLOBAL_MUTATION")
    for label, pattern in SIDE_EFFECT_PATTERNS.items():
        if re.search(pattern, combined):
            effects.add(label)
    return effects, hidden_reads


def _argument_mutation(code: str, params: set[str]) -> bool:
    methods = "|".join(map(re.escape, MUTATING_METHODS))
    for name in params:
        prop = rf"\b{re.escape(name)}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])+"
        if re.search(prop + r"\s*(?:=|\+=|-=|\*=|/=|\+\+|--)(?!=)", code):
            return True
        if re.search(rf"\b{re.escape(name)}(?:\.[A-Za-z_$][\w$]*|\[[^\]]+\])*\.(?:{methods})\s*\(", code):
            return True
    return False


def _direct_effects(code: str, params: set[str]) -> set[str]:
    effects: set[str] = set()
    if any(pattern.search(code) for pattern in GLOBAL_MUTATION_PATTERNS):
        effects.add("STATE_MUTATION")
    if _argument_mutation(code, params):
        effects.add("ARG_MUTATION")
    for label, pattern in SIDE_EFFECT_PATTERNS.items():
        if re.search(pattern, code):
            effects.add(label)
    return effects


def static_check_pure(name: str, body: str) -> None:
    _, params_text, body_text = extract_function_parts(body, name)
    code = strip_strings_and_comments(body_text)
    params, _ = _simple_parameter_names(params_text)
    if GLOBAL_STATE_PATTERN.search(code):
        raise AssertionError(f"pure function {name} regained hidden global editor state")
    effects = _direct_effects(code, params)
    if effects:
        raise AssertionError(f"pure function {name} gained side effect(s): {sorted(effects)}")


def static_check_candidate(name: str, body: str, allowed_reads: list[str]) -> None:
    _, params_text, body_text = extract_function_parts(body, name)
    code = strip_strings_and_comments(body_text)
    params, _ = _simple_parameter_names(params_text)
    effects = _direct_effects(code, params)
    if effects:
        raise AssertionError(f"easy-purity candidate {name} is no longer read-only: {sorted(effects)}")
    for global_name in ("state", "editorViewState"):
        if re.search(rf"\b{global_name}\b", code) and global_name not in allowed_reads:
            raise AssertionError(f"easy-purity candidate {name} has undeclared global read {global_name}")


def _named_function_names(web: str) -> list[str]:
    names = re.findall(r"\bfunction\s+([A-Za-z_$][\w$]*)\s*\(", web)
    duplicates = sorted({name for name in names if names.count(name) > 1})
    if duplicates:
        raise AssertionError(f"purity call-graph requires unique named function declarations; duplicates: {duplicates}")
    return names


def _internal_calls(code: str, all_names: set[str], own_name: str) -> set[str]:
    calls: set[str] = set()
    for match in re.finditer(r"(?<![\w$.])([A-Za-z_$][\w$]*)\s*\(", code):
        name = match.group(1)
        if name in all_names and name != own_name:
            calls.add(name)
    # Named callbacks passed directly to standard iteration/event helpers execute too.
    for match in re.finditer(r"\.(?:map|filter|forEach|some|every|find|findIndex|reduce|sort)\s*\(\s*([A-Za-z_$][\w$]*)\b", code):
        name = match.group(1)
        if name in all_names and name != own_name:
            calls.add(name)
    return calls


def _unknown_external_calls(code: str, all_names: set[str], local_names: set[str]) -> set[str]:
    unknown: set[str] = set()
    for match in re.finditer(r"(?<![\w$.])([A-Za-z_$][\w$]*)\s*\(", code):
        name = match.group(1)
        if name in all_names or name in SAFE_EXTERNAL_CALLS or name in local_names:
            continue
        if name in {"if", "for", "while", "switch", "catch", "function", "typeof", "return", "throw", "new"}:
            continue
        # Explicitly classified effect surfaces are not also UNKNOWN.
        if name in {"send", "localStatus", "status", "confirm", "prompt", "alert", "notice",
                    "fetch", "setTimeout", "clearTimeout", "setInterval", "clearInterval",
                    "requestAnimationFrame", "cancelAnimationFrame", "dispatchEvent"}:
            continue
        if name == "$":
            continue
        unknown.add(name)
    return unknown


def analyze_function_graph(web: str) -> dict[str, dict[str, Any]]:
    names = _named_function_names(web)
    all_names = set(names)
    mutable_globals = discover_mutable_module_bindings(web)
    graph: dict[str, dict[str, Any]] = {}
    for name in names:
        try:
            _, params_text, body_text = extract_function_parts(web, name)
        except AssertionError as exc:
            graph[name] = {
                "name": name, "direct_effects": set(), "effects": set(), "hidden_reads": set(),
                "transitive_hidden_reads": set(), "calls": set(), "unknown_calls": set(),
                "parse_error": str(exc), "complex_params": True,
            }
            continue
        signature_code = strip_strings_and_comments(params_text)
        code = strip_strings_and_comments(body_text)
        params, complex_params = _simple_parameter_names(params_text)
        local_names = (
            set(re.findall(r"\b(?:const|let|var|function)\s+([A-Za-z_$][\w$]*)\b", code))
            | set(re.findall(r"(?:\b(?:const|let|var)\s+|,)\s*([A-Za-z_$][\w$]*)\s*=", code))
            | params
        )
        effects, hidden_reads = analyze_direct_function_effects(
            signature_code, code, params, mutable_globals, local_names
        )
        graph[name] = {
            "name": name,
            "direct_effects": effects,
            "effects": set(),
            "hidden_reads": hidden_reads,
            "transitive_hidden_reads": set(),
            "calls": _internal_calls(signature_code + "\n" + code, all_names, name),
            "unknown_calls": _unknown_external_calls(signature_code + "\n" + code, all_names, local_names),
            "parse_error": None,
            "complex_params": complex_params,
        }

    # Fixed point over cycles/SCCs: propagate hard effects and hidden mutable reads.
    for row in graph.values():
        row["effects"] = set(row["direct_effects"])
        row["transitive_hidden_reads"] = set(row["hidden_reads"])
    changed = True
    while changed:
        changed = False
        for row in graph.values():
            effects = set(row["direct_effects"])
            hidden_reads = set(row["hidden_reads"])
            for callee in row["calls"]:
                child = graph[callee]
                effects.update(child["effects"])
                hidden_reads.update(child["transitive_hidden_reads"])
            if effects != row["effects"] or hidden_reads != row["transitive_hidden_reads"]:
                row["effects"] = effects
                row["transitive_hidden_reads"] = hidden_reads
                changed = True

    def classify(row: dict[str, Any]) -> str:
        if row["parse_error"]:
            return "UNKNOWN"
        hard = set(row["effects"])
        if len(hard) > 1:
            return "COMPOSITE"
        if hard:
            effect = next(iter(hard))
            return {
                "STATE_MUTATION": "STATE_MUTATING",
                "GLOBAL_MUTATION": "GLOBAL_MUTATING",
                "ARG_MUTATION": "ARG_MUTATING",
                "DOM_UI": "DOM_UI",
                "BACKEND_COMMAND": "COMMAND",
                "STATUS_PROMPT": "STATUS_IO",
                "STORAGE_NETWORK": "IO",
                "TIMERS_EVENTS": "EVENT_LOOP",
                "NONDETERMINISTIC": "NONDETERMINISTIC",
                "CONSOLE_IO": "IO",
            }[effect]
        if row["unknown_calls"] or row["complex_params"]:
            return "UNKNOWN"
        if row["transitive_hidden_reads"]:
            child_candidates = [graph[c]["category"] for c in row["calls"]]
            if row["hidden_reads"] and all(cat == "PURE" for cat in child_candidates):
                return "EASY_CANDIDATE"
            return "TRANSITIVE_CANDIDATE"
        return "PURE"

    for row in graph.values():
        row["category"] = "UNKNOWN"
    for _ in range(len(graph) + 2):
        changed = False
        for row in graph.values():
            category = classify(row)
            if category != row["category"]:
                row["category"] = category
                changed = True
        if not changed:
            break
    return graph


def _call_trace_to_effect(graph: dict[str, dict[str, Any]], start: str, effect: str) -> list[str]:
    queue: list[tuple[str, list[str]]] = [(start, [start])]
    seen = {start}
    while queue:
        name, path = queue.pop(0)
        row = graph[name]
        if effect in row["direct_effects"]:
            return path
        for callee in sorted(row["calls"]):
            if callee not in seen and effect in graph[callee]["effects"]:
                seen.add(callee)
                queue.append((callee, path + [callee]))
    return [start]


def validate_contracted_graph(graph: dict[str, dict[str, Any]], contract: dict[str, Any]) -> None:
    for entry in contract["pure"]:
        name = entry["name"]
        category = graph[name]["category"]
        if category != "PURE":
            row = graph[name]
            raise AssertionError(
                f"certified pure function {name} is not transitively pure: category={category}, "
                f"effects={sorted(row['effects'])}, unknown_calls={sorted(row['unknown_calls'])}, hidden_reads={sorted(row.get('transitive_hidden_reads', []))}"
            )
    for entry in contract["easy_candidates"]:
        name = entry["name"]
        category = graph[name]["category"]
        if category not in {"EASY_CANDIDATE", "TRANSITIVE_CANDIDATE", "PURE"}:
            row = graph[name]
            traces = []
            for effect in sorted(row["effects"]):
                traces.append(" -> ".join(_call_trace_to_effect(graph, name, effect)))
            raise AssertionError(
                f"certified purity candidate {name} gained a hard transitive effect: "
                f"category={category}, effects={sorted(row['effects'])}, traces={traces}, "
                f"unknown_calls={sorted(row['unknown_calls'])}, hidden_reads={sorted(row.get('transitive_hidden_reads', []))}"
            )


def graph_report(graph: dict[str, dict[str, Any]]) -> dict[str, list[str]]:
    groups: dict[str, list[str]] = {}
    for name, row in graph.items():
        groups.setdefault(row["category"], []).append(name)
    for values in groups.values():
        values.sort()
    return dict(sorted(groups.items()))

def validate_static_baseline(graph: dict[str, dict[str, Any]], baseline: dict[str, Any]) -> None:
    if baseline.get("schema") != 1:
        raise AssertionError("unsupported FUNCTION_PURITY_STATIC_BASELINE schema")
    pure = set(baseline.get("pure", []))
    easy_map = baseline.get("easy_candidates", {})
    transitive_map = baseline.get("transitive_candidates", {})
    easy = set(easy_map)
    transitive = set(transitive_map)
    deferred = set(baseline.get("deferred", []))
    baseline_names = pure | easy | transitive | deferred
    current_names = set(graph)
    added = sorted(current_names - baseline_names)
    missing = sorted(baseline_names - current_names)
    if added or missing:
        raise AssertionError(
            "function-purity static inventory changed and requires explicit review: "
            f"added={added}, missing={missing}"
        )
    overlaps = (pure & easy) | (pure & transitive) | (pure & deferred) | (easy & transitive) | (easy & deferred) | (transitive & deferred)
    if overlaps:
        raise AssertionError(f"FUNCTION_PURITY_STATIC_BASELINE categories overlap: {sorted(overlaps)}")

    for name in sorted(pure):
        category = graph[name]["category"]
        if category != "PURE":
            row = graph[name]
            raise AssertionError(
                f"static purity regression: {name} was PURE in v0.10.66 and is now {category}; "
                f"effects={sorted(row['effects'])}, hidden={sorted(row['transitive_hidden_reads'])}, "
                f"unknown={sorted(row['unknown_calls'])}"
            )

    for name in sorted(easy):
        row = graph[name]
        if row["category"] not in {"PURE", "EASY_CANDIDATE"}:
            raise AssertionError(
                f"static purity regression: {name} was EASY_CANDIDATE and is now {row['category']}; "
                f"effects={sorted(row['effects'])}, hidden={sorted(row['transitive_hidden_reads'])}"
            )
        allowed = set(easy_map[name])
        current = set(row["transitive_hidden_reads"])
        extra = sorted(current - allowed)
        if extra:
            raise AssertionError(
                f"hidden dependency regression in EASY_CANDIDATE {name}: new mutable globals {extra}; "
                f"baseline={sorted(allowed)}, current={sorted(current)}"
            )

    for name in sorted(transitive):
        row = graph[name]
        if row["category"] not in {"PURE", "EASY_CANDIDATE", "TRANSITIVE_CANDIDATE"}:
            raise AssertionError(
                f"static purity regression: {name} was TRANSITIVE_CANDIDATE and is now {row['category']}; "
                f"effects={sorted(row['effects'])}, hidden={sorted(row['transitive_hidden_reads'])}"
            )
        allowed = set(transitive_map[name])
        current = set(row["transitive_hidden_reads"])
        extra = sorted(current - allowed)
        if extra:
            raise AssertionError(
                f"hidden dependency regression in TRANSITIVE_CANDIDATE {name}: new mutable globals {extra}; "
                f"baseline={sorted(allowed)}, current={sorted(current)}"
            )


def build_node_program(web: str, contract: dict[str, Any]) -> str:
    names: list[str] = []
    constants: list[str] = []
    for group in ("pure", "easy_candidates"):
        for entry in contract[group]:
            names.append(entry["name"])
            names.extend(entry.get("support", []))
            constants.extend(entry.get("constants", []))
    # activeRenderLod is intentionally a stable test adapter, not copied from UI;
    # candidates use it only as a read-only projection of fixture state.
    names = [name for name in dict.fromkeys(names) if name != "activeRenderLod"]
    constants = list(dict.fromkeys(constants))
    source_parts = [extract_const(web, name) for name in constants]
    source_parts.extend(extract_function(web, name) for name in names)
    source = "\n".join(source_parts)
    payload = json.dumps(contract, ensure_ascii=False)
    return f"""
import * as THREE from './src/assets/webui/vendor/three/three.module.js';
const contract = {payload};
{source}
let state = {{}};
function activeRenderLod() {{
  const i = Number(state?.activeLod ?? 0);
  return state?.asset?.renderLods?.[i] ?? null;
}}
function revive(value) {{
  if (Array.isArray(value)) return value.map(revive);
  if (value && typeof value === 'object') {{
    if (Object.prototype.hasOwnProperty.call(value, '__set__')) return new Set(value.__set__.map(revive));
    if (Object.prototype.hasOwnProperty.call(value, '__map__')) return new Map(value.__map__.map(([k,v]) => [revive(k), revive(v)]));
    if (Object.prototype.hasOwnProperty.call(value, '__matrix4__')) return new THREE.Matrix4().fromArray(value.__matrix4__.map(Number));
    if (Object.prototype.hasOwnProperty.call(value, '__vector3__')) return new THREE.Vector3(...value.__vector3__.map(Number));
    const out = {{}};
    for (const [k,v] of Object.entries(value)) out[k] = revive(v);
    return out;
  }}
  return value;
}}
function canonical(value) {{
  if (value === undefined) return {{__undefined__: true}};
  if (typeof value === 'number' && Number.isNaN(value)) return {{__number__: 'NaN'}};
  if (value === Infinity) return {{__number__: 'Infinity'}};
  if (value === -Infinity) return {{__number__: '-Infinity'}};
  if (value instanceof Set) return {{__set__: [...value].map(canonical)}};
  if (value instanceof Map) return {{__map__: [...value.entries()].map(([k,v]) => [canonical(k), canonical(v)])}};
  if (value?.isMatrix4) return {{__matrix4__: value.elements.map(Number)}};
  if (value?.isVector3) return {{__vector3__: [Number(value.x),Number(value.y),Number(value.z)]}};
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') {{
    const out = {{}};
    for (const k of Object.keys(value).sort()) out[k] = canonical(value[k]);
    return out;
  }}
  return value;
}}
function snapshot(value) {{ return JSON.stringify(canonical(value)); }}
function deepFreeze(value, seen = new Set()) {{
  if (!value || typeof value !== 'object' || seen.has(value)) return value;
  seen.add(value);
  if (value instanceof Map) {{ for (const [k,v] of value) {{ deepFreeze(k,seen); deepFreeze(v,seen); }} return value; }}
  if (value instanceof Set) {{ for (const v of value) deepFreeze(v,seen); return value; }}
  for (const v of Object.values(value)) deepFreeze(v, seen);
  Object.freeze(value);
  return value;
}}
function callNamed(name, args) {{ return eval(name)(...args); }}
const results = {{pure: {{}}, easy_candidates: {{}}}};
for (const group of ['pure','easy_candidates']) {{
  for (const entry of contract[group]) {{
    const rows = [];
    for (const fixture of entry.fixtures) {{
      state = revive(fixture.state || {{}});
      const args = revive(fixture.args || []);
      const stateBefore = snapshot(state);
      const argsBefore = snapshot(args);
      deepFreeze(state); deepFreeze(args);
      let value;
      if (fixture.invoke) value = eval(fixture.invoke);
      else value = callNamed(entry.name, args);
      const first = canonical(value);
      const stateAfter = snapshot(state);
      const argsAfter = snapshot(args);
      if (stateBefore !== stateAfter) throw new Error(`${{entry.name}} mutated fixture state`);
      if (argsBefore !== argsAfter) throw new Error(`${{entry.name}} mutated fixture arguments`);
      // Determinism/referential behaviour check on the same frozen snapshot.
      let again;
      if (fixture.invoke) again = eval(fixture.invoke);
      else again = callNamed(entry.name, args);
      if (JSON.stringify(first) !== JSON.stringify(canonical(again))) throw new Error(`${{entry.name}} is not deterministic on a fixed input`);
      rows.push(first);
    }}
    results[group][entry.name] = rows;
  }}
}}
process.stdout.write(JSON.stringify(results));
"""


def resolve_node_executable() -> str:
    """Resolve Node reliably from native Windows, MSYS2/MinGW and POSIX Python.

    MSYS2 bash can resolve a Windows Node installation through POSIX-form PATH entries
    even when native MinGW Python cannot hand the bare `node` command to CreateProcess.
    """
    for env_name in ("MODEL_ASSET_EDITOR_NODE", "NODE_EXE"):
        value = os.environ.get(env_name, "").strip()
        if value:
            candidate = Path(value)
            if candidate.is_file():
                return str(candidate)

    for executable in ("node", "node.exe"):
        found = shutil.which(executable)
        if found:
            return found

    if os.name == "nt":
        # Derive the owning MSYS2 root from e.g.
        # C:\\msys64\\mingw64\\bin\\python.exe and ask that exact bash to resolve
        # the command using the same shell semantics as the user's terminal.
        python_path = Path(sys.executable).resolve()
        for root in python_path.parents:
            bash = root / "usr" / "bin" / "bash.exe"
            if not bash.is_file():
                continue
            probe = subprocess.run(
                [
                    str(bash),
                    "-lc",
                    'p="$(command -v node 2>/dev/null)" || exit 1; '
                    '[ -n "$p" ] || exit 1; cygpath -w "$p"',
                ],
                encoding="utf-8",
                errors="strict",
                capture_output=True,
                cwd=ROOT,
            )
            if probe.returncode == 0 and probe.stdout.strip():
                resolved = probe.stdout.strip().splitlines()[-1]
                if Path(resolved).is_file():
                    return resolved

        # Native Windows fallbacks for common installations.
        common: list[Path] = []
        if os.environ.get("ProgramFiles"):
            common.append(Path(os.environ["ProgramFiles"]) / "nodejs" / "node.exe")
        if os.environ.get("ProgramFiles(x86)"):
            common.append(Path(os.environ["ProgramFiles(x86)"]) / "nodejs" / "node.exe")
        if os.environ.get("LOCALAPPDATA"):
            common.append(Path(os.environ["LOCALAPPDATA"]) / "Programs" / "nodejs" / "node.exe")
        for candidate in common:
            if candidate.is_file():
                return str(candidate)

    raise AssertionError(
        "Node.js executable was not found for the function-purity runtime harness. "
        "If `node --version` works in MSYS2 but this test still cannot resolve it, "
        "set MODEL_ASSET_EDITOR_NODE to the full Windows path of node.exe."
    )


def run_dynamic(web: str, contract: dict[str, Any]) -> dict[str, Any]:
    program = build_node_program(web, contract)
    node = resolve_node_executable()
    result = subprocess.run(
        [node, "--input-type=module", "-"],
        input=program,
        encoding="utf-8",
        errors="strict",
        capture_output=True,
        cwd=ROOT,
    )
    if result.returncode != 0:
        raise AssertionError("function purity runtime harness failed:\n" + result.stderr.strip())
    if "\ufffd" in result.stdout or "\ufffd" in result.stderr:
        raise AssertionError(
            "function-purity Node transport produced Unicode replacement characters; "
            "the Python/Node pipe must remain strict UTF-8"
        )
    return json.loads(result.stdout)


def entry_oracle_payload(entry: dict[str, Any]) -> dict[str, Any]:
    # `invoke`, classification, support helpers and constants are adapters around the
    # frozen calculation. They may change while the v0.10.66 state/args/result oracle
    # remains immutable.
    return {
        "name": entry["name"],
        "fixtures": [
            {
                key: fixture[key]
                for key in ("state", "args", "expected")
                if key in fixture
            }
            for fixture in entry["fixtures"]
        ],
    }


def entry_oracle_digest(entry: dict[str, Any]) -> str:
    raw = json.dumps(
        entry_oracle_payload(entry),
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def verify_entry_oracle_digests(contract: dict[str, Any]) -> None:
    for group in ("pure", "easy_candidates"):
        for entry in contract[group]:
            expected = str(entry.get("oracle_sha256", ""))
            if not expected:
                raise AssertionError(
                    f"function-purity oracle hash missing for certified function {entry['name']}; "
                    "existing frozen fixtures may not be silently re-baselined"
                )
            actual = entry_oracle_digest(entry)
            if actual != expected:
                raise AssertionError(
                    f"function-purity oracle changed for {entry['name']}: expected {expected}, got {actual}. "
                    "Signature/invoke adapters may change; frozen v0.10.66 state/args/expected may not."
                )


def baseline_payload(contract: dict[str, Any]) -> dict[str, Any]:
    # Global digest remains as an inventory-level fence; per-function oracle hashes make
    # incremental extension safe because adding a new fixture cannot re-baseline an old one.
    rows = [
        entry_oracle_payload(entry)
        for group in ("pure", "easy_candidates")
        for entry in contract[group]
    ]
    rows.sort(key=lambda row: row["name"])
    return {"functions": rows}


def baseline_digest(contract: dict[str, Any]) -> str:
    raw = json.dumps(
        baseline_payload(contract),
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def verify_baseline_digest(contract: dict[str, Any]) -> None:
    expected = str(contract.get("baseline_sha256", ""))
    actual = baseline_digest(contract)
    if not expected:
        raise AssertionError("FUNCTION_PURITY_CONTRACT baseline_sha256 is missing")
    if actual != expected:
        raise AssertionError(
            "function-purity v0.10.66 input/output baseline changed. "
            f"expected {expected}, got {actual}. "
            "Changing invoke adapters is allowed; changing baseline inputs or expected outputs "
            "requires an explicit reviewed baseline migration."
        )


def record_goldens(contract: dict[str, Any], results: dict[str, Any]) -> None:
    changed = False
    for group in ("pure", "easy_candidates"):
        by_name = results[group]
        for entry in contract[group]:
            existing_hash = str(entry.get("oracle_sha256", ""))
            missing_expected = any("expected" not in fixture for fixture in entry["fixtures"])
            if existing_hash:
                actual_hash = entry_oracle_digest(entry)
                if actual_hash != existing_hash:
                    raise AssertionError(
                        f"refusing to re-record changed oracle for {entry['name']}: "
                        f"expected {existing_hash}, got {actual_hash}"
                    )
            elif not missing_expected:
                raise AssertionError(
                    f"certified function {entry['name']} has complete expected outputs but no oracle_sha256; "
                    "refusing silent baseline adoption"
                )
            rows = by_name[entry["name"]]
            if len(rows) != len(entry["fixtures"]):
                raise AssertionError(f"fixture count mismatch for {entry['name']}")
            for fixture, expected in zip(entry["fixtures"], rows):
                if "expected" in fixture:
                    continue
                fixture["expected"] = expected
                changed = True
            if not existing_hash:
                entry["oracle_sha256"] = entry_oracle_digest(entry)
                changed = True
    if not changed:
        raise AssertionError(
            "all function-purity goldens are already recorded; refusing to overwrite the v0.10.66 oracle"
        )
    contract["baseline_sha256"] = baseline_digest(contract)
    CONTRACT_PATH.write_text(json.dumps(contract, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def assert_goldens(contract: dict[str, Any], results: dict[str, Any]) -> None:
    for group in ("pure", "easy_candidates"):
        for entry in contract[group]:
            actual_rows = results[group][entry["name"]]
            for i, (fixture, actual) in enumerate(zip(entry["fixtures"], actual_rows)):
                if "expected" not in fixture:
                    raise AssertionError(f"{entry['name']} fixture {i} has no frozen v0.10.66 expected result")
                expected = fixture["expected"]
                if actual != expected:
                    raise AssertionError(
                        f"behaviour drift in {entry['name']} fixture {i}:\n"
                        f"  expected (v0.10.66): {json.dumps(expected, ensure_ascii=False, sort_keys=True)}\n"
                        f"  actual:              {json.dumps(actual, ensure_ascii=False, sort_keys=True)}"
                    )


def promotable_candidates(graph: dict[str, dict[str, Any]], contract: dict[str, Any]) -> list[str]:
    # Contracted candidates whose current implementation is now transitively PURE.
    return sorted(
        entry["name"] for entry in contract["easy_candidates"]
        if graph[entry["name"]]["category"] == "PURE"
    )


def analyzer_self_test() -> None:
    synthetic = """<script type="module">
const state={materials:[],meshes:[]};
function regexPure(value){return String(value??'').replace(/[&<>"']/g,ch=>ch);}
function readState(){return state.materials.length;}
function viaRead(){return readState()+1;}
function defaultRead(value=readState()){return value;}
function mapCopy(){const materials=state.materials||[],out=materials.map(m=>({v:m.v}));out.push({v:0});return out;}
function mutateStateAlias(){for(const mesh of state.meshes){mesh.visible=false;}return 1;}
function viaMutation(){return mutateStateAlias();}
function mutateArgument(lod){const targets=lod.items||[],target=targets[0];if(target)target.value=1;return lod;}
</script>"""
    extract_function(synthetic, "regexPure")
    graph = analyze_function_graph(synthetic)
    expected = {
        "regexPure": "PURE",
        "readState": "EASY_CANDIDATE",
        "viaRead": "TRANSITIVE_CANDIDATE",
        "defaultRead": "TRANSITIVE_CANDIDATE",
        "mapCopy": "EASY_CANDIDATE",
        "mutateStateAlias": "STATE_MUTATING",
        "viaMutation": "STATE_MUTATING",
        "mutateArgument": "ARG_MUTATING",
    }
    actual = {name: graph[name]["category"] for name in expected}
    if actual != expected:
        raise AssertionError(f"function-purity analyzer self-test failed: expected={expected}, actual={actual}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--record-goldens", action="store_true", help="fill missing baseline outputs only")
    parser.add_argument("--report", action="store_true", help="print call-graph purity census and low-risk migration queues")
    parser.add_argument("--report-all", action="store_true", help="also print deferred/hard categories")
    args = parser.parse_args()

    analyzer_self_test()
    contract = json.loads(CONTRACT_PATH.read_text(encoding="utf-8"))
    if contract.get("schema") != 2:
        raise AssertionError("unsupported FUNCTION_PURITY_CONTRACT schema")
    if not args.record_goldens:
        verify_baseline_digest(contract)
        verify_entry_oracle_digests(contract)
    web = (ROOT / contract["source"]).read_text(encoding="utf-8")

    pure_names = {entry["name"] for entry in contract["pure"]}
    candidate_names = {entry["name"] for entry in contract["easy_candidates"]}
    overlap = pure_names & candidate_names
    if overlap:
        raise AssertionError(f"functions cannot be both pure and candidates: {sorted(overlap)}")

    for entry in contract["pure"]:
        static_check_pure(entry["name"], extract_function(web, entry["name"]))
    for entry in contract["easy_candidates"]:
        static_check_candidate(entry["name"], extract_function(web, entry["name"]), entry.get("allowed_global_reads", []))

    graph = analyze_function_graph(web)
    static_baseline = json.loads(STATIC_BASELINE_PATH.read_text(encoding="utf-8"))
    if static_baseline.get("source") != contract.get("source"):
        raise AssertionError("function-purity static/dynamic contracts point at different sources")
    validate_static_baseline(graph, static_baseline)
    validate_contracted_graph(graph, contract)

    results = run_dynamic(web, contract)
    if args.record_goldens:
        record_goldens(contract, results)
        print("[RECORDED] Model Asset Editor v0.10.66 function-purity behaviour goldens")
        return
    assert_goldens(contract, results)

    total_named = len(graph)
    certified = len(pure_names) + len(candidate_names)
    print(
        f"[PASS] Model Asset Editor function purity / v0.10.66 behavioural equivalence: "
        f"{len(pure_names)} certified pure + {len(candidate_names)} certified purity candidates"
    )

    if args.report or args.report_all:
        groups = graph_report(graph)
        contracted = pure_names | candidate_names
        pure_all = set(groups.get("PURE", []))
        easy_all = set(groups.get("EASY_CANDIDATE", []))
        transitive_all = set(groups.get("TRANSITIVE_CANDIDATE", []))
        low_risk = pure_all | easy_all | transitive_all
        unc_pure = sorted(pure_all - contracted)
        unc_easy = sorted(easy_all - contracted)
        unc_transitive = sorted(transitive_all - contracted)
        deferred = total_named - len(low_risk)
        print(
            f"[CENSUS] named functions={total_named}; dynamically certified={certified}; "
            f"static PURE={len(pure_all)}; EASY_CANDIDATE={len(easy_all)}; "
            f"TRANSITIVE_CANDIDATE={len(transitive_all)}; deferred/hard={deferred}"
        )
        print(
            f"[QUEUE] uncontracted low-risk={len(unc_pure)+len(unc_easy)+len(unc_transitive)}: "
            f"already-pure={len(unc_pure)}, direct hidden-read candidates={len(unc_easy)}, "
            f"transitive hidden-read candidates={len(unc_transitive)}"
        )
        print(
            "[INFO] PURE/EASY/TRANSITIVE are statically protected within the named-function call-graph scope; "
            "unknown/external behaviour is deferred. Only dynamically certified functions additionally have "
            "frozen v0.10.66 input/output fixtures."
        )
        promotable = promotable_candidates(graph, contract)
        print(f"[INFO] contracted candidates ready for pure promotion ({len(promotable)}):")
        for name in promotable:
            print(f"  + {name}")
        print(f"[QUEUE:PURE] uncontracted functions already statically pure ({len(unc_pure)}):")
        for name in unc_pure:
            print(f"  = {name}")
        print(f"[QUEUE:EASY] direct hidden-read candidates ({len(unc_easy)}):")
        for name in unc_easy:
            reads = ",".join(sorted(graph[name]["hidden_reads"])) or "-"
            print(f"  - {name}  [reads: {reads}]")
        print(f"[QUEUE:TRANSITIVE] call-chain candidates ({len(unc_transitive)}):")
        for name in unc_transitive:
            reads = ",".join(sorted(graph[name]["transitive_hidden_reads"])) or "-"
            print(f"  ~ {name}  [reads via graph: {reads}]")

        if args.report_all:
            low_categories = {"PURE", "EASY_CANDIDATE", "TRANSITIVE_CANDIDATE"}
            for category, names in groups.items():
                if category in low_categories:
                    continue
                print(f"[DEFER:{category}] ({len(names)}):")
                for name in names:
                    row = graph[name]
                    effects = ",".join(sorted(row["effects"])) or "-"
                    unknown = ",".join(sorted(row["unknown_calls"])) or "-"
                    print(f"  ! {name}  [effects: {effects}; unknown: {unknown}]")


if __name__ == "__main__":
    main()
