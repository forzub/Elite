"""The hub PLAN for the game's Hub map (author 2026-11-18: «в 3Д нам достаточно будет просто коробок и отметок для
дока... что касается самого хаба - это будет в 2Д карте. там можно выбирать кварталы - в игре появляется метка на HUD»).

plan_document(hub_id, layout) -> elite.hub_plan.v1, in the city frame (module <hub>.city, metres, +X / +Z the plane,
+Y up): the blocks (zone, centre, half size, the top / under building heights - the 3D map's boxes), every module's
footprint per layer (the 2D plan), the streets, the docks (the dockable modules: module id + its docking anchor), the
spinning bodies. The lanes and red zones stay in hub_lanes/<hub>.json (HubLaneNetwork reads them already).
Written by hub_belt_world.py to src/assets/data/navigation/hub_plans/<hub>.json.
"""
import math


def _rot(R, v):
    return tuple(sum(R[i][k] * v[k] for k in range(3)) for i in range(3))


def plan_document(hub_id, L):
    import pads
    from units.hub_belt import _module_obb
    from units.hub_plane import CELL, ZONE_TITLE
    obb = _module_obb()
    blocks = {}
    for d in L.get("districts", []):
        i, j = d["cell"]
        blocks[d["id"]] = {"id": f"b{i}_{j}", "i": i, "j": j, "zone": d["kind"], "title": ZONE_TITLE.get(d["kind"], d["kind"]),
                           "centre": [round(d["centre"][0], 1), round(d["centre"][2], 1)], "half_m": CELL / 2,
                           "top_h": 0.0, "under_h": 0.0, "modules": 0}
    modules = []
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        if role not in ("module", "dock") or folder not in obb:
            continue
        lo, hi, _ = obb[folder]
        R = pads.euler(rot)
        cs = [_rot(R, (x, y, z)) for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])]
        xs = [c[0] + pos[0] for c in cs]
        ys = [c[1] + pos[1] for c in cs]
        zs = [c[2] + pos[2] for c in cs]
        under = pos[1] < -1.0
        g = L["module_of"].get(nid)
        b = blocks.get(g)
        modules.append({"block": b["id"] if b else None, "layer": "under" if under else "top", "module": folder,
                        "role": role, "rect": [round(min(xs), 1), round(min(zs), 1), round(max(xs), 1), round(max(zs), 1)],
                        "y": [round(min(ys), 1), round(max(ys), 1)]})
        if b and role == "module":
            b["modules"] += 1
            if under:
                b["under_h"] = max(b["under_h"], round(-min(ys), 1))
            else:
                b["top_h"] = max(b["top_h"], round(max(ys), 1))
    # the streets: the road bridges between two blocks (the nodes of role "road" at street level, merged per street)
    streets = {}
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        if role != "road" or abs(pos[1]) > 1.0:
            continue
        g = L["module_of"].get(nid)
        s = streets.setdefault(g, {"xs": [], "zs": [], "monorail": False, "avenue": False})
        s["xs"].append(pos[0])
        s["zs"].append(pos[2])
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        g = L["module_of"].get(nid)
        if g in streets and role == "monorail":
            streets[g]["monorail"] = True
        if g in streets and role == "tube":
            streets[g]["avenue"] = True
    street_list = []
    for g, s in sorted(streets.items()):
        along_x = (max(s["xs"]) - min(s["xs"])) > (max(s["zs"]) - min(s["zs"]))
        if along_x:
            z = sum(s["zs"]) / len(s["zs"])
            a, b = [min(s["xs"]), z], [max(s["xs"]) + 100.0, z]
        else:
            x = sum(s["xs"]) / len(s["xs"])
            a, b = [x, min(s["zs"])], [x, max(s["zs"]) + 100.0]
        street_list.append({"id": g, "a": [round(v, 1) for v in a], "b": [round(v, 1) for v in b],
                            "avenue": s["avenue"], "monorail": s["monorail"]})
    docks = []
    for dk in L.get("docks", []):
        if dk["type"] != "hub_hex_dock":
            continue
        p = dk["position"]
        R = pads.euler(dk["rotation_deg"])
        o = _rot(R, (0.0, 0.0, -1.0))                              # the slot's outward direction
        docks.append({"module_id": f"{hub_id}.{dk['id']}", "anchor_id": "dock_gate_front", "type": dk["type"],
                      "title": dk["name"], "x": round(p[0], 1), "y": round(p[1], 1), "z": round(p[2], 1),
                      "out": [round(o[0], 3), round(o[2], 3)]})
    spinning = [{"id": f"{hub_id}.{s['id']}", "module": s["module"], "x": round(s["position"][0], 1), "z": round(s["position"][2], 1),
                 "r": 160.0 if s["module"] == "hubb_hab_drum" else 95.0} for s in L.get("spinning", [])]
    # the road bridges / monorail as the generator builds them (hub_plane.py streets): the traffic of the client
    # (src/scene/HubCityTraffic) drives on them - the deck top over the plane (y 0) and under it (UNDER_Y, upside
    # down), two lanes each way at 3 / 9 m off the centre line (right-hand traffic), the monorail beam's rail top on
    # the avenues of the passenger zones 18 m to the street's +X (Z streets) / -Z (X streets) side
    from units.hub_plane import UNDER_Y
    roads = {"top_y": 0.0, "under_y": round(UNDER_Y, 3), "lanes_m": [3.0, 9.0], "monorail_y": 16.8, "monorail_side_m": 18.0}
    xs = [b["centre"][0] for b in blocks.values()]
    zs = [b["centre"][1] for b in blocks.values()]
    h = CELL / 2
    return {"schema": "elite.hub_plan.v1", "hub_id": hub_id, "frame_module_id": f"{hub_id}.city",
            "description": "generated by tools/models/hub_belt_world.py (tools/models/hub_plan.py) from the hub map",
            "cell_m": CELL, "bounds": [round(min(xs) - h, 1), round(min(zs) - h, 1), round(max(xs) + h, 1), round(max(zs) + h, 1)],
            "blocks": sorted(blocks.values(), key=lambda b: (b["i"], b["j"])), "modules": modules, "streets": street_list,
            "docks": docks, "spinning": spinning, "roads": roads}


def population_estimate(plan):
    """a rough count of the residents from the habitable footprints (see the 2026-11-18 estimate: ~40 m2 per person)"""
    flats = {"hubb_hab_tower", "hubb_hab_slab", "hubk_hab_b"}
    area = 0.0
    for m in plan["modules"]:
        base = m["module"].split("_dmg_")[0]
        if base in flats:
            x0, z0, x1, z1 = m["rect"]
            area += (x1 - x0) * (z1 - z0) * math.floor((m["y"][1] - m["y"][0]) / 4.0) * 0.8
    return int(area / 40.0)
