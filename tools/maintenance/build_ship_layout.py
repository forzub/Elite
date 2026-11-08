#!/usr/bin/env python3
"""Ship layouts 3126: the LAYOUT tree of every ship (compartments -> units -> parts) with reference
designations in its maker's standard, and the FUNCTIONAL chains built from the fault catalog's buses.

Author 2026-11-09: designations come from the layout, not the function ("корпус 46; два двигательных отсека
46A1, 46A2, кабина-капсула 46A3; 46A1A1 - реактор, 46A1A2 - передний двигатель ... пятак двигателя -
46A1A2PT1"); the unit number is the ship passport's model code; the functional schema joins units by function
("пятак - двигатель - реактор - двигатель - пятак", one card with x2); each engine has its own cooling pad.
The designation standard belongs to the maker (canon ship_naming.json designation_standard): ASME Y14.44,
IEC 81346, GB/T 5094 (China), NORSOK (Skjoldborg), donor (Stone Docks refits).

The layout itself is GENERATED from the ship's equipment by the rules below (PROPOSAL) - "сначала
оборудование, потом корабль"; an authored layout per ship will override it later.

Inputs: Notes/html/ship_reference/data.js (all ships + equipment), src/assets/data/maintenance/fault_catalog.json,
tools/colonization_lab/canon/ship_naming.json. Outputs: src/assets/data/ships/ship_layouts_3126.json,
Notes/html/ship_reference/layout_data.js.

usage: python tools/maintenance/build_ship_layout.py [--check]
"""
import argparse
import json
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA_JS = os.path.join(ROOT, "Notes", "html", "ship_reference", "data.js")
CATALOG = os.path.join(ROOT, "src", "assets", "data", "maintenance", "fault_catalog.json")
NAMING = os.path.join(ROOT, "tools", "colonization_lab", "canon", "ship_naming.json")
OUT = os.path.join(ROOT, "src", "assets", "data", "ships", "ship_layouts_3126.json")
REF = os.path.join(ROOT, "Notes", "html", "ship_reference", "layout_data.js")

# category -> class letter (ASME / NORSOK family) and IEC 81346-2 letter
CAT_CLASS = {
    "drive": ("ME", "M"), "reactor": ("GR", "G"), "service_reactor": ("GS", "G"), "radiator": ("RD", "E"), "coolant_loop": ("PU", "G"),
    "jump_core": ("JC", "G"), "tank": ("TK", "C"), "cryo_tank": ("TK", "C"), "rcs": ("RC", "M"), "computer": ("CP", "K"), "radar": ("RA", "B"),
    "sensor_mast": ("RA", "B"), "ecm": ("CM", "K"), "comm": ("CM", "K"), "transponder": ("TX", "K"), "life_support": ("LS", "H"),
    "medbay": ("A", "A"), "escape_capsule": ("EC", "A"), "lifeboat": ("EC", "A"), "container_rack": ("CR", "U"), "bulk_hold": ("CR", "U"),
    "passenger_module": ("A", "A"), "train_module": ("A", "A"), "docking_port": ("DK", "X"), "landing_gear": ("MP", "U"),
    "heat_shield": ("MP", "F"), "armour": ("MP", "F"), "weapon": ("WE", "A"), "hangar": ("A", "U"), "boat_bay": ("A", "U"),
    "drone_bay": ("A", "U"), "workshop": ("A", "A"), "mining_rig": ("A", "M"), "tow_rig": ("A", "M"),
}
# part letters inside units: ASME / NORSOK letter -> IEC 81346-2 letter
SRU_IEC = {"PT": "E", "HX": "E", "RD": "E", "PU": "G", "VL": "Q", "TK": "C", "IJ": "Q", "L": "R", "MP": "U", "W": "W", "WP": "W", "B": "B",
           "PS": "T", "A": "K", "BT": "C", "FL": "V", "G": "H", "E": "F", "CM": "K", "WE": "A"}
# units with their own place in the tree (A / separate tag); the rest are numbered pieces in their compartment
ASSEMBLY = {"drive", "reactor", "service_reactor", "coolant_loop", "computer", "radar", "sensor_mast", "ecm", "comm", "life_support",
            "escape_capsule", "docking_port", "weapon", "passenger_module", "train_module", "hangar", "boat_bay", "drone_bay", "workshop",
            "medbay", "mining_rig", "tow_rig", "bulk_hold", "cryo_tank", "landing_gear", "heat_shield"}
# compartments in layout order (propulsion bays first, one per reactor-drive group)
COMPARTMENTS = [
    ("cockpit", "кабина-капсула", {"escape_capsule", "life_support"}),
    ("systems", "отсек бортовых систем", {"service_reactor", "computer", "radar", "comm", "transponder", "ecm", "sensor_mast"}),
    ("thermal", "контур охлаждения и радиаторы", {"coolant_loop", "radiator"}),
    ("jump", "прыжковый отсек", {"jump_core", "tank:jump_charge"}),
    ("fuel", "топливный отсек", {"tank:fusion_pellets", "tank:argon", "tank:other"}),
    ("rcs", "маневровые блоки", {"rcs"}),
    ("payload", "грузовой / пассажирский отсек", {"container_rack", "bulk_hold", "passenger_module", "train_module", "cryo_tank", "hangar",
                                                  "boat_bay", "drone_bay", "workshop", "mining_rig", "tow_rig", "lifeboat", "medbay"}),
    ("weapons", "вооружение", {"weapon"}),
    ("docking", "стыковка и посадка", {"docking_port", "landing_gear", "heat_shield"}),
    ("hull", "корпус и броня", {"armour"}),
]


def load_data():
    with open(DATA_JS, encoding="utf-8") as f:
        txt = f.read()
    return json.loads(txt[txt.index("=") + 1:].rstrip().rstrip(";"))


def key_of(part):
    if part["cat"] == "tank":
        fl = part.get("fluid")
        return "tank:" + (fl if fl in ("jump_charge", "fusion_pellets", "argon") else "other")
    return part["cat"]


class Formatter:
    """designations of one ship in its maker's standard"""

    def __init__(self, std, unit):
        self.std, self.u = std, unit

    def letter(self, cls_asme, cls_iec):
        return cls_iec if self.std in ("iec_81346", "gb_t_5094") else cls_asme

    def compartment(self, k):
        if self.std in ("iec_81346", "gb_t_5094"):
            return f"={self.u}+{k}"
        if self.std == "norsok":
            return f"{self.u}-{k:02d}"
        return f"{self.u}A{k}"

    def assembly(self, k, a, cls, seq, inseparable=False):
        if self.std in ("iec_81346", "gb_t_5094"):
            return f"={self.u}+{k}-{cls}{seq}"
        if self.std == "norsok":
            return f"{self.u}-{k:02d}-{cls}-{seq:03d}"
        return f"{self.u}A{k}{'U' if inseparable else 'A'}{a}"

    def piece(self, k, cls, n):
        if self.std in ("iec_81346", "gb_t_5094"):
            return f"={self.u}+{k}-{cls}{n}"
        if self.std == "norsok":
            return f"{self.u}-{k:02d}-{cls}-{n:03d}"
        return f"{self.u}A{k}{cls}{n}"

    def sru(self, parent, cls, n):
        if self.std in ("iec_81346", "gb_t_5094"):
            return f"{parent}-{cls}{n}"
        if self.std == "norsok":
            return f"{parent}/{cls}{n}"
        return f"{parent}{cls}{n}"


def split_even(n, k):
    base, extra = divmod(n, k)
    return [base + (1 if i < extra else 0) for i in range(k)]


def build_ship(s, parts, cls_of, doctrines, ships_by_id):
    mdoc = doctrines.get(s["manufacturer"], {})
    std = mdoc.get("designation_standard", "asme_y14_44")
    if std == "donor":
        donor = (s.get("reference") or {}).get("variant_of")
        dm = ships_by_id.get(donor, {}).get("manufacturer") if donor else None
        std = doctrines.get(dm, {}).get("designation_standard", "iec_81346") if dm else "iec_81346"
        if std == "donor":
            std = "iec_81346"
    f = Formatter(std, s["model_code"])
    units = []   # (key, part, qty)
    for b in s["bom"]:
        p = parts[b["part_id"]]
        units.append((key_of(p), p, b["qty"]))
    reactors = [(p, q) for k, p, q in units if k == "reactor"]
    drives = [(p, q) for k, p, q in units if k == "drive"]
    n_r, n_d = sum(q for _, q in reactors), sum(q for _, q in drives)
    bays = max(1, min(n_r, n_d)) if (n_r or n_d) else 0
    comps = []
    k = 0

    def expand(lst):
        out = []
        for p, q in lst:
            out += [p] * q
        return out

    def unit_node(kk, p, seq_by_cls, a_index):
        ca, ci = CAT_CLASS.get(p["cat"], ("A", "A"))
        cls = f.letter(ca, ci)
        seq_by_cls[cls] = seq_by_cls.get(cls, 0) + 1
        fc = cls_of.get(p["id"])
        inseparable = p["cat"] == "jump_core"
        des = f.assembly(kk, a_index, cls, seq_by_cls[cls], inseparable)
        srus = []
        if fc:
            per = {}
            for sru, sc in fc.get("sru_class", {}).items():
                letter = f.letter(sc, SRU_IEC.get(sc, sc))
                per[letter] = per.get(letter, 0) + 1
                srus.append(dict(des=f.sru(des, letter, per[letter]), sru=sru, ru=fc["srus"][sru], cls=letter))
        return dict(des=des, kind="unit", part=p["id"], cat=p["cat"], cls=cls, inseparable=inseparable, fault_class=fc["id"] if fc else None, srus=srus)

    # propulsion bays: reactors and drives shared out evenly; each drive carries its own cooling pad (PT)
    if bays:
        rs, ds = expand(reactors), expand(drives)
        rsplit, dsplit = split_even(len(rs), bays), split_even(len(ds), bays)
        ri = di = 0
        for b in range(bays):
            k += 1
            seq, kids, a = {}, [], 0
            for p in rs[ri:ri + rsplit[b]]:
                a += 1
                kids.append(unit_node(k, p, seq, a))
            for p in ds[di:di + dsplit[b]]:
                a += 1
                kids.append(unit_node(k, p, seq, a))
            ri += rsplit[b]
            di += dsplit[b]
            comps.append(dict(des=f.compartment(k), key="propulsion", ru=f"двигательный отсек {b + 1}", children=kids))
    # the other compartments
    for key, ru, members in COMPARTMENTS:
        mine = [(kk, p, q) for kk, p, q in units if kk in members]
        if not mine:
            continue
        k += 1
        seq, kids, a, pieces = {}, [], 0, {}
        for kk, p, q in mine:
            if p["cat"] in ASSEMBLY or p["cat"] == "jump_core":
                for _ in range(q):
                    a += 1
                    kids.append(unit_node(k, p, seq, a))
            else:
                ca, ci = CAT_CLASS.get(p["cat"], ("MP", "U"))
                cls = f.letter(ca, ci)
                first = seq.get(cls, 0) + 1          # one counter per class in the compartment (units and pieces)
                seq[cls] = first + q - 1
                fc = cls_of.get(p["id"])
                kids.append(dict(des=f.piece(k, cls, first) if q == 1 else f.piece(k, cls, first) + " … " + f.piece(k, cls, first + q - 1),
                                 kind="pieces", part=p["id"], cat=p["cat"], cls=cls, qty=q, fault_class=fc["id"] if fc else None))
        comps.append(dict(des=f.compartment(k), key=key, ru=ru, children=kids))
    return dict(id=s["id"], name=s["name"], unit=s["model_code"], standard=std, compartments=comps)


def chains(layout, catalog):
    """functional chains: propulsion bays (identical ones merged, xN) and the bus chains"""
    out = []
    sig = {}
    for c in layout["compartments"]:
        if c["key"] != "propulsion":
            continue
        rs = [u for u in c["children"] if u["cat"] == "reactor"]
        ds = [u for u in c["children"] if u["cat"] == "drive"]
        # pad - drive - reactor - drive - pad: drives split to both sides of the reactors
        left, right = ds[:(len(ds) + 1) // 2], ds[(len(ds) + 1) // 2:]
        seq = []
        for d in left:
            seq += [dict(role="pad", ru="пятак двигателя", des=next((s["des"] for s in d["srus"] if s["sru"] == "pad"), None)),
                    dict(role="drive", part=d["part"], des=d["des"])]
        for r in rs:
            seq.append(dict(role="reactor", part=r["part"], des=r["des"],
                            pad=next((s["des"] for s in r["srus"] if s["sru"] == "pad"), None)))
        for d in right:
            seq += [dict(role="drive", part=d["part"], des=d["des"]),
                    dict(role="pad", ru="пятак двигателя", des=next((s["des"] for s in d["srus"] if s["sru"] == "pad"), None))]
        key = tuple((x["role"], x.get("part")) for x in seq)
        if key in sig:
            sig[key]["instances"].append([x for x in seq])
            sig[key]["compartments"].append(c["des"])
        else:
            sig[key] = dict(id="propulsion", ru="двигательная группа", pattern=[dict(role=x["role"], part=x.get("part")) for x in seq],
                            instances=[[x for x in seq]], compartments=[c["des"]])
    out += list(sig.values())
    units = [u for c in layout["compartments"] for u in c["children"]]
    by_cat = {}
    for u in units:
        by_cat.setdefault(u["cat"], []).append(u)
    for bname, bus in catalog["buses"].items():
        src = []
        for s in bus["source"]:
            if s.startswith("tank:"):
                fluid = s[5:]
                src += [u for u in by_cat.get("tank", []) if fluid in u["part"] or (fluid == "fusion_pellets" and "pellets" in u["part"])
                        or (fluid == "jump_charge" and "jumpcharge" in u["part"]) or (fluid == "argon" and "argon" in u["part"])]
            else:
                src += by_cat.get(s, [])
        sinks = [u for cat in bus["sinks"] for u in by_cat.get(cat, [])]
        if src and sinks:
            out.append(dict(id=bname, ru=bus["ru"], sources=[dict(des=u["des"], part=u["part"], qty=u.get("qty", 1)) for u in src],
                            sinks=[dict(des=u["des"], part=u["part"], qty=u.get("qty", 1)) for u in sinks]))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    D = load_data()
    with open(CATALOG, encoding="utf-8") as fh:
        catalog = json.load(fh)
    with open(NAMING, encoding="utf-8") as fh:
        naming = json.load(fh)
    parts = {p["id"]: p for p in D["parts"]}
    cls_of = {pid: c for c in catalog["classes"] for pid in c["applies_to"]}
    ships_by_id = {s["id"]: s for s in D["ships"]}
    layouts = []
    problems = []
    for s in D["ships"]:
        L = build_ship(s, parts, cls_of, naming["manufacturers"], ships_by_id)
        L["chains"] = chains(L, catalog)
        layouts.append(L)
        seen = set()
        for c in L["compartments"]:
            for d in [c["des"]] + [u["des"] for u in c["children"]] + [x["des"] for u in c["children"] for x in u.get("srus", [])]:
                if d in seen:
                    problems.append(f"{s['id']}: duplicate designation {d}")
                seen.add(d)
        n_bom = sum(b["qty"] for b in s["bom"])
        n_lay = sum(u.get("qty", 1) for c in L["compartments"] for u in c["children"])
        if n_bom != n_lay:
            problems.append(f"{s['id']}: {n_bom} units in the BOM, {n_lay} placed")
    doc = dict(schema="elite.ship_layouts.v1", status="PROPOSAL (generated)",
               note="Компоновка и позиционные обозначения кораблей, сгенерированные из оборудования (tools/maintenance/build_ship_layout.py); руками не править. Стандарт обозначений - у производителя (ship_naming.json designation_standard).",
               standards=naming.get("designation_standards", {}), ships=layouts)
    text = json.dumps(doc, ensure_ascii=False, indent=1) + "\n"
    ref = "window.ELITE_LAYOUT_DATA=" + json.dumps(doc, ensure_ascii=False, separators=(",", ":")) + ";\n"
    if a.check:
        for path, new in ((OUT, text), (REF, ref)):
            if not os.path.exists(path) or open(path, encoding="utf-8").read() != new:
                problems.append(f"out of date: {os.path.relpath(path, ROOT)}")
    else:
        for path, new in ((OUT, text), (REF, ref)):
            with open(path, "w", encoding="utf-8", newline="\n") as fh:
                fh.write(new)
    for p in problems:
        print("PROBLEM", p)
    stds = {}
    for L in layouts:
        stds[L["standard"]] = stds.get(L["standard"], 0) + 1
    print(f"ship layouts: {len(layouts)} ships, standards {stds}; {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
