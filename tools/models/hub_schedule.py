"""The hub's cargo turnover and its daily SHIP SCHEDULE (author 2026-11-18: «достаточно для начала прописать товарооборот и
сделать тупо расписание, которое будет лежать и на сервере и на клиенте. исходя из этого становится понятно какой
корабль когда прибывает и сколько реально нужно доков. примерно, точные расчеты не нужны. все доки сделать доками - но на
клиенте, серверу про них сейчас знать не обязательно. что серверу надо знать - игрок нападает на корабль. тот становится
настоящим и сервер начинает его симуляцию»).

schedule_document(hub_id, layout, plan) -> elite.hub_schedule.v1 (assets/data/navigation/hub_schedules/<hub>.json), in the
city frame (module <hub>.city), a 24 h cycle repeating every day:
  docks     the client-side berths: cargo berths along the rim piers (two per pier), fuel berths, frontier pads (the
            parking decks, 8 pads each; the star pads) - position, the approach axis (out of the city / up), max hull
  turnover  t/day by flow (approximate): consumer goods ~8 kg / resident / day, industry 1500 t/day per block, stores /
            logistics / port 2500 t/day transshipment per block, refuel 2000 t/day per block; 40 % of the consumer
            goods + 800 t/day per store block go out by the frontiers (the local carriers) through the pads
  visits    the ships (the hub plan's air fleet: the bloc's hulls) that carry it: kind, hull, dock, arrival second of the
            day, dwell (handling ~600 t/h on a berth, ~200 t/h on a pad, + a fixed half / quarter hour), the wait at the
            holding area when no dock was free
  demand    per dock kind: the docks available, the peak number the schedule needs (no waiting), the ships that waited
"""
import math
import random

APPROACH_S = 150.0          # the client flies the final approach / the departure over this time (see HubTrafficSchedule)
DEPART_S = 150.0
DAY = 86400.0

PIERS = {"hubb_pier_l": (300.0, 64.0, 260.0, "cargo"), "hubb_pier_m": (160.0, 44.0, 140.0, "cargo"),
         "hubb_fuel_dock": (160.0, 44.0, 140.0, "fuel")}
PIER_W = 40.0


def _rot(R, v):
    return tuple(sum(R[i][k] * v[k] for k in range(3)) for i in range(3))


def _add(a, b, s=1.0):
    return tuple(x + y * s for x, y in zip(a, b))


def _docks(L):
    import pads
    from units.hub_belt import PK_X, PK_Z
    docks = []
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        R = pads.euler(rot)
        up = _rot(R, (0.0, 1.0, 0.0))
        if role == "dock" and folder in PIERS:
            length, bw, max_len, kind = PIERS[folder]
            out = _rot(R, (0.0, 0.0, 1.0))
            side = _rot(R, (1.0, 0.0, 0.0))
            for sgn in (-1, 1):
                c = _add(_add(pos, out, length / 2), side, sgn * (PIER_W / 2 + bw / 2))
                c = _add(c, up, 8.0)                      # the hull's centre a little over the deck
                docks.append({"id": f"{nid}.{'l' if sgn < 0 else 'r'}", "kind": kind, "pos": c, "axis": out, "up": up,
                              "max_len": max_len, "vertical": False})
        elif role == "module" and folder == "hubb_parking":
            for ix in range(PK_X):
                for iz in range(PK_Z):
                    x = (ix - (PK_X - 1) / 2) * 100.0 + (20.0 if ix else -20.0)
                    z = (iz - (PK_Z - 1) / 2) * 70.0
                    c = _add(_add(pos, _rot(R, (x, 0.0, z))), up, 6.0)
                    docks.append({"id": f"{nid}.p{ix}{iz}", "kind": "pad", "pos": c, "axis": up, "up": up, "max_len": 60.0,
                                  "vertical": True, "heading": _rot(R, (0.0, 0.0, 1.0))})
        elif role == "module" and folder in ("hubs_police_dock", "hubs_rescue_dock"):
            # the services' free pads (tools/models/units/hub_services.py: the police deck's middle pad at 16 m, the rescue
            # station's second pad; the others hold parked craft)
            local = (74.0, 16.0, 0.0) if folder == "hubs_police_dock" else (62.0, 0.0, 46.0)
            c = _add(_add(pos, _rot(R, local)), up, 5.0)
            docks.append({"id": f"{nid}.{'police' if folder == 'hubs_police_dock' else 'rescue'}",
                          "kind": "police" if folder == "hubs_police_dock" else "rescue", "pos": c, "axis": up, "up": up,
                          "max_len": 50.0, "vertical": True, "heading": _rot(R, (0.0, 0.0, 1.0))})
        elif role == "module" and folder == "hubv_pad_star":
            c = _add(pos, up, 6.0)
            docks.append({"id": f"{nid}.star", "kind": "pad", "pos": c, "axis": up, "up": up, "max_len": 70.0, "vertical": True,
                          "heading": _rot(R, (0.0, 0.0, 1.0))})
    return docks


def _catalog():
    import json
    import os
    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
    d = json.load(open(os.path.join(root, "src", "assets", "data", "ships", "ship_catalog_3126.json"), encoding="utf-8"))
    items = d["ships"] if isinstance(d, dict) and "ships" in d else list(d.values())[0]
    return {s["id"]: s for s in items}


def schedule_document(hub_id, L, plan):
    rng = random.Random(f"schedule:{hub_id}")
    docks = _docks(L)
    cat = _catalog()
    fleet = (plan.get("air_fleet") or {}).get("ships", [])
    hulls = []
    for i, s in enumerate(fleet):
        c = cat.get(s.get("passport", ""), {})
        cargo = float(c.get("cargo_t") or 0.0)
        # the service hulls (police, defence, recon, fighters) fly the lanes but carry no trade
        if cargo <= 0.0 or c.get("cls") in ("patrol_cutter", "impact_defence_tug", "interceptor", "recon_picket"):
            continue
        hulls.append({"fleet": i, "model": s["model"], "name": s["name"], "length": s["length_m"], "cargo": cargo,
                      "weight": s["weight"]})
    frontier_hulls = [h for h in hulls if 100.0 <= h["cargo"] <= 300.0]
    bulk_hulls = [h for h in hulls if h["cargo"] > 300.0]
    if not frontier_hulls:
        frontier_hulls = sorted(hulls, key=lambda h: h["cargo"])[:1]
    if not bulk_hulls:
        bulk_hulls = sorted(hulls, key=lambda h: -h["cargo"])[:1]
    zones = {}
    for b in plan["blocks"]:
        zones[b["zone"]] = zones.get(b["zone"], 0) + 1
    pop = plan.get("population_estimate", 0)
    consumer = pop * 0.008
    turnover = {
        "consumer_goods_t": round(consumer),
        "industry_t": 1500 * zones.get("industry", 0),
        "transshipment_t": 2500 * (zones.get("storage", 0) + zones.get("logistics", 0) + zones.get("port", 0)),
        "fuel_t": 2000 * zones.get("refuel", 0),
    }
    flows = {"bulk": consumer * 0.6 + turnover["industry_t"] + turnover["transshipment_t"],
             "frontier": consumer * 0.4 + 800 * (zones.get("storage", 0) + zones.get("logistics", 0)),
             "fuel": turnover["fuel_t"]}
    visits = []
    for flow, tonnage in flows.items():
        pool = frontier_hulls if flow == "frontier" else bulk_hulls
        if not pool or tonnage <= 0.0:
            continue
        carried = 0.0
        while carried < tonnage:
            h = rng.choices(pool, weights=[p["weight"] for p in pool])[0]
            load = h["cargo"] * rng.uniform(0.6, 0.95)
            carried += load
            rate, fixed = (200.0, 900.0) if flow == "frontier" else (600.0, 1800.0) if flow == "bulk" else (800.0, 1800.0)
            visits.append({"kind": flow, "fleet": h["fleet"], "model": h["model"], "name": h["name"], "length": h["length"],
                           "cargo_t": round(load), "dwell_s": round(fixed + load / rate * 3600.0),
                           "arrive_s": round(rng.uniform(0.0, DAY))})
    # the services: a patrol cutter on every police pad each 90 min (25 min down), a rescue tender on every rescue pad
    # each 4 h (40 min) - the hulls of the bloc's fleet by class (any small hull when the bloc has none)
    service_hull = {}
    for i, s in enumerate(fleet):
        cls = cat.get(s.get("passport", ""), {}).get("cls")
        if cls in ("patrol_cutter", "service_tender") and cls not in service_hull:
            service_hull[cls] = {"fleet": i, "model": s["model"], "name": s["name"], "length": s["length_m"]}
    small = min(hulls, key=lambda h: h["length"]) if hulls else None
    for d in [d for d in docks if d["kind"] in ("police", "rescue")]:
        h = service_hull.get("patrol_cutter" if d["kind"] == "police" else "service_tender") or small
        if not h or h["length"] > d["max_len"]:
            continue
        period, dwell = (5400.0, 1500.0) if d["kind"] == "police" else (14400.0, 2400.0)
        t0 = rng.uniform(0.0, period)
        while t0 < DAY:
            visits.append({"kind": "patrol" if d["kind"] == "police" else "rescue", "fleet": h["fleet"], "model": h["model"],
                           "name": h["name"], "length": h["length"], "cargo_t": 0, "dwell_s": round(dwell),
                           "arrive_s": round(t0), "dock_fixed": d["id"]})
            t0 += period
    # the docks: each visit takes a free dock of its kind (fuel falls back on a cargo berth); none free - it waits at the
    # holding area for the first one
    want = {"bulk": ("cargo",), "fuel": ("fuel", "cargo"), "frontier": ("pad",), "patrol": ("police",), "rescue": ("rescue",)}
    free_at = {d["id"]: 0.0 for d in docks}
    by_id = {d["id"]: d for d in docks}
    visits.sort(key=lambda v: v["arrive_s"])
    waited = {}
    for n, v in enumerate(visits):
        kinds = want[v["kind"]]
        cands = [d for d in docks if d["kind"] in kinds and d["max_len"] >= v["length"]]
        if v.get("dock_fixed"):
            cands = [by_id[v["dock_fixed"]]]
        if not cands:
            v["dock"] = None
            continue
        start = v["arrive_s"] - APPROACH_S
        free = [d for d in cands if free_at[d["id"]] <= start]
        if free:
            d = rng.choice(free)
            wait = 0.0
        else:
            d = min(cands, key=lambda d: free_at[d["id"]])
            wait = free_at[d["id"]] - start
            waited[v["kind"]] = waited.get(v["kind"], 0) + 1
        v["wait_s"] = round(wait)
        v["arrive_s"] = round(v["arrive_s"] + wait)
        v["dock"] = d["id"]
        free_at[d["id"]] = v["arrive_s"] + v["dwell_s"] + DEPART_S + 60.0
        v["id"] = f"{hub_id}.v{n:04d}"
    # the demand: the peak concurrency if every ship got a dock at once (no waiting)
    demand = {}
    for kind in ("cargo", "fuel", "pad", "police", "rescue"):
        events = []
        for v in visits:
            if v.get("dock") and by_id[v["dock"]]["kind"] == kind:
                a = v["arrive_s"] - v.get("wait_s", 0) - APPROACH_S
                events += [(a, 1), (a + v["dwell_s"] + APPROACH_S + DEPART_S, -1)]
        cur = peak = 0
        for _, e in sorted(events):
            cur += e
            peak = max(peak, cur)
        demand[kind] = {"available": sum(1 for d in docks if d["kind"] == kind), "peak_needed": peak,
                        "ships_waited": waited.get({"cargo": "bulk", "fuel": "fuel", "pad": "frontier", "police": "patrol",
                                                    "rescue": "rescue"}[kind], 0)}
    r = lambda p: [round(x, 2) for x in p]
    return {"schema": "elite.hub_schedule.v1", "hub_id": hub_id, "frame_module_id": f"{hub_id}.city",
            "description": "generated by tools/models/hub_schedule.py (hub_belt_world.py): a 24 h cycle, every day the same",
            "cycle_s": DAY, "approach_s": APPROACH_S, "depart_s": DEPART_S, "population": pop, "turnover_t_per_day": turnover,
            "demand": demand,
            "docks": [{"id": d["id"], "kind": d["kind"], "pos": r(d["pos"]), "axis": r(d["axis"]), "up": r(d["up"]),
                       "heading": r(d.get("heading", d["axis"])), "max_len": d["max_len"], "vertical": d["vertical"]} for d in docks],
            "visits": [{"id": v["id"], "kind": v["kind"], "model": v["model"], "name": v["name"], "length_m": v["length"],
                        "cargo_t": v["cargo_t"], "dock": v["dock"], "arrive_s": v["arrive_s"], "dwell_s": v["dwell_s"],
                        "wait_s": v.get("wait_s", 0)} for v in visits if v.get("dock")]}
