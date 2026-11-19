"""Construction plans of the hub cities in space (author 2026-11-19: строящиеся хабы из строительных модулей разной
готовности - центр готов, периферия только собирается; план расширения готовых хабов и его реализация во времени;
остановки / заморозки строек; author's correction: краны только там, где есть гравитация - на орбитальных и
пространственных хабах тягачи, корабли с клешнями, кондукторы).

  python tools/models/hub_expansion.py            every hub with a frozen city layout (hub_cities/<hub>.json)
  python tools/models/hub_expansion.py <hub> ...

Input: the hub's world site row (src/assets/data/galaxy_details/world_sites/sites_3126.json: `construction.project`
for a hub still being built, `expansion` for an operating one - tools/colonization_lab/hub_construction.py) and its
frozen city layout. Output: src/assets/data/navigation/hub_expansion/<hub>.json (a hub without projects gets no file):

schema elite.hub_expansion.v1
  epoch_universe_s, day_s         day = floor((universe seconds - epoch) / day_s): the plans' day 0 is 3126-01-01
  gravity                         false for every city here (orbital): no cranes, tugs / claw ships instead
  kits: {kit: [stage folders]}    hubc_<kit>_s1..s4 (mast / minehead: s1..s3), tools/models/units/hub_construction.py
  projects: [{id, kind, title, cause, start_day, work_days, halts: [[from, to | null, cause]],
              replace: [[layout node index, kit, a, b]]           the build of the hub itself: a layout node shows
                                                                  its kit's stage by u = (p - a) / (b - a)
              add: [[node id, final folder, kit, x, y, z, rx, ry, rz, a, b]]   a new building of an expansion
              fixed: [[node id, folder, x, y, z, rx, ry, rz]]     the expansion's deck plate / spoke girders: present
                                                                  from its start on, kept when done
              props: [[node id, folder, x, y, z, rx, ry, rz]]     jigs, temporary platforms, barracks, stacks: present
                                                                  while the project is under way, halted or abandoned
              platforms: [[x, y, z, yaw deg]]                     the temporary decks (the traffic's pick-up points)
              front: [[x, y, z]]                                  build-front points (the claw ships' targets)}]
Stage of a node (src/model_asset/HubExpansion.cpp does the same): u < 0.25 -> s1, < 0.5 -> s2, < 0.75 -> s3,
< 1 -> s4, else the final folder (a 3-stage kit: thirds). A replaced node shows s1 before its window opens; an added
node is absent before its project starts.
"""
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SITES = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json")
CITIES = os.path.join(ROOT, "src", "assets", "data", "navigation", "hub_cities")
OUT_DIR = os.path.join(ROOT, "src", "assets", "data", "navigation", "hub_expansion")
SCHEMA = "elite.hub_expansion.v1"
EPOCH_UNIVERSE_S = 853005600.0          # 3126-01-01 in universe seconds (galaxy/jump_approach_shells.json)
KITS = {k: [f"hubc_{k}_s{i}" for i in range(1, 5)] for k in ("tower", "hab", "slab", "hall", "dock", "yard")}
KITS.update({k: [f"hubc_{k}_s{i}" for i in range(1, 4)] for k in ("mast", "minehead")})
# kit footprints (X, Z) and the default finished folder when the city has none of the kind
KIT_SIZE = {"tower": (100, 100), "hab": (40, 30), "slab": (220, 70), "hall": (240, 240), "dock": (130, 300),
            "yard": (240, 190), "mast": (30, 30), "minehead": (60, 60)}
KIT_DEFAULT = {"tower": "hubb_hab_tower", "hab": "hubk_hab_a", "slab": "hubb_hab_slab", "hall": "hubb_warehouse",
               "dock": "hubb_pier_m", "yard": "hubp_container_yard", "mast": "hubv_mast", "minehead": "hubb_works"}
# the city's infrastructure (the girder lattice, streets, tubes...) is the frame of the city: built first, kept as is
INFRA = ("hubb_girder", "hubb_road", "hubb_tube", "hubb_monorail", "hubp_crossing", "hubb_junction", "hubsp_causeway",
         "hubv_canopy", "hubl_bridge", "hubt_")
DOCKS = ("hubb_pier", "hubb_fuel_dock", "hubb_repair_dock", "hubs_police_dock", "hubs_rescue_dock", "hubsp_capital_berth")
FLAT = ("hubp_container_yard", "hubb_parking", "hubv_pad_star", "hubx_american_houses")
DECK_Y = 0.0
GIRDER_Y = -27.0


def h01(*parts):
    d = hashlib.md5("|".join(str(p) for p in parts).encode("utf-8")).hexdigest()
    return int(d[:12], 16) / float(16 ** 12)


def kit_of(folder, lo, hi):
    """the construction kit a finished module is built with (None: infrastructure, kept)"""
    if folder.startswith(INFRA):
        return None
    if folder.startswith(DOCKS):
        return "dock"
    h = hi[1] - lo[1]
    w, d = hi[0] - lo[0], hi[2] - lo[2]
    if folder.startswith(FLAT) or h < 50:
        return "yard"
    if folder == "hubv_mast":
        return "mast"
    if h >= 200:
        return "tower"
    if max(w, d) <= 70:
        return "hab"
    if max(w, d) >= 2.0 * min(w, d):
        return "slab"
    return "hall"


def r1(v):
    return round(v, 1) + 0.0


def node_row(nid, folder, p, rot):
    return [nid, folder, r1(p[0]), r1(p[1]), r1(p[2]), r1(rot[0]), r1(rot[1]), r1(rot[2])]


def city_frame(layout):
    """the city's building extent (x, z half sizes of the non-infrastructure nodes on the top plane) and the module kits"""
    mods = layout["modules"]
    xs, zs = [], []
    kit_folders = {}
    for n in layout["nodes"]:
        m = mods[n[1]]
        k = kit_of(m["folder"], m["lo"], m["hi"])
        if k is None or abs(n[4] - DECK_Y) > 30.0 or abs(n[6]) > 90.0:
            continue
        xs.append(abs(n[3]))
        zs.append(abs(n[5]))
        kit_folders.setdefault(k, {}).setdefault(m["folder"], 0)
        kit_folders[k][m["folder"]] += 1
    ex = max(xs) if xs else 1000.0
    ez = max(zs) if zs else 1000.0
    finals = {k: max(v.items(), key=lambda kv: kv[1])[0] for k, v in kit_folders.items()}
    return ex, ez, finals


def build_project(hub, layout, proj, ex, ez):
    """the hub itself under construction: every building node by its kit, a readiness window growing from the centre
    (the core first: power, docks, the first housing) to the periphery; temporary decks, jigs and the worker camp on
    the city's rim"""
    mods = layout["modules"]
    replace, front = [], []
    for i, n in enumerate(layout["nodes"]):
        m = mods[n[1]]
        k = kit_of(m["folder"], m["lo"], m["hi"])
        if k is None:
            continue
        r = max(abs(n[3]) / max(1.0, ex), abs(n[5]) / max(1.0, ez))
        r = min(1.0, r)
        a = 0.62 * r ** 1.3 + 0.06 * (h01(hub, i, "a") - 0.5)
        if k == "dock":
            a *= 0.5                    # the docks come early: the materials arrive through them
        a = max(0.0, min(0.85, a))
        b = min(1.0, a + 0.22 + 0.1 * h01(hub, i, "b"))
        replace.append([i, k, round(a, 3), round(b, 3)])
        if 0.25 < r < 0.95 and len(front) < 24 and abs(n[4] - DECK_Y) < 30.0 and abs(n[6]) < 90.0 and h01(hub, i, "front") < 0.08:
            front.append([r1(n[3]), r1(n[4] + 40.0), r1(n[5])])
    props, platforms = rim_camp(hub, proj["id"], ex, ez, n_platforms=4 + int(3 * h01(hub, "np")), camps=2)
    out = dict(proj)
    out.update(replace=replace, add=[], fixed=[], props=props, platforms=platforms, front=front)
    return out


def rim_camp(hub, pid, ex, ez, n_platforms, camps):
    """temporary decks on the rim (the cargo haulers' and claw ships' pick-up points), stacks beside them, barrack
    camps and jigs - just outside the building extent, on the top plane"""
    props, platforms = [], []
    sides = [((1, 0), ex, ez), ((-1, 0), ex, ez), ((0, 1), ez, ex), ((0, -1), ez, ex)]
    for k in range(n_platforms + camps):
        (dx, dz), e_out, e_along = sides[k % 4]
        along = (h01(pid, k, "along") * 1.6 - 0.8) * e_along
        out = e_out + 90.0
        x = dx * out + (along if dz else 0.0)
        z = dz * out + (along if dx else 0.0)
        yaw = math.degrees(math.atan2(dx, dz))
        tx, tz = dz, -dx                 # the rim's tangent
        if k < n_platforms:
            props.append(node_row(f"{pid}.platform{k}", "hubc_platform", (x, DECK_Y, z), (0, yaw, 0)))
            platforms.append([r1(x), 15.0, r1(z), r1(yaw)])
            props.append(node_row(f"{pid}.stack{k}", ("hubc_stack_beams", "hubc_stack_panels", "hubc_containers")[k % 3],
                                  (x + tx * 55.0, DECK_Y, z + tz * 55.0), (0, yaw, 0)))
            props.append(node_row(f"{pid}.jig{k}", "hubc_jig", (x - tx * 75.0 - dx * 10.0, DECK_Y, z - tz * 75.0 - dz * 10.0),
                                  (0, yaw, 0)))
        else:
            props.append(node_row(f"{pid}.camp{k}", "hubc_barracks_camp", (x + dx * 30.0, DECK_Y, z + dz * 30.0), (0, yaw, 0)))
    return props, platforms


def expansion_project(hub, proj, slot, ex, ez, finals):
    """one expansion of an operating hub: a new deck plate on the rim joined by girder spokes, the new building on it
    in stages, a temporary deck + jig (+ barracks for the big ones) beside it"""
    kind = proj["kind"]
    kit = kind if kind in KITS else "hall"
    pid = proj["id"]
    side = slot % 4
    (dx, dz) = ((1, 0), (-1, 0), (0, 1), (0, -1))[side]
    e_out, e_along = (ex, ez) if dx else (ez, ex)
    along = ((slot // 4) * 0.37 + h01(pid, "along")) % 1.0 * 1.4 - 0.7
    sx, sz = KIT_SIZE[kit]
    out = e_out + 140.0 + max(sx, sz) * 0.5
    cx = dx * out + (along * e_along if dz else 0.0)
    cz = dz * out + (along * e_along if dx else 0.0)
    yaw = math.degrees(math.atan2(dx, dz))   # +Z of the module points out of the city (a pier sticks out)
    tx, tz = dz, -dx
    fixed = [node_row(f"{pid}.deck", "hubb_parking", (cx, DECK_Y - 2.0, cz), (0, yaw, 0))]
    # the spokes: girders from the city's edge to the plate (100 m each, along +Z of the girder)
    edge = e_out - 20.0
    n_g = max(1, int(math.ceil((out - 150.0 - edge) / 100.0)))
    for g in range(n_g):
        d0 = edge + 100.0 * g
        for off in (-60.0, 60.0):
            gx = dx * d0 + (along * e_along if dz else 0.0) + tx * off
            gz = dz * d0 + (along * e_along if dx else 0.0) + tz * off
            fixed.append(node_row(f"{pid}.spoke{g}{'ab'[off > 0]}", "hubb_girder", (gx, GIRDER_Y, gz), (0, yaw, 0)))
    final = finals.get(kit, KIT_DEFAULT[kit])
    add = [[f"{pid}.bld", final, kit, r1(cx), DECK_Y, r1(cz), 0.0, r1(yaw), 0.0, 0.0, 1.0]]
    px, pz = cx + tx * (sx * 0.5 + 70.0), cz + tz * (sx * 0.5 + 70.0)
    props = [node_row(f"{pid}.platform", "hubc_platform", (px, DECK_Y, pz), (0, yaw, 0)),
             node_row(f"{pid}.stack", ("hubc_stack_beams", "hubc_stack_panels", "hubc_containers")[slot % 3],
                      (px + dx * 45.0, DECK_Y, pz + dz * 45.0), (0, yaw, 0)),
             node_row(f"{pid}.jig", "hubc_jig", (cx - tx * (sx * 0.5 + 50.0), DECK_Y, cz - tz * (sx * 0.5 + 50.0)), (0, yaw, 0))]
    if kit in ("tower", "slab", "hall", "dock", "minehead"):
        props.append(node_row(f"{pid}.barracks", "hubc_barracks", (px + dx * 60.0 + tx * 40.0, DECK_Y, pz + dz * 60.0 + tz * 40.0),
                              (0, yaw + 90.0, 0)))
    out_p = dict(proj)
    out_p.update(replace=[], add=add, fixed=fixed, props=props, platforms=[[r1(px), 15.0, r1(pz), r1(yaw)]],
                 front=[[r1(cx), r1(DECK_Y + 60.0), r1(cz)]])
    return out_p


def plan_doc(hub, site, layout):
    ex, ez, finals = city_frame(layout)
    projects = []
    c = site.get("construction")
    if c and c.get("project"):
        projects.append(build_project(hub, layout, c["project"], ex, ez))
    for slot, p in enumerate(site.get("expansion") or []):
        projects.append(expansion_project(hub, p, slot, ex, ez, finals))
    if not projects:
        return None
    return {"schema": SCHEMA, "hub_id": hub, "epoch_universe_s": EPOCH_UNIVERSE_S, "day_s": 86400.0,
            "gravity": False, "extent_m": [r1(ex), r1(ez)],
            "note": "generated by tools/models/hub_expansion.py from the world site's construction / expansion "
                    "(tools/colonization_lab/hub_construction.py); do not edit by hand",
            "kits": KITS, "projects": projects}


def site_index():
    d = json.load(open(SITES, encoding="utf-8"))
    rows = d["sites"] + d.get("hidden_sites", [])
    by = {}
    for s in rows:
        rb = s.get("runtime_binding") or ""
        if rb.startswith("initial_world_state.orbital_hubs["):
            by[rb[len("initial_world_state.orbital_hubs["):-1]] = s
        by.setdefault(s["id"], s)
    return by


def write(hub, sites):
    path = os.path.join(CITIES, hub + ".json")
    site = sites.get(hub)
    if not site or not os.path.exists(path):
        return None
    doc = plan_doc(hub, site, json.load(open(path, encoding="utf-8")))
    out = os.path.join(OUT_DIR, hub + ".json")
    if doc is None:
        return None
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        f.write("{\n")
        keys = [k for k in doc if k != "projects"]
        for k in keys:
            f.write(f" {json.dumps(k)}: {json.dumps(doc[k], ensure_ascii=False)},\n")
        f.write(' "projects": [\n')
        f.write(",\n".join("  " + json.dumps(p, ensure_ascii=False) for p in doc["projects"]))
        f.write("\n ]\n}\n")
    return doc


def test_build(hub, progress, out_dir, sites):
    """DEV: a test plan for a live check (the game reads it with ELITE_DEV_HUB_EXPANSION_DIR=<out_dir>): the hub's
    city shown as being built - a build project that reaches `progress` today (no halts) - plus its real expansions"""
    import datetime
    today = (datetime.datetime.now(datetime.timezone.utc).date() - datetime.date(2026, 1, 1)).days
    work = 3000
    site = dict(sites.get(hub) or {"id": hub})
    site["construction"] = {"project": {"id": hub + ".testbuild", "kind": "build", "title": "DEV test", "cause": "DEV",
                                        "budget_mcr": 0, "start_day": today - int(progress * work), "work_days": work,
                                        "halts": []}}
    doc = plan_doc(hub, site, json.load(open(os.path.join(CITIES, hub + ".json"), encoding="utf-8")))
    os.makedirs(out_dir, exist_ok=True)
    json.dump(doc, open(os.path.join(out_dir, hub + ".json"), "w", encoding="utf-8"), ensure_ascii=False)
    print(f"test plan {hub} at progress {progress} -> {out_dir}")


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    sites = site_index()
    if "--test-build" in sys.argv:
        i = sys.argv.index("--test-build")
        test_build(sys.argv[i + 1], float(sys.argv[i + 2]), sys.argv[i + 3], sites)
        return
    hubs = [a for a in sys.argv[1:] if not a.startswith("--")]
    if not hubs:
        hubs = sorted(f[:-5] for f in os.listdir(CITIES) if f.endswith(".json"))
    n = nb = 0
    for h in hubs:
        doc = write(h, sites)
        if doc:
            n += 1
            nb += any(p["kind"] == "build" for p in doc["projects"])
    print(f"{n} hub expansion plans ({nb} hubs under construction) -> {os.path.relpath(OUT_DIR, ROOT)}")


if __name__ == "__main__":
    main()
