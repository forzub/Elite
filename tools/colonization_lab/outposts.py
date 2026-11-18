"""Outposts 3126: where the missing kinds of places stand (offline, deterministic, stdlib only).

Called by world_sites.py after the sites, comms and piracy layers exist. Inputs: canon/outposts.json (rules and the
authored points, PROPOSAL), the beacon model out/world_3126/beacons_3126.json (world_economy.py), the route graph,
piracy_by_system, hazard zones, the physical catalog and src/assets/data/galaxy/jump_approach_shells.json.

Layers:
  beacons   - every beacon the model already counts (bridge / relay / military) gets a place on the map; station,
              trade-hub and remote-science beacons are parts of hubs and are only counted; interstellar route relays
              are placed by the medium-mapping rule (canon/outposts.json beacons.route_relays);
  labs      - authored places with a stated reason (the World's Ass, the Lotus rim, Vega, the Frost stream) plus
              untouched belts by rule; selective, rejected candidates are listed with the reason;
  military  - beacon-guard posts, contact-line pickets, authored pickets;
  refuel    - jump-point stations for transit, shell-mouth staging posts, the interstellar range check;
  pirates   - second lairs where the piracy index is high;
  smugglers - hubs where a strict goods regime meets corruption and traffic, in the hardest-to-reach place.

Every manned object carries staff, legal status, operator and supply (t/day, goods, source hub). One id = one object.
"""
import hashlib
import json
import math
import os

import logistics as lg

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SHELLS = os.path.join(ROOT, "src", "assets", "data", "galaxy", "jump_approach_shells.json")
TAG = "PROPOSAL"
AU_KM = 1.495978707e8
LY_KM = 9.4607e12
SECONDS_Y = 365.25 * 86400.0
COVER = ("kuiper_belt", "asteroid_belt", "trojans", "debris_disk")
GOODS_RU = {"food": "продовольствие", "water_volatiles": "вода и газы", "consumer": "бытовые расходники", "medical": "медицина",
            "machinery": "запчасти и механика", "electronics": "электроника", "jump_charge": "заряд прыжка", "science_kit": "научные расходники",
            "weapons": "боеприпасы", "reactor_fuel": "реакторное топливо"}
FORM_RU = {"orbital": "орбитальная станция", "moon_surface": "на поверхности луны", "small_asteroid": "на малом астероиде",
           "free_floating": "свободно плавающая", "lagrange": "в точке Лагранжа"}

# "из чего собирать": existing models (tools/models/units) per kind; gaps need a new model
ASSEMBLY = {
    "beacon_bridge": dict(models=["space_stations.stn_beacon_core", "space_stations.stn_beacon_hab_ring"],
                          note="мост-маяк = корпус маяка + жилое кольцо (штат 40); как есть", gap=None),
    "beacon_relay": dict(models=["space_stations.stn_beacon_core (без жилого кольца)"],
                         note="ретранслятор без штата: только хребет, антенны, реактор, док обслуживания", gap="облегчённый вариант stn_beacon_core без кольца и стоянки истребителей (LOD-вариант, не новая модель)"),
    "beacon_military": dict(models=["space_stations.stn_beacon_core", "hub_services.hubs_police_dock (площадки катеров)"],
                            note="военный маяк при базе или посту", gap="военная окраска/маркировка — материал, не геометрия"),
    "lab_orbital": dict(models=["space_stations.stn_lab_torus_a + stn_lab_torus_a_ring", "space_stations.stn_lab_torus_b"],
                        note="A — жилой тор с гравитацией (штат > 30), Б — микрогравитация (физика, поле)", gap=None),
    "lab_free_floating": dict(models=["space_stations.stn_lab_torus_b", "space_stations.stn_lab_torus_c (телескоп)"],
                              note="Б — у поля и потока; В — обсерватория", gap=None),
    "lab_small_asteroid": dict(models=["space_stations.stn_lab_surface (грунт = астероид)", "фермы"],
                               note="Г на опорах, вкопанная в малый астероид; астероид — из генератора тел", gap="крепление к неровному астероиду (якоря/тросы) — новый узел"),
    "lab_moon_surface": dict(models=["space_stations.stn_lab_surface"], note="как есть", gap=None),
    "military_outpost": dict(models=["hub_services.hubs_police_dock (3 площадки PC-30)", "hub_style_west.hubx_practical_landmark (бастион с радаром)",
                                     "hub_style_west.hubx_practical_block", "фермы"],
                             note="пост = полицейский док + бастион на ферменной платформе; катера — PC-30 «Фарос» каталога", gap="орбитальная платформа-основание (палуба на фермах без города) — новая простая модель"),
    "refuel_station": dict(models=["space_stations.stn_beacon_core (док + реактор)", "hub_services.hubs_rescue_dock (баки горючего)", "фермы",
                                   "корпус танкера Хэ-T12G «Хэсе» / «Камень» как хранилище"],
                           note="заправка = ферма с баками + док; сферические баки заряда", gap="модуль «ферма баков заряда» (сферы на решётке) — новая модель"),
    "pirate_settlement": dict(models=["hub_style_west.hubx_belt_clans_* (цистерны, корпуса на корме, скала с обломком)",
                                      "корпуса каталога как жильё: «Камень», HT-20K, Хэ-120"],
                              note="кланы пояса подходят прямо: сварные цистерны и корпуса", gap=None),
    "smuggler_hub": dict(models=["hub_style_west.hubx_belt_clans_block / _office", "корпус Cobrett-I или C-120 «Вахак» как склад", "фермы"],
                         note="маленький, спрятанный: 2-4 цистерны в скале или на луне, маскировка", gap="маскировочная «скорлупа» под астероид — новая модель"),
}


def load():
    with open(os.path.join(HERE, "canon", "outposts.json"), encoding="utf-8") as f:
        return json.load(f)


def r3(x):
    return round(x, 3)


def h01(*parts):
    d = hashlib.md5("|".join(str(p) for p in parts).encode("utf-8")).hexdigest()
    return int(d[:12], 16) / float(16 ** 12)


def vsub(a, b):
    return [a[i] - b[i] for i in range(3)]


def vadd(a, b):
    return [a[i] + b[i] for i in range(3)]


def vmul(a, s):
    return [x * s for x in a]


def vlen(a):
    return math.sqrt(sum(x * x for x in a))


def sysname(I, s):
    return I.E[s]["name"] if s in I.E else str(s)


class Ctx:
    def __init__(self, I, out, g, base_site, surface_latlon):
        self.I, self.out, self.g = I, out, g
        self.base_site, self.surface_latlon = base_site, surface_latlon
        self.C = load()
        self.S = out["sites"]["sites"]
        self.by_sys = {}
        for s in self.S:
            self.by_sys.setdefault(s["system"], []).append(s)
        self.beacons = I.X["beacons"]
        self.bridge_of = {b["system"]: b for b in self.beacons["bridge"]}
        self.infra = I.X["infrastructure"]["polities"]
        self.pir = {p["system"]: p for p in out["frontier"]["piracy_by_system"]}
        self.dso = {d["id"]: d for d in out["frontier"]["deep_space_objects"]}
        self.planets = {}
        for bid, b in I.bodies.items():
            if b["kind"] == "planet":
                s = int(bid.split(".")[0][7:])
                self.planets.setdefault(s, []).append(bid)
        for s in self.planets:
            self.planets[s].sort(key=lambda b: (-I.bodies[b]["physical"]["mass_kg"], b))

    def star_of(self, bid):
        b = self.I.bodies.get(bid)
        while b and b.get("parent_id") in self.I.bodies:
            b = self.I.bodies[b["parent_id"]]
        return b["parent_id"] if b else None

    def lagrange(self, planet, point, hill):
        return dict(lagrange_point=dict(primary=self.star_of(planet), secondary=planet, point=point, hill_ratio=hill))

    def owner(self, s):
        row = next((b for b in self.beacons["by_system"] if b["system"] == s), None)
        if row and row.get("owner"):
            return row["owner"]
        b = self.bridge_of.get(s)
        return b["operator"] if b else None

    def fuel_market(self, s):
        for x in self.by_sys.get(s, []):
            if x["type"] in ("belt_hub", "military_hub", "deep_space_refuel_point"):
                return True
            if x["type"] in ("trade_hub", "orbital_hub") and x.get("level") == "A":
                return True
        return False

    def market_hub(self, s, polity=None):
        """The hub that sells to a remote station: the operator's own hub first, then the busiest open market."""
        rank = {"trade_hub": 0, "orbital_hub": 1, "belt_hub": 2, "free_port": 3, "deep_space_refuel_point": 4, "military_hub": 5}
        rows = [x for x in self.by_sys.get(s, []) if x["type"] in rank and x.get("legal") != "illegal"]
        if not rows:
            return None
        big = lambda x: -(self.I.pol[x["polity"]]["population"] if x["polity"] in self.I.pol else 0)
        rows.sort(key=lambda x: (0 if polity and x["polity"] == polity else 1, rank[x["type"]], big(x), -x["population"], x["id"]))
        return rows[0]

    def supply_dsos(self):
        """Deep-space hubs that keep stocks and sell them (not derelicts, not hidden caches, not military)."""
        return [d for d in self.I.WS["deep_space_objects"] if d.get("stocks") and not d.get("hidden") and d["id"] in self.dso
                and d.get("site_type") not in ("derelict_hub", "military_hub")]

    def nearest_market(self, s, polity=None):
        """Same system, else the nearest system (straight ly, <= max hop) with a market hub, else the nearest
        stocked deep-space hub (frontier systems beyond the hop limit are fed through their relay / cache)."""
        h = self.market_hub(s, polity)
        if h:
            return h, 0.0
        best = None
        for t in sorted(k for k in self.by_sys if k is not None):
            if t is None or t == s or t not in self.I.pos or s not in self.I.pos:
                continue
            d = vlen(vsub(self.I.pos[t], self.I.pos[s]))
            hh = self.market_hub(t, polity)
            if hh and d <= self.I.hop and (best is None or d < best[1] - 1e-9):
                best = (hh, d)
        if best:
            return best
        for d in self.supply_dsos():
            dd = vlen(vsub(self.dso[d["id"]]["position_ly"], self.I.pos[s]))
            if dd <= self.I.hop and (best is None or dd < best[1] - 1e-9):
                best = (dict(id=d["id"], name=d["name"], system=None), dd)
        return best if best else (None, None)


# ---------------------------------------------------------------- supply
def supply(X, staff, on_ice, source, extra=None, ly=0.0, bulk=None):
    """Life support + extras (t/day). `bulk` = {good: t/day} carried by a jump tanker, not by the supply Cobrett."""
    L = X.C["life_support"]
    kg = dict(L["kg_per_person_day"])
    if on_ice:
        kg["water_volatiles"] = 0.0
    t = {k: staff * v / 1000.0 for k, v in kg.items() if v > 0}
    for k, v in (extra or {}).items():
        t[k] = t.get(k, 0.0) + v
    small = sum(t.values())
    for k, v in (bulk or {}).items():
        t[k] = t.get(k, 0.0) + v
    total = sum(t.values())
    sname = L["supply_ship"]
    ship = X.I.SE["ship_classes"][sname]
    interval = min(L["max_interval_days"], ship["cargo_t"] / small) if small > 0 else L["max_interval_days"]
    rf_kg_y = staff * L["power_kw_per_person"] * 1e3 * SECONDS_Y / (3.5e14 * X.I.PE["fusion_electric_efficiency"])
    src = dict(id=source["id"], name=source["name"], system=source.get("system")) if source else None
    out = dict(t_day=r3(total), goods={k: r3(v) for k, v in sorted(t.items(), key=lambda kv: -kv[1])},
               goods_ru=[GOODS_RU.get(k, k) for k, _ in sorted(t.items(), key=lambda kv: -kv[1])],
               water_local=bool(on_ice), reactor_fuel_kg_y=round(rf_kg_y, 2), source=src, source_ly=r3(ly) if ly else 0.0,
               delivery=f"{sname}: рейс раз в {round(interval)} сут ({round(small * interval, 1)} т)")
    if bulk:
        b = sum(bulk.values())
        # the smallest carrier that brings at least a month of charge per trip (a tanker for trunk transit, a freighter or a Cobrett for a trickle)
        fleet = sorted(("cobrett_i_n1", "medium_freighter", "jump_tanker"), key=lambda c: X.I.SE["ship_classes"][c]["cargo_t"])
        c = next((c for c in fleet if X.I.SE["ship_classes"][c]["cargo_t"] >= 30.0 * b), fleet[-1])
        cap = X.I.SE["ship_classes"][c]["cargo_t"]
        days = min(L["max_interval_days"], cap / b)
        out["bulk_delivery"] = f"{c}: {round(b * days)} т заряда раз в {round(days)} сут"
    return out


# ---------------------------------------------------------------- beacons
def beacon_layer(X, access):
    I, C = X.I, X.C["beacons"]
    rows = []
    # bridge: one per networked system, natural orbit at L4 of the largest planet (beacons_3126.bridge.motion)
    for b in X.beacons["bridge"]:
        s = b["system"]
        pl = X.planets.get(s, [])
        if s == 0:
            pl = ["system_0.Sol.Юпитер"] + [p for p in pl if p != "system_0.Sol.Юпитер"]
        place = X.lagrange(pl[0], "L4", 0.01) if pl else None
        rows.append(dict(id=b["id"], name=f"Мост-маяк {b['id']}", kind="bridge", system_id=s, region="bridge", anchor=pl[0] if pl else None,
                         operator=b["operator"], polity=b["operator"], staff=b["staff"], since=b["since"],
                         reason=f"мост-маяк системы {sysname(I, s)}: точка выхода и старта прыжков, эфемериды, время, почта, транспондеры ({b['transponder_tracks']} треков)",
                         placement=place))
    # in-system relays: 2 per trunk edge (>= 2e7 t/y) per polity of the system (world_economy.beacons_model)
    model = {r["system"]: r for r in X.beacons["by_system"]}
    for k in sorted(I.pol):
        s = I.system_of(k)
        if s == 0 and C["sol_relays_from_sol_network"]:
            continue
        trunks = sorted(t for t, e in X.g.get(s, {}).items() if isinstance(t, int) and e["tonnes"] >= 2.0e7)
        if not trunks:
            continue
        pl = X.planets.get(s, [])
        cyc = pl[1:] or pl
        for i, t in enumerate(trunks):
            planet = cyc[i % len(cyc)]
            for j, (pt, role) in enumerate((("L4", "приём"), ("L5", "выход"))):
                rid = f"BCN-{s:03d}-R-{k}-{t:03d}{'ab'[j]}"
                rows.append(dict(id=rid, name=f"Ретранслятор R-{s:03d}-{t:03d}{'ab'[j]}", kind="relay", system_id=s, region="trunk", anchor=planet,
                                 operator=k, polity=k, staff=0,
                                 reason=f"магистраль {sysname(I, s)} ↔ {sysname(I, t)} ({round(X.g[s][t]['tonnes'] / 1e6, 1)} млн т/год): {role} — очередь, эфемериды коридора, повтор сигнала моста",
                                 placement=X.lagrange(planet, pt, 0.02 + 0.005 * i)))
    # military: one per military base (infrastructure.military_base), placed at the polity's military hubs
    for k in sorted(I.pol):
        n = X.infra[k]["assets"].get("military_base", {}).get("count", 0)
        if n <= 0:
            continue
        s = I.system_of(k)
        hubs = sorted((x for x in X.by_sys.get(s, []) if x["type"] == "military_hub" and x["polity"] == k), key=lambda x: x["id"])
        for i in range(n):
            rid = f"BCN-{s:03d}-M-{k}-{i + 1:02d}"
            if hubs:
                h = hubs[i % len(hubs)]
                j = i // len(hubs)
                if "orbit" in h["placement"]:
                    place = dict(parent_body_id=h["body"], orbit=dict(altitude_km=round(h["placement"]["orbit"]["altitude_km"] * (1.25 + 0.12 * j)),
                                                                      inclination_deg=round(5.0 + 7.0 * j, 1)))
                elif "belt" in h["placement"]:
                    bp = h["placement"]["belt"]
                    place = dict(parent_body_id=h["body"], belt=dict(radius_au=bp["radius_au"], longitude_deg=round((bp["longitude_deg"] + 1.5 * (j + 1)) % 360, 2)))
                else:
                    place = dict(h["placement"], parent_body_id=h["body"])
                anchor, why = h["id"], f"военный маяк базы {h['name']} ({i + 1} из {n}): закрытый канал флота, наведение, опознавание"
            else:
                pl = X.planets.get(s, [])
                place, anchor, why = X.lagrange(pl[0], "L5", 0.02), None, f"военный маяк флота {I.pol[k]['name']} у точки прыжка"
            rows.append(dict(id=rid, name=f"Военный маяк M-{s:03d}-{i + 1:02d}" + (f" ({I.pol[k]['name']})" if sum(1 for q in I.pol if I.system_of(q) == s) > 1 else ""),
                             kind="military", system_id=s, region="military", anchor=anchor, operator=k, polity=k, staff=0, reason=why, placement=place))
    for r in rows:
        if r["placement"] and "parent_body_id" not in r["placement"]:
            lp = r["placement"].get("lagrange_point")
            r["placement"]["parent_body_id"] = lp["secondary"] if lp else None
    # interstellar route relays: medium mapping radius rule
    R = C["route_relays"]
    span = 2.0 * R["medium_mapping_radius_ly"]
    edges = {}
    for a in sorted(k for k in X.g if isinstance(k, int)):
        for b, e in sorted(X.g[a].items(), key=lambda kv: str(kv[0])):
            if not isinstance(b, int) or a >= b:
                continue
            if e["klass"] in R["edge_classes"] or e["tonnes"] >= R["feeder_min_tonnes_y"]:
                edges[(a, b)] = dict(ly=e["ly"], why=f"{e['klass']}, {round(e['tonnes'] / 1e6, 2)} млн т/год")
    for fid in R["frontier_chains"]:
        ch = [n[1] for n in access[fid] if n[0] == "system"]
        full = [n for n in access[fid]]
        for x, y in zip(full, full[1:]):
            if x[0] == "system" and y[0] == "system":
                a, b = sorted((x[1], y[1]))
                if (a, b) not in edges:
                    edges[(a, b)] = dict(ly=r3(vlen(vsub(I.pos[a], I.pos[b]))), why=f"звено цепочки {fid}")
    route = []
    skipped = []
    for (a, b), e in sorted(edges.items()):
        n = max(0, math.ceil(e["ly"] / span) - 1)
        if n == 0:
            skipped.append(dict(edge=[a, b], ly=e["ly"], why=f"короче {span} св. лет: карты мостов на концах покрывают ребро"))
            continue
        pa, pb = I.pos[a], I.pos[b]
        popa = sum(v["population"] for q, v in I.pol.items() if I.system_of(q) == a)
        popb = sum(v["population"] for q, v in I.pol.items() if I.system_of(q) == b)
        guard_sys = a if popa >= popb else b
        guard = (X.bridge_of.get(guard_sys) or X.bridge_of.get(a) or X.bridge_of.get(b) or {}).get("operator")
        for i in range(1, n + 1):
            f = i / float(n + 1)
            p = vadd(pa, vmul(vsub(pb, pa), f))
            route.append(dict(id=f"relay.ism.{a:03d}_{b:03d}.{i}", name=f"Трасса {a:03d}–{b:03d}/{i}", kind="route_relay", system_id=None,
                              region="interstellar", anchor=None, operator=R["operator"], polity=guard, staff=R["staff"], uptime=R["uptime"],
                              reason=f"середина ребра {sysname(I, a)} – {sysname(I, b)} ({e['ly']} св. лет, {e['why']}): карта среды (течения, отстойники) и почтовая станция; страж — {guard}",
                              placement=dict(deep_space=dict(frame="ICRS_heliocentric", position_ly=[r3(x) for x in p], edge=[a, b], fraction=r3(f)))))
    counts = {k: sum(1 for r in rows if r["kind"] == k) for k in ("bridge", "relay", "military")}
    model_tot = X.beacons["totals"]
    sol_rel = sum(1 for r in X.out["sol_network"]["relays"])
    check = dict(bridge=[counts["bridge"], model_tot["bridge"]], relay=[counts["relay"] + model.get(0, {}).get("relay", 0), model_tot["relay"]],
                 military=[counts["military"], model_tot["military"]])
    return dict(in_system=rows, route=route, route_skipped=skipped, counts=dict(counts, route_relay=len(route), sol_network_relays=sol_rel),
                count_only={k: model_tot.get(k, 0) for k in C["count_only_kinds"]}, count_only_note=C["count_only_kinds"],
                model_check=check, route_rule=R["basis"], status=TAG)


# ---------------------------------------------------------------- labs
def deep_position(X, spec, pos_of_dso):
    if "near" in spec:
        ref = pos_of_dso[spec["near"]]
        return [r3(x) for x in vadd(ref["position_ly"], vmul(spec["offset_au"], AU_KM / LY_KM))], ref.get("velocity_km_s", [0.0, 0.0, 0.0])
    p = spec["position_ly"]
    to = X.I.pos[17]
    u = vsub(to, p)
    u = vmul(u, 1.0 / vlen(u))
    return [r3(x) for x in vadd(p, vmul(u, spec["edge_offset_ly"]))], [0.0, 0.0, 0.0]


def lab_layer(X):
    I, L = X.I, X.C["labs"]
    labs, deep = [], []
    for a in L["authored"]:
        if "deep_space" in a:
            pos, vel = deep_position(X, a["deep_space"], X.dso)
            rec = dict(id=a["id"], name=a["name"], kind="lab", form=a["form"], form_ru=FORM_RU[a["form"]], system=None, polity=a["polity"], operator=a["operator"],
                       staff=a["staff"], legal=a["legal"], interest=a["interest"], reason=a["reason"], position_ly=pos, velocity_km_s=vel,
                       near=a["deep_space"].get("near") or a["deep_space"].get("object"), on_ice=False)
            deep.append(rec)
            continue
        s = a["system"]
        if a.get("placement_surface"):
            place = dict(surface=X.surface_latlon(I, a["anchor"], a["id"]))
            body = a["anchor"]
        else:
            place = dict(a["placement"])
            body = a.get("parent") or a["anchor"]
        labs.append(dict(id=a["id"], name=a["name"], kind="lab", form=a["form"], form_ru=FORM_RU[a["form"]], system=s, body=body, placement=place,
                         polity=a["polity"], operator=a["operator"], staff=a["staff"], legal=a["legal"], interest=a["interest"], reason=a["reason"],
                         zone=a.get("zone"), artifact=a.get("artifact"), on_ice=a["form"] == "small_asteroid" and "trojans" in (a.get("parent") or "")))
    # untouched belts
    U = L["untouched_belts"]
    used_res = {x.get("body") for x in X.S}
    have_lab = {x["system"] for x in labs}
    net = {k for k in X.g if isinstance(k, int)} | {19, 36, 20}
    cands, rejected = [], []
    for rid, r in sorted(X.I.reservoirs.items()):
        if r["kind"] not in U["kinds"]:
            continue
        s = int(rid.split(".")[0][7:])
        m = r.get("total_mass_earth", 0.0)
        why = None
        if m < U["min_mass_earth"]:
            continue
        if s not in net:
            why = "система вне сети маяков и не цель фронтира"
        elif rid in used_res:
            kinds = sorted({x["type"] for x in X.S if x.get("body") == rid})
            why = "уже занят: " + ", ".join(kinds)
        elif s in have_lab:
            why = "в системе уже есть лаборатория"
        if why:
            rejected.append(dict(reservoir=rid, mass_earth=r3(m), why=why))
            continue
        cands.append((m, rid, s))
    cands.sort(key=lambda x: (-x[0], x[1]))
    taken = set()
    for m, rid, s in cands:
        if s in taken or len(taken) >= U["max_labs"]:
            rejected.append(dict(reservoir=rid, mass_earth=r3(m), why="ниже по массе (лимит выборки)" if s not in taken else "одна на систему"))
            continue
        taken.add(s)
        r = X.I.reservoirs[rid]
        pol = X.owner(s) or ("tau_ceti_cognium" if s == 36 else None)
        lid = f"lab.{s:03d}.belt"
        front = {36: " — цель ресурсного фронтира «Сундук» (frontier.ceti_chest)"}.get(s, "")
        labs.append(dict(id=lid, name=f"Разведстанция «{sysname(I, s)}: {dict(kuiper_belt='пояс Койпера', asteroid_belt='пояс', debris_disk='диск', trojans='трояны')[r['kind']]}»",
                         kind="lab", form=U["form"], form_ru=FORM_RU[U["form"]], system=s, body=rid,
                         placement=dict(belt=dict(radius_au=r3(r.get("peak_au") or (r["inner_au"] + r["outer_au"]) / 2.0), longitude_deg=round(360.0 * h01(lid), 2))),
                         polity=pol, operator=f"геологоразведка {I.pol[pol]['name'] if pol in I.pol else 'Управления Маяков'}", staff=U["staff"], legal="lawful",
                         interest="untouched_belt", on_ice=r["kind"] in X.C["life_support"]["ice_bodies"],
                         reason=f"нетронутый {r['kind']} ({r3(m)} масс Земли), рудников нет{front}: состав, плотность роя, отстойник для прыжка — до заявок"))
    return labs, deep, rejected, dict(L["skipped_reasons"])


# ---------------------------------------------------------------- military
def military_layer(X):
    I, M = X.I, X.C["military"]
    rows = []
    crew = {k: X.I.SE["ship_classes"][k]["crew"] for k in ("patrol_cutter", "escort_frigate", "recon_picket")}
    G = M["beacon_guard"]
    per = X.I.PE["defence_tasks"]["patrol_per_beacon"]
    for b in sorted(X.beacons["bridge"], key=lambda b: b["system"]):
        s = b["system"]
        if s == 0:
            continue
        here = X.by_sys.get(s, [])
        if any(x["type"] == "military_hub" for x in here) or any(w in (x.get("purpose") or "") for x in here for w in G["skip_if_purpose"]):
            continue
        pl = X.planets.get(s, [])
        if not pl:
            continue
        forces = {"patrol_cutter": per}
        staff = G["base_staff"] + G["shifts"] * per * crew["patrol_cutter"]
        rows.append(dict(id=f"mil.{s:03d}.beacon_guard", name=f"Пост охраны маяка {b['id']}", kind="military_outpost", role="beacon_guard", system=s,
                         body=pl[0], placement=X.lagrange(pl[0], "L4", 0.015), polity=b["operator"],
                         operator=f"флот {I.pol[b['operator']]['name']}" if b["operator"] in I.pol else "Управление Маяков", staff=staff, forces=forces, legal="lawful",
                         reason=f"в {sysname(I, s)} нет военной базы, а {per} катера охраны маяка (patrol_per_beacon) надо где-то держать; мандат — маяк, не торговля"
                                + (f" (индекс пиратства {X.pir[s]['index']} пост не снижает)" if s in X.pir else ""),
                         size="малый"))
    K = M["contact_line"]
    for (a, b), gr in sorted(I.rel.items()):
        if gr < K["min_grievance"] or a not in I.pol or b not in I.pol or I.system_of(a) != I.system_of(b):
            continue
        s = I.system_of(a)
        belt = next((rid for kind in ("asteroid_belt", "kuiper_belt", "debris_disk") for rid in sorted(X.I.reservoirs)
                     if rid.startswith(f"system_{s}.") and X.I.reservoirs[rid]["kind"] == kind), None)
        for side, other in ((a, b), (b, a)):
            mine = [x for x in X.by_sys.get(s, []) if x["polity"] == side and "belt" in x["placement"]]
            theirs = [x for x in X.by_sys.get(s, []) if x["polity"] == other and "belt" in x["placement"]]
            pid = f"mil.{s:03d}.{side}.contact"
            if belt and mine and theirs:
                la = sum(x["placement"]["belt"]["longitude_deg"] for x in mine) / len(mine)
                lb = sum(x["placement"]["belt"]["longitude_deg"] for x in theirs) / len(theirs)
                mid = (la + ((lb - la + 180) % 360 - 180) / 2.0) % 360
                d = 4.0 if ((lb - la + 180) % 360 - 180) > 0 else -4.0
                lon = (mid - d) % 360
                r = X.I.reservoirs[belt]
                place, body = dict(belt=dict(radius_au=r3(r.get("peak_au") or 3.0), longitude_deg=round(lon, 2))), belt
            else:
                pl = X.planets.get(s, [])
                body = pl[0]
                place = dict(orbit=dict(altitude_km=round(X.I.bodies[body]["physical"]["radius_km"] * (12.0 if side == a else 14.0)),
                                        inclination_deg=20.0 if side == a else -20.0))
            staff = K["base_staff"] + 2 * sum(v * crew[c] for c, v in K["forces"].items())
            rows.append(dict(id=pid, name=f"Пикет линии соприкосновения ({I.pol[side]['name']})", kind="military_outpost", role="contact_line", system=s, body=body,
                             placement=place, polity=side, operator=f"флот {I.pol[side]['name']}", staff=staff, forces=dict(K["forces"]), legal="lawful",
                             reason=f"обида {gr} с {I.pol[other]['name']} в той же системе: пограничный патруль и разведка (border_patrol, recon) на линии соприкосновения",
                             size="малый"))
    for a in M["authored"]:
        s = a["system"]
        home = sorted((x for x in X.by_sys.get(s, []) if x["polity"] == a["anchor_polity"] and x.get("body") in X.I.bodies),
                      key=lambda x: (x["type"] != "moon_hub", -x["population"], x["id"]))
        if not home:
            continue
        moon = home[0]["body"]
        mb = X.I.bodies[moon]
        giant = mb["parent_id"] if mb["kind"] == "moon" and mb.get("parent_id") in X.I.bodies else moon
        zone = next(z for z in X.out["frontier"]["hazard_zones"] if z["id"] == a["zone"])
        gb = X.I.bodies[giant]["physical"]
        moon_a_km = mb["orbit"]["semi_major_axis_au"] * AU_KM if giant != moon else 0.0
        alt = min(moon_a_km + zone["radius_km"], 0.45 * (gb.get("hill_radius_km") or 1e12)) - gb["radius_km"]
        staff = a["base_staff"] + 2 * sum(v * crew[c] for c, v in a["forces"].items())
        rows.append(dict(id=a["id"], name=a["name"], kind="military_outpost", role="quarantine_picket", system=s, body=giant,
                         placement=dict(orbit=dict(altitude_km=round(alt), inclination_deg=35.0)), polity=a["polity"], operator=f"флот {I.pol[a['polity']]['name']}",
                         staff=staff, forces=dict(a["forces"]), legal="lawful", size="средний",
                         reason=a["reason"] + f" Зона — вокруг луны {moon.split('.')[-1]} (город «остатков Гайи-Прайм»); пикет на орбите гиганта {giant.split('.')[-1]} за внешним краем зоны."))
    return rows


# ---------------------------------------------------------------- refuel
def corridor_place(X, sysid, corridor):
    sh = json.load(open(SHELLS, encoding="utf-8"))
    shell = next(x for x in sh["shells"] if x["systemId"] == sysid)
    c = next(x for x in shell["corridors"] if x["name"] == corridor)
    d = c["dir"]
    n = vlen(d)
    d = [x / n for x in d]
    a_au = 1.5 * shell["outerAu"]
    node = math.degrees(math.atan2(d[1], d[0])) % 360.0
    lat = math.degrees(math.asin(max(-1.0, min(1.0, d[2]))))
    pl = X.planets.get(sysid, [])
    star = X.star_of(pl[0]) if pl else f"system_{sysid}"
    return dict(parent_body_id=star, heliocentric=dict(anchor=f"shell.{sysid}.{corridor}", anchor_name=f"устье коридора «{corridor}»", a_au=round(a_au, 1), e=0.0,
                                                        i_deg=90.0, node_deg=round(node, 3), peri_deg=0.0, mean_anomaly_epoch_deg=round(lat, 3)),
                corridor=dict(name=corridor, dir=c["dir"], drift_deg_per_year=c["driftDegPerYear"], shell_outer_au=shell["outerAu"],
                              note="направление на эпоху; стоянка смещается вслед за дрейфом коридора")), star


def refuel_layer(X, access, frontier_chains):
    I, F = X.I, X.C["refuel"]
    J = F["jump_point"]
    rows = []
    transit_t, charge_t_y = {}, {}
    for f in I.X["trade_flows"]["flows"]:
        prof = lg.jump_profile(I.cal, I.SE, I.SE["ship_classes"][f["ship_class"]])
        for n in f["path"][1:-1]:
            transit_t[n] = transit_t.get(n, 0.0) + f["tonnes_y"]
            charge_t_y[n] = charge_t_y.get(n, 0.0) + f["loaded_trips_y"] * 2.0 * prof["charge_per_mean_jump_t"] * J["refuel_share"]
    refinery = {}
    for k, p in X.infra.items():
        c = p["assets"].get("charge_refinery", {}).get("count", 0)
        if c:
            refinery[I.system_of(k)] = refinery.get(I.system_of(k), 0) + c
    for s in sorted(transit_t):
        t = transit_t[s]
        if any(x["type"] == "deep_space_refuel_point" for x in X.by_sys.get(s, [])):
            continue
        market = X.fuel_market(s)
        if not (t >= J["min_transit_t_y"] or (t > 0 and not market)):
            continue
        pl = X.planets.get(s, [])
        b = X.bridge_of.get(s)
        per_day = charge_t_y[s] / 365.0
        src = min((x for x in refinery if refinery[x] >= 5), key=lambda x: (vlen(vsub(I.pos[x], I.pos[s])), x))
        rows.append(dict(id=f"fuel.{s:03d}.jump_point", name=f"Заправка у точки прыжка {sysname(I, s)}", kind="refuel_station", role="jump_point", system=s,
                         body=pl[0], placement=X.lagrange(pl[0], "L4", 0.03), polity=b["operator"] if b else None,
                         operator=f"Управление Маяков + {I.pol[b['operator']]['name']}" if b and b["operator"] in I.pol else "Управление Маяков",
                         staff=J["staff"], legal="lawful", transit_t_y=round(t), charge_t_day=r3(per_day), stock_charge_t=round(per_day * J["stock_days"]),
                         charge_source=dict(system=src, name=sysname(I, src), ly=r3(vlen(vsub(I.pos[src], I.pos[s]))), refineries=refinery[src], ship="jump_tanker"),
                         reason=f"транзит {round(t / 1e6, 2)} млн т/год идёт через {sysname(I, s)} без захода к хабам"
                                + ("" if market else "; своего рынка заряда в системе нет") + ": заряд у моста-маяка экономит плечо ~1 а.е. до хаба и обратно",
                         size="средний" if t >= 5e6 else "малый"))
    # shell-mouth staging posts
    for a in F["shell_staging"]["sites"]:
        place, star = corridor_place(X, a["system"], a["corridor"])
        rows.append(dict(id=a["id"], name=a["name"], kind="refuel_station", role="shell_staging", system=a["system"], body=star, placement=place,
                         polity=a["polity"], operator=a["operator"], staff=a["staff"], legal=a["legal"], reason=a["reason"], size="малый"))
    # interstellar range check: chains of legs between charge points vs a loaded Cobrett tank
    cob = I.SE["ship_classes"]["cobrett_i_n1"]
    m = cob["dry_mass_t"] + cob["jump_tank_t"] + lg.cargo_gross_t(cob, cob["cargo_t"])
    rng = cob["jump_tank_t"] / lg.jump_fuel_t(I.cal, I.SE, cob, m, 1.0)
    new_fuel = {r["system"] for r in rows if r["role"] == "jump_point"}

    def has_fuel(n):
        if isinstance(n, str):
            return (X.dso.get(n) and next((d for d in I.WS["deep_space_objects"] if d["id"] == n), {}).get("stocks", {}).get("jump_charge_t", 0) > 0)
        return X.fuel_market(n) or n in new_fuel or any(x["type"] == "deep_space_refuel_point" for x in X.by_sys.get(n, []))
    checks = []
    for fid in list(access):
        ch = access[fid]
        pos = [I.pos[n[1]] if n[0] == "system" else X.dso[n[1]]["position_ly"] for n in ch]
        run, worst = 0.0, 0.0
        for i in range(1, len(ch)):
            run += vlen(vsub(pos[i], pos[i - 1]))
            worst = max(worst, run)
            if has_fuel(ch[i][1]):
                run = 0.0
        checks.append(dict(chain=fid, nodes=[n[1] for n in ch], longest_dry_run_ly=r3(worst), loaded_cobrett_range_ly=r3(rng), ok=worst <= rng,
                           verdict="хватает" if worst <= rng else "нужна промежуточная заправка"))
    return rows, dict(rule=F["interstellar_rule"], loaded_cobrett_range_ly=r3(rng), chains=checks,
                      new_interstellar_points=[c["chain"] for c in checks if not c["ok"]],
                      verdict="новых межзвёздных заправок по правилу дальности не нужно: существующие «Дальний Причал», «Китовая Щель», «Ноль-Вега», депо «Перевал» и рынки систем закрывают все плечи"
                              if all(c["ok"] for c in checks) else "есть плечи без заряда — см. chains")


# ---------------------------------------------------------------- pirates and smugglers
def far_small_bodies(X, s):
    rows = []
    for bid, b in X.I.bodies.items():
        if bid.startswith(f"system_{s}.") and b["kind"] in ("dwarf_planet", "moon"):
            a = b["orbit"]["semi_major_axis_au"]
            if b["kind"] == "moon" and b.get("parent_id") in X.I.bodies:
                a = X.I.bodies[b["parent_id"]]["orbit"]["semi_major_axis_au"] + a
            rows.append((-a, bid))
    rows.sort()
    return [bid for _, bid in rows]


def pirate_layer(X):
    I, P = X.I, X.C["pirates"]
    thr = I.WS["piracy"]["enclave_threshold"]
    rows = []
    for s, p in sorted(X.pir.items()):
        if p["index"] < P["min_index"]:
            continue
        n = int(math.floor((p["index"] - thr) / P["step"] + 1e-9))
        encl = next((x for x in X.by_sys.get(s, []) if x["type"] == "pirate_enclave"), None)
        if n <= 0 or encl is None:
            continue
        legal_bodies = {x.get("body") for x in X.by_sys.get(s, []) if x["type"] != "pirate_enclave"}
        covers = [rid for rid in sorted(X.I.reservoirs, key=lambda r: -X.I.reservoirs[r].get("total_mass_earth", 0.0))
                  if rid.startswith(f"system_{s}.") and X.I.reservoirs[rid]["kind"] in COVER and rid != encl.get("body") and rid not in legal_bodies]
        smalls = [b for b in far_small_bodies(X, s) if b not in legal_bodies]
        clouds = [rid for rid in sorted(X.I.reservoirs) if rid.startswith(f"system_{s}.") and X.I.reservoirs[rid]["kind"] == "comet_cloud"]
        opts = [("belt", rid) for rid in covers] + [("body", b) for b in smalls] + [("cloud", rid) for rid in clouds]
        for i in range(n):
            sid = f"pir.{s:03d}.{i + 1}"
            kind, where = opts[i] if i < len(opts) else ("same", encl["body"])
            if kind in ("belt", "cloud"):
                r = X.I.reservoirs[where]
                rad = r.get("peak_au") or (r["inner_au"] + r["outer_au"]) / 2.0
                if kind == "cloud":
                    rad = r["inner_au"]
                place, cover = dict(belt=dict(radius_au=r3(rad), longitude_deg=round(360 * h01(sid), 2))), f"{r['kind']} {where.split('.')[-1] if kind == 'belt' else ''}".strip()
            elif kind == "body":
                place, cover = dict(surface=X.surface_latlon(I, where, sid)), f"далёкое малое тело {where.split('.')[-1]}"
            else:
                bp = encl["placement"].get("belt", {"radius_au": 3.0, "longitude_deg": 0.0})
                place, cover = dict(belt=dict(radius_au=bp["radius_au"], longitude_deg=round((bp["longitude_deg"] + 180.0) % 360, 2))), "другой сектор того же пояса"
            pop = int(encl["population"] * P["share_of_enclave"])
            rows.append(dict(id=sid, name=f"Пиратское поселение {sysname(I, s)}-{i + 1}", kind="pirate_settlement", system=s, body=where, placement=place,
                             polity=None, operator="отколовшаяся команда братства" if i == 0 else "клан скупщиков и семьи экипажей",
                             staff=pop, ships=round(pop / 25.0), legal="illegal", known_to_player="hidden", on_ice=kind in ("belt", "cloud"),
                             reason=f"индекс пиратства {p['index']} (трафик {p['traffic']}, слабый надзор {p['weak_enforcement']}, укрытие {p['cover']}, "
                                    f"тихая гавань {p['safe_haven']}, коррупция {p['corruption']}): одного анклава мало — второе логово ({cover})",
                             supply_via=encl["id"], size="малое"))
    return rows


def smuggler_layer(X):
    I, M = X.I, X.C["smugglers"]
    cm = json.load(open(os.path.join(HERE, "canon", "commodities.json"), encoding="utf-8"))
    fams = cm["regime_family"]
    leg = {}
    for fam in ("open", "strict", "faith", "military"):
        r = sum(1 for c in cm["commodities"] if c["legality"][fam] == "restricted")
        b = sum(1 for c in cm["commodities"] if c["legality"][fam] == "banned")
        leg[fam] = (r + 2 * b) / float(len(cm["commodities"]))
    scored = []
    for s in sorted({I.system_of(k) for k in I.pol}):
        if s == 0:
            continue
        ks = sorted((k for k in I.pol if I.system_of(k) == s), key=lambda k: -I.pol[k]["population"])
        main = ks[0]
        gp = ((I.colony.get(main) or {}).get("law_policy") or {}).get("goods_policy")
        fam = gp or fams.get(I.regime(main), "open")
        if fam == "open":
            continue
        p = X.pir.get(s)
        if not p:
            continue
        score = leg[fam] * (0.5 + p["corruption"]) * p["traffic"]
        scored.append((score, s, main, fam, p))
    scored.sort(key=lambda x: (-x[0], x[1]))
    rows, rejected = [], []
    lo, hi = M["population"]
    for score, s, main, fam, p in scored:
        if score < M["min_score"] or len(rows) >= M["max_hubs"]:
            rejected.append(dict(system=s, name=sysname(I, s), score=r3(score), why="ниже порога" if score < M["min_score"] else "лимит"))
            continue
        legal_bodies = {x.get("body") for x in X.by_sys.get(s, [])}
        zone = next((z for z in X.out["frontier"]["hazard_zones"] if (z.get("anchor") or "").startswith(f"system_{s}.") and z["kind"] != "quarantine"), None)
        sid = f"smug.{s:03d}"
        if zone:
            body = zone["anchor"]
            place = dict(orbit=dict(altitude_km=round(0.8 * zone["radius_km"]), inclination_deg=round(70 * h01(sid), 1)))
            where = f"в зоне «{zone['name']}»"
        else:
            smalls = [b for b in far_small_bodies(X, s) if b not in legal_bodies]
            if smalls:
                body = smalls[0]
                place = dict(surface=X.surface_latlon(I, body, sid))
                where = f"на далёком малом теле {body.split('.')[-1]}"
            else:
                body = next((rid for rid in sorted(X.I.reservoirs, key=lambda r: -(X.I.reservoirs[r].get("peak_au") or 0))
                             if rid.startswith(f"system_{s}.") and X.I.reservoirs[rid]["kind"] in COVER), None)
                if body is None:
                    continue
                r = X.I.reservoirs[body]
                place = dict(belt=dict(radius_au=r3(r.get("outer_au") or r.get("peak_au")), longitude_deg=round(360 * h01(sid), 2)))
                where = f"на внешнем краю {r['kind']}"
        pop = int(lo + (hi - lo) * min(1.0, (score - M["min_score"]) / 0.4))
        rows.append(dict(id=sid, name=f"Тайная стоянка {sysname(I, s)}", kind="smuggler_hub", system=s, body=body, placement=place, polity=None,
                         operator="контрабандная артель перевозчиков (свои люди в таможне)", staff=pop, legal="illegal", known_to_player="rumour",
                         reason=f"режим товаров «{fam}» у {I.pol[main]['name']}: запрещено/ограничено {round(leg[fam] * 100)} % позиций (вес запрета x2); "
                                f"коррупция {p['corruption']}, трафик {p['traffic']} — score {r3(score)}; {where}: туда не заходят патрули и плановые рейсы",
                         score=r3(score), on_ice=False, size="малый"))
    for a in M["authored"]:
        place, star = corridor_place(X, a["system"], a["corridor"])
        rows.append(dict(id=a["id"], name=a["name"], kind="smuggler_hub", system=a["system"], body=star, placement=place, polity=None, operator=a["operator"],
                         staff=a["population"], legal=a["legal"], known_to_player="rumour", reason=a["reason"], on_ice=False, size="малый"))
    return rows, rejected


# ---------------------------------------------------------------- assemble
def site_record(X, r, typ, map_level=None):
    """A new object in the shape of world_sites.base_site (so the runtime export and economy_live read it unchanged)."""
    I = X.I
    s = X.base_site(I, r["id"], r["name"], typ, r["system"], r.get("polity"), r.get("operator"), r.get("staff", 0), r["reason"],
                    body=r.get("body"), placement=r["placement"], legal=r.get("legal"), source="outposts.py")
    small = {"military_outpost": (4, 20000, "basic"), "lab": (2, 3000, "basic"), "refuel_station": (4, None, "basic"),
             "pirate_settlement": (4, 30000, "basic"), "smuggler_hub": (3, 8000, "none")}[r["kind"]]
    s["docks"] = small[0]
    if small[1]:
        s["storage_t"] = small[1]
    if r["kind"] == "refuel_station" and r.get("stock_charge_t"):
        s["storage_t"] = max(s["storage_t"], r["stock_charge_t"])
    s["repair"] = small[2]
    s["level"] = "C"
    s["map_level"] = map_level or ("detail" if r["kind"] in ("lab", "military_outpost") else "system")
    s["outpost_kind"] = r["kind"]
    s["known_to_player"] = r.get("known_to_player", "public")
    s["placement_reason"] = r["reason"]
    s["language"] = I.language(r.get("polity")) if r.get("polity") else I.WS["languages"]["network_lingua_franca"]
    for k in ("form", "form_ru", "forces", "role", "supply", "interest", "zone", "artifact", "size", "ships", "assembly"):
        if r.get(k) is not None:
            s[k] = r[k]
    return s


def compute(I, out, g, base_site, surface_latlon, access, frontier_chains):
    X = Ctx(I, out, g, base_site, surface_latlon)
    beacons = beacon_layer(X, access)
    labs, deep_labs, lab_rej, lab_skip = lab_layer(X)
    mil = military_layer(X)
    fuel, inter = refuel_layer(X, access, frontier_chains)
    pir = pirate_layer(X)
    smug, smug_rej = smuggler_layer(X)
    # supply for every manned object
    hubs_by_id = {s["id"]: s for s in X.S}
    for r in labs + mil + fuel + pir + smug:
        if r.get("supply_via") and r["supply_via"] in hubs_by_id:
            src, ly = hubs_by_id[r["supply_via"]], 0.0
        else:
            src, ly = X.nearest_market(r["system"], r.get("polity"))
        extra = {}
        if r["kind"] == "lab":
            extra["science_kit"] = X.C["labs"]["science_kit_t_day"]
        if r["kind"] == "military_outpost":
            extra["weapons"] = 0.02 * sum(r.get("forces", {}).values())
            extra["jump_charge"] = 0.05 * sum(r.get("forces", {}).values())
        bulk = {"jump_charge": r["charge_t_day"]} if r["kind"] == "refuel_station" and r.get("charge_t_day") else None
        r["supply"] = supply(X, r["staff"], r.get("on_ice", False), src, extra, ly or 0.0, bulk)
        if r["kind"] == "refuel_station" and r.get("charge_source"):
            r["supply"]["jump_charge_from"] = r["charge_source"]
        if r["kind"] in ("pirate_settlement", "smuggler_hub"):
            r["supply"]["note"] = "покупают через скупщиков и подставные компании; часть — краденое"
    for r in deep_labs:
        pref = [d for d in I.WS["deep_space_objects"] if d.get("polity") == r["polity"] and d.get("stocks") and d["id"] in X.dso
                and vlen(vsub(X.dso[d["id"]]["position_ly"], r["position_ly"])) < 1.0]
        near = sorted((vlen(vsub(X.dso[d["id"]]["position_ly"], r["position_ly"])), d["id"]) for d in X.supply_dsos())
        if pref:
            src = dict(id=pref[0]["id"], name=pref[0]["name"])
        elif near and near[0][0] < 1.0:
            d = next(x for x in I.WS["deep_space_objects"] if x["id"] == near[0][1])
            src = dict(id=d["id"], name=d["name"])
        else:
            best = min((t for t in X.by_sys if isinstance(t, int) and X.market_hub(t)), key=lambda t: vlen(vsub(I.pos[t], r["position_ly"])))
            h = X.market_hub(best)
            src = dict(id=h["id"], name=h["name"], system=best)
        r["supply"] = supply(X, r["staff"], False, src, {"science_kit": X.C["labs"]["science_kit_t_day"]})
    for r in labs + deep_labs:
        r["assembly"] = "lab_" + {"orbital": "orbital", "free_floating": "free_floating", "small_asteroid": "small_asteroid", "moon_surface": "moon_surface"}[r["form"]]
    for r in mil:
        r["assembly"] = "military_outpost"
    for r in fuel:
        r["assembly"] = "refuel_station"
    for r in pir:
        r["assembly"] = "pirate_settlement"
    for r in smug:
        r["assembly"] = "smuggler_hub"
    visible = [site_record(X, r, "science_hub") for r in labs] + [site_record(X, r, "military_hub") for r in mil] + \
              [site_record(X, r, "deep_space_refuel_point") for r in fuel]
    hidden = [site_record(X, r, "pirate_enclave") for r in pir] + [site_record(X, r, "free_port") for r in smug]
    for h in hidden:
        h["map_level"] = "none"
    summary = {}
    for kind, rows in (("labs", labs + deep_labs), ("military_outposts", mil), ("refuel_stations", fuel), ("pirate_settlements", pir), ("smuggler_hubs", smug)):
        by = {}
        for r in rows:
            key = str(r["system"]) if r.get("system") is not None else "deep_space"
            by[key] = by.get(key, 0) + 1
        summary[kind] = dict(total=len(rows), by_system=dict(sorted(by.items())), staff=sum(r["staff"] for r in rows),
                             supply_t_day=r3(sum(r["supply"]["t_day"] for r in rows)))
    summary["beacons"] = beacons["counts"]
    return dict(beacons=beacons, labs=labs + deep_labs, labs_rejected=lab_rej, labs_skipped_zones=lab_skip, military=mil, refuel=fuel,
                interstellar_refuel=inter, pirates=pir, smugglers=smug, smugglers_rejected=smug_rej, assembly=ASSEMBLY, summary=summary,
                visible_sites=visible, hidden_sites=hidden, deep_labs=deep_labs, rules=X.C, status=TAG)


def checks(I, O):
    res = []

    def add(name, ok, detail):
        res.append(dict(check="outposts." + name, ok=bool(ok), detail=detail))
    mc = O["beacons"]["model_check"]
    add("beacons_match_model", all(a == b for a, b in mc.values()), ", ".join(f"{k} {a}/{b}" for k, (a, b) in mc.items()))
    rows = O["visible_sites"] + O["hidden_sites"]
    bad = [s["id"] for s in rows if s.get("body") and s["body"] not in I.bodies and s["body"] not in I.reservoirs and not str(s["body"]).count(".") == 1]
    add("outpost_bodies_exist", not bad, ",".join(bad[:6]) or f"{len(rows)} объектов")
    nore = [s["id"] for s in rows if not s.get("placement_reason")] + [d["id"] for d in O["deep_labs"] if not d.get("reason")]
    add("every_outpost_has_reason", not nore, ",".join(nore[:6]) or "ok")
    nosup = [r["id"] for k in ("labs", "military", "refuel", "pirates", "smugglers") for r in O[k] if not (r.get("supply") or {}).get("source")]
    add("every_manned_outpost_has_supply_source", not nosup, ",".join(nosup[:6]) or "ok")
    ids = [s["id"] for s in rows] + [d["id"] for d in O["deep_labs"]] + [b["id"] for b in O["beacons"]["in_system"] + O["beacons"]["route"]]
    dup = sorted({x for x in ids if ids.count(x) > 1})
    add("outpost_ids_unique", not dup, ",".join(dup[:6]) or f"{len(ids)} ids")
    wa = sum(1 for r in O["labs"] if r.get("system") == 19 or (r.get("near") or "").startswith("dso.lotus"))
    add("labs_selective_and_dense_at_worlds_ass", wa >= 4 and len(O["labs"]) < 30, f"всего {len(O['labs'])}, у «Жопы мира» {wa}, отвергнуто {len(O['labs_rejected'])}")
    return res
