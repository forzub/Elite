"""Checks of the hub map (the hub editor's document) against the generator, no game needed:
python tools/models/hub_map_check.py
 1. generated map -> layout == the generator's layout (identity of the map step; access bridges by count)
 2. edits: a block added outside the extent (the others stay put: the anchor is frozen), a lane switched off (it and its
    connectors gone), a red zone drawn, a card moved / rotated (the node follows), a dock moved to another rim block
"""
import copy
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import units.hub_plane as hp  # noqa: E402


def node_index(L):
    return {n[0]: n for n in L["nodes"]}


def main():
    fails = []

    def check(ok, what):
        print(("PASS " if ok else "FAIL ") + what)
        if not ok:
            fails.append(what)

    m = hp._generate_map("plane_demo")
    L0 = hp.layout_from_map(m)
    m["grid"]["anchor"] = L0["anchor"]
    L1 = hp.layout_from_map(m)
    check([n for n in L0["nodes"]] == [n for n in L1["nodes"]], "frozen anchor: the same city")
    # 1. a block outside the extent
    i0, j0, i1, j1 = m["grid"]["extent"]
    m2 = copy.deepcopy(m)
    m2["cells"].append({"i": i1 + 1, "j": j0, "zone": "storage", "top": [{"module": "hubb_warehouse", "x": 0.0, "z": 0.0, "yaw": 90}], "under": []})
    m2["grid"]["extent"] = [i0, j0, i1 + 1, j1]
    L2 = hp.layout_from_map(m2)
    a, b = node_index(L1), node_index(L2)
    same = [k for k in a if k in b and a[k][1] == b[k][1] and a[k][2] == b[k][2]]
    moved_mod = [k for k in a if k in b and a[k][4] == "module" and a[k][2] != b[k][2]]
    check(not moved_mod, f"added block {i1 + 1},{j0}: no existing module moved ({len(same)} nodes identical)")
    new = [n for n in L2["nodes"] if n[0].startswith(f"c{i1 + 1}_{j0}_storage_m")]
    cx = L2["centres"]["x"][i1 + 1]
    check(len(new) == 1 and abs(new[0][2][0] - cx) < 1e-6, f"the new block's card at its centre x={cx}")
    # 2. a lane off
    lane = next(ln for ln in L1["lanes"] if ln["id"].startswith("green.") and not ln.get("plaza"))
    m3 = copy.deepcopy(m)
    m3["lanes_off"] = [lane["id"]]
    L3 = hp.layout_from_map(m3)
    check(all(ln["id"] != lane["id"] for ln in L3["lanes"]), f"lane {lane['id']} off: gone")
    check(all(cn["from_lane"] != lane["id"] and cn["to_lane"] != lane["id"] for cn in L3["connectors"]),
          f"its connectors gone ({len(L1['connectors'])} -> {len(L3['connectors'])})")
    # 3. a red zone drawn
    m4 = copy.deepcopy(m)
    m4["red_zones"] = [{"id": "r1", "x": 100.0, "z": -200.0, "w": 400.0, "d": 300.0, "h": 600.0}]
    L4 = hp.layout_from_map(m4)
    rz = [r for r in L4["red_zones"] if r["id"] == "red.map.r1"]
    check(len(rz) == 1 and tuple(rz[0]["half_extents"]) == (200.0, 300.0, 150.0), "red zone r1 in the layout")
    # 4. a card moved and rotated
    m5 = copy.deepcopy(m)
    c = next(c for c in m5["cells"] if c["top"] and c["zone"] not in hp.SPECIAL_ZONES)
    c["top"][0]["x"] += 50.0
    c["top"][0]["yaw"] = (c["top"][0]["yaw"] + 90) % 360
    L5 = hp.layout_from_map(m5)
    nid = f"c{c['i']}_{c['j']}_{c['zone']}_m0"
    a, b = node_index(L1)[nid], node_index(L5)[nid]
    check(abs(b[2][0] - a[2][0] - 50.0) < 1e-6 and b[3] != a[3], f"card {nid} moved 50 m and turned")
    # 5. the dock to another rim block
    m6 = copy.deepcopy(m)
    dock = next(c for c in m6["cells"] if c["zone"] == "dock")
    cells = {(q["i"], q["j"]) for q in m6["cells"]}
    other = next(q for q in m6["cells"] if q is not dock and q["zone"] in ("industry", "storage") and hp._open_sides((q["i"], q["j"]), cells))
    dock["zone"], dock["top"] = "storage", []
    del dock["dock_side"]
    other["zone"], other["top"], other["under"] = "dock", [], []
    other["dock_side"] = list(hp._open_sides((other["i"], other["j"]), cells)[0])
    other.pop("piers", None)
    L6 = hp.layout_from_map(m6)
    d0 = next(d for d in L1["docks"] if d["type"] == "hub_hex_dock")
    d1 = next(d for d in L6["docks"] if d["type"] == "hub_hex_dock")
    blue = [ln for ln in L6["lanes"] if ln.get("kind") == "blue"]
    check(d0["position"] != d1["position"] and blue, f"dock moved to {other['i']},{other['j']}: {len(blue)} blue approaches")
    print("ALL PASS" if not fails else f"{len(fails)} FAILED")
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
