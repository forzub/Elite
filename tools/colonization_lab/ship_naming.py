#!/usr/bin/env python3
"""Ship model names 3126: the single reader of canon/ship_naming.json.

A ship model's name is a property of its stable id (kb.ship.cobrett-i.frnu.n1 ...); the ids are legacy
technical ids and never change. Every other place that shows a model name is a MIRROR written by --write:

  canon/ship_passports_3126.json  passports[]: name (= display_name_ru), model_index, official_name,
                                  native_script, romanization, short_name, short_name_ru, legacy_name;
                                  model_code = <manufacturer code_prefix><number>, functional group codes
                                  <model_code>A<k>; replacements[].name by ship id
  canon/ship_designs.json         ships[]: name (= display_name_ru), designation (= export_index, ASCII:
                                  the root of the runtime installation codes)

The reference proposals (Notes/html/ship_reference/tools/build_proposals.js) read the same canon file.
After --write run ship_designer.py (runtime catalog, drawings) and lore_ships.py as usual.

usage: ship_naming.py [--write | --check]
"""
import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CANON = os.path.join(HERE, "canon")
NAMING = os.path.join(CANON, "ship_naming.json")
PASSPORTS = os.path.join(CANON, "ship_passports_3126.json")
DESIGNS = os.path.join(CANON, "ship_designs.json")

NAME_FIELDS = ("model_index", "official_name", "native_script", "romanization", "short_name", "short_name_ru", "legacy_name")


def load(path):
    with open(path, "rb") as f:
        raw = f.read()
    return json.loads(raw.decode("utf-8")), raw


def detect_indent(raw):
    """The indent the file was written with (canon files differ: 1 or 2), so a mirror rewrite changes
    only the mirrored values."""
    data = json.loads(raw.decode("utf-8"))
    body = raw.replace(b"\r\n", b"\n").rstrip(b"\n")
    for indent in (2, 1, 4):
        if json.dumps(data, ensure_ascii=False, indent=indent).encode("utf-8") == body:
            return indent
    raise ValueError("cannot reproduce the file's JSON formatting")


def dump(data, raw_before):
    text = json.dumps(data, ensure_ascii=False, indent=detect_indent(raw_before)).encode("utf-8")
    if b"\r\n" in raw_before:
        text = text.replace(b"\n", b"\r\n")
    tail = raw_before[len(raw_before.rstrip(b"\r\n")):]
    return text + tail


def code_number(code):
    m = re.search(r"(\d+)$", str(code))
    if not m:
        raise ValueError(f"model_code {code!r} has no number")
    return int(m.group(1))


def model_code(naming, mfr, number):
    prefix = naming["manufacturers"].get(mfr, {}).get("code_prefix")
    if not prefix:
        raise ValueError(f"no code_prefix for manufacturer {mfr}")
    return f"{prefix}{number}"


def rename_passport(p, naming):
    n = naming["ships"].get(p["id"])
    if n is None:
        raise ValueError(f"passport {p['id']} has no entry in ship_naming.json")
    code = model_code(naming, p["manufacturer"], code_number(p["model_code"]))
    out = {}
    for k, v in p.items():
        if k in NAME_FIELDS or k == "model_code":
            continue
        if k == "name":
            out["name"] = n["display_name_ru"]
            out["model_code"] = code
            for f in NAME_FIELDS:
                if n.get(f):
                    out[f] = n[f]
            continue
        out[k] = v
    for g in out["functional_groups"]:
        g["code"] = code + "A" + g["code"].split("A")[-1]
    for r in (out.get("replacements") or {}).values():
        if isinstance(r, dict) and r.get("ship_id") in naming["ships"]:
            r["name"] = naming["ships"][r["ship_id"]]["display_name_ru"]
    return out


def compute():
    naming, _ = load(NAMING)
    passports, praw = load(PASSPORTS)
    designs, draw = load(DESIGNS)
    passports["passports"] = [rename_passport(p, naming) for p in passports["passports"]]
    for s in designs["ships"]:
        n = naming["ships"].get(s["id"])
        if n is None:
            raise ValueError(f"design {s['id']} has no entry in ship_naming.json")
        s["name"] = n["display_name_ru"]
        s["designation"] = n["export_index"]
    return {PASSPORTS: (dump(passports, praw), praw), DESIGNS: (dump(designs, draw), draw)}, naming, passports


def checks(naming, passports):
    problems = []
    codes = {}
    for p in passports["passports"]:
        if p["model_code"] in codes:
            problems.append(f"model_code {p['model_code']} shared by {codes[p['model_code']]} and {p['id']}")
        codes[p["model_code"]] = p["id"]
    for sid, n in naming["ships"].items():
        for f in ("model_index", "official_name", "display_name_ru", "export_index", "naming", "reason", "legacy_name"):
            if not n.get(f):
                problems.append(f"{sid}: missing {f}")
        if not all(ord(c) < 128 for c in n.get("export_index", "")):
            problems.append(f"{sid}: export_index is not ASCII")
        rom = n.get("romanization", "")
        if re.search(r"[āáǎàēéěèīíǐìōóǒòūúǔùǖǘǚǜ]", rom):
            problems.append(f"{sid}: romanization carries tones / diacritics ({rom}) - export form is toneless")
        if n.get("native_script") and not n.get("romanization"):
            problems.append(f"{sid}: native_script without an export romanization")
    used = {p["manufacturer"] for p in passports["passports"]}
    for m in used:
        if m not in naming["manufacturers"]:
            problems.append(f"manufacturer {m} has no naming doctrine")
    return problems


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--write", action="store_true")
    g.add_argument("--check", action="store_true")
    a = ap.parse_args()
    mirrors, naming, passports = compute()
    problems = checks(naming, passports)
    stale = [p for p, (new, old) in mirrors.items() if new != old]
    if a.write:
        for path, (new, old) in mirrors.items():
            if new != old:
                with open(path, "wb") as f:
                    f.write(new)
                print("wrote", os.path.relpath(path, HERE))
    else:
        for p in stale:
            problems.append(f"mirror out of date: {os.path.relpath(p, HERE)} (run ship_naming.py --write)")
    for p in problems:
        print("PROBLEM", p)
    print(f"ship_naming: {len(naming['ships'])} named models, {len(naming['manufacturers'])} manufacturers, "
          f"{len(passports['passports'])} passports, {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
