"""World sites 3126: the spatial civilisation layer (offline, deterministic, stdlib only).

Hubs, settlements, deep-space objects with world lines, hazard zones, frontiers
(the World's Ass), social state, local administrators, law and jurisdiction,
beacon communication topology and delays, passenger logistics, fleet age and
the secondary market, piracy, wreck fields, rare resource and artifacts.

One object = one id. Inputs: canon/world_sites.json (authored PROPOSAL), the
physical catalog (src/assets/data/galaxy_details), canon/world.json /
missions.json, and the GENERATED economy (out/world_3126/*_3126.json from
world_economy.py, run that first). Nothing here recomputes the economy.

Outputs (out/world_3126/): sites_3126.json, society_3126.json, law_3126.json,
comms_3126.json, passengers_3126.json, fleet_age_3126.json,
frontier_3126.json, world_events_3126.json, sites_checks_3126.json and the
readable WORLD_SITES_3126.md. Runtime copies (GENERATED, never edited by hand):
src/assets/data/galaxy_details/world_sites/sites_3126.json (in-system sites and
zones for System / Detail maps) and objects_details/world_*.json (deep-space
objects as galaxy_object documents for the Galaxy Map).

    python world_sites.py            # compute, write out/ and the runtime export
    python world_sites.py --check    # recompute, compare, run the consistency checks
"""
import argparse
import glob
import hashlib
import heapq
import json
import math
import os
import random
import sys

import endowment as endow
import lab
import logistics as lg

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out", "world_3126")
CATALOG = endow.CATALOG
RUNTIME_SITES = os.path.join(CATALOG, "world_sites", "sites_3126.json")
RUNTIME_OBJECTS = os.path.join(CATALOG, "objects_details")
YEAR = 3126
SOL = "sol_authority"
LY_KM = 9.4607e12
AU_KM = 1.495978707e8
C_KM_S = 299792.458
SECONDS_Y = 365.25 * 86400.0
TAG = "PROPOSAL"

# corruption baseline by legal family (PROPOSAL): how much the system tolerates private deals
CORRUPTION_BASE = {"technocratic_civil": 0.15, "confucian_administrative": 0.22, "party_administrative": 0.32, "sol_codified": 0.25,
                   "theocratic": 0.3, "corporate": 0.35, "city_state_arbitration": 0.28, "parliamentary_civil": 0.2,
                   "military_administration": 0.42, "clan_law": 0.45, "customary_council": 0.32, "machine_oracle": 0.05, "none": 0.85}
INSTITUTIONS = {"technocratic_civil": 0.85, "confucian_administrative": 0.85, "party_administrative": 0.75, "sol_codified": 0.8,
                "theocratic": 0.65, "corporate": 0.6, "city_state_arbitration": 0.6, "parliamentary_civil": 0.75,
                "military_administration": 0.7, "clan_law": 0.35, "customary_council": 0.55, "machine_oracle": 0.9, "none": 0.05}


def jload(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def clamp(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


def r3(x):
    return round(x, 3)


def h01(*parts):
    """Deterministic number in [0, 1) from a stable key (no Python hash())."""
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


def vnorm(a):
    n = vlen(a)
    return [x / n for x in a] if n > 0 else [0.0, 0.0, 0.0]


def vcross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


# ---------------------------------------------------------------- inputs
class Inputs:
    def __init__(self):
        self.WS = lab.load("world_sites.json")
        self.W = lab.load("world.json")
        self.PE = lab.load("polity_economy.json")
        self.SE = lg.load_ship_economics()
        self.MI = {m["id"]: m for m in lab.load("missions.json")["missions"]}
        self.E = endow.load_endowment()
        self.X = {n: jload(os.path.join(OUT, f"{n}_3126.json")) for n in
                  ("economy", "infrastructure", "trade_flows", "fleet_census", "defence", "beacons")}
        self.pos = {k: list(v["pos"]) for k, v in self.E.items()}
        self.net = lg.network_stats(self.W, self.E)
        self.cal = lg.calibrate(self.SE, self.net)
        self.hop = self.net["max_hop_ly"]
        self.pol = self.X["economy"]["polities"]
        self.colony = {c["id"]: c for c in self.W["colonies"]}
        self.rel = {tuple(sorted((r["a"], r["b"]))): r["grievance"] for r in self.W["relations_anchor"]}
        self.bodies, self.reservoirs = self.load_bodies()

    def load_bodies(self):
        bodies, res = {}, {}
        for d in sorted(glob.glob(os.path.join(CATALOG, "systems", "*", "generated"))):
            for f in sorted(glob.glob(os.path.join(d, "bodies", "*.json"))):
                b = jload(f)
                bodies[b["id"]] = b
            for f in sorted(glob.glob(os.path.join(d, "reservoirs", "*.json"))):
                r = jload(f)
                res[r["id"]] = r
        return bodies, res

    def regime(self, k):
        if k == SOL:
            return "sol_authority"
        return self.colony[k]["regime_3126"] if k in self.colony else "none_quiet"

    def law_family(self, k):
        if k is None:
            return "none"
        return self.WS["regime_law"].get(self.regime(k), "none")

    def system_of(self, k):
        return 0 if k == SOL else self.pol[k]["system"]

    def language(self, k):
        if k is None:
            return self.WS["languages"]["network_lingua_franca"]
        if k == SOL:
            return "en"
        cu = {}
        for f in self.colony[k]["founders"]:
            for c, v in (self.MI.get(f, {}).get("cultures") or {}).items():
                cu[c] = cu.get(c, 0.0) + v
        if not cu:
            return "en"
        top = sorted(cu.items(), key=lambda kv: (-kv[1], kv[0]))[0][0]
        return self.WS["languages"]["culture_to_language"].get(top, "en")


def body_id_for(I, system, designation):
    """Runtime body id from a catalog designation (settlements.json / economy cities use designations)."""
    for bid, b in I.bodies.items():
        if bid.startswith(f"system_{system}.") and b.get("designation") == designation:
            return bid
    return None


# ---------------------------------------------------------------- route graph (the one the economy uses)
def route_graph(I):
    g = {}
    for r in I.X["trade_flows"]["routes"]:
        a, b = r["edge"]
        g.setdefault(a, {})[b] = dict(ly=r["ly"], klass=r["klass"], unbeaconed=r["unbeaconed"], tonnes=r["tonnes_y"])
        g.setdefault(b, {})[a] = dict(ly=r["ly"], klass=r["klass"], unbeaconed=r["unbeaconed"], tonnes=r["tonnes_y"])
    # beacon edges without trade still carry mail and people
    for a, nb in I.net["_edges"].items():
        for b, d in nb.items():
            if b not in g.get(a, {}):
                g.setdefault(a, {})[b] = dict(ly=round(d, 2), klass="feeder", unbeaconed=False, tonnes=0)
    return g


def dijkstra(g, src, w):
    dist = {src: 0.0}
    prev = {}
    pq = [(0.0, src)]
    while pq:
        d, u = heapq.heappop(pq)
        if d > dist.get(u, 1e18):
            continue
        for v in sorted(g.get(u, {})):
            nd = d + w(u, v, g[u][v])
            if nd < dist.get(v, 1e18) - 1e-12:
                dist[v] = nd
                prev[v] = u
                heapq.heappush(pq, (nd, v))
    return dist, prev


def path_to(prev, src, dst):
    if dst != src and dst not in prev:
        return None
    p = [dst]
    while p[-1] != src:
        p.append(prev[p[-1]])
    return p[::-1]


# real hop chains beyond the network edge (systems and deep-space objects; every leg <= max hop)
ACCESS = {
    "frontier.worlds_ass": [("system", 25), ("system", 24), ("system", 45), ("dso", "dso.lotus"), ("system", 19)],
    "frontier.ceti_chest": [("system", 13), ("system", 17), ("dso", "dso.ceti_gap_cache"), ("system", 36)],
    "frontier.vega_observatory": [("system", 41), ("dso", "dso.vega_null"), ("system", 20)],
    "kern_pocket": [("system", 25), ("system", 24), ("system", 45), ("system", 37)],
}
FRONTIER_CHAINS = ("frontier.worlds_ass", "frontier.ceti_chest", "frontier.vega_observatory")
FRONTIER_SYSTEMS = (19, 36, 20)


# ---------------------------------------------------------------- deep-space objects (world lines)
def dso_records(I):
    out = []
    for d in I.WS["deep_space_objects"]:
        pl = d["placement"]
        a, b = I.pos[pl["from_system"]], I.pos[pl["toward_system"]]
        u = vnorm(vsub(b, a))
        side = vnorm(vcross(u, [0.0, 0.0, 1.0]))
        p = vadd(vadd(a, vmul(u, pl["distance_ly"])), vmul(side, pl.get("lateral_offset_ly", 0.0)))
        v = d.get("velocity_km_s", {})
        if v.get("speed", 0.0) > 0:
            vdir = vnorm(vcross(u, side)) if v["direction"] == "toward_system_perpendicular" else u
            vel = vmul(vdir, v["speed"])
        else:
            vel = [0.0, 0.0, 0.0]
        out.append(dict(id=d["id"], name=d["name"], site_type=d["site_type"], object_type=d["object_type"], hidden=bool(d.get("hidden")),
                        frame="ICRS_heliocentric", epoch_year=I.WS["epoch_year"], position_ly=[r3(x) for x in p],
                        velocity_km_s=[r3(x) for x in vel], drift_km_per_year=round(vlen(vel) * SECONDS_Y),
                        legs_ly={str(pl["from_system"]): r3(vlen(vsub(p, a))), str(pl["toward_system"]): r3(vlen(vsub(p, b)))},
                        source="placement", status=TAG))
    # generation ships still in flight: positions from the mission canon (source -> target, share of the path)
    for sid in I.WS["in_flight_generation_ships"]:
        m = I.MI[sid]
        a, b = I.pos[m["source_system"]], I.pos[m["target_system"]]
        total = vlen(vsub(b, a))
        if sid == "FINAL-ARK-1":
            share, arrival = 0.85, 3236      # missions canon note: ~85 % of the way, arrival ~3236
        else:
            arrival = 4120                   # LEGACY-1: legacy arrival 4120
            share = (YEAR - m["launch_year"]) / float(arrival - m["launch_year"])
        p = vadd(a, vmul(vsub(b, a), share))
        speed_ly_y = total * (1 - share) / max(1, arrival - YEAR)
        vel = vmul(vnorm(vsub(b, a)), speed_ly_y * LY_KM / SECONDS_Y)
        out.append(dict(id=f"dso.genship.{sid.lower()}", name=sid, site_type=None, object_type="generation_ship", hidden=False,
                        frame="ICRS_heliocentric", epoch_year=I.WS["epoch_year"], position_ly=[r3(x) for x in p],
                        velocity_km_s=[r3(x) for x in vel], drift_km_per_year=round(vlen(vel) * SECONDS_Y),
                        legs_ly={}, source=f"missions.json {sid} (in flight, share {share:.2f}, arrival ~{arrival})",
                        living_aboard=m.get("population") if sid != "FINAL-ARK-1" else 14500, status="GENERATED"))
    return out


# ---------------------------------------------------------------- sites
def base_site(I, sid, name, typ, system, polity, operator, population, purpose, **kw):
    st = I.WS["site_types"][typ]
    s = dict(id=sid, name=name, type=typ, type_ru=st["ru"], system=system, system_name=I.E[system]["name"] if system in I.E else None,
             polity=polity, operator=operator, population=int(population), purpose=purpose, map_level=st["map"],
             docks=st["docks"] if typ != "abandoned_hub" else 0, storage_t=st["storage_t"], repair=st["repair"],
             medical_beds=round(population * st["medical_beds_per_1000"] / 1000.0, 1),
             quarantine_beds=round(population * st["quarantine_beds_per_1000"] / 1000.0, 1), security_base=st["security"], status=TAG)
    s.update(kw)
    return s


def biggest_bodies(I, system, n=3):
    rows = []
    for bid, b in I.bodies.items():
        if bid.startswith(f"system_{system}.") and b["kind"] in ("planet", "dwarf_planet", "moon"):
            rows.append((-b["physical"]["mass_kg"], bid))
    rows.sort()
    return [bid for _, bid in rows[:n]]


def belt_of(I, system):
    for kind in ("asteroid_belt", "kuiper_belt", "trojans"):
        for rid, r in sorted(I.reservoirs.items()):
            if rid.startswith(f"system_{system}.") and r["kind"] == kind:
                return rid
    return None


def surface_latlon(I, bid, key):
    """Climate-first placement: temperate band for hot bodies, low latitude for cold ones; longitude seeded.
    Relief is NOT sampled yet (runtime terrain sampler, Stage 6): placement_status says so."""
    b = I.bodies.get(bid)
    t = b["thermal"].get("mean_surface_temperature_k", 250.0) if b else 250.0
    band = (35.0, 60.0) if t > 300 else (0.0, 25.0) if t < 240 else (15.0, 45.0)
    lat = band[0] + (band[1] - band[0]) * h01(key, "lat")
    lat = lat if h01(key, "hemi") < 0.5 else -lat
    lon = 360.0 * h01(key, "lon")
    return dict(lat_deg=round(lat, 3), lon_deg=round(lon, 3), elevation_ref="terrain_surface", placement_status="climate_only_terrain_not_sampled",
                seed=hashlib.md5(key.encode("utf-8")).hexdigest()[:16], generator_version=1)


def build_sites(I, dsos):
    sites = []
    used = set()
    # authored named + frontier sites
    for s in I.WS["named_sites"] + I.WS["frontier_sites"]:
        if s["id"] in used:
            continue
        rec = base_site(I, s["id"], s["name"], s["type"], s["system"], s.get("polity"), s.get("operator"), s.get("population", 0), s.get("purpose"),
                        body=s.get("body"), placement={k: s[k] for k in ("orbit", "surface", "belt") if k in s}, founded=s.get("founded"),
                        history=s.get("history"), legal=s.get("legal", "lawful"), stocks=s.get("stocks"), beacon=s.get("beacon"),
                        runtime_binding=s.get("runtime_binding"), source="authored")
        if "surface" in s:
            rec["placement"]["surface"].update(placement_status="authored")
        sites.append(rec)
        used.add(s["id"])
    # authored deep-space hubs
    for d in I.WS["deep_space_objects"]:
        rec = base_site(I, d["id"], d["name"], d["site_type"], None, d.get("polity"), d.get("operator"), d.get("population", 0), d.get("purpose"),
                        body=None, placement=dict(deep_space=d["id"]), founded=d.get("founded"), history=d.get("history"),
                        legal="contested" if d.get("polity") is None else "licensed", stocks=d.get("stocks"), beacon=d.get("beacon"),
                        hidden=bool(d.get("hidden")), identity=d.get("identity"), source="authored")
        rec["system_name"] = "межзвёздное пространство"
        sites.append(rec)
        used.add(d["id"])
    # generated from the economy: cities (habitats, ports), then hubs from infrastructure counts
    for k in sorted(I.pol):
        e = I.pol[k]
        system = I.system_of(k)
        inf = I.X["infrastructure"]["polities"][k]["assets"]
        A = {a: v["count"] for a, v in inf.items()}
        big = biggest_bodies(I, system, 4)
        belt = belt_of(I, system)
        n = 0
        for c in e["cities"]:
            if c["body"] == "—":
                continue
            bid = body_id_for(I, system, c["body"])
            if not bid:
                continue
            n += 1
            sid = f"site.{system:03d}.{k}.city{n:02d}"
            b = I.bodies[bid]
            surface_ok = b["kind"] in ("planet", "moon", "dwarf_planet") and b["physical"]["surface_gravity_ms2"] < 30 and b["physical"].get("body_class") != "gas_giant"
            typ = "planetary_surface_port" if (surface_ok and b["kind"] == "planet") else "moon_hub" if surface_ok else "orbital_hub"
            pl = dict(surface=surface_latlon(I, bid, sid)) if typ != "orbital_hub" else dict(orbit=dict(altitude_km=round(1.2 * b["physical"]["radius_km"])))
            sites.append(base_site(I, sid, c["name"], typ, system, k, e["name"], c["pop"], c["role"], body=bid, placement=pl,
                                   night_light=dict(source="population", pop=c["pop"]), source="economy.cities"))
            # main habitats get an orbital port above them
            if c["pop"] >= 2e7 and typ != "orbital_hub":
                oid = f"site.{system:03d}.{k}.port{n:02d}"
                sites.append(base_site(I, oid, f"{c['name']}: орбитальный порт", "orbital_hub", system, k, e["name"], max(2000, c["pop"] * 0.002),
                                       "порт над поселением: челноки поверхность ↔ орбита, таможня", body=bid,
                                       placement=dict(orbit=dict(altitude_km=round(0.08 * b["physical"]["radius_km"] + 300))), source="economy.cities"))
        # hubs from infrastructure counts (one site aggregates many plants; counts stay in the record)
        groups = [("trade_hub", A.get("trade_hub", 0), 1, "торговый хаб: межзвёздные грузы, биржа, склады"),
                  ("mining_hub", A.get("mine", 0) + A.get("ice_mine", 0), 60, "горнодобывающий хаб: рудники, лёд, плоты буксиров"),
                  ("industrial_hub", A.get("metallurgy", 0) + A.get("factory", 0) + A.get("electronics_fab", 0) + A.get("chemical_plant", 0), 150, "промышленный хаб"),
                  ("military_hub", A.get("military_base", 0), 8, "военная база"),
                  ("science_hub", A.get("research_station", 0), 60, "научный хаб")]
        for typ, count, per, purpose in groups:
            if count <= 0:
                continue
            m = min(6, max(1, math.ceil(count / per)))
            for i in range(m):
                sid = f"site.{system:03d}.{k}.{typ}{i + 1:02d}"
                where = belt if (typ == "mining_hub" and belt and i % 2 == 0) else big[i % len(big)] if big else None
                if where is None:
                    continue
                staff = {"trade_hub": 9000, "mining_hub": 2500, "industrial_hub": 12000, "military_hub": 6000, "science_hub": 800}[typ]
                pl = dict(belt=dict(radius_au=r3(I.reservoirs[where].get("peak_au") or (I.reservoirs[where]["inner_au"] + I.reservoirs[where]["outer_au"]) / 2.0),
                                    longitude_deg=round(360.0 * h01(sid, "belt"), 2))) if where in I.reservoirs else \
                    dict(orbit=dict(altitude_km=round((0.5 + 2.0 * h01(sid, "alt")) * I.bodies[where]["physical"]["radius_km"])))
                sites.append(base_site(I, sid, f"{e['name']}: {I.WS['site_types'][typ]['ru']} {i + 1}", typ, system, k, e["name"], staff, purpose,
                                       body=where, placement=pl, aggregates=dict(asset_count=count, sites=m), source="economy.infrastructure"))
        # quarantine hub for every large polity
        if e["population"] >= 1e8 and big:
            sid = f"site.{system:03d}.{k}.quarantine01"
            sites.append(base_site(I, sid, f"{e['name']}: карантинная станция", "quarantine_hub", system, k, e["name"], 900,
                                   "изоляция прибывающих, карантин кораблей, госпиталь инфекций", body=big[0],
                                   placement=dict(orbit=dict(altitude_km=round(3.0 * I.bodies[big[0]]["physical"]["radius_km"]))), source="generated"))
    # free ports where an open or clan regime sits on a busy belt
    for k in sorted(I.pol):
        fam = I.law_family(k)
        sysid = I.system_of(k)
        if fam in ("clan_law", "city_state_arbitration") and belt_of(I, sysid):
            sid = f"site.{sysid:03d}.{k}.free_port01"
            sites.append(base_site(I, sid, f"вольный порт {I.pol[k]['name']}", "free_port", sysid, k, "держатели причалов", 3000,
                                   "вольный порт: ремонт без вопросов, рынок, перебивка документов", body=belt_of(I, sysid),
                                   placement=dict(belt=dict(radius_au=r3(I.reservoirs[belt_of(I, sysid)].get("peak_au") or 3.0), longitude_deg=round(360 * h01(sid), 2))),
                                   legal="semi_legal", source="generated"))
    # outposts and frontier pools of world.json are real places too
    for o in I.W["outposts"] + I.W.get("frontier_pools", []):
        sysid = o["system"]
        big = biggest_bodies(I, sysid, 1)
        if not big:
            continue
        sid = f"site.{sysid:03d}.{o['id']}"
        if sid in used:
            continue
        typ = "mining_hub" if any(w in (o.get("purpose") or "") for w in ("He-3", "металл", "руда", "заявк")) else "science_hub" if "наук" in (o.get("purpose") or "") else "belt_hub"
        sites.append(base_site(I, sid, o["name"], typ, sysid, o["owners"][0] if o.get("owners") else None, ", ".join(o.get("owners", [])),
                               o.get("pop_3126_authored", 1000), o.get("purpose"), body=big[0],
                               placement=dict(orbit=dict(altitude_km=round(1.5 * I.bodies[big[0]]["physical"]["radius_km"]))), founded=o.get("founded"),
                               legal="contested" if o.get("contested") else "lawful", co_owners=o.get("owners", [])[1:], source="world.json outposts"))
        used.add(sid)
    # beacon relays as deep-space infrastructure records (for comms) are in comms; abandoned hubs: declining outposts
    for s in sites:
        s["language"] = I.language(s["polity"]) if s["polity"] else I.WS["languages"]["network_lingua_franca"]
    return sites


# Earth in the maps is the real Earth (texture and coastlines), so its agglomerations stand where
# today's great urban regions stand (coasts, deltas, rivers did not move). Working names are the
# historical regions; the names of 3126 are an author decision. (lat, lon east-positive)
EARTH_REGIONS = [
    ("Токио", 35.68, 139.69), ("Дели", 28.61, 77.21), ("Шанхай", 31.23, 121.47), ("Сан-Паулу", -23.55, -46.63), ("Мехико", 19.43, -99.13),
    ("Каир", 30.04, 31.24), ("Мумбаи", 19.08, 72.88), ("Пекин", 39.90, 116.40), ("Дакка", 23.81, 90.41), ("Осака", 34.69, 135.50),
    ("Нью-Йорк", 40.71, -74.01), ("Карачи", 24.86, 67.01), ("Буэнос-Айрес", -34.60, -58.38), ("Чунцин", 29.56, 106.55), ("Стамбул", 41.01, 28.98),
    ("Калькутта", 22.57, 88.36), ("Манила", 14.60, 120.98), ("Лагос", 6.52, 3.38), ("Рио-де-Жанейро", -22.91, -43.17), ("Тяньцзинь", 39.34, 117.36),
    ("Киншаса", -4.44, 15.27), ("Гуанчжоу", 23.13, 113.26), ("Лос-Анджелес", 34.05, -118.24), ("Москва", 55.76, 37.62), ("Шэньчжэнь", 22.54, 114.06),
    ("Лахор", 31.55, 74.34), ("Бангалор", 12.97, 77.59), ("Париж", 48.86, 2.35), ("Богота", 4.71, -74.07), ("Джакарта", -6.21, 106.85),
    ("Ченнаи", 13.08, 80.27), ("Лима", -12.05, -77.04), ("Бангкок", 13.76, 100.50), ("Сеул", 37.57, 126.98), ("Нагоя", 35.18, 136.91),
    ("Хайдарабад", 17.39, 78.49), ("Лондон", 51.51, -0.13), ("Тегеран", 35.69, 51.39), ("Чикаго", 41.88, -87.63), ("Чэнду", 30.57, 104.07),
    ("Нанкин", 32.06, 118.80), ("Ухань", 30.59, 114.31), ("Хошимин", 10.82, 106.63), ("Луанда", -8.84, 13.23), ("Ахмадабад", 23.02, 72.57),
    ("Куала-Лумпур", 3.14, 101.69), ("Сиань", 34.34, 108.94), ("Гонконг", 22.32, 114.17), ("Ханчжоу", 30.27, 120.16), ("Шэньян", 41.81, 123.43),
    ("Эр-Рияд", 24.71, 46.68), ("Багдад", 33.31, 44.36), ("Сантьяго", -33.45, -70.67), ("Сурат", 21.17, 72.83), ("Мадрид", 40.42, -3.70),
    ("Пуна", 18.52, 73.86), ("Харбин", 45.80, 126.53), ("Хьюстон", 29.76, -95.37), ("Торонто", 43.65, -79.38), ("Дар-эс-Салам", -6.79, 39.21),
]


def earth_settlements(I):
    """Earth surface population as persistent agglomerations (Zipf) at the real great urban regions
    (the map Earth is the real Earth); the rest are towns, counted only."""
    e = I.pol[SOL]["settlement_structure"]
    surface = sum(v for k, v in e.items() if "Земля" in k)
    n = len(EARTH_REGIONS)
    w = [1.0 / (i + 1) ** 1.05 for i in range(n)]
    big = 0.55 * surface   # 55 % of Earth lives in the 60 largest agglomerations, the rest in towns (counted only)
    rows = []
    for i, (nm, lat, lon) in enumerate(EARTH_REGIONS):
        sid = f"city.000.earth.{i + 1:03d}"
        pop = big * w[i] / sum(w)
        rows.append(dict(id=sid, name=f"агломерация «{nm}»", body="system_0.Sol.Земля", population=int(pop), role="megacity" if pop > 5e7 else "city",
                         surface=dict(lat_deg=lat, lon_deg=lon, elevation_ref="terrain_surface", placement_status="real_earth_urban_region",
                                      seed=hashlib.md5(sid.encode("utf-8")).hexdigest()[:16], generator_version=1),
                         spaceport=pop > 2e7, night_light=dict(source="population", pop=int(pop), radius_km=round(8.0 * (pop / 1e6) ** 0.45, 1)),
                         naming="PROPOSAL (историческое название региона; имя 3126 — решение автора)", status=TAG))
    rows.append(dict(id="city.000.earth.rest", name="прочие поселения Земли", body="system_0.Sol.Земля", population=int(surface - big),
                     role="aggregate", status=TAG))
    return rows


# ---------------------------------------------------------------- comms: beacons as a network
def comms_model(I, sites, dsos, g):
    C = I.WS["comms"]
    trips = {}
    for f in I.X["trade_flows"]["flows"]:
        for a, b in zip(f["path"], f["path"][1:]):
            e = tuple(sorted((a, b)))
            trips[e] = trips.get(e, 0.0) + 2.0 * f["loaded_trips_y"]
    links = []
    lg_ = {}
    for a in sorted(g):
        for b in sorted(g[a]):
            if a >= b:
                continue
            r = g[a][b]
            klass = r["klass"]
            fmin = C["link_min_departures_per_day"][klass if not r["unbeaconed"] else "feeder"] * (0.5 if r["unbeaconed"] else 1.0)
            f = max(fmin, trips.get((a, b), 0.0) / 365.0)
            hop_h = 24.0 / (2.0 * f) + C["mail_drone_charge_h"] + C["relay_hop_processing_h"]
            links.append(dict(a=a, b=b, ly=r["ly"], klass=klass, departures_per_day=r3(f), routine_hop_h=round(hop_h, 2),
                              flash_hop_h=round(C["mail_drone_charge_h"] + C["relay_hop_processing_h"], 2), unbeaconed=r["unbeaconed"]))
            lg_.setdefault(a, {})[b] = hop_h
            lg_.setdefault(b, {})[a] = hop_h
    # frontier chains join the graph beyond the network edge (deep-space objects are string ids)
    dso = {d["id"]: d for d in dsos}
    up_of = {x["id"]: x.get("beacon", {}).get("uptime", 1.0) for x in I.WS["deep_space_objects"]}
    frontier_nodes = set()
    for fid in FRONTIER_CHAINS:
        ch = [n[1] for n in ACCESS[fid]]
        for a, b in zip(ch, ch[1:]):
            if b in lg_.get(a, {}):
                continue
            up = min(up_of.get(a, 1.0), up_of.get(b, 1.0))
            f = C["link_min_departures_per_day"]["frontier"]
            hop_h = (24.0 / (2.0 * f) + C["mail_drone_charge_h"] + C["relay_hop_processing_h"]) * (2.0 - up)
            lg_.setdefault(a, {})[b] = hop_h
            lg_.setdefault(b, {})[a] = hop_h
            pa = I.pos[a] if isinstance(a, int) else dso[a]["position_ly"]
            pb = I.pos[b] if isinstance(b, int) else dso[b]["position_ly"]
            links.append(dict(a=a, b=b, ly=r3(vlen(vsub(pa, pb))), klass="frontier", departures_per_day=f, routine_hop_h=round(hop_h, 2),
                              flash_hop_h=round((C["mail_drone_charge_h"] + C["relay_hop_processing_h"]) * (2.0 - up), 2), unbeaconed=False,
                              relay_uptime=up))
        frontier_nodes |= set(ch[1:]) - set(g)
    refuse = {c["system"] for c in I.W["colonies"] if c["demography"].get("refuses_network")}
    nodes = sorted(lg_, key=lambda x: (isinstance(x, str), str(x)))

    def delays(src, factor):
        dist = {src: 0.0}
        pq = [(0.0, str(src), src)]
        while pq:
            d, _, u = heapq.heappop(pq)
            if d > dist.get(u, 1e18):
                continue
            for v, w in sorted(lg_.get(u, {}).items(), key=lambda kv: str(kv[0])):
                if v in refuse:
                    continue
                nd = d + w * factor
                if nd < dist.get(v, 1e18) - 1e-9:
                    dist[v] = nd
                    heapq.heappush(pq, (nd, str(v), v))
        return dist
    from_sol = delays(0, C["priority_factor"]["routine"])
    worst = {}
    for s in nodes:
        if s in refuse or s in frontier_nodes:
            continue
        dd = delays(s, 1.0)
        worst[str(s)] = max(v for k, v in dd.items() if k not in frontier_nodes) / 24.0
    diameter = max(worst.values())
    frontier_from_sol = {str(k): r3(v / 24.0) for k, v in sorted(from_sol.items(), key=lambda kv: str(kv[0])) if k in frontier_nodes}
    # in-system light lag from the bridge / hub beacon to every site
    site_rows = []
    for s in sites:
        lag_s = 0.0
        cov = "full"
        if s["system"] is None:
            cov = "own_relay" if s.get("beacon") else "none"
        else:
            beac = I.X["beacons"]["by_system"]
            row = next((b for b in beac if b["system"] == s["system"]), None)
            if s["system"] in refuse or row is None or row["total"] == 0:
                cov = "none"
            elif s["type"] in ("pirate_enclave", "abandoned_hub") or s.get("legal") == "illegal":
                cov = "partial"
            a = site_au(I, s)
            lag_s = (a * AU_KM) / C_KM_S if a is not None else 0.0
        site_rows.append(dict(site=s["id"], coverage=cov, light_lag_s=round(lag_s, 1)))
    examples = []
    for tgt in (9, 8, 25, 24, 45, 37, "dso.lotus", 19, 36, 20):
        if tgt in from_sol:
            examples.append(dict(to=tgt, name=I.E[tgt]["name"] if isinstance(tgt, int) else tgt, routine_days=r3(from_sol[tgt] / 24.0),
                                 flash_days=r3(delays(0, 0.0)[tgt] / 24.0 if False else sum_flash(lg_, 0, tgt, C) / 24.0)))
    return dict(links=links, from_sol_routine_h={str(k): round(v, 2) for k, v in sorted(from_sol.items(), key=lambda kv: str(kv[0]))},
                eccentricity_days=dict(sorted(worst.items())), diameter_days=r3(diameter), author_target_days=C["author_target_days_to_far_periphery"],
                frontier_nodes=sorted(str(x) for x in frontier_nodes), frontier_from_sol_days=frontier_from_sol, frontier_note=C["frontier_note"],
                disconnected=sorted(refuse), sites=site_rows, examples=examples,
                node_fields=["coverage_ls", "capacity_msgs_per_h", "operator", "trust_authority", "relay_links", "message_queue", "clock_sync",
                             "navigation_ephemeris", "news_cache", "legal_bulletin_cache", "banking_sync", "traffic_control", "emergency_channel"],
                nodes=beacon_nodes(I, dsos), offline_rule="вне покрытия: местное радио, корабль-корабль, станция, бедствие, направленная передача, кэш; при возврате — синхронизация сообщений, новостей, банка, ордеров, рынков, навигации, правового состояния",
                status=TAG)


def sum_flash(lg_, src, dst, C):
    # flash: dedicated courier leaves at once on every hop -> only charge + processing per hop
    flash = C["mail_drone_charge_h"] + C["relay_hop_processing_h"]
    dist = {src: 0.0}
    pq = [(0.0, str(src), src)]
    while pq:
        d, _, u = heapq.heappop(pq)
        if u == dst:
            return d
        if d > dist.get(u, 1e18):
            continue
        for v, w in sorted(lg_.get(u, {}).items(), key=lambda kv: str(kv[0])):
            step = flash if w < 1e6 else w
            nd = d + step
            if nd < dist.get(v, 1e18) - 1e-9:
                dist[v] = nd
                heapq.heappush(pq, (nd, str(v), v))
    return float("nan")


def beacon_nodes(I, dsos):
    C = I.WS["comms"]
    rows = []
    for b in I.X["beacons"]["bridge"]:
        rows.append(dict(id=b["id"], system=b["system"], kind="bridge", operator=b["operator"], coverage_ls=C["coverage_ls"]["bridge"],
                         capacity_msgs_per_h=20000, trust_authority="Управление Маяков" if b["operator"] != "sol_authority" else "Власть Сола + Управление Маяков",
                         relay_links="все межзвёздные рёбра системы", clock_sync=True, navigation_ephemeris=True, news_cache=True, legal_bulletin_cache=True,
                         banking_sync=True, traffic_control=True, emergency_channel=True, status=TAG))
    for d in I.WS["deep_space_objects"]:
        if d.get("beacon"):
            rows.append(dict(id=f"BCN-{d['id']}", system=None, deep_space=d["id"], kind=d["beacon"]["kind"], operator=d.get("operator"),
                             coverage_ls=C["coverage_ls"].get(d["beacon"]["kind"], 30), capacity_msgs_per_h=600, uptime=d["beacon"]["uptime"],
                             trust_authority="оператор хаба (без подписи Управления)" if d.get("polity") is None else "Управление Маяков",
                             relay_links="два ребра доступа", clock_sync=True, navigation_ephemeris=True, news_cache=True, legal_bulletin_cache=d.get("polity") is not None,
                             banking_sync=d.get("polity") is not None, traffic_control=False, emergency_channel=True, status=TAG))
    for s in I.WS["frontier_sites"]:
        if s.get("beacon"):
            rows.append(dict(id=f"BCN-{s['id']}", system=s["system"], kind=s["beacon"]["kind"], operator=s.get("operator"), coverage_ls=C["coverage_ls"][s["beacon"]["kind"]],
                             capacity_msgs_per_h=2000, uptime=s["beacon"]["uptime"], trust_authority="лицензия Керна", relay_links="через «Лотос»",
                             clock_sync=True, navigation_ephemeris=True, news_cache=True, legal_bulletin_cache=True, banking_sync=True, traffic_control=True,
                             emergency_channel=True, status=TAG))
    return rows


def site_au(I, s):
    bid = s.get("body")
    if not bid:
        return None
    if bid in I.reservoirs:
        r = I.reservoirs[bid]
        return r.get("peak_au") or (r.get("inner_au", 0) + r.get("outer_au", 0)) / 2.0
    b = I.bodies.get(bid)
    if not b:
        return None
    a = b["orbit"]["semi_major_axis_au"]
    par = b.get("parent_id")
    if par in I.bodies:
        a = I.bodies[par]["orbit"]["semi_major_axis_au"]
    return a


# ---------------------------------------------------------------- social state, administrators
def social_state(I, s, comms_site, piracy_sys, eco):
    k = s["polity"]
    fam = I.law_family(k)
    gdp_pc = eco[k]["gdp_per_capita_cr"] if k in eco else 0.0
    rich = max(v["gdp_per_capita_cr"] for v in eco.values())
    prosperity = clamp(1.15 * gdp_pc / rich) if k else 0.25
    if s["type"] in ("pirate_enclave", "abandoned_hub"):
        prosperity *= 0.6
    activity = clamp(math.log10(1 + s["population"]) / 7.5 + (0.25 if s["type"] in ("trade_hub", "free_port", "orbital_hub") else 0.0))
    cov = comms_site["coverage"]
    isolation = clamp((0.15 if s["system"] is not None else 0.55) + (0.0 if cov == "full" else 0.2 if cov in ("partial", "own_relay") else 0.4)
                      + (0.25 if s["system"] in (19, 36, 20, 24, 37, 41, 45) else 0.0))
    inst = INSTITUTIONS[fam] * (1.0 - 0.5 * isolation)
    growth = eco[k]["growth_per_year"] if k in eco else 0.0
    boom = clamp(growth * 25.0) * (1.0 if s["population"] < 5e6 or s["type"] in ("mining_hub", "free_port", "trade_hub") else 0.6)
    pir = piracy_sys.get(s["system"], 0.0) if s["system"] is not None else 0.6
    corruption = clamp(CORRUPTION_BASE[fam] + 0.3 * isolation * (1 - INSTITUTIONS[fam]) + (0.25 if s["type"] in ("free_port", "pirate_enclave") else 0.0))
    crime = clamp(0.1 + 0.6 * pir + 0.3 * corruption + (0.3 if s["type"] in ("free_port", "pirate_enclave") else 0.0) - 0.2 * inst)
    cohesion = clamp(0.4 + 0.3 * INSTITUTIONS[fam] + (0.15 if fam in ("theocratic", "confucian_administrative") else 0.0) - 0.3 * boom - 0.2 * crime)
    trust = clamp(0.2 + 0.5 * inst - 0.5 * corruption + 0.2 * cohesion)
    hostile = [g for (a, b), g in I.rel.items() if k in (a, b) and g >= 30]
    volatility = clamp(0.1 + (max(hostile) / 100.0 if hostile else 0.0) * 0.6 + 0.3 * boom + 0.2 * (1 - cohesion))
    imp = eco[k].get("imports_cr_y", 0.0) / max(1.0, eco[k].get("gdp_cr_y", 1.0)) if k in eco else 0.5
    supply = clamp(0.85 - 2.0 * imp - 0.35 * isolation + (0.1 if (s.get("stocks") or {}).get("food_days", 60) > 180 else 0.0))
    beds = s["medical_beds"] / max(1.0, s["population"] / 1000.0)
    medical = clamp(0.2 + 0.08 * beds + 0.3 * prosperity - 0.2 * isolation)
    security = clamp(0.2 + 0.5 * pir + (max(hostile) / 100.0 if hostile else 0.0) * 0.4 + (0.3 if s["system"] in (19,) else 0.0))
    m = dict(prosperity=prosperity, activity=activity, isolation=isolation, institutional_capacity=inst, social_cohesion=cohesion, trust=trust,
             crime=crime, corruption=corruption, political_volatility=volatility, supply_security=supply, medical_resilience=medical, security_pressure=security)
    return {a: r3(b) for a, b in m.items()}


def archetype(s, m):
    if s["type"] == "abandoned_hub" or (s["population"] < 200 and m["activity"] < 0.3):
        return "half_abandoned"
    if s["type"] in ("military_hub", "quarantine_hub") or (m["institutional_capacity"] > 0.7 and m["corruption"] < 0.2 and s["type"] == "science_hub"):
        return "closed_disciplined"
    if s["type"] == "pirate_enclave":
        return "criminal_cesspit"
    if s["type"] in ("free_port", "derelict_hub") and m["corruption"] > 0.45:
        return "corrupt_free_port"
    if m["crime"] > 0.8:
        return "criminal_cesspit"
    if (m["political_volatility"] > 0.55 and m["activity"] > 0.45 and m["social_cohesion"] < 0.45) or             (s["system"] in FRONTIER_SYSTEMS and m["activity"] > 0.5):
        return "nervous_boomtown"
    if m["corruption"] > 0.5 and m["activity"] > 0.5:
        return "corrupt_free_port"
    if m["prosperity"] > 0.75 and m["activity"] > 0.7:
        return "rich_hub"
    if m["prosperity"] < 0.45:
        return "poor_stagnant"
    if m["activity"] < 0.45:
        return "sleepy_periphery"
    return "calm_provided"


def shock_reactions(m):
    """How population and administration react: response + severity multiplier (1 = nominal)."""
    out = {}
    order = m["institutional_capacity"] * 0.5 + m["social_cohesion"] * 0.3 + m["trust"] * 0.2
    buffer = m["supply_security"] * 0.6 + m["prosperity"] * 0.4
    for sh in ("accident", "epidemic", "shortage", "war", "sanctions", "refugee_influx", "pirates", "mass_migration", "quarantine",
               "beacon_loss", "route_loss", "infrastructure_destruction"):
        base = {"epidemic": 1.0 - 0.6 * m["medical_resilience"], "shortage": 1.0 - 0.7 * buffer, "sanctions": 1.0 - 0.6 * buffer,
                "quarantine": 1.0 - 0.5 * m["trust"], "refugee_influx": 1.0 - 0.4 * m["prosperity"], "pirates": 1.0 - 0.5 * m["institutional_capacity"],
                "beacon_loss": 0.5 + 0.6 * m["isolation"], "route_loss": 0.5 + 0.5 * m["isolation"] + 0.3 * (1 - m["supply_security"])}.get(sh, 1.0 - 0.4 * order)
        sev = r3(clamp(base * (1.4 - order), 0.2, 2.0))
        if order > 0.65:
            resp = "дисциплина" if m["institutional_capacity"] > 0.75 and m["trust"] < 0.45 else "организованный ответ"
        elif m["social_cohesion"] > 0.55:
            resp = "взаимопомощь"
        elif m["crime"] > 0.55 or m["corruption"] > 0.55:
            resp = "мародёрство и спекуляция"
        elif m["political_volatility"] > 0.5:
            resp = "волнения"
        else:
            resp = "паника и запасание"
        if sh in ("quarantine", "epidemic") and m["trust"] < 0.35:
            resp = "карантин только на бумаге"
        out[sh] = dict(response=resp, severity=sev)
    return out


DESCRIPTION = {
    "rich_hub": "Плотный трафик, деньги и сервис; всё есть, всё дорого.",
    "nervous_boomtown": "Быстрый рост, приезжие, слабые связи; деньги и драки.",
    "poor_stagnant": "Нехватка, старые корабли, люди уезжают.",
    "calm_provided": "Спокойно и обеспечено, работают институты.",
    "sleepy_periphery": "Мало событий, мало кораблей, всё по расписанию.",
    "half_abandoned": "Пустые отсеки, автоматика, редкие люди.",
    "corrupt_free_port": "Можно всё — за деньги; закон договорной.",
    "criminal_cesspit": "Силу решает оружие; заходить с охраной.",
    "closed_disciplined": "Пропуска, режим, порядок; чужих не любят.",
}


def administrators(I, sites, social):
    A = I.WS["administrators"]
    out = []
    for s in sites:
        m = social[s["id"]]["metrics"]
        if m["isolation"] < A["remoteness_threshold"] or s["type"] in ("abandoned_hub",) or s.get("hidden"):
            continue
        fam = I.law_family(s["polity"])
        pool = A["regime_backgrounds"].get(fam, ["local_notable"])
        bg_id = pool[int(h01(s["id"], "bg") * len(pool))]
        bg = A["backgrounds"][bg_id]
        inst = INSTITUTIONS[fam]
        traits = {}
        for t in ("competence", "loyalty", "corruption", "ambition", "risk_tolerance", "empathy", "ideology", "connections", "bribery"):
            sel = {"competence": 0.15, "loyalty": 0.1, "corruption": -0.15, "bribery": -0.15}.get(t, 0.0) * (inst - 0.5)
            traits[t] = r3(clamp(bg[t] + sel + 0.24 * (h01(s["id"], t) - 0.5)))
        traits["cruelty_empathy"] = traits.pop("empathy")
        traits["ideological_commitment"] = traits.pop("ideology")
        traits["susceptibility_to_bribery"] = traits.pop("bribery")
        delay_d = social[s["id"]]["comms_delay_days"]
        f = dict(institution_strength=r3(inst), remoteness=m["isolation"], comms_quality=r3(clamp(1.0 - 0.15 * delay_d)), order_delay_days=delay_d,
                 inspection_per_year=r3(max(0.2, 4.0 * inst * (1 - m["isolation"]))), force_presence=r3(clamp(s["security_base"] * inst)),
                 local_elite_dependence=r3(clamp(0.2 + 0.5 * m["isolation"])), economic_autonomy=r3(clamp(0.3 + 0.5 * m["isolation"] - 0.2 * m["supply_security"])))
        freedom = clamp(0.25 + 0.45 * f["remoteness"] + 0.25 * f["economic_autonomy"] - 0.35 * f["institution_strength"] * f["comms_quality"]
                        - 0.15 * f["force_presence"] + 0.1 * f["order_delay_days"] / 7.0)
        drift = clamp(traits["corruption"] * freedom * (0.6 + m["crime"]))
        verdict = "превращает пост в контрабандный узел" if drift > 0.35 else "карьерист, ищет выгоду" if traits["ambition"] > 0.65 and drift > 0.18 else "служит делу"
        lang = s["language"]
        names = A["given_names"].get(lang) or A["given_names"]["en"]
        nm = names[int(h01(s["id"], "name") * len(names))]
        appointed = YEAR - 1 - int(8 * h01(s["id"], "year"))
        out.append(dict(id=f"adm.{s['id']}", site=s["id"], name=nm, background=bg_id, background_ru=bg["ru"], appointed=appointed, traits=traits,
                        oversight=f, effective_freedom=r3(freedom), drift=r3(drift), verdict=verdict,
                        origin=f"{bg['ru'].capitalize()}, назначен в {appointed}: так обычно отбирают руководителей в системе «{I.WS['law_profiles'][fam]['ru']}». "
                               f"Свобода рук {freedom:.2f} — удалённость {f['remoteness']:.2f}, приказы идут {delay_d:.1f} сут, проверок {f['inspection_per_year']:.1f}/год.",
                        status=TAG))
    return out


# ---------------------------------------------------------------- law
def law_model(I, sites, comms):
    WS = I.WS
    rows = []
    for s in sites:
        k = s["polity"]
        fam = I.law_family(k)
        L = WS["law_profiles"][fam]
        if s["system"] is None:
            sovereign = k
            juris = "лицензия" if k else "конкурирующие претензии / договор держателей"
        else:
            sovereign = k
            juris = {"illegal": "вне закона (де-факто сила)", "semi_legal": "договор держателей при формальном суверенитете", "contested": "оспаривается"}.get(s.get("legal"), "государственная")
        claims = []
        if s.get("legal") in ("contested", "licensed_and_contested") or s["system"] is None:
            if s["id"] in ("dso.lotus", "site.019.mandala"):
                claims = ["cyg61_kern (находка, лицензия)", "tau_ceti_cognium (наука, покупка артефактов)", "Путь Бесконечного Света (наследник спонсора NIRVANA-1)"]
            elif s["system"] == 19:
                claims = ["cyg61_kern", "tau_ceti_cognium", "gj832_tianhe"]
            elif s.get("co_owners"):
                claims = [k] + s["co_owners"]
        rows.append(dict(site=s["id"], sovereign_polity=sovereign, local_jurisdiction=juris, applicable_law=fam, applicable_law_ru=L["ru"],
                         enforcement=L["police"], court=L["courts"], customs=L["controlled_goods"], immigration="визы" if fam not in ("clan_law", "none") else "нет контроля",
                         quarantine_authority=L["police"] if fam != "none" else "никто", salvage_finder_share=L["salvage_finder_share"], registration=L["registration"],
                         weapons=L["weapons"], controlled_goods=L["controlled_goods"], docking="разрешение диспетчера" if fam != "none" else "договор с причалом",
                         search_seizure=L["search_seizure"], extradition=L["extradition"], private_security=L["private_security"], competing_claims=claims, status=TAG))
    # offence matrix: what each legal family does about each offence
    sev = {}
    for fam, L in sorted(WS["law_profiles"].items()):
        strict = {"very_strict": 1.0, "strict": 0.8, "taxed": 0.4, "free": 0.0}.get(L["controlled_goods"], 0.5)
        row = {}
        for o in WS["space_law"]["offences"]:
            if fam == "none":
                row[o] = "нет наказания (месть, долг)"
                continue
            if o in ("murder", "piracy"):
                row[o] = "смертная казнь" if L["death_penalty"] else "тюрьма, конфискация"
            elif o in ("beacon_sabotage", "navigation_interference"):
                row[o] = "тюрьма, конфискация корабля (Конвенция)"
            elif o in ("contraband", "smuggling", "customs_evasion"):
                row[o] = "конфискация груза + штраф" if strict >= 0.8 else "штраф/пошлина" if strict > 0 else "нет"
            elif o in ("false_transponder", "forged_registration"):
                row[o] = "арест корабля" if L["registration"] in ("strict",) else "штраф"
            elif o == "unlawful_salvage":
                row[o] = "возврат + штраф" if L["salvage_finder_share"] < 0.5 else "спор в арбитраже"
            elif o == "quarantine_violation":
                row[o] = "тюрьма" if fam in ("confucian_administrative", "party_administrative", "military_administration", "theocratic") else "штраф, принудительная изоляция"
            elif o == "restricted_weapons":
                row[o] = {"forbidden": "тюрьма", "military_only": "трибунал", "licensed": "конфискация + штраф", "allowed": "нет"}.get(L["weapons"], "штраф")
            elif o == "illegal_docking":
                row[o] = "штраф, арест на стоянке"
            else:
                row[o] = "тюрьма" if o in ("assault", "theft") else "штраф"
        sev[fam] = row
    # warrants travel at priority speed over the comms graph; recognition by treaty
    treaty = sorted([f for f, L in WS["law_profiles"].items() if L["extradition"] == "treaty"])
    ship_status_fields = ["registered_owner", "operator", "registry_polity", "transponder_identity", "insurance", "liens", "warrants", "impound_status",
                          "salvage_claim", "stolen_status", "state_immunity"]
    return dict(sites=rows, offence_matrix=sev, treaty_extradition_families=treaty, ship_legal_status_fields=ship_status_fields,
                space_law=WS["space_law"], warrant_rule="ордер приходит по сети маяков с приоритетом 'priority' (см. comms) и действует там, где семья права признаёт экстрадицию",
                status=TAG)


# ---------------------------------------------------------------- passengers
def passengers(I, g):
    P = I.WS["passengers"]
    eco = I.pol
    keys = sorted(k for k in eco if eco[k]["population"] > 0)
    pop = {k: eco[k]["population"] for k in keys}
    sysof = {k: I.system_of(k) for k in keys}
    refuse = {c["id"] for c in I.W["colonies"] if c["demography"].get("refuses_network")}
    jumps = {}
    for k in keys:
        dist, prev = dijkstra(g, sysof[k], lambda u, v, e: 1.0)
        jumps[k] = dist
    rate = sum(P["trips_per_capita_y_interstellar"].values())
    od = {}
    for i in keys:
        if i in refuse:
            continue
        w = {}
        for j in keys:
            if j == i or j in refuse:
                continue
            nj = 0 if sysof[i] == sysof[j] else jumps[i].get(sysof[j])
            if nj is None:
                continue
            host = I.rel.get(tuple(sorted((i, j))), 0)
            f = (pop[j] ** 0.8) / (1.0 + nj) ** 2 * (0.05 if host >= 60 else 1.0) * (1.5 if I.language(i) == I.language(j) else 1.0)
            w[j] = f
        tot = sum(w.values())
        r = rate + (P["trips_per_capita_y_interstellar"]["pilgrimage"] * 2 if I.regime(i) in P["pilgrimage_regimes"] else 0.0)
        for j, f in w.items():
            od[(i, j)] = od.get((i, j), 0.0) + pop[i] * r * f / tot
    # permanent migration 3126: Earth visa window + network migration (gravity), refugees from hostile pairs
    win = [x for x in lab.load("population_history.json")["earth_programme_windows"] if x["from"] <= YEAR < x["to"]]
    earth_out = win[0]["per_year"] if win else 0.0
    mig = {}
    recruit = {k: pop[k] for k in keys if k not in refuse and k != SOL and pop[k] > 5e7}
    for k, v in recruit.items():
        mig[(SOL, k)] = earth_out * v / sum(recruit.values())
    net_m = P["network_migration_per_1000_y"] / 1000.0
    for (i, j), v in od.items():
        if i != SOL:
            mig[(i, j)] = mig.get((i, j), 0.0) + pop[i] * net_m * v / max(1.0, sum(x for (a, _), x in od.items() if a == i))
    refugees = {}
    for (a, b), gv in sorted(I.rel.items()):
        if gv >= 60 and a in pop and b in pop:
            n = P["refugees_per_grievance_point_per_million"] * gv * min(pop[a], pop[b]) / 1e6
            refugees[(a, b)] = n
    mil = {k: I.X["defence"]["polities"][k]["personnel"] * P["military_rotation_per_person_y"] * 0.2 for k in keys}
    # fleet: liners for big links, colonist transports for migration + refugees, Cobretts with P20 for small links
    SC = I.SE["ship_classes"]
    links = {}
    for (i, j), v in od.items():
        e = tuple(sorted((i, j)))
        links.setdefault(e, dict(trips=0.0, migrants=0.0))["trips"] += v
    for (i, j), v in mig.items():
        e = tuple(sorted((i, j)))
        links.setdefault(e, dict(trips=0.0, migrants=0.0))["migrants"] += v
    for (a, b), v in refugees.items():
        links.setdefault((a, b), dict(trips=0.0, migrants=0.0))["migrants"] += v
    fleet = {"liner": 0.0, "colonist_transport": 0.0, "cobrett_i_n1_pax": 0.0, "surface_lifter": 0.0}
    rows = []
    lf = P["load_factor_target"]
    for e in sorted(links):
        a, b = e
        nj = 0 if sysof[a] == sysof[b] else jumps[a].get(sysof[b], 1.0)
        hops = hop_lengths(g, sysof[a], sysof[b])
        L = links[e]
        if L["trips"] >= P["small_link_threshold_pax_y"]:
            cls, seats = "liner", SC["liner"]["passengers"]
        else:
            cls, seats = "cobrett_i_n1", I.SE["cargo_model"]["passenger_module_p20"]["seats_economy"] * SC["cobrett_i_n1"]["container_slots"]
        load_t = seats * SC.get(cls, {}).get("pax_mass_t", 0.12) if cls == "liner" else seats * 0.12 + SC[cls]["container_slots"] * 9.0
        tc = lg.trip_cost(I.SE, I.cal, cls, hops or [5.0], min(load_t, SC[cls]["cargo_t"] + 135.0) if cls == "liner" else min(load_t, SC[cls]["cargo_t"]), 0.0)
        rt_days = 2.0 * tc["trip_days"] + 2.0
        trips_y = SC[cls]["utilisation"] * 365.0 / rt_days
        pax_y = L["trips"]          # one-way trips both directions counted in od
        ships = pax_y / (seats * lf * trips_y * 2.0)
        fleet["liner" if cls == "liner" else "cobrett_i_n1_pax"] += ships
        dep_y = pax_y / (seats * lf)
        price_economy = tc["total_cr"] / (seats * lf) * 1.15
        ct = SC["colonist_transport"]
        mt = lg.trip_cost(I.SE, I.cal, "colonist_transport", hops or [5.0], ct["passengers"] * ct["pax_mass_t"], 0.0)
        m_ships = L["migrants"] / (ct["passengers"] * lf * ct["utilisation"] * 365.0 / (2.0 * mt["trip_days"] + 4.0))
        fleet["colonist_transport"] += m_ships
        rows.append(dict(link=list(e), jumps=nj, pax_y=round(pax_y), migrants_y=round(L["migrants"]), ship_class=cls, ships=r3(ships),
                         colonist_transports=r3(m_ships), departures_y=round(dep_y), headway_days=r3(365.0 / max(dep_y, 0.1)),
                         wait_days_mean=r3(365.0 / max(dep_y, 0.1) / 2.0),
                         fares_cr={c: round(price_economy * f) for c, f in P["class_price_factor"].items()},
                         migrant_fare_cr=round(mt["total_cr"] / (ct["passengers"] * lf) * 1.1)))
    # surface <-> orbit: everyone on a planet surface or in a port above it
    surf = 0.0
    for k in keys:
        st = eco[k]["settlement_structure"]
        surf += sum(v for a, v in st.items() if "поверхность" in a or "подзем" in a or "Земля" in a)
    sl = SC["surface_lifter"]
    lifter_trips_y = sl["utilisation"] * 365.0 * 6.0
    fleet["surface_lifter"] = surf * P["trips_per_capita_y_surface_orbit"] * 2.0 / (sl["passengers"] * lf * lifter_trips_y)
    reserve = {c: v * 0.12 for c, v in fleet.items()}
    capacity_mig = fleet["colonist_transport"] * SC["colonist_transport"]["passengers"] * SC["colonist_transport"]["utilisation"] * 365.0 / 20.0
    total_mig = sum(mig.values()) + sum(refugees.values())
    # evacuation scenario: 1.2 M people from the New Shanghai ring disaster with the world reserve + requisition
    ev_people = 1.2e6
    ev_cap_day = (reserve["colonist_transport"] * SC["colonist_transport"]["passengers"] + 0.25 * fleet["liner"] * SC["liner"]["passengers"]) / 3.0
    ev_days = ev_people / max(1.0, ev_cap_day)
    return dict(od_top=sorted(rows, key=lambda r: -r["pax_y"])[:30], links=len(rows),
                interstellar_trips_y=round(sum(r["pax_y"] for r in rows)), migrants_y=round(sum(mig.values())), refugees_y=round(sum(refugees.values())),
                earth_visa_window_y=earth_out, military_rotation_y=round(sum(mil.values())), surface_orbit_trips_y=round(surf * P["trips_per_capita_y_surface_orbit"] * 2.0),
                fleet={c: round(v, 1) for c, v in fleet.items()}, reserve={c: round(v, 1) for c, v in reserve.items()},
                migration_capacity_y=round(capacity_mig), migration_demand_y=round(total_mig), migration_limited=total_mig > capacity_mig,
                evacuation_new_shanghai=dict(people=ev_people, capacity_people_per_day=round(ev_cap_day), days=r3(ev_days),
                                             note="резерв переселенческих транспортов мира + реквизиция 25 % лайнеров; рейс ~3 суток туда-обратно до соседней системы"),
                class_split=P["class_split"], status=TAG)


def hop_lengths(g, a, b):
    if a == b:
        return []
    dist, prev = dijkstra(g, a, lambda u, v, e: e["ly"])
    p = path_to(prev, a, b)
    if not p:
        return None
    return [g[x][y]["ly"] for x, y in zip(p, p[1:])]


# ---------------------------------------------------------------- fleet age and secondary market
def fleet_age(I):
    FA = I.WS["fleet_age"]
    SC = I.SE["ship_classes"]
    eco = I.pol
    gpc = sorted(eco[k]["gdp_per_capita_cr"] for k in eco if eco[k]["population"] > 0)
    out = {}
    tot = {s: 0.0 for s in FA["states"]}
    used_sales = 0.0
    for k, rows in sorted(I.X["fleet_census"]["polities"].items()):
        p = eco.get(k)
        if not p:
            continue
        rank = gpc.index(p["gdp_per_capita_cr"]) / max(1, len(gpc) - 1) if p["gdp_per_capita_cr"] in gpc else 0.5
        tier = "rich" if rank > 0.66 else "middle" if rank > 0.33 else "poor"
        b = next((x for x in I.W["beacons"] if x["system"] == I.system_of(k)), None)
        joined = b["year"] if b else 3100
        g = max(0.0, p["growth_per_year"])
        out[k] = {}
        for cls, r in sorted(rows.items()):
            sc = SC.get(cls)
            if not sc:
                continue
            L = sc["service_life_years"]
            retire = L * FA["retirement_age_share_of_life"][tier]
            cap = retire if not sc.get("jump") else min(retire, max(3.0, YEAR - joined))
            # age pdf of a fleet whose deliveries grow at g: n(a) ~ exp(-g a), 0 <= a < cap
            bins = {"new": 0.0, "mature": 0.0, "old": 0.0, "heavily_worn": 0.0}
            steps = 200
            for i in range(steps):
                a = (i + 0.5) * cap / steps
                w = math.exp(-g * a)
                key = "new" if a < 0.1 * L else "mature" if a < 0.5 * L else "old" if a < L else "heavily_worn"
                bins[key] += w
            s = sum(bins.values())
            active = r["active"]
            dist = {x: active * v / s for x, v in bins.items()}
            dist["reserve"] = r["reserve"]
            dist["mothballed"] = (I.X["defence"]["polities"][k]["mothballed_hulls"] if cls == "line_combatant" else 0.0) + \
                (active * (0.02 if tier == "rich" else 0.04) if sc["family"] == "civil" else 0.0)
            dist["restoration_project"] = active * (0.005 if tier == "poor" else 0.001)
            dist["scrap_candidate"] = r["retired_per_year"] * 0.5
            eff = {}
            n = sum(dist[x] for x in ("new", "mature", "old", "heavily_worn"))
            for m in ("failure_rate_x", "opex_x", "availability", "insurance_x", "fuel_x", "delay_probability"):
                eff[m] = r3(sum(dist[x] * FA["effects"][x][m] for x in ("new", "mature", "old", "heavily_worn")) / max(n, 1e-9))
            mean_age = sum(((i + 0.5) * cap / steps) * math.exp(-g * (i + 0.5) * cap / steps) for i in range(steps)) / \
                sum(math.exp(-g * (i + 0.5) * cap / steps) for i in range(steps))
            out[k][cls] = dict(tier=tier, mean_age_y=round(mean_age, 1), max_age_y=round(cap, 1), states={x: round(v, 1) for x, v in dist.items()},
                               effects=eff, jump_era_capped=bool(sc.get("jump")) and cap < retire)
            for x, v in dist.items():
                tot[x] += v
            if tier == "rich" and sc["family"] == "civil":
                used_sales += r["retired_per_year"] * 0.6
    price = {c: lg.build_cost(SC[c])["list_price_cr"] for c in ("cobrett_i_n1", "medium_freighter", "liner", "ocs4_lastochka")}
    market = {c: {x: round(price[c] * FA["effects"][x]["resale_share"]) for x in ("new", "mature", "old", "heavily_worn", "scrap_candidate")} for c in price}
    pirates = dict(composition=FA["pirate_fleet"], note="угнанные, восстановленные, старые, нелегально переделанные, перерегистрированные, собранные из нескольких доноров")
    return dict(polities=out, world_states={x: round(v) for x, v in tot.items()}, used_ships_sold_rich_to_poor_y=round(used_sales),
                secondary_market_cr=market, pirate_fleet=pirates, effects=FA["effects"], rule=FA["jump_era_rule"],
                registry_rule="вторичный рынок, утиль и восстановление используют тот же ShipInstance и серийные агрегаты (без второго реестра); "
                              "стадии: sold -> reflagged -> restoration -> scrapped; краденые агрегаты — legality reported_stolen / salvage_unregistered",
                status=TAG)


# ---------------------------------------------------------------- piracy, wrecks
def piracy_model(I, sites, social_pre):
    D = I.X["defence"]["polities"]
    eco = I.pol
    by_sys = {}
    arrivals = {}
    value = {}
    for f in I.X["trade_flows"]["flows"]:
        for s in f["path"][1:]:
            arrivals[s] = arrivals.get(s, 0.0) + f["loaded_trips_y"]
            value[s] = value.get(s, 0.0) + f["value_cr_y"]
    owner = {}
    for k, p in eco.items():
        owner.setdefault(I.system_of(k), []).append(k)
    for s in sorted(set(arrivals) | {19}):
        ks = owner.get(s, [])
        traffic = clamp(math.log10(1 + arrivals.get(s, 0.0)) / 5.0) if s != 19 else 0.5
        cargo = clamp(math.log10(1 + value.get(s, 0.0) / max(1.0, arrivals.get(s, 1.0))) / 8.0) if s != 19 else 1.0
        cutters = sum(D[k]["forces"].get("patrol_cutter", 0.0) for k in ks)
        weak = clamp(1.0 - cutters / (cutters + 0.02 * max(1.0, arrivals.get(s, 0.0)) + 5.0)) if s != 19 else 0.9
        cover = 0.3
        for rid, r in I.reservoirs.items():
            if rid.startswith(f"system_{s}.") and r["kind"] in I.WS["piracy"]["cover_bodies"]:
                cover = max(cover, clamp(0.4 + 0.15 * math.log10(1 + 1e3 * r.get("total_mass_earth", 0.0))))
        fams = [I.law_family(k) for k in ks] or ["none"]
        corr = max(CORRUPTION_BASE[f] for f in fams)
        haven = clamp(0.3 + 0.5 * corr + (0.3 if any(f in ("clan_law", "none") for f in fams) else 0.0))
        resale = clamp(0.3 + (0.4 if any(a.get("type") == "black_market" for k in ks for a in I.colony.get(k, {}).get("assets", [])) else 0.0) +
                       (0.3 if any(f in ("clan_law", "city_state_arbitration", "none") for f in fams) else 0.0))
        idx = (traffic * cargo * weak * cover * haven * corr * resale) ** (1.0 / 7.0) if s != 19 else 0.92
        by_sys[s] = dict(system=s, name=I.E[s]["name"], index=r3(idx), traffic=r3(traffic), valuable_cargo=r3(cargo), weak_enforcement=r3(weak),
                         cover=r3(cover), safe_haven=r3(haven), corruption=r3(corr), resale_market=r3(resale))
    enclaves = []
    for s, r in sorted(by_sys.items(), key=lambda kv: -kv[1]["index"]):
        if r["index"] < I.WS["piracy"]["enclave_threshold"] or s == 19:
            continue
        belt = belt_of(I, s)
        if not belt:
            continue
        sid = f"site.{s:03d}.pirate_enclave"
        pop = int(r["index"] * I.WS["piracy"]["people_per_index_point"])
        enclaves.append(base_site(I, sid, f"пиратский анклав {I.E[s]['name']}", "pirate_enclave", s, None, "братство", pop,
                                  "ремонт, топливо, склады, рынок, контрабанда, информаторы, скупщики, разборка, фальшивые документы, перерегистрация",
                                  body=belt, placement=dict(belt=dict(radius_au=r3(I.reservoirs[belt].get("peak_au") or 3.0), longitude_deg=round(360 * h01(sid), 2))),
                                  legal="illegal", ships=round(pop / 25.0), deals_with="начальник ближайшего удалённого поста или местная компания (см. administrators.drift)",
                                  source="piracy_model"))
    return by_sys, enclaves


def wreck_fields(I):
    fl = I.X["fleet_census"]["totals"]
    lost_civil = sum(v["lost_per_year"] for v in fl.values() if v["family"] == "civil")
    lost_mil = sum(v["lost_per_year"] for v in fl.values() if v["family"] == "military")
    persist = 40.0
    rows = []
    rt = sorted(I.X["trade_flows"]["routes"], key=lambda r: -r["tonnes_y"])
    tot_t = sum(r["tonnes_y"] for r in rt) or 1.0
    for r in rt[:8]:
        a, b = r["edge"]
        n = lost_civil * persist * r["tonnes_y"] / tot_t * 0.5
        rows.append(dict(id=f"wreck.{a:03d}_{b:03d}", kind="route_graveyard", near=f"точки выхода маяков {I.E[a]['name']} / {I.E[b]['name']}",
                         wrecks=round(n), ownership="страховщики и владельцы (по праву системы)", salvage="по лицензии системы", status=TAG))
    rows.append(dict(id="wreck.strife_graveyard", kind="war_graveyard", zone="hz.strife_graveyard", near="Пояс Раздора, Эпсилон Эридана",
                     wrecks=round(lost_mil * persist * 0.3 + 1800), ownership="спорная (война 550 лет)", salvage="клановое право: находка за нашедшим", status=TAG))
    rows.append(dict(id="wreck.lotus_sargasso", kind="sargasso", zone="hz.lotus_sargasso", near="«Лотос»", wrecks=210,
                     ownership="брошены (заявки на обломки)", salvage="кто первый доберётся; Керн требует регистрации", status=TAG))
    rows.append(dict(id="wreck.gaia_ark", kind="derelict", zone="hz.gaia_quarantine", near="Гайя, GJ 832", wrecks=1, ownership="GREEN-ARK-1 (канон)",
                     salvage="запрещён карантином", status="CANON"))
    claim_schema = ["wreck_id", "claimant", "basis (finder / owner / insurer / state / treaty)", "filed_at_site", "filed_year", "law_family", "status (filed / contested / granted / denied)"]
    return dict(fields=rows, salvage_claim_fields=claim_schema, lost_civil_per_year=round(lost_civil, 1), lost_military_per_year=round(lost_mil, 1),
                persistence_years=persist, status=TAG)


# ---------------------------------------------------------------- frontiers, hazards, resource
def jump_sigma_km(d_ly, lock):
    lockf = {"both_ends": 200.0, "origin_only": 12.0, "destination_only": 6.0, "none": 1.0}[lock]
    return d_ly * LY_KM * 1.0e-9 * 1.0 / lockf * 1.0 / 1.0 if False else d_ly * 1e-9 * LY_KM / lockf


def p_inside_sphere(sigma, offset, radius, n=20000, seed=7):
    rnd = random.Random(seed)
    hit = 0
    for _ in range(n):
        x, y, z = rnd.gauss(0, sigma), rnd.gauss(0, sigma), rnd.gauss(0, sigma)
        if (x - offset) ** 2 + y * y + z * z <= radius * radius:
            hit += 1
    return hit / n


def frontier_model(I, dsos, sites, g):
    WS = I.WS
    dso = {d["id"]: d for d in dsos}
    hop = I.hop
    # access chains: real hops only (systems and deep-space objects)
    chains = ACCESS

    def P(n):
        return I.pos[n[1]] if n[0] == "system" else dso[n[1]]["position_ly"]
    access = {}
    for fid, ch in chains.items():
        legs = [r3(vlen(vsub(P(a), P(b)))) for a, b in zip(ch, ch[1:])]
        access[fid] = dict(chain=[n[1] for n in ch], legs_ly=legs, max_leg_ly=max(legs), within_max_hop=max(legs) <= hop + 1e-9,
                           from_sol_jumps_to_start=None)
    # how far the frontier start is from Sol over the network
    for fid, a in access.items():
        dist, prev = dijkstra(g, 0, lambda u, v, e: 1.0)
        a["from_sol_jumps_to_start"] = dist.get(a["chain"][0])
        a["from_sol_path_to_start"] = path_to(prev, 0, a["chain"][0])
    # the World's Ass access: Cobrett range, extra tanks, dead-zone risk with / without beacon lock
    cob = I.SE["ship_classes"]["cobrett_i_n1"]
    wa = access["frontier.worlds_ass"]
    leg_lotus, leg_wa = wa["legs_ly"][2], wa["legs_ly"][3]
    m_dry = cob["dry_mass_t"]
    fuel_round = 0.0
    m = m_dry + cob["jump_tank_t"] + lg.cargo_gross_t(cob, 40.0)
    for d in (leg_lotus, leg_wa, leg_wa, leg_lotus):
        f = lg.jump_fuel_t(I.cal, I.SE, cob, m, d)
        fuel_round += f
        m -= f
    j20 = dict(form="C20 place", charge_t=6.0, mass_loaded_t=8.0, note="бак заряда J20 вместо контейнера: -1 место груза, +6 т заряда (PROPOSAL)")
    tanks_needed = max(0, math.ceil((fuel_round - cob["jump_tank_t"]) / j20["charge_t"]))
    zone = next(z for z in WS["hazard_zones"] if z["id"] == "hz.lotus_dead")
    sig_none = jump_sigma_km(leg_lotus, "none")
    sig_lock = jump_sigma_km(leg_lotus, "both_ends")
    p_none = p_inside_sphere(sig_none, zone["offset_km"][0], zone["radius_km"])
    p_lock = p_inside_sphere(sig_lock, zone["offset_km"][0], zone["radius_km"])
    up = next(x for x in WS["deep_space_objects"] if x["id"] == "dso.lotus")["beacon"]["uptime"]
    p_dead = up * p_lock + (1 - up) * p_none
    # expedition economics (one Cobrett expedition 3126): costs, outcome distribution, EV
    res = WS["rare_resources"][0]
    price = res["price_cr_per_kg"]
    base = lg.trip_cost(I.SE, I.cal, "cobrett_i_n1", [leg_lotus, leg_wa], 40.0, 0.0)
    hull = lg.build_cost(cob)["list_price_cr"]
    cost = dict(charge_and_fuel=round(2 * (base["costs_cr"].get("jump_charge", 0) + base["costs_cr"].get("reactor_fuel", 0)) * 2.2 + tanks_needed * 6000 * 5.65 * 2.0),
                tanker_support=180000, repair=95000, insurance=round(0.12 * hull), crew_hazard_pay=round(2.5 * 2 * 4000 * 3.0),
                provisions=24000, spare_parts=60000, coordinates_and_information=65000, bribes_and_tolls=40000, licence=30000)
    p_loss = 0.04 + p_dead * 0.5
    ctot = sum(cost.values())
    # equilibrium: expeditions keep coming while the AVERAGE one loses no more than the optimism bias of
    # prospectors (25 % of its cost) -> that fixes the price; demand at that price fixes the quantity, and the
    # quantity fixes the number of expeditions, the ships lost and the depletion of the deposits
    finds = [("small find 0.1 kg", 0.30, 0.1), ("good find 0.6 kg", 0.08, 0.6), ("jackpot: 3 kg or artifact", 0.02, 3.0)]
    e_kg = sum(pf * kg for _, pf, kg in finds)
    bias = 0.25
    seller_share = 0.7          # assay, broker, Kern licence and black-market discount
    pr = ((1.0 - bias) * ctot + p_loss * 0.6 * hull) / (seller_share * e_kg)
    q0 = res["demand_kg_y_at_base_price"]
    eps = res["price_elasticity"]
    q = q0 * (pr / price) ** (-eps)
    expeditions_y = q / e_kg
    outcomes = [dict(outcome="ship lost", p=r3(p_loss), value_cr=round(-hull * 0.6)),
                dict(outcome="nothing", p=r3(1.0 - p_loss - sum(pf for _, pf, _ in finds)), value_cr=0)] + \
               [dict(outcome=nm, p=pf, value_cr=round(kg * pr * seller_share)) for nm, pf, kg in finds]
    ev = sum(o["p"] * o["value_cr"] for o in outcomes) - ctot
    median_outcome = -ctot
    remaining = sum(d["reserve_kg"] - d["extracted_kg"] for d in res["deposits"])
    chain = ["extraordinary reward", "expedition demand", "specialised transport", "expensive support", "piracy", "security", "black market",
             "salvage", "repeated exploration"]
    risk_reward = {}
    art_max = max((a["market_value_cr"] or 0) for a in WS["artifacts"])
    freighter = I.SE["ship_classes"]["medium_freighter"]
    metals_cr_t = 1100.0
    for f in WS["frontiers"]:
        z = [h for h in WS["hazard_zones"] if h.get("anchor", "").startswith(f"system_{f['system']}.") or h.get("anchor") in f["region"]]
        if f["specialisation"] == "risk_reward_max":
            reward, risk = max(art_max, 3.0 * price * 0.7), p_loss
        elif f["specialisation"] == "resource":
            # a full freighter of metals from the Chest, one trip (licensed rush, no single jackpot)
            reward, risk = freighter["cargo_t"] * metals_cr_t, 0.004 * 4
        else:
            reward, risk = 2.0e6, 0.002 * 3
        risk = risk + 0.02 * len(z)
        risk_reward[f["id"]] = dict(max_payoff_cr=round(reward), risk=r3(risk), hazard_zones=len(z),
                                    basis={"risk_reward_max": "дороже всего: артефакт или 3 кг лотосита; риск = потеря корабля",
                                           "resource": "полный фрейтер металлов за рейс; риск = обычные потери x 4 прыжка",
                                           "science": "грант/контракт; риск = обычные потери"}[f["specialisation"]])
    hz = []
    for z in WS["hazard_zones"]:
        hz.append(dict(z, status=TAG if z["kind"] not in ("quarantine",) else "CANON"))
    return dict(access=access, worlds_ass=dict(
        official_names=WS["frontiers"][0]["official_names"], pilot_names=WS["frontiers"][0]["pilot_names"], story=WS["frontiers"][0]["story"],
        cobrett_round_trip_charge_t=r3(fuel_round), cobrett_tank_t=cob["jump_tank_t"], j20_tank=j20, j20_tanks_needed=tanks_needed,
        dead_zone=dict(sigma_km_no_lock=round(sig_none), sigma_km_both_ends=round(sig_lock), p_inside_no_lock=r3(p_none), p_inside_locked=r3(p_lock),
                       beacon_uptime=up, p_inside_expected=r3(p_dead)),
        expedition_cost_cr=cost, expedition_cost_total_cr=round(ctot), outcomes=outcomes, expected_value_cr=round(ev), median_result_cr=round(median_outcome),
        average_expedition_profitable=ev > 0, causal_chain=chain, expeditions_per_year=round(expeditions_y),
        ships_lost_per_year=r3(expeditions_y * p_loss), optimism_bias=bias,
        lotosite_market=dict(equilibrium_kg_y=r3(q), base_price_cr_per_kg=price, base_demand_kg_y=q0, price_cr_per_kg=round(pr), black_market_share=res["black_market_share"],
                             remaining_reserve_kg=remaining, years_to_depletion=r3(remaining / q), elasticity=eps,
                             monopoly_threat="Керн + Когниум держат лицензии и пробирную палату «Порога»; Тяньхэ требует доли",
                             price_shock_rule="крупная находка (+Q кг) сбивает цену: P = P* x (1 + Q/q)^(-1/elasticity); монополия на пробирную палату удерживает цену выше P*"),
        information_market=["координаты безопасной точки выхода", "положение тайников заряда", "состояние аномальных зон", "новые обломки", "месторождения",
                            "расписание патрулей", "засады", "судьба пропавших экспедиций"],
        information_rule="часть сведений ложна; цена зависит от репутации продавца (см. KnowledgeRecord.confidence)"),
        risk_reward=risk_reward, hazard_zones=hz, rare_resources=WS["rare_resources"], artifacts=WS["artifacts"],
        frontiers=[{k: f[k] for k in ("id", "specialisation", "system", "region")} for f in WS["frontiers"]], status=TAG)


# ---------------------------------------------------------------- world events and knowledge (schema + one example)
SCHEMAS = {
    "Hub": ["id", "name", "type", "system", "body", "placement", "polity", "operator", "population", "purpose", "docks", "storage_t", "repair",
            "medical_beds", "quarantine_beds", "security_base", "status"],
    "Settlement": ["id", "name", "type", "system", "body", "placement", "polity", "population", "status"],
    "City": ["id", "name", "body", "population", "role", "surface", "night_light", "status"],
    "Spaceport": ["id", "name", "type", "body", "placement", "docks"],
    "DeepSpaceObject": ["id", "name", "object_type", "frame", "epoch_year", "position_ly", "velocity_km_s", "status"],
    "HazardZone": ["id", "kind", "name", "shape", "effects", "knowledge"],
    "LocalAdministrator": ["id", "site", "name", "background", "appointed", "traits", "oversight", "effective_freedom", "origin", "status"],
    "Jurisdiction": ["site", "sovereign_polity", "local_jurisdiction", "applicable_law", "enforcement", "court", "status"],
    "LawProfile": ["ru", "courts", "police", "salvage_finder_share", "registration", "weapons", "controlled_goods", "search_seizure", "private_security", "extradition"],
    "PassengerFlow": ["link", "pax_y", "migrants_y", "ship_class", "ships", "departures_y", "wait_days_mean", "fares_cr"],
    "FleetAgeState": ["tier", "mean_age_y", "max_age_y", "states", "effects"],
    "Epidemic": ["id", "origin", "incubation_days", "transmission", "severity", "mortality", "detectability", "treatment", "immunity", "environmental_survival"],
    "QuarantineState": ["site", "level", "patrols", "dock_control", "ship_checks", "holding_zones", "supply", "exceptions", "illegal_routes_watched"],
    "WorldEvent": ["id", "kind", "truth", "location", "time", "status"],
    "KnowledgeRecord": ["event", "holder", "source", "observed_at", "received_at", "confidence", "freshness", "classification", "channel", "text", "language"],
    "BeaconCommunicationNode": ["id", "kind", "operator", "coverage_ls", "capacity_msgs_per_h", "trust_authority", "clock_sync", "emergency_channel"],
    "Artifact": ["id", "name", "found_at", "found_year", "provenance", "claims", "scientific_value", "market_value_cr", "known", "unknown", "hazard", "fraud"],
    "RareResourceDeposit": ["id", "anchor", "zone", "grade_g_per_t", "reserve_kg", "extracted_kg", "difficulty", "operator"],
    "ExpeditionSupport": ["cobrett_round_trip_charge_t", "j20_tank", "j20_tanks_needed", "expedition_cost_cr", "outcomes"],
    "SalvageClaim": ["wreck_id", "claimant", "basis", "filed_at_site", "filed_year", "law_family", "status"],
}


def world_events(I, comms):
    """One worked event: WorldTruth is single; every holder gets its own KnowledgeRecord when the news
    physically arrives over the beacon net (routine) or by courier (flash)."""
    h = comms["from_sol_routine_h"]
    lotus_to_porog_h = next(l["routine_hop_h"] for l in comms["links"] if {l["a"], l["b"]} == {"dso.lotus", 19})
    ev = dict(id="EVT-3126-0001", kind="ship_missing", status=TAG, time=3126.412, location="hz.lotus_dead",
              truth=dict(ship="экспедиционный Cobrett «Мэй-Лин-2»", state="вышел из прыжка внутри мёртвой зоны «Лотоса», цел, экипаж 3, запасов на 19 суток, сигнал бедствия слабый"))
    t0 = ev["time"]
    day = 1.0 / 365.25

    def rec(holder, ch, src, delay_h, conf, cls, text, lang):
        return dict(event=ev["id"], holder=holder, source=src, channel=ch, observed_at=r3(t0), received_at=round(t0 + delay_h / 24.0 * day, 5),
                    delay_days=r3(delay_h / 24.0), confidence=conf, freshness="fresh" if delay_h < 48 else "stale", classification=cls, text=text, language=lang)
    recs = [
        rec("dso.lotus", "local_radio", "сигнал бедствия", 0.2, 0.9, "public", "Слабый сигнал бедствия из мёртвой зоны: «Мэй-Лин-2», трое живы.", "en"),
        rec("site.019.porog", "official", "диспетчер «Лотоса»", lotus_to_porog_h, 0.8, "public", "Экспедиционное судно не вышло на связь согласно плану.", "en"),
        rec("site.019.troyan_market", "freeband", "пилоты", lotus_to_porog_h + 3.0, 0.4, "rumour", "Они нашли второй корабль, поэтому и застряли.", "zh"),
        rec("lalande_hellas", "commercial", "страховой брокер Агоры", float(h.get("4", 72.0)) + 50.0, 0.7, "commercial", "Страховой случай: корпус и груз под вопросом, ставки по маршруту +2 %.", "el"),
        rec("sol_authority", "official", "Управление Маяков (сводка)", float(h.get("19", 120.0)), 0.6, "public", "В районе HD 102195 пропало судно; спасательная операция организуется лицензиатом.", "en"),
    ]
    return dict(schemas=SCHEMAS, events=[ev], knowledge=recs,
                rule="WorldTruth один; KnowledgeRecord у каждого держателя приходит с задержкой сети; новости = представление записей, а не отдельная истина; "
                     "радио = звуковое представление тех же потоков (Public / Freeband)",
                status=TAG)


# ---------------------------------------------------------------- checks, report, export
def checks(I, out):
    res = []

    def add(name, ok, detail):
        res.append(dict(check=name, ok=bool(ok), detail=detail))
    S = out["sites"]["sites"]
    ids = [s["id"] for s in S] + [d["id"] for d in out["frontier"]["deep_space_objects"] if d["id"] not in {s["id"] for s in S}]
    dup = sorted({x for x in ids if ids.count(x) > 1})
    add("one_id_one_object", not dup, ",".join(dup) or f"{len(ids)} ids")
    badb = [s["id"] for s in S if s["system"] is not None and s.get("body") and s["body"] not in I.bodies and s["body"] not in I.reservoirs]
    add("site_bodies_exist_in_catalog", not badb, ",".join(badb[:8]) or "ok")
    nopos = [s["id"] for s in S if not s.get("placement")]
    add("every_site_has_placement", not nopos, ",".join(nopos[:8]) or "ok")
    acc = out["frontier"]["access"]
    bad = [f"{k}:{a['max_leg_ly']}" for k, a in acc.items() if not a["within_max_hop"]]
    add("access_chains_use_real_hops_only", not bad, ",".join(bad) or ", ".join(f"{k} max {a['max_leg_ly']}" for k, a in acc.items()))
    add("kern_pocket_reachable_without_exceptions", acc["kern_pocket"]["within_max_hop"], f"legs {acc['kern_pocket']['legs_ly']}")
    dsos = out["frontier"]["deep_space_objects"]
    add("deep_space_objects_have_world_lines", all("position_ly" in d and "velocity_km_s" in d and "epoch_year" in d for d in dsos), f"{len(dsos)} objects")
    rr = out["frontier"]["risk_reward"]
    wa = rr["frontier.worlds_ass"]
    add("worlds_ass_is_max_risk_and_reward", all(wa["max_payoff_cr"] >= v["max_payoff_cr"] and wa["risk"] >= v["risk"] for v in rr.values()),
        ", ".join(f"{k}: {v['max_payoff_cr']} / {v['risk']}" for k, v in rr.items()))
    w = out["frontier"]["worlds_ass"]
    add("average_expedition_not_profitable", not w["average_expedition_profitable"], f"EV {w['expected_value_cr']} Cr, cost {w['expedition_cost_total_cr']}")
    c = out["comms"]
    far = max(c["frontier_from_sol_days"].values())
    add("news_reaches_far_periphery_within_author_target", c["diameter_days"] <= c["author_target_days"] + 1e-9 and far <= c["author_target_days"] + 1e-9,
        f"network diameter {c['diameter_days']} d, Sol -> farthest frontier {far} d, target {c['author_target_days']} d (AUTHOR_TARGET)")
    p = out["passengers"]
    add("migration_within_transport_capacity", not p["migration_limited"], f"demand {p['migration_demand_y']} <= capacity {p['migration_capacity_y']} per year")
    fa = out["fleet_age"]["polities"]
    badj = [f"{k}:{c}" for k, rows in fa.items() for c, r in rows.items() if I.SE["ship_classes"][c].get("jump") and r["max_age_y"] > YEAR - 2750 + 1e-9]
    add("no_jump_ship_older_than_jump_drive", not badj, ",".join(badj[:6]) or "ok")
    sch = out["events"]["schemas"]
    miss = []
    for s in S:
        for f in sch["Hub"]:
            if f not in s:
                miss.append(f"{s['id']}:{f}")
    for d in dsos:
        for f in sch["DeepSpaceObject"]:
            if f not in d:
                miss.append(f"{d['id']}:{f}")
    for a in out["society"]["administrators"]:
        for f in sch["LocalAdministrator"]:
            if f not in a:
                miss.append(f"{a['id']}:{f}")
    add("records_match_schemas", not miss, ",".join(miss[:8]) or "Hub / DeepSpaceObject / LocalAdministrator")
    rec = out["events"]["knowledge"]
    add("knowledge_never_before_truth", all(r["received_at"] >= r["observed_at"] for r in rec), f"{len(rec)} records")
    juris = {r["site"] for r in out["law"]["sites"]}
    add("every_site_has_jurisdiction", all(s["id"] in juris for s in S), f"{len(juris)} sites")
    nolbl = [s["id"] for s in S if s["status"] not in ("PROPOSAL", "CANON", "GENERATED")]
    add("status_present", not nolbl, ",".join(nolbl[:5]) or "ok")
    return res


def runtime_export(I, out):
    """GENERATED runtime copies: in-system sites + zones (System / Detail maps) and deep-space objects (Galaxy Map)."""
    S = out["sites"]["sites"]
    rows = []
    for s in S:
        if s["system"] is None or s.get("hidden") or s["type"] == "pirate_enclave":
            continue
        rows.append(dict(id=s["id"], name=s["name"], type=s["type"], system_id=s["system"], parent_body_id=s.get("body"), placement=s["placement"],
                         map_level=s["map_level"], polity=s["polity"], population=s["population"], runtime_binding=s.get("runtime_binding")))
    for c in out["sites"]["earth"]:
        if c.get("spaceport"):
            rows.append(dict(id=c["id"], name=c["name"], type="planetary_surface_port", system_id=0, parent_body_id=c["body"],
                             placement=dict(surface=c["surface"]), map_level="detail", polity=SOL, population=c["population"], runtime_binding=None))
    zones = []
    for z in out["frontier"]["hazard_zones"]:
        anc = z.get("anchor", "")
        if anc.startswith("system_") and "." in anc and z["knowledge"] in ("official", "mapped", "partial"):
            zones.append(dict(id=z["id"], name=z["name"], kind=z["kind"], anchor_body_id=anc, shape=z["shape"], radius_km=z.get("radius_km"),
                              knowledge=z["knowledge"]))
    doc = dict(schema="elite.world_sites_runtime.v1", generated_by="tools/colonization_lab/world_sites.py", do_not_edit=True, epoch_year=I.WS["epoch_year"],
               status=TAG, sites=rows, zones=zones)
    objs = []
    for d in out["frontier"]["deep_space_objects"]:
        if d["hidden"]:
            continue
        ss = next((s for s in S if s["id"] == d["id"]), None)
        objs.append(dict(schema_version=1, kind="galaxy_object", id=d["id"], name=d["name"],
                         object_type=d["object_type"], position_ly=dict(x=d["position_ly"][0], y=d["position_ly"][1], z=d["position_ly"][2]),
                         description=(ss or {}).get("purpose") or d["source"], tags=["world_site", d["object_type"]],
                         properties=dict(site_type=d["site_type"], epoch_year=d["epoch_year"], velocity_km_s=d["velocity_km_s"], frame=d["frame"],
                                         population=(ss or {}).get("population"), polity=(ss or {}).get("polity"),
                                         provenance=dict(generator="tools/colonization_lab/world_sites.py", status=TAG))))
    return doc, objs


def md_report(I, out, chk):
    S = out["sites"]["sites"]
    by = {}
    for s in S:
        by[s["type_ru"]] = by.get(s["type_ru"], 0) + 1
    w = out["frontier"]["worlds_ass"]
    c = out["comms"]
    p = out["passengers"]
    L = ["# Мировые объекты 3126 (PROPOSAL)", "", "Сгенерировано `tools/colonization_lab/world_sites.py` из `canon/world_sites.json`, каталога и out/world_3126 (экономика).", "",
         "## Объекты", "", "| Тип | Число |", "|---|---:|"] + [f"| {a} | {b} |" for a, b in sorted(by.items(), key=lambda kv: -kv[1])] + [
        "", f"Всего сайтов {len(S)}; объектов глубокого космоса {len(out['frontier']['deep_space_objects'])}; агломераций Земли {len(out['sites']['earth'])}.", "",
        "## Дорога в карман Керна", "", f"Цепочка {out['frontier']['access']['kern_pocket']['chain']}: плечи {out['frontier']['access']['kern_pocket']['legs_ly']} св. лет (предел {I.hop}).", "",
        "## Жопа мира (月工门)", "",
        f"- доступ: {out['frontier']['access']['frontier.worlds_ass']['chain']}, плечи {out['frontier']['access']['frontier.worlds_ass']['legs_ly']} св. лет",
        f"- Cobrett туда-обратно от 83 Leonis: {w['cobrett_round_trip_charge_t']} т заряда при баке {w['cobrett_tank_t']} т -> баков J20: {w['j20_tanks_needed']}",
        f"- мёртвая зона «Лотоса»: сигма без захвата {w['dead_zone']['sigma_km_no_lock']} км, с захватом {w['dead_zone']['sigma_km_both_ends']} км; "
        f"вероятность выйти внутрь {w['dead_zone']['p_inside_no_lock']} / {w['dead_zone']['p_inside_locked']}, ожидаемая при маяке {w['dead_zone']['beacon_uptime']} — {w['dead_zone']['p_inside_expected']}",
        f"- экспедиция: стоимость {w['expedition_cost_total_cr']} Cr, ожидание {w['expected_value_cr']} Cr (средняя экспедиция убыточна), медиана {w['median_result_cr']} Cr",
        f"- лотосит: {w['lotosite_market']['equilibrium_kg_y']} кг/год, цена {w['lotosite_market']['price_cr_per_kg']} Cr/кг, запасов на {w['lotosite_market']['years_to_depletion']} лет; "
        f"экспедиций ~{w['expeditions_per_year']}/год, теряется кораблей {w['ships_lost_per_year']}/год", "",
        "## Связь", "", f"Диаметр сети (обычная новость): {c['diameter_days']} сут (AUTHOR_TARGET {c['author_target_days']}).", "",
        "| Куда | Обычная, сут | Срочная, сут |", "|---|---:|---:|"] + [f"| {e['name']} | {e['routine_days']} | {e['flash_days']} |" for e in c["examples"]] + [
        "", "## Пассажиры", "", f"Межзвёздных поездок {p['interstellar_trips_y']:,}/год, переселенцев {p['migrants_y']:,}, беженцев {p['refugees_y']:,}; поверхность ↔ орбита {p['surface_orbit_trips_y']:,}.".replace(",", " "),
        "", "| Класс | Кораблей |", "|---|---:|"] + [f"| {a} | {b} |" for a, b in p["fleet"].items()] + [
        "", f"Эвакуация 1.2 млн с кольца Нового Шанхая: {p['evacuation_new_shanghai']['days']} сут.", "",
        "## Проверки", ""] + [f"- {'OK' if x['ok'] else 'FAIL'} {x['check']}: {x['detail']}" for x in chk]
    return "\n".join(L) + "\n"


def compute():
    I = Inputs()
    g = route_graph(I)
    dsos = dso_records(I)
    sites = build_sites(I, dsos)
    comms = comms_model(I, sites, dsos, g)
    csite = {r["site"]: r for r in comms["sites"]}
    eco = I.pol
    pir_pre = {}
    social = {}
    # piracy needs nothing from the social state; social needs piracy
    by_sys, enclaves = piracy_model(I, sites, None)
    for s in enclaves:
        s["language"] = I.WS["languages"]["network_lingua_franca"]
    sites += enclaves
    comms = comms_model(I, sites, dsos, g)
    csite = {r["site"]: r for r in comms["sites"]}
    pir = {s: v["index"] for s, v in by_sys.items()}
    sol_h = comms["from_sol_routine_h"]
    for s in sites:
        m = social_state(I, s, csite[s["id"]], pir, eco)
        arch = archetype(s, m)
        key = str(s["system"]) if s["system"] is not None else s["id"]
        cap_sys = I.system_of(s["polity"]) if s["polity"] else 0
        delay = max(0.0, float(sol_h.get(key, 0.0)) - float(sol_h.get(str(cap_sys), 0.0))) / 24.0 if key in sol_h else 7.0
        social[s["id"]] = dict(metrics=m, archetype=arch, archetype_ru=I.WS["social"]["archetypes"][arch], description=DESCRIPTION[arch],
                               reactions=shock_reactions(m), comms_delay_days=r3(delay))
    admins = administrators(I, sites, social)
    law = law_model(I, sites, comms)
    pax = passengers(I, g)
    age = fleet_age(I)
    wrecks = wreck_fields(I)
    front = frontier_model(I, dsos, sites, g)
    front["deep_space_objects"] = dsos
    front["piracy_by_system"] = [by_sys[k] for k in sorted(by_sys)]
    front["wrecks"] = wrecks
    out = dict(
        sites=dict(sites=sites, earth=earth_settlements(I), status=TAG,
                   note="хаб — объект мира; маркер карты — только визуализация. Счётные объекты (рудники, заводы) агрегированы в хабы с полем aggregates."),
        society=dict(sites=social, administrators=admins, archetypes=I.WS["social"]["archetypes"], status=TAG),
        law=law, comms=comms, passengers=pax, fleet_age=age, frontier=front)
    out["events"] = world_events(I, comms)
    return I, out


NAMES = ("sites", "society", "law", "comms", "passengers", "fleet_age", "frontier", "events")


def serialise(I, out, chk):
    files = {f"{n if n != 'events' else 'world_events'}_3126.json": json.dumps(out[n], ensure_ascii=False, indent=1, sort_keys=True) + "\n" for n in NAMES}
    files["sites_checks_3126.json"] = json.dumps(dict(checks=chk, status=TAG), ensure_ascii=False, indent=1, sort_keys=True) + "\n"
    files["WORLD_SITES_3126.md"] = md_report(I, out, chk)
    doc, objs = runtime_export(I, out)
    runtime = {RUNTIME_SITES: json.dumps(doc, ensure_ascii=False, indent=1, sort_keys=True) + "\n"}
    for o in objs:
        runtime[os.path.join(RUNTIME_OBJECTS, f"world_{o['id'].replace('.', '_')}.json")] = json.dumps(o, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    return files, runtime


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    I, out = compute()
    chk = checks(I, out)
    files, runtime = serialise(I, out, chk)
    bad = [c for c in chk if not c["ok"]]
    if a.check:
        ok = not bad
        for name, text in files.items():
            p = os.path.join(OUT, name)
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != text:
                print(f"STALE: {p}")
                ok = False
        for p, text in runtime.items():
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != text:
                print(f"STALE: {p}")
                ok = False
        stale = [f for f in glob.glob(os.path.join(RUNTIME_OBJECTS, "world_*.json")) if os.path.normpath(f) not in {os.path.normpath(p) for p in runtime}]
        for f in stale:
            print(f"ORPHAN: {f}")
            ok = False
        for c in bad:
            print(f"FAIL {c['check']}: {c['detail']}")
        print("world_sites check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    os.makedirs(OUT, exist_ok=True)
    for name, text in files.items():
        with open(os.path.join(OUT, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    os.makedirs(os.path.dirname(RUNTIME_SITES), exist_ok=True)
    for f in glob.glob(os.path.join(RUNTIME_OBJECTS, "world_*.json")):
        os.remove(f)
    for p, text in runtime.items():
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
    for c in chk:
        print(("OK  " if c["ok"] else "FAIL"), c["check"], "|", c["detail"])
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
