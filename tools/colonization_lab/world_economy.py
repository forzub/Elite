"""World economy and logistics balance of 3126 (offline, deterministic, stdlib only).

One physical system that politics, economy and war all act on (stocks / flows /
decisions). From the lab's population (out/lab_state_3126.json, run lab.py first)
and the canon inputs it derives, for every polity and system:

  population and where it lives (habitat tiers -> surface / underground /
  burrowed moons / asteroids / rotating habitats / industrial stations; named
  cities from canon/settlements.json) -> demand by 15 commodity classes ->
  local production (endowment x technology) -> interstellar flows over the real
  beacon network (cheapest delivered source, hostile pairs via neutral
  re-export) -> upstream production by recipes -> infrastructure counts (mines,
  plants, yards, depots, hubs, ports, power, beacons) with workforce, power,
  equipment wear and spare parts -> fleet from flows (fleet census: active,
  reserve, repair, new / lost / retired per year) -> shipyard demand vs
  capacity -> fuel balance (reactor fuel, jump charge) -> security / defence
  budget from doctrine and tasks -> beacons and routes -> strategic
  vulnerabilities -> disaster / war scenarios.

Outputs (out/world_3126/): economy_3126.json, infrastructure_3126.json,
trade_flows_3126.json, fleet_census_3126.json, defence_3126.json,
beacons_3126.json, scenarios_3126.json, checks_3126.json and the readable
WORLD_BALANCE_3126.md. Every number is a PROPOSAL unless marked CANON or
AUTHOR_TARGET.

    python world_economy.py            # compute and write out/world_3126/
    python world_economy.py --check    # recompute, compare, run the consistency checks
"""
import argparse
import copy
import json
import math
import os
import sys

import endowment as endow
import lab
import logistics as lg

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out", "world_3126")
YEAR = 3126
SOL = "sol_authority"
EARTH_UNITS = ("earth_north", "earth_south")
BULK = ("food", "water_volatiles", "ore", "metals", "chemicals", "construction")
FUELS = ("reactor_fuel", "jump_charge")
HIGH = ("electronics", "medical", "luxury", "controlled")
SECONDS_Y = 365.25 * 86400.0


def jload(name):
    with open(os.path.join(HERE, name), encoding="utf-8") as f:
        return json.load(f)


def clamp(x, lo=0.0, hi=1.0):
    return lo if x < lo else hi if x > hi else x


def r3(x):
    return round(x, 3)


# ---------------------------------------------------------------- inputs
class Inputs:
    def __init__(self):
        self.S = jload("out/lab_state_3126.json")
        self.W = lab.load("world.json")
        self.PH = lab.load("population_history.json")
        self.ST = lab.load("settlements.json")["polities"]
        self.C = lab.load("commodities.json")
        self.SE = lg.load_ship_economics()
        self.PE = lab.load("polity_economy.json")
        self.E = endow.load_endowment()
        self.cats = [c["id"] for c in self.C["commodities"]]
        self.cat = {c["id"]: c for c in self.C["commodities"]}
        self.units = self.S["demography"]["units"]
        self.net = lg.network_stats(self.W, self.E)
        self.cal = lg.calibrate(self.SE, self.net)
        self.rel = {tuple(sorted((r["a"], r["b"]))): r["grievance"] for r in self.W["relations_anchor"]}
        self.beacon_of = {b["system"]: b for b in self.W["beacons"]}


# ---------------------------------------------------------------- network with unbeaconed links
def trade_graph(I):
    """Beacon graph of 3126 plus short unbeaconed links (<= 8 ly) for inhabited systems without a
    beacon that do not refuse the network (jumps without a beacon are allowed, only less precise
    and dearer: x1.5 cost, see navigation model)."""
    edges = {a: dict(b) for a, b in I.net["_edges"].items()}
    nodes = set(I.net["_nodes"])
    refuse = {c["system"] for c in I.W["colonies"] if c["demography"].get("refuses_network")}
    inhabited = sorted({c["system"] for c in I.W["colonies"]} | {0})
    unbeaconed = set()
    for s in inhabited:
        if s in nodes or s in refuse:
            continue
        best = None
        for n in sorted(nodes):
            d = lab_dist(I.E, s, n)
            if d <= 8.0 and (best is None or d < best[1]):
                best = (n, d)
        if best:
            edges.setdefault(s, {})[best[0]] = best[1]
            edges.setdefault(best[0], {})[s] = best[1]
            unbeaconed.add(tuple(sorted((s, best[0]))))
            nodes.add(s)
    # pockets of the beacon network beyond the drive's regular hop (the far Kern frontier) are
    # reached by expedition ships with extra tanks over the shortest gap (<= 45 ly): x3 cost
    expedition = set()
    main = component(edges, 0)
    for n in sorted(nodes):
        if n in main:
            continue
        comp = component(edges, n)
        best = None
        for a in sorted(comp):
            for b in sorted(main):
                d = lab_dist(I.E, a, b)
                if d <= 45.0 and (best is None or d < best[2]):
                    best = (a, b, d)
        if best:
            a, b, d = best
            edges.setdefault(a, {})[b] = d
            edges.setdefault(b, {})[a] = d
            expedition.add(tuple(sorted((a, b))))
            main |= comp
    return edges, nodes, unbeaconed | expedition, expedition


def component(edges, n):
    seen = {n}
    st = [n]
    while st:
        u = st.pop()
        for v in sorted(edges.get(u, {})):
            if v not in seen:
                seen.add(v)
                st.append(v)
    return seen


def lab_dist(E, a, b):
    pa, pb = E[a]["pos"], E[b]["pos"]
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(pa, pb)))


def shortest_path(edges, a, b, blocked=frozenset()):
    dist = {a: 0.0}
    prev = {}
    todo = [a]
    while todo:
        todo.sort(key=lambda x: (dist[x], x))
        u = todo.pop(0)
        for v, d in sorted(edges.get(u, {}).items()):
            if v in blocked and v != b:
                continue
            nd = dist[u] + 1.0 + d / 10.0
            if v not in dist or nd < dist[v] - 1e-12:
                dist[v] = nd
                prev[v] = u
                todo.append(v)
    if b not in dist:
        return None
    path = [b]
    while path[-1] != a:
        path.append(prev[path[-1]])
    return list(reversed(path))


# ---------------------------------------------------------------- polities
def build_polities(I):
    """Polities = 22 colonies + Sol (two Earth regions). Outposts and frontier pools belong to their
    first owner as sites in their own systems."""
    U = I.units
    pols = {}
    tmax = {}
    for uid, u in U.items():
        for d, v in u["tech_3126"].items():
            tmax[d] = max(tmax.get(d, 0.0), v)
    for c in I.W["colonies"]:
        u = U[c["id"]]
        last = u["checkpoints"][-1] if u["checkpoints"] else {}
        pols[c["id"]] = dict(id=c["id"], name=c["name"], polity=c["polity"], system=c["system"], regime=c["regime_3126"],
                             pop=float(u["pop_3126"]), income=last.get("income", 1.0), automation=last.get("automation", 0.5),
                             tech={d: v / tmax[d] if tmax[d] else 0.0 for d, v in u["tech_3126"].items()},
                             habitat=dict(u["habitat_3126"]), sites=[], assets=c["assets"], problems=c["problems"],
                             refuses=bool(c["demography"].get("refuses_network")), target=u.get("target"),
                             growth=growth_rate(u), founded=c["founded"])
    e_pop = sum(U[e]["pop_3126"] for e in EARTH_UNITS)
    inc = sum(U[e]["checkpoints"][-1]["income"] * U[e]["pop_3126"] for e in EARTH_UNITS) / e_pop
    aut = sum(U[e]["checkpoints"][-1]["automation"] * U[e]["pop_3126"] for e in EARTH_UNITS) / e_pop
    hab = {"earth_surface": 0, "sol_habitats": 0, "sol_burrowed": 0, "sol_rotating": 0}
    for e in EARTH_UNITS:
        for k, v in U[e]["habitat_3126"].items():
            hab[k] += v
    tech = {d: sum(U[e]["tech_3126"][d] * U[e]["pop_3126"] for e in EARTH_UNITS) / e_pop / tmax[d] if tmax[d] else 0.0 for d in tmax}
    anchor = I.W["earth_3126"]
    pols[SOL] = dict(id=SOL, name=anchor["name"], polity=anchor["name"], system=0, regime="sol_authority", pop=float(e_pop), income=inc,
                     automation=aut, tech=tech, habitat=hab, sites=[], assets=[], problems=["300 лет за Кордоном", "численность пугает колонии"],
                     refuses=False, target=None, growth=sum(growth_rate(U[e]) * U[e]["pop_3126"] for e in EARTH_UNITS) / e_pop, founded=None)
    for o in I.W["outposts"] + I.W.get("frontier_pools", []):
        u = U.get(o["id"])
        if not u:
            continue
        owner = o["owners"][0]
        pols[owner]["sites"].append(dict(id=o["id"], name=o["name"], system=o["system"], pop=float(u["pop_3126"]), founded=o["founded"],
                                         co_owners=o["owners"][1:]))
        pols[owner]["pop"] += float(u["pop_3126"])
    for p in pols.values():
        p["gdp"] = p["pop"] * p["income"] * I.PE["gdp_cr_per_income_unit"]
        p["workers"] = p["pop"] * 0.5
    return pols


def growth_rate(u):
    cp = [c for c in u["checkpoints"] if c["year"] >= 3050]
    if len(cp) < 2 or cp[0]["pop"] <= 0:
        return 0.0
    return math.log(max(cp[-1]["pop"], 1) / cp[0]["pop"]) / max(1, cp[-1]["year"] - cp[0]["year"])


# ---------------------------------------------------------------- settlement structure
def settlement_structure(I, p):
    """Population by habitat type (sums to the polity population) and named cities."""
    P = p["pop"]
    E = I.E[p["system"]]
    out = {}
    if p["id"] == SOL:
        h = p["habitat"]
        out["Земля: поверхность"] = h["earth_surface"]
        rest = P - h["earth_surface"] - sum(s["pop"] for s in p["sites"])
        for body, sh in sorted(I.PE["sol_bodies"]["shares"].items()):
            out[f"Сол: {body}"] = rest * sh
    else:
        h = p["habitat"]
        own = sum(h.values())
        t1, t2, t3 = h["surface_and_underground"], h["burrowed_moons_asteroids"], h["rotating_habitats"]
        under = 0.6 if E["n_open"] == 0 else 0.25
        out["поверхность (купола, открытые поселения)"] = t1 * (1 - under)
        out["подземные поселения (лавовые трубки, шахтные города)"] = t1 * under
        belt_w = math.log10(1 + 1e4 * E["belt_me"]) + 0.5 * math.log10(1 + 1e3 * E["comet_me"])
        moon_w = 0.3 * E["n_minor"]
        ast = belt_w / (belt_w + moon_w) if belt_w + moon_w > 0 else 0.5
        out["выработки в лунах"] = t2 * (1 - ast)
        out["астероидные поселения"] = t2 * ast
        cyl = 0.7 if own > 1e8 else 0.4
        out["вращающиеся цилиндры"] = t3 * cyl
        out["орбитальные кольца и станции"] = t3 * (1 - cyl)
    for s in p["sites"]:
        out[f"аванпост: {s['name']}"] = s["pop"]
    return {k: v for k, v in out.items()}


def cities(I, p, structure, industrial_workers):
    """Capital, largest cities by Zipf, industrial / mining / port / military / research sites from
    canon/settlements.json; the rest is aggregated."""
    P = p["pop"]
    n_set = max(1, int(P / 2.0e5) + 1)
    hn = sum(1.0 / k for k in range(1, min(n_set, 5000) + 1)) + (math.log(n_set / 5000) if n_set > 5000 else 0)
    largest = P / hn
    named = I.ST.get(p["id"], [])
    rows = []
    rank = 1
    for st in named:
        role = st["role"]
        if role == "capital":
            pop = largest
        elif role in ("habitat", "food_habitat"):
            rank += 1
            pop = largest / rank
        elif role in ("mine", "refinery", "gas_platform", "ice_export", "factory", "electronics_fab", "shipyard", "arsenal_shipyard"):
            pop = min(largest / 20.0, max(2000.0, industrial_workers * 0.02 * 2.2))
        elif role in ("logistics_hub", "legal_hub", "station_core", "jump_infrastructure", "black_market"):
            pop = largest / 30.0
        elif role in ("fortress", "intel"):
            pop = largest / 60.0
        else:
            pop = largest / 120.0
        kind = {"capital": "столица", "habitat": "крупный город", "food_habitat": "аграрный город", "mine": "шахтёрский посёлок",
                "refinery": "промышленный город", "factory": "промышленный город", "electronics_fab": "промышленный город",
                "shipyard": "город-верфь", "arsenal_shipyard": "военная верфь", "gas_platform": "газовые платформы",
                "ice_export": "ледовый промысел", "logistics_hub": "орбитальный порт / биржа", "legal_hub": "орбитальный порт / реестр",
                "station_core": "станция-ядро", "jump_infrastructure": "маячная станция", "black_market": "вольный порт",
                "fortress": "военная база", "intel": "закрытый объект", "research": "научное поселение"}.get(role, role)
        rows.append(dict(name=st["name"], role=kind, body=st["body"], pop=round(pop), status=st["knowledge"]["status"]))
    named_sum = sum(r["pop"] for r in rows)
    if named_sum > P * 0.95:
        f = P * 0.95 / named_sum
        for r in rows:
            r["pop"] = round(r["pop"] * f)
    rest = P - sum(r["pop"] for r in rows)
    agg = dict(name=f"прочие поселения ({max(0, n_set - len(rows))})", role="агрегат", body="—", pop=round(max(0.0, rest)), status="PROPOSAL")
    return rows + [agg], dict(settlements_total=n_set, largest_city=round(largest))


# ---------------------------------------------------------------- demand / capability
def demand(I, p):
    """Final demand by commodity, t/year: households + habitat construction and upkeep + weapons."""
    P = p["pop"]
    y = p["income"]
    d = {c: 0.0 for c in I.cats}
    for c in I.cats:
        pc = I.cat[c]["per_capita_t_y"]
        if c in ("consumer", "luxury", "medical", "electronics", "controlled"):
            pc *= math.sqrt(max(y, 0.3) / 3.0)
        d[c] += pc * P
    hp = I.PE["habitat_tonnes_per_place"]
    h = p["habitat"]
    if p["id"] == SOL:
        t1, t2, t3 = h["earth_surface"], h["sol_burrowed"], h["sol_rotating"]
    else:
        t1, t2, t3 = h["surface_and_underground"], h["burrowed_moons_asteroids"], h["rotating_habitats"]
    g = max(0.0, p["growth"])
    places_t = t1 * hp["tier1"] + t2 * hp["tier2"] + t3 * hp["tier3"]
    d["construction"] += places_t * (g + hp["maintenance_share"])
    # households and habitats: life support, light, heat (power_kw_per_capita) -> fusion fuel
    d["reactor_fuel"] += P * I.PE["power_kw_per_capita"] * 1e3 * SECONDS_Y / (3.5e14 * I.PE["fusion_electric_efficiency"]) / 1000.0
    return d


def energy_fuel_t_per_t(I, c):
    """Reactor fuel (t) per tonne of output from the commodity's energy use."""
    return I.cat[c]["energy_mwh_per_t"] * 3.6e9 / (3.5e14 * I.PE["fusion_electric_efficiency"]) / 1000.0


def capability(I, p, c):
    """Share of own demand a polity can produce itself (endowment x technology); 1 = self-sufficient."""
    if p["id"] == SOL:
        e = I.E[0]["idx"]
    else:
        e = I.E[p["system"]]["idx"]
    t = p["tech"]
    if c == "food":
        return clamp(0.95 + 0.05 * e["agri"] - (0.15 if e["water"] < 0.1 else 0.0))
    if c == "water_volatiles":
        return clamp(0.25 + 3.0 * e["water"] + 2.0 * e["volatiles"])
    if c == "ore":
        return clamp(0.15 + 2.2 * e["metals"])
    if c == "metals":
        return clamp((0.15 + 2.2 * e["metals"]) * (0.5 + 0.5 * t.get("metallurgy", 0.5)))
    if c == "chemicals":
        return clamp(0.3 + 2.0 * e["volatiles"])
    if c == "construction":
        return clamp(0.35 + 2.0 * e["metals"] + e["water"])
    if c == "reactor_fuel":
        # He-3 from gas giants, or D-D fusion with deuterium from water (dearer, needs energy tech)
        return clamp(2.0 * e["he3"] + 1.5 * e["water"] * min(1.0, t.get("energy", 0.5)))
    if c == "jump_charge":
        return clamp(1.4 * e["strategic"]) if t.get("jump", 0) > 0.3 else 0.0
    if c == "machinery":
        return clamp(0.15 + 0.9 * t.get("manufacturing", 0.5))
    if c == "electronics":
        return clamp(t.get("electronics", 0.0) ** 1.5)
    if c == "medical":
        return clamp(0.1 + 0.9 * t.get("bio", 0.0))
    if c == "consumer":
        return clamp(0.6 + 0.4 * t.get("manufacturing", 0.5))
    if c == "luxury":
        return clamp(0.45 + 0.35 * e["agri"])
    if c == "controlled":
        blk = any(a["type"] == "black_market" for a in p["assets"])
        return 1.0 if blk else (0.5 if I.C["regime_family"].get(p["regime"]) == "open" else 0.2)
    if c == "weapons":
        return clamp(0.2 + 0.8 * t.get("military", 0.0))
    return 1.0


# ---------------------------------------------------------------- transport
class Transport:
    def __init__(self, I, edges, unbeaconed, expedition):
        self.I = I
        self.edges = edges
        self.unb = unbeaconed
        self.exped = expedition
        self.cache = {}

    def route(self, a, b, blocked=frozenset()):
        if a == b:
            return [a]
        return shortest_path(self.edges, a, b, blocked)

    def hops(self, path):
        return [self.edges[x][y] for x, y in zip(path, path[1:])]

    def klass(self, c, tonnes):
        if c in ("reactor_fuel", "jump_charge"):
            return "jump_tanker" if tonnes >= 2.0e5 else "medium_freighter"
        if c in BULK:
            return "jump_train" if tonnes >= 2.0e5 else "medium_freighter"
        if c == "controlled":
            return "cobrett_i_n1"
        if c in HIGH:
            return "medium_freighter" if tonnes >= 5.0e3 else "cobrett_i_n1"
        return "medium_freighter" if tonnes >= 2.0e4 else "cobrett_i_n1"

    def payload(self, sc_key, c):
        sc = self.I.SE["ship_classes"][sc_key]
        dens = self.I.cat[c]["density_t_m3"]
        return min(sc["cargo_t"], sc["cargo_m3"] * dens) if sc["cargo_m3"] else sc["cargo_t"]

    def per_tonne(self, sc_key, c, path):
        key = (sc_key, c, tuple(path))
        if key not in self.cache:
            hops = self.hops(path)
            load = self.payload(sc_key, c)
            value = load * self.I.cat[c]["cost_cr_per_t"]
            tc = lg.trip_cost(self.I.SE, self.I.cal, sc_key, hops, load, value)
            unb = sum(1 for x, y in zip(path, path[1:]) if tuple(sorted((x, y))) in self.unb)
            exp = sum(1 for x, y in zip(path, path[1:]) if tuple(sorted((x, y))) in self.exped)
            cost = tc["cr_per_tonne"] - tc["costs_cr"]["customs"] / load
            cost *= (1.0 + 0.5 * (unb - exp) / max(1, len(hops))) * (1.0 + 2.0 * exp / max(1, len(hops)))
            days = tc["trip_days"] * (1.0 + 0.4 * unb / max(1, len(hops)))
            self.cache[key] = dict(cr_per_t=cost, trip=tc, trip_days=days, load=load, unbeaconed_hops=unb, expedition_hops=exp)
        return self.cache[key]


# ---------------------------------------------------------------- the model
class Model:
    def __init__(self, I, shock=None):
        self.I = I
        self.shock = shock or {}
        self.edges, self.nodes, self.unb, self.exped = trade_graph(I)
        if self.shock.get("lost_beacon") is not None:
            s = self.shock["lost_beacon"]
            # the bridge beacon is gone: jumps to / from s are unbeaconed (dearer, slower, riskier)
            for x in sorted(self.edges.get(s, {})):
                self.unb.add(tuple(sorted((s, x))))
        self.T = Transport(I, self.edges, self.unb, self.exped)
        self.pols = build_polities(I)
        self.order = sorted(self.pols, key=lambda k: (-self.pols[k]["pop"], k))

    # -- capability with shocks
    def cap(self, p, c):
        v = capability(self.I, p, c)
        for (pid, cc), f in self.shock.get("capacity", {}).items():
            if pid == p["id"] and cc == c:
                v *= f
        return v

    def hostile(self, a, b):
        return self.I.rel.get(tuple(sorted((a, b))), 0) >= 60 or (a, b) in self.shock.get("war", set()) or (b, a) in self.shock.get("war", set())

    def sanctioned(self, a, b):
        return (a, b) in self.shock.get("sanctions", set()) or (b, a) in self.shock.get("sanctions", set())

    def run(self):
        I = self.I
        P = self.pols
        self.final = {k: demand(I, p) for k, p in P.items()}
        for k, add in self.shock.get("extra_demand", {}).items():
            for c, v in add.items():
                self.final[k][c] += v
        # industrial demand through recipes: iterate (production -> inputs -> production)
        need = {k: dict(v) for k, v in self.final.items()}
        for _ in range(6):
            prod_req = {k: dict(v) for k, v in need.items()}
            inputs = {k: {c: 0.0 for c in I.cats} for k in P}
            for k in P:
                for c in I.cats:
                    q = prod_req[k][c] * self.cap(P[k], c)
                    for ic, f in I.cat[c]["inputs"].items():
                        inputs[k][ic] += q * f
                    inputs[k]["reactor_fuel"] += q * energy_fuel_t_per_t(I, c)
            new = {k: {c: self.final[k][c] + inputs[k][c] for c in I.cats} for k in P}
            need = new
        self.need = need
        self.trade()
        self.balance()
        return self

    # -- interstellar trade: deficits filled from the cheapest delivered source
    def trade(self):
        I = self.I
        P = self.pols
        flows = []
        self.imports = {k: {c: 0.0 for c in I.cats} for k in P}
        self.exports = {k: {c: 0.0 for c in I.cats} for k in P}
        self.unmet = {k: {c: 0.0 for c in I.cats} for k in P}
        for c in I.cats:
            exporters = [k for k in sorted(P) if self.cap(P[k], c) >= 0.9 and not P[k]["refuses"]]
            for k in self.order:
                p = P[k]
                deficit = self.need[k][c] * (1.0 - self.cap(p, c))
                if deficit <= 1.0:
                    continue
                if p["refuses"]:
                    self.unmet[k][c] += deficit       # autarky by choice: substitutes / goes without
                    continue
                if I.cat[c].get("local_only") and self.cap(p, c) > 0.5:
                    self.unmet[k][c] += deficit
                    continue
                offers = []
                for q in exporters:
                    if q == k:
                        continue
                    channel = "official"
                    blocked = frozenset()
                    path = self.T.route(P[q]["system"], p["system"])
                    markup = 1.0
                    via = None
                    if self.hostile(k, q) or self.sanctioned(k, q):
                        # legal channel closed: re-export through the cheapest neutral hub
                        best = None
                        for h in exporters + [x for x in sorted(P) if x not in exporters]:
                            if h in (k, q) or P[h]["refuses"] or self.hostile(h, k) or self.hostile(h, q) or self.sanctioned(h, k):
                                continue
                            p1 = self.T.route(P[q]["system"], P[h]["system"])
                            p2 = self.T.route(P[h]["system"], p["system"])
                            if p1 is None or p2 is None:
                                continue
                            ln = len(p1) + len(p2)
                            if best is None or ln < best[0]:
                                best = (ln, h, p1 + p2[1:])
                        if best is None:
                            continue
                        via, path = best[1], best[2]
                        channel = "re_export"
                        markup = 1.25
                    if path is None:
                        continue
                    t = self.T.klass(c, deficit)
                    tr = self.T.per_tonne(t, c, path)
                    prod_cost = I.cat[c]["cost_cr_per_t"] * (1.25 - 0.3 * self.cap(P[q], c))
                    delivered = (prod_cost + tr["cr_per_t"]) * markup * (1 + I.SE["fees"]["customs_share_of_cargo_value"])
                    offers.append((round(delivered, 6), q, path, channel, via, t, tr))
                if not offers:
                    self.unmet[k][c] += deficit
                    continue
                offers.sort(key=lambda o: (o[0], o[1]))
                # no-local-source rule: bulk goes interstellar only if the delivered import is cheaper
                # than the polity's own marginal source (scarce local deposits cost 3x)
                local_marginal = I.cat[c]["cost_cr_per_t"] * 3.0
                if c in BULK and self.cap(p, c) > 0.3 and offers[0][0] > local_marginal:
                    self.unmet[k][c] += deficit
                    continue
                split = [0.6, 0.3, 0.1][:len(offers)]
                ssum = sum(split)
                for w, o in zip(split, offers):
                    tonnes = deficit * w / ssum
                    delivered, q, path, channel, via, t, tr = o
                    if tonnes < 1.0:
                        continue
                    t = self.T.klass(c, tonnes)
                    tr = self.T.per_tonne(t, c, path)
                    flows.append(dict(seller=q, buyer=k, commodity=c, tonnes=tonnes, path=path, ship_class=t, channel=channel, via=via,
                                      transport_cr_per_t=tr["cr_per_t"], delivered_cr_per_t=delivered,
                                      value_cr=tonnes * I.cat[c]["cost_cr_per_t"], unbeaconed_hops=tr["unbeaconed_hops"]))
                    self.imports[k][c] += tonnes
                    self.exports[q][c] += tonnes
        # war / smuggling channel for controlled goods into strict regimes (always a little)
        self.flows = flows

    # -- production = local share of needs + exports; balance identity per commodity
    def balance(self):
        I = self.I
        P = self.pols
        self.prod = {}
        self.bal = {}
        loss = 0.002            # transit losses, booked to the exporter (shipped = delivered x (1 + loss))
        for k in P:
            self.prod[k] = {}
            self.bal[k] = {}
            for c in I.cats:
                local = self.need[k][c] * self.cap(P[k], c)
                prod = local + self.exports[k][c] * (1 + loss)
                self.prod[k][c] = prod
                consumption = self.need[k][c] - self.unmet[k][c]
                losses = self.exports[k][c] * loss
                stock_change = prod + self.imports[k][c] - consumption - self.exports[k][c] - losses
                self.bal[k][c] = dict(production=prod, imports=self.imports[k][c], consumption=consumption, exports=self.exports[k][c],
                                      losses=losses, stock_change=stock_change, unmet=self.unmet[k][c])


# ---------------------------------------------------------------- infrastructure
OUTPUT_ASSET = {"ore": "mine", "water_volatiles": "ice_mine", "metals": "metallurgy", "chemicals": "chemical_plant",
                "construction": "construction_works", "machinery": "factory", "consumer": "consumer_factory",
                "electronics": "electronics_fab", "food": "agro_module", "medical": "biotech_plant", "luxury": "luxury_works",
                "controlled": "drug_lab", "weapons": "arms_works", "reactor_fuel": "gas_platform", "jump_charge": "charge_refinery"}
NAMED_TYPE = {"mine": "mine", "gas_platform": "gas_platform", "refinery": "metallurgy", "factory": "factory", "electronics_fab": "electronics_fab",
              "shipyard": "civil_shipyard", "arsenal_shipyard": "military_shipyard", "food_habitat": "agro_module", "ice_export": "ice_mine",
              "logistics_hub": "trade_hub", "legal_hub": "trade_hub", "station_core": "orbital_port", "fortress": "military_base",
              "research": "research_station", "black_market": "trade_hub", "intel": "military_base"}


def asset_counts(M, k, fleet_k, yard_need_k, defence_k):
    I = M.I
    p = M.pols[k]
    A = I.PE["asset_types"]
    out = {}
    util = 0.85
    for c, a in OUTPUT_ASSET.items():
        q = M.prod[k][c]
        cap = A[a]["capacity_t_y"]
        n = math.ceil(q / (cap * util)) if q > 0.5 else 0
        out[a] = dict(count=n, output_t_y=q, capacity_t_y=n * cap)
    # habitat construction yards: rotating / burrowed places built per year
    out["habitat_yard"] = dict(count=math.ceil(M.final[k]["construction"] * 0.6 / (A["habitat_yard"]["capacity_t_y"] * util)) if M.final[k]["construction"] > 0 else 0)
    # power: population + industry
    ind_mw = sum(out[a]["count"] * A[a]["power_mw"] for a in out if a in A)
    pop_mw = p["pop"] * I.PE["power_kw_per_capita"] / 1000.0
    out["power_plant"] = dict(count=math.ceil((ind_mw + pop_mw) / A["power_plant"]["capacity_mw"] / util), demand_mw=ind_mw + pop_mw)
    thr = sum(M.imports[k].values()) + sum(M.exports[k].values())
    domestic = sum(M.prod[k][c] for c in I.cats if c not in ("ore", "water_volatiles")) * 0.25
    out["trade_hub"] = dict(count=max(1, math.ceil(thr / A["trade_hub"]["throughput_t_y"])) if thr > 0 else (1 if p["pop"] > 1e6 else 0), throughput_t_y=thr)
    out["orbital_port"] = dict(count=max(1, math.ceil((thr + domestic) / A["orbital_port"]["throughput_t_y"]) + math.ceil(p["pop"] / 2.5e7)), throughput_t_y=thr + domestic)
    stock = (sum(M.prod[k].values()) - M.prod[k]["ore"] - M.prod[k]["water_volatiles"]) * 30.0 / 365.0
    out["warehouse"] = dict(count=max(1, math.ceil(stock / A["warehouse"]["capacity_t"])), stock_t=stock)
    out["fuel_depot"] = dict(count=0)       # set by fuel balance
    ships = sum(v["active"] for v in fleet_k.values()) if fleet_k else 0
    out["repair_yard"] = dict(count=max(1, math.ceil(ships / A["repair_yard"]["ships_served"])) if ships else 0, ships=ships)
    out["civil_shipyard"] = dict(count=0)
    out["small_yard"] = dict(count=0)
    out["military_shipyard"] = dict(count=0)
    if yard_need_k:
        civ = yard_need_k["civil_dry_t_y"]
        mil = yard_need_k["military_dry_t_y"]
        out["civil_shipyard"]["count"] = math.floor(civ * 0.8 / A["civil_shipyard"]["dry_t_y"])
        rest = civ - out["civil_shipyard"]["count"] * A["civil_shipyard"]["dry_t_y"]
        out["small_yard"]["count"] = max(0, math.ceil(rest / A["small_yard"]["dry_t_y"]))
        out["military_shipyard"]["count"] = math.ceil(mil / A["military_shipyard"]["dry_t_y"]) if mil > 0 else 0
    out["research_station"] = dict(count=math.ceil(p["pop"] * p["tech"].get("research", 0.5) / A["research_station"]["per_people"]))
    if defence_k:
        out["military_base"] = dict(count=defence_k["bases"]["military"])
        out["patrol_base"] = dict(count=defence_k["bases"]["patrol"])
    return out


def asset_derived(I, counts):
    """Workforce, power, equipment, wear and spare parts per asset type."""
    A = I.PE["asset_types"]
    rows = {}
    tot = dict(workers=0.0, power_mw=0.0, equipment_t=0.0, spare_parts_t_y=0.0)
    for a, v in sorted(counts.items()):
        if a not in A:
            continue
        n = v.get("count", 0)
        w = n * A[a]["workers"]
        pw = n * A[a]["power_mw"]
        eq = n * A[a]["equipment_t"]
        sp = eq * A[a]["wear_per_year"]
        rows[a] = dict(count=n, workers=round(w), power_mw=round(pw), equipment_t=round(eq), spare_parts_t_y=round(sp), name=A[a]["name"])
        for kk, x in (("workers", w), ("power_mw", pw), ("equipment_t", eq), ("spare_parts_t_y", sp)):
            tot[kk] += x
    return rows, {k: round(v) for k, v in tot.items()}


def major_and_named(I, M, k, counts):
    """Major = sites well above the standard size (Pareto tail ~ sqrt of the count); strategic named =
    named assets of world.json / settlements.json."""
    p = M.pols[k]
    named = {}
    for a in p["assets"]:
        t = NAMED_TYPE.get(a["type"])
        if t:
            named.setdefault(t, []).append(dict(id=a["id"], name=a["name"], importance=a["importance"]))
    out = {}
    for a, v in counts.items():
        n = v.get("count", 0)
        out[a] = dict(total=n, major=min(n, int(round(math.sqrt(n)))) if n >= 4 else n, named=named.get(a, []))
    return out


def mines_detail(I, M, k, counts):
    """Mines are not infinite ore generators: reserve, grade, power, workers, automation, equipment
    stock and wear, spare parts, fuel, storage, transport, security; mined volume and excavation
    sites (the data a procedural asteroid can render later)."""
    p = M.pols[k]
    sysid = p["system"]
    E = I.E[sysid]
    A = I.PE["asset_types"]
    rows = []
    for a, res, idx_key, dens in (("mine", "ore", "metals", 2.4), ("ice_mine", "water_volatiles", "water", 0.95)):
        n = counts[a]["count"]
        if n == 0:
            continue
        out = M.prod[k][res]
        grade = I.E[sysid]["idx"][idx_key]
        reserve_t = (E.get("metals_s", 0.0) if res == "ore" else E.get("water_s", 0.0))
        eq = n * A[a]["equipment_t"]
        power = n * A[a]["power_mw"]
        fuel_t = power * 1e6 * SECONDS_Y / (3.5e14 * I.PE["fusion_electric_efficiency"]) / 1000.0
        mined_m3 = out / dens
        belt = E["belt_me"] > 0.001 or E["comet_me"] > 0.5
        rows.append(dict(asset=a, resource=res, count=n, annual_output_t=round(out), grade=round(grade, 2),
                         estimated_reserve_index=round(reserve_t, 3), workers=n * A[a]["workers"],
                         automation=round(p["automation"], 2), power_mw=power, equipment_stock_t=eq,
                         equipment_wear_per_year=A[a]["wear_per_year"], spare_parts_t_y=round(eq * A[a]["wear_per_year"]),
                         reactor_fuel_t_y=round(fuel_t, 2), storage_t=round(out * 20 / 365.0),
                         transport="рудные буксиры (плоты)" if belt else "челноки и рельсовые катапульты",
                         security="патрули полиции" if p["regime"] not in ("clan_contracts", "corporate_board") else "частная охрана",
                         mined_volume_m3_y=round(mined_m3), excavation_sites=n * 3, base_modules=n))
    return rows


# ---------------------------------------------------------------- fleets
def fleet_census(M):
    """Fleet from flows: interstellar (owner = seller), in-system shuttles / tugs / tankers / tenders.
    Ships = tonnes / (payload x loaded trips per year); + reserve + repair; new = retired + lost + growth."""
    I = M.I
    SE = I.SE["ship_classes"]
    need = {k: {} for k in M.pols}
    jumps = {k: 0.0 for k in M.pols}             # jumps per year (for the charge balance)
    insys_legs = {k: {} for k in M.pols}
    route_tonnage = {}
    for f in M.flows:
        sc = SE[f["ship_class"]]
        tr = M.T.per_tonne(f["ship_class"], f["commodity"], f["path"])
        rt_days = 2.0 * tr["trip_days"]
        trips = 365.0 * sc["utilisation"] / rt_days
        load = tr["load"] * 0.85
        ships = f["tonnes"] / (load * trips)
        o = f["seller"]
        need[o][f["ship_class"]] = need[o].get(f["ship_class"], 0.0) + ships
        loaded_trips = f["tonnes"] / load
        jumps[o] += loaded_trips * 2 * (len(f["path"]) - 1)
        for x, y in zip(f["path"], f["path"][1:]):
            e = tuple(sorted((x, y)))
            route_tonnage[e] = route_tonnage.get(e, 0.0) + f["tonnes"]
        f["ships"] = ships
        f["trips_per_year"] = loaded_trips
    for k, p in M.pols.items():
        thr = sum(M.imports[k].values()) + sum(M.exports[k].values())
        domestic = sum(M.prod[k][c] for c in I.cats if c not in ("ore", "water_volatiles", "construction")) * LOCAL["domestic_share"]
        habitat_freight = M.prod[k]["construction"] * LOCAL["habitat_shuttle_share"]
        shuttle_t = 0.6 * thr + domestic + habitat_freight
        oc = SE["ocs4_lastochka"]
        leg_h = local_leg_h(I, oc, LOCAL["shuttle_leg_au"])
        trips = 365.0 * oc["utilisation"] / ((2 * leg_h + 8) / 24.0)
        need[k]["ocs4_lastochka"] = shuttle_t / (oc["cargo_t"] * 0.8 * trips)
        tg = SE["ore_tug"]
        leg_h = local_leg_h(I, tg, LOCAL["raft_leg_au"])
        trips = 365.0 * tg["utilisation"] / ((2 * leg_h + 24) / 24.0)
        raft = M.prod[k]["ore"] * LOCAL["ore_raft_share"] + M.prod[k]["water_volatiles"] * LOCAL["ice_raft_share"] + M.prod[k]["construction"] * 0.3
        need[k]["ore_tug"] = raft / (tg["cargo_t"] * trips)
        gt = SE["gas_tanker"]
        leg_h = lg.insystem_leg(gt, I.SE["insystem_rule"], gt["cargo_t"] * 0.5)[0] / 3600.0
        trips = 365.0 * gt["utilisation"] / ((2 * leg_h + 12) / 24.0)
        # deuterium / He-3 plus volatiles feedstock from gas giants (x40 of the fuel mass)
        need[k]["gas_tanker"] = M.prod[k]["reactor_fuel"] * 40.0 / (gt["cargo_t"] * trips)
        insys_legs[k] = dict(shuttle=shuttle_t / (oc["cargo_t"] * 0.8), tug=raft / tg["cargo_t"])
        # couriers and charters: urgent parts, passengers, small valuable lots (business-aviation analogue)
        cour = p["pop"] / 1e6 * LOCAL["courier_per_million"] * p["income"] / 3.0
        need[k]["cobrett_i_n1"] = need[k].get("cobrett_i_n1", 0.0) + cour
        jumps[k] += cour * LOCAL["courier_jumps_per_year"]
        insys_legs[k]["courier_cobretts"] = cour
    return need, jumps, route_tonnage, insys_legs


LOCAL = {"domestic_share": 0.10, "habitat_shuttle_share": 0.05, "shuttle_leg_au": 0.05, "raft_leg_au": 0.1,
         "ore_raft_share": 0.5, "ice_raft_share": 0.3, "courier_per_million": 1.0, "courier_jumps_per_year": 8}


def local_leg_h(I, sc, au):
    """Brachistochrone time scales with sqrt(distance): a short local leg from the class's mean leg."""
    t = lg.insystem_leg(sc, I.SE["insystem_rule"], sc["cargo_t"] * 0.5)[0] / 3600.0
    return t * math.sqrt(au / I.SE["insystem_rule"]["mean_leg_au"])


def census_rows(I, need, growth):
    SE = I.SE["ship_classes"]
    rows = {}
    for cls, n in sorted(need.items()):
        if n <= 0:
            continue
        sc = SE[cls]
        active = math.ceil(n * (1.0 + sc["downtime_share"]))
        reserve = math.ceil(active * (0.2 if sc["family"] != "civil" else 0.05))
        repair = math.ceil(active * sc["downtime_share"])
        fleet = active + reserve
        retired = fleet / sc["service_life_years"]
        lost = fleet * sc["loss_rate_per_year"]
        new = retired + lost + max(0.0, growth) * fleet
        rows[cls] = dict(role=sc["role"], family=sc["family"], active=active, reserve=reserve, repair=repair,
                         new_per_year=round(new, 1), lost_per_year=round(lost, 1), retired_per_year=round(retired, 1),
                         dry_t_new_per_year=round(new * sc["dry_mass_t"]))
    return rows


# ---------------------------------------------------------------- defence
STRUCT = {"ships_procurement_operations_crews": 0.30, "bases_infrastructure": 0.14, "shore_personnel": 0.22, "intelligence": 0.07,
          "training": 0.06, "stockpiles_munitions_fuel": 0.09, "impact_defence_programme": 0.06, "beacon_security": 0.06}


def defence(M, k, fleet_civil, route_tonnage, beacons_k):
    """Security / defence budget = share of output from the doctrine and data; forces from tasks
    (customs, SAR, beacons, impact defence, convoys, recon) first, deterrence from the rest."""
    I = M.I
    p = M.pols[k]
    PE = I.PE
    T = PE["defence_tasks"]
    SE = I.SE["ship_classes"]
    E = I.E[p["system"]]
    doc = PE["doctrines"].get(k, {"why": "", "roles": []})
    roles = set(doc["roles"])
    comps = {}
    comps["law_customs_sar"] = 0.004
    orb = (p["habitat"].get("rotating_habitats", 0) + p["habitat"].get("burrowed_moons_asteroids", 0) + p["habitat"].get("sol_habitats", 0)) / max(p["pop"], 1)
    debris = clamp(math.log10(1 + 1e4 * E["belt_me"]) / 3.0 + math.log10(1 + 10 * E["comet_me"]) / 2.0)
    comps["impact_defence"] = 0.001 + 0.003 * orb * debris
    trade_share = sum(f["value_cr"] for f in M.flows if f["seller"] == k) / max(p["gdp"], 1.0)
    comps["route_protection"] = min(0.004, 0.02 * trade_share) + (0.001 if "convoy_protection" in roles or "piracy_suppression" in roles else 0.0)
    threat = 0.0
    hostile = []
    for (a, b), g in sorted(I.rel.items()):
        if k in (a, b) and g >= 30:
            o = b if a == k else a
            same = M.pols.get(o, {}).get("system") == p["system"]
            threat = max(threat, 0.00016 * g * (1.5 if same else 1.0))
            hostile.append(dict(other=o, grievance=g, same_system=same))
    comps["threat"] = threat
    comps["regime"] = PE["regime_security_premium"].get(p["regime"], 0.0)
    strat = 0.0
    if any(a["type"] == "jump_infrastructure" for a in p["assets"]):
        strat += 0.004
    strat += min(0.004, 0.0004 * beacons_k)
    if any(a["type"] in ("arsenal_shipyard", "intel") for a in p["assets"]):
        strat += 0.003
    comps["strategic_assets"] = strat
    internal = 0.0
    for pr in p["problems"]:
        if any(w in pr for w in ("пират", "контрабанд", "двоевласт", "раскол", "бунт", "наследств")):
            internal += 0.0015
    comps["internal"] = min(0.004, internal)
    share = clamp(sum(comps.values()), 0.003, 0.06)
    if not doc["roles"]:
        share = 0.001
    budget = share * p["gdp"]
    # ---- task-based forces
    arrivals = sum(f.get("trips_per_year", 0.0) for f in M.flows if f["buyer"] == k)
    task = {}
    task["patrol_cutter"] = beacons_k * T["patrol_per_beacon"] + arrivals / 1e4 * T["patrol_per_10k_arrivals"] + p["pop"] / 1e8 * T["patrol_per_100m_people_sar"]
    orbital_pop = orb * p["pop"]
    task["interceptor"] = orbital_pop / 1e8 * T["interceptor_per_100m_orbital"] + (T["interceptor_threat_bonus"] if hostile else 0)
    belt = 1 if (E["belt_me"] > 0.001 or E["comet_me"] > 0.5) else 0
    task["impact_defence_tug"] = orbital_pop / 1e8 * T["impact_tug_per_100m_orbital"] + belt * T["impact_tug_per_belt_system"] + 1
    risky = 0.0
    for f in M.flows:
        if f["seller"] != k:
            continue
        r = 1.0 if f["channel"] != "official" else 0.0
        for x in f["path"]:
            if x in (9, 10) or f["unbeaconed_hops"]:
                r = 1.0
        if f["ship_class"] in ("jump_train", "jump_tanker"):
            risky += f.get("trips_per_year", 0.0) * r
    sc = SE["escort_frigate"]
    task["escort_frigate"] = risky * T["escort_days_per_risky_jump_trip"] / (365.0 * sc["utilisation"]) + (4 if "convoy_protection" in roles else 0)
    task["recon_picket"] = len(hostile) * T["recon_per_hostile_neighbour"] + (2 if beacons_k else 0)
    if not doc["roles"]:
        task = {x: 0.0 for x in task}
    annual = {}
    for cls in ("patrol_cutter", "interceptor", "impact_defence_tug", "escort_frigate", "recon_picket", "line_combatant", "fleet_tender"):
        s = SE[cls]
        price = lg.build_cost(s)["list_price_cr"]
        fuel = lg.insystem_leg(s, I.SE["insystem_rule"], 0.5 * s["cargo_t"])[1] * 20 * I.SE["anchors"]["reactor_fuel_cr_per_kg"]
        annual[cls] = price / s["service_life_years"] + price * s["maintenance_share_per_year"] + \
            s["crew"] * (I.SE["anchors"]["pilot_wage_cr_per_month"] * 12 * T["military_wage_factor"] + T["crew_per_ship_year_overhead_cr"]) + fuel
    ships_budget = budget * STRUCT["ships_procurement_operations_crews"]
    task_cost = sum(task[c] * annual[c] for c in task)
    scale = 1.0 if task_cost <= ships_budget * 0.85 else ships_budget * 0.85 / task_cost
    forces = {c: task[c] * scale for c in task}
    rest = ships_budget - task_cost * scale
    # deterrence gets most of what the tasks leave; the rest stays in readiness (stocks, reserve crews)
    deter_w = 0.7 if ("deterrence" in roles or "regime_survival" in roles) else 0.25
    lc = rest * deter_w / (annual["line_combatant"] + annual["fleet_tender"] / T["tender_per_combatants"])
    forces["line_combatant"] = lc
    forces["fleet_tender"] = (lc + forces["escort_frigate"]) / T["tender_per_combatants"]
    readiness = max(0.0, rest - lc * annual["line_combatant"] - forces["fleet_tender"] * annual["fleet_tender"])
    under = scale < 1.0
    structure = dict(STRUCT)
    total_ships = sum(forces.values())
    bases = dict(military=math.ceil((forces["line_combatant"] + forces["escort_frigate"] + forces["fleet_tender"] + forces["recon_picket"]) / I.PE["asset_types"]["military_base"]["ships"]) if total_ships > 1 else 0,
                 patrol=math.ceil((forces["patrol_cutter"] + forces["interceptor"] + forces["impact_defence_tug"]) / I.PE["asset_types"]["patrol_base"]["ships"]) if total_ships > 1 else 0)
    personnel = sum(forces[c] * SE[c]["crew"] for c in forces) * 1.8 + bases["military"] * 3000 + bases["patrol"] * 500
    st = {a: round(b * budget) for a, b in structure.items()}
    st["readiness_reserve_from_ship_budget"] = round(readiness)
    return dict(share_of_output=round(share, 4), components={a: round(b, 4) for a, b in comps.items()}, budget_cr_y=budget,
                why=doc["why"], roles=doc["roles"], hostile=hostile, task_ships={c: round(v, 1) for c, v in task.items()},
                forces=forces, annual_cost_per_ship=annual, under_resourced=under, structure=st,
                bases=bases, personnel=round(personnel), risky_train_trips=round(risky, 1), arrivals_per_year=round(arrivals))


# ---------------------------------------------------------------- beacons and navigation
JUMP_ERROR = {
    "base_fraction": 1.0e-9,
    "lock": {"both_ends": 200.0, "origin_only": 12.0, "destination_only": 6.0, "none": 1.0},
    "drive_quality": {"new": 0.8, "standard": 1.0, "worn": 1.6, "pirated_copy": 2.5},
    "computer": {"military": 0.7, "standard": 1.0, "obsolete": 1.8},
    "ephemeris_age_factor_per_year": 0.6,
    "damage_factor": {"none": 1.0, "light": 1.5, "heavy": 4.0},
    "velocity_error_km_s_per_1e5_km": 0.3,
    "note": "sigma_r = d * base_fraction * drive * computer * (1 + age_years * f_age) * damage / lock. One sigma; 3 sigma is the planning box. "
            "Velocity error ~ 0.3 km/s per 1e5 km of position error. Deterministic and predictable as a range, not a roulette.",
}


def jump_error_km(d_ly, lock="both_ends", drive="standard", computer="standard", eph_age_y=0.0, damage="none"):
    J = JUMP_ERROR
    d_km = d_ly * 9.4607e12
    s = d_km * J["base_fraction"] * J["drive_quality"][drive] * J["computer"][computer] * (1 + eph_age_y * J["ephemeris_age_factor_per_year"])
    s *= J["damage_factor"][damage] / J["lock"][lock]
    return s, s / 1e5 * J["velocity_error_km_s_per_1e5_km"]


def beacons_model(M, defence_by, route_tonnage, infra):
    """Physical beacons: the bridge beacon of each networked system (world.json: operator, year),
    hub / station / relay / military / remote-science beacons from infrastructure and traffic."""
    I = M.I
    rows = []
    by_sys = {}
    sysowner = {}
    for k, p in M.pols.items():
        sysowner.setdefault(p["system"], k)
        for s in p["sites"]:
            sysowner.setdefault(s["system"], k)
    trunk_sys = {}
    for (a, b), t in route_tonnage.items():
        if t >= 2.0e7:
            trunk_sys[a] = trunk_sys.get(a, 0) + 1
            trunk_sys[b] = trunk_sys.get(b, 0) + 1
    for b in sorted(I.W["beacons"], key=lambda x: x["system"]):
        s = b["system"]
        own = b["operator"]
        rows.append(dict(id=f"BCN-{s:03d}-BR", system=s, kind="bridge", operator=own, since=b["year"],
                         motion="естественная орбита у точки, безопасной для прыжка (L4/L5 крупного тела)",
                         broadcasts=["position", "velocity", "precise_time", "network_id", "health"], coverage_ls=60,
                         transponder_tracks=5000, staff=40, cost_cr_y=round(9.0e7), guard=own, status=b["knowledge"]["status"]))
        by_sys[s] = by_sys.get(s, 0) + 1
    extra = {}
    for k, p in M.pols.items():
        inf = infra[k]
        s = p["system"]
        n_hub = inf["trade_hub"]["count"]
        n_port = inf["orbital_port"]["count"]
        n_mil = inf.get("military_base", {}).get("count", 0)
        n_res = inf["research_station"]["count"]
        relay = 2 * trunk_sys.get(s, 0)
        e = extra.setdefault(s, dict(trade_hub=0, station=0, relay=0, military=0, remote_science=0))
        e["trade_hub"] += n_hub
        e["station"] += min(n_port, 400)
        e["relay"] += relay
        e["military"] += n_mil
        e["remote_science"] += max(0, n_res // 8)
        for st in p["sites"]:
            e2 = extra.setdefault(st["system"], dict(trade_hub=0, station=0, relay=0, military=0, remote_science=0))
            e2["remote_science"] += 1
    totals = {"bridge": len(rows)}
    for s, e in extra.items():
        for kk, v in e.items():
            totals[kk] = totals.get(kk, 0) + v
    cost = {"bridge": 9.0e7, "trade_hub": 3.0e7, "station": 6.0e6, "relay": 4.0e7, "military": 5.0e7, "remote_science": 2.0e7}
    systems = []
    for s in sorted(set(list(extra) + list(by_sys))):
        e = extra.get(s, {})
        n = by_sys.get(s, 0) + sum(e.values())
        systems.append(dict(system=s, name=I.E[s]["name"] if s in I.E else str(s), owner=sysowner.get(s), bridge=by_sys.get(s, 0),
                            **{kk: e.get(kk, 0) for kk in ("trade_hub", "station", "relay", "military", "remote_science")}, total=n,
                            cost_cr_y=round(by_sys.get(s, 0) * cost["bridge"] + sum(e.get(kk, 0) * cost[kk] for kk in e))))
    examples = []
    for lock in ("both_ends", "origin_only", "none"):
        for age in (0.0, 5.0):
            r, v = jump_error_km(20.064, lock=lock, eph_age_y=age)
            examples.append(dict(d_ly=20.064, lock=lock, ephemeris_age_y=age, sigma_km=round(r), sigma_v_km_s=round(v, 3)))
    r, v = jump_error_km(20.064, lock="none", drive="pirated_copy", eph_age_y=5.0, damage="light")
    examples.append(dict(d_ly=20.064, lock="none", ephemeris_age_y=5.0, drive="pirated_copy", damage="light", sigma_km=round(r), sigma_v_km_s=round(v, 3)))
    return dict(bridge=rows, by_system=systems, totals=totals, jump_error_model=JUMP_ERROR, jump_error_examples=examples,
                transponder_fields=["id", "position", "velocity", "heading", "destination", "emergency_status", "cargo_declaration"],
                light_lag_rule="вне покрытия маяка связь идёт со скоростью света: данные устаревают, карта показывает последний известный трек и эллипс неопределённости",
                towing={"note": "маяк можно отбуксировать: дорого, медленно, регистрируется сетью, меняет навигационную геометрию",
                        "tug": "impact_defence_tug или ore_tug", "speed_km_s": 5.0, "cost_cr_per_au": 2.5e6})


# ---------------------------------------------------------------- the whole balance
def compute(I, shock=None):
    M = Model(I, shock).run()
    # second pass: the fleet's own reactor fuel and jump charge become demand of the owner polity
    R0 = compute_once(I, M)
    sh = dict(shock or {})
    extra = {k: dict(v) for k, v in sh.get("extra_demand", {}).items()}
    for k, d in R0["defence"].items():
        e = extra.setdefault(k, {})
        # munitions: 70 % of the stockpile budget buys weapons and ammunition
        e["weapons"] = e.get("weapons", 0.0) + d["budget_cr_y"] * STRUCT["stockpiles_munitions_fuel"] * 0.7 / I.cat["weapons"]["cost_cr_per_t"]
    for k, f in R0["fuel"].items():
        e = extra.setdefault(k, {})
        e["reactor_fuel"] = e.get("reactor_fuel", 0.0) + f["reactor_fuel_ships_t_y"]
        e["jump_charge"] = e.get("jump_charge", 0.0) + f["jump_charge_t_y"]
    sh["extra_demand"] = extra
    M = Model(I, sh).run()
    return compute_once(I, M)


def compute_once(I, M):
    need, jumps, route_tonnage, insys = fleet_census(M)
    SE = I.SE["ship_classes"]
    # beacons operated per polity
    beacons_op = {}
    for b in I.W["beacons"]:
        beacons_op[b["operator"]] = beacons_op.get(b["operator"], 0) + 1
    # first infrastructure pass (no fleet / defence yet) -> defence -> fleets -> yards -> second pass
    infra0 = {k: asset_counts(M, k, None, None, None) for k in M.pols}
    deff = {k: defence(M, k, None, route_tonnage, beacons_op.get(k, 0)) for k in M.pols}
    census = {}
    for k in M.pols:
        n = dict(need[k])
        for c, v in deff[k]["forces"].items():
            n[c] = n.get(c, 0.0) + v
        sv = sum(infra0[k][a]["count"] for a in ("mine", "ice_mine", "metallurgy", "chemical_plant", "gas_platform", "orbital_port", "trade_hub"))
        n["service_tender"] = (sv + beacons_op.get(k, 0)) / 40.0
        census[k] = census_rows(I, n, M.pols[k]["growth"])
    yards = shipyards(M, census)
    infra = {k: asset_counts(M, k, census[k], yards["by_polity"][k], deff[k]) for k in M.pols}
    fuel = fuel_balance(M, census, jumps, infra)
    for k in M.pols:
        infra[k]["fuel_depot"] = dict(count=fuel[k]["depots"])
    beacons = beacons_model(M, deff, route_tonnage, infra)
    routes = route_classes(M, route_tonnage, deff)
    return dict(M=M, census=census, defence=deff, infra=infra, yards=yards, fuel=fuel, beacons=beacons, routes=routes,
                route_tonnage=route_tonnage, jumps=jumps)


def shipyards(M, census):
    """New-build demand (dry tonnes / year) vs yard capacity; ships of polities without yards come from
    their largest trade partner with yards; jump drives from the core makers."""
    I = M.I
    SE = I.SE["ship_classes"]
    by = {}
    for k in M.pols:
        civ = sum(v["dry_t_new_per_year"] for c, v in census[k].items() if SE[c]["family"] == "civil")
        mil = sum(v["dry_t_new_per_year"] for c, v in census[k].items() if SE[c]["family"] != "civil")
        jd = sum(v["new_per_year"] for c, v in census[k].items() if SE[c]["jump"])
        named_yard = any(a["type"] in ("shipyard", "arsenal_shipyard") for a in M.pols[k]["assets"])
        by[k] = dict(civil_dry_t_y=civ, military_dry_t_y=mil, jump_drives_y=jd,
                     has_yard_tech=named_yard or M.pols[k]["refuses"] or (M.pols[k]["tech"].get("shipbuilding", 0) > 0.3 and M.pols[k]["pop"] > 2e7))
    # polities without their own yards buy from the partner they trade most with that has yards
    imports = {}
    for k in sorted(by):
        if by[k]["has_yard_tech"]:
            continue
        partners = {}
        for f in M.flows:
            if f["buyer"] == k and by.get(f["seller"], {}).get("has_yard_tech"):
                partners[f["seller"]] = partners.get(f["seller"], 0.0) + f["value_cr"]
        src = sorted(partners.items(), key=lambda kv: (-kv[1], kv[0]))[0][0] if partners else SOL
        imports[k] = src
        by[src]["civil_dry_t_y"] += by[k]["civil_dry_t_y"]
        by[src]["military_dry_t_y"] += by[k]["military_dry_t_y"]
        by[k]["supplied_by"] = src
        by[k]["civil_dry_t_y"] = 0.0
        by[k]["military_dry_t_y"] = 0.0
    core_makers = {}
    for j in I.W["jump_technology"]:
        if j["how"] in ("state_secret", "own_math", "own", "reverse_engineered"):
            core_makers[j["unit"]] = j["how"]
    if not core_makers:
        core_makers = {"tau_ceti_cognium": "state_secret"}
    tot_jd = sum(v["jump_drives_y"] for v in by.values())
    return dict(by_polity=by, imports=imports, core_makers=core_makers, jump_drives_total_y=round(tot_jd, 1))


def fuel_balance(M, census, jumps, infra):
    I = M.I
    SE = I.SE["ship_classes"]
    fe = I.PE["fusion_electric_efficiency"]
    out = {}
    for k, p in M.pols.items():
        power_mw = infra[k]["power_plant"]["demand_mw"]
        reactor_t = power_mw * 1e6 * SECONDS_Y / (3.5e14 * fe) / 1000.0
        goods_rf = M.need[k]["reactor_fuel"]
        ship_rf = 0.0
        jump_t = 0.0
        for cls, v in census[k].items():
            sc = SE[cls]
            leg_kg = lg.insystem_leg(sc, I.SE["insystem_rule"], 0.5 * sc["cargo_t"])[1]
            legs = v["active"] * sc["utilisation"] * 365.0 / max(1.0, 2 * lg.insystem_leg(sc, I.SE["insystem_rule"], 0.5 * sc["cargo_t"])[0] / 86400.0 + 0.3)
            ship_rf += legs * leg_kg / 1000.0
            if sc["jump"] and sc["family"] != "civil":
                prof = lg.jump_profile(I.cal, I.SE, sc)
                jump_t += v["active"] * 6 * prof["charge_per_mean_jump_t"]      # patrol / exercise jumps
        for f in M.flows:
            if f["seller"] != k:
                continue
            sc = SE[f["ship_class"]]
            prof = lg.jump_profile(I.cal, I.SE, sc)
            jump_t += f.get("trips_per_year", 0.0) * 2 * (len(f["path"]) - 1) * prof["charge_per_mean_jump_t"]
        cour = M.pols[k]["pop"] / 1e6 * LOCAL["courier_per_million"] * M.pols[k]["income"] / 3.0
        jump_t += cour * LOCAL["courier_jumps_per_year"] * lg.jump_profile(I.cal, I.SE, SE["cobrett_i_n1"])["charge_per_mean_jump_t"]
        rf_need = max(goods_rf, reactor_t + ship_rf)
        rf_local = M.prod[k]["reactor_fuel"] - M.exports[k]["reactor_fuel"]
        jc_local = M.prod[k]["jump_charge"] - M.exports[k]["jump_charge"]
        reserve_days = 180 if p["regime"] in ("security_oligarchy", "caste_theocracy", "party_technocracy") else 90
        res_rf = rf_need * reserve_days / 365.0
        res_jc = jump_t * reserve_days / 365.0
        depots = math.ceil((res_rf + res_jc) / I.PE["asset_types"]["fuel_depot"]["capacity_t"]) + (1 if jump_t > 0 else 0)
        out[k] = dict(reactor_fuel_t_y=round(rf_need, 1), reactor_fuel_power_t_y=round(reactor_t, 1), reactor_fuel_ships_t_y=round(ship_rf, 1),
                      reactor_fuel_local_t_y=round(min(rf_local, rf_need), 1), reactor_fuel_imports_t_y=round(M.imports[k]["reactor_fuel"], 1),
                      reactor_fuel_unmet_t_y=round(M.unmet[k]["reactor_fuel"], 1), jump_charge_unmet_t_y=round(M.unmet[k]["jump_charge"], 1),
                      jump_charge_t_y=round(jump_t, 1), jump_charge_local_t_y=round(min(jc_local, jump_t), 1),
                      jump_charge_imports_t_y=round(M.imports[k]["jump_charge"], 1), reserve_days=reserve_days,
                      strategic_reserve_reactor_t=round(res_rf, 1), strategic_reserve_charge_t=round(res_jc, 1), depots=depots,
                      reactor_fuel_cost_cr_y=round(rf_need * 1000 * I.SE["anchors"]["reactor_fuel_cr_per_kg"]),
                      jump_charge_cost_cr_y=round(jump_t * 1000 * I.cal["jump_charge_cr_per_kg"]))
        # the fuel commodities in the goods balance cover power and ships: add the ships' and jumps'
        # fuel to final demand would double count; the check compares need vs supply here
    return out


def route_classes(M, route_tonnage, deff):
    I = M.I
    value = {}
    for f in M.flows:
        for x, y in zip(f["path"], f["path"][1:]):
            e = tuple(sorted((x, y)))
            value[e] = value.get(e, 0.0) + f["value_cr"]
    rows = []
    sysowner = {}
    for k, p in M.pols.items():
        sysowner.setdefault(p["system"], k)
    for e in sorted(route_tonnage):
        t = route_tonnage[e]
        v = value.get(e, 0.0)
        cls = "trunk" if (t >= 2.0e7 or v >= 5.0e9) else "secondary" if (t >= 1.0e6 or v >= 5.0e8) else "feeder"
        guards = sorted({sysowner.get(s) for s in e if sysowner.get(s)})
        rows.append(dict(edge=list(e), names=[I.E[s]["name"] for s in e], ly=round(M.edges[e[0]][e[1]], 2), tonnes_y=round(t), value_cr_y=round(v),
                         klass=cls, unbeaconed=e in M.unb, expedition=e in M.exped, guarded_by=guards,
                         beacons_supporting=[f"BCN-{s:03d}-BR" for s in e if s in M.I.beacon_of]))
    return rows


# ---------------------------------------------------------------- vulnerabilities and scenarios
def vulnerabilities(R):
    M = R["M"]
    I = M.I
    out = {}
    for k, p in M.pols.items():
        items = []
        # single-source imports
        by = {}
        for f in M.flows:
            if f["buyer"] == k:
                by.setdefault(f["commodity"], []).append(f)
        for c, fs in sorted(by.items()):
            tot = sum(f["tonnes"] for f in fs)
            share = tot / max(M.need[k][c], 1.0)
            top = max(fs, key=lambda f: (f["tonnes"], f["seller"]))
            if share > 0.15:
                items.append(dict(kind="import_dependency", what=f"{I.cat[c]['name']}: {round(100 * share)} % потребности — импорт, крупнейший поставщик {top['seller']}",
                                  severity=round(share * top["tonnes"] / tot, 3), consequence=f"потеря поставщика: дефицит {round(100 * share * top['tonnes'] / tot)} % на время переключения"))
        # bridge beacon: edges whose loss disconnects the polity from its suppliers
        s = p["system"]
        if s in I.beacon_of:
            items.append(dict(kind="bridge_beacon", what=f"маяк BCN-{s:03d}-BR (оператор {I.beacon_of[s]['operator']})", severity=0.6,
                              consequence="прыжки в систему без маяка: ошибка выхода растёт в ~200 раз, расписания рвутся, страховка растёт, грузопоток падает"))
        yd = R["infra"][k]
        if yd["civil_shipyard"]["count"] or yd["military_shipyard"]["count"]:
            items.append(dict(kind="shipyard", what=f"верфи: {yd['civil_shipyard']['count']} крупных гражданских, {yd['military_shipyard']['count']} военных",
                              severity=0.5, consequence="замена флота встаёт: потери не восполняются, фрахт дорожает"))
        if any(a["type"] == "jump_infrastructure" for a in p["assets"]):
            items.append(dict(kind="jump_core_fab", what="фабрика ядер прыжка / Управление Маяков", severity=0.9,
                              consequence="вся сеть остаётся без новых прыжковых двигателей: через 5-10 лет флоты стареют"))
        fb = R["fuel"][k]
        if fb["reactor_fuel_imports_t_y"] > 0.3 * fb["reactor_fuel_t_y"]:
            items.append(dict(kind="fuel_depot", what=f"импорт реакторного топлива {round(100 * fb['reactor_fuel_imports_t_y'] / max(1, fb['reactor_fuel_t_y']))} %",
                              severity=0.8, consequence=f"запаса на {fb['reserve_days']} суток; затем веерные отключения хабитатов"))
        mines = R["infra"][k]["mine"]["count"]
        if mines:
            items.append(dict(kind="major_mine", what=f"крупнейшие рудники ({min(mines, max(1, int(math.sqrt(mines))))} из {mines})",
                              severity=0.3, consequence="металл и стройматериалы дорожают; хабитатное строительство замедляется"))
        items.sort(key=lambda x: (-x["severity"], x["kind"], x["what"]))
        out[k] = items[:10]
    return out


def flows_summary(M):
    agg = {}
    for f in M.flows:
        key = (f["seller"], f["buyer"])
        e = agg.setdefault(key, dict(tonnes=0.0, value=0.0))
        e["tonnes"] += f["tonnes"]
        e["value"] += f["value_cr"]
    return agg


def price_index(base_M, M2, k, c):
    """Price change: scarcity (demand / supplied)^2 (elasticity 0.5) x the delivered cost of imports."""
    def scarcity(M):
        n = M.need[k][c]
        s_ = n - M.unmet[k][c]
        return (n / s_) ** 2.0 if s_ > 0 else None

    def delivered(M):
        fs = [f for f in M.flows if f["buyer"] == k and f["commodity"] == c]
        t = sum(f["tonnes"] for f in fs)
        local = M.need[k][c] - M.unmet[k][c] - t
        cost = sum(f["tonnes"] * f["delivered_cr_per_t"] for f in fs) + max(0.0, local) * M.I.cat[c]["cost_cr_per_t"]
        return cost / max(1.0, t + max(0.0, local))
    a, b = scarcity(base_M), scarcity(M2)
    if a is None or b is None:
        return None
    return round(b / a * delivered(M2) / delivered(base_M), 3)


def scenarios(I, base):
    """Six stress tests run through the same physical system: shock -> capacity / routes -> flows ->
    prices -> fleet -> recovery. Aid goes through the SupportProgram chain."""
    M0 = base["M"]
    out = []

    def metrics(R, k):
        M = R["M"]
        imp = [f for f in M.flows if f["buyer"] == k]
        t = sum(f["tonnes"] for f in imp)
        cost = sum(f["tonnes"] * f["delivered_cr_per_t"] for f in imp)
        re_t = sum(f["tonnes"] for f in imp if f["channel"] != "official")
        ships = sum(f.get("ships", 0.0) for f in M.flows if k in (f["buyer"], f["seller"]))
        unmet = sum(M.unmet[k].values())
        return dict(import_t=round(t), import_delivered_cr=round(cost), avg_delivered_cr_per_t=round(cost / t, 1) if t else None,
                    re_export_t=round(re_t), ships_on_its_flows=round(ships, 1), shortage_t=round(unmet))

    def diff(R2, name, text, recovery, focus):
        M2 = R2["M"]
        rows = []
        mt = {k: dict(before=metrics(base, k), after=metrics(R2, k)) for k in focus}
        for k, v in mt.items():
            a, b = v["before"]["avg_delivered_cr_per_t"], v["after"]["avg_delivered_cr_per_t"]
            v["import_price_index"] = round(b / a, 3) if a and b else None
        for k in focus:
            for c in I.cats:
                a0, a1 = M0.unmet[k][c], M2.unmet[k][c]
                i0, i1 = M0.imports[k][c], M2.imports[k][c]
                if abs(a1 - a0) > 0.01 * max(M0.need[k][c], 1) or abs(i1 - i0) > 0.05 * max(i0, 1e3):
                    rows.append(dict(polity=k, commodity=c, imports_t_before=round(i0), imports_t_after=round(i1), shortage_t_before=round(a0),
                                     shortage_t_after=round(a1), price_index=price_index(M0, M2, k, c)))
        fleet0 = {k: sum(v["active"] for v in base["census"][k].values()) for k in focus}
        fleet1 = {k: sum(v["active"] for v in R2["census"][k].values()) for k in focus}
        out.append(dict(id=name, text=text, metrics=mt, changes=rows[:40], fleet_active_before=fleet0, fleet_active_after=fleet1, recovery=recovery))

    # 1. a major mine destroyed: Harmony loses 40 % of ore
    k1 = "eps_eri_harmony"
    R1 = compute(I, dict(capacity={(k1, "ore"): 0.6, (k1, "metals"): 0.75}))
    eq = R1["infra"][k1]["mine"]["count"] * 0.4 * I.PE["asset_types"]["mine"]["equipment_t"]
    diff(R1, "major_mine_destroyed", f"Гармония: уничтожен крупнейший рудный комплекс пояса (−40 % руды, −25 % металла)", dict(
        equipment_to_replace_t=round(eq), cobrett_trips=math.ceil(eq / 56), train_trips=math.ceil(eq / 57600), years=round(2 + eq / 4e5, 1),
        text="замена оборудования: местные заводы + импорт машин; пока руда идёт из резервных выработок, цена металла выше"), [k1, "eps_eri_strife", "gj832_tianhe"])
    # 2. a jump beacon lost
    s2 = 12
    R2 = compute(I, dict(lost_beacon=s2))
    diff(R2, "jump_beacon_lost", "потерян мост-маяк Gliese 832 (Тяньхэ): прыжки в систему без маяка", dict(
        years=1.5, text="новый маяк: изготовление 8 месяцев + буксировка; до тех пор поезда ходят реже, страховка ×2"), ["gj832_tianhe", "delta_pav_freekarma", "tau_ceti_cognium"])
    # 3. shipyard damaged
    k3 = "tau_ceti_cognium"
    yb = base["yards"]["by_polity"][k3]
    lost = yb["civil_dry_t_y"] * 0.6
    others = sorted(((v["civil_dry_t_y"], kk) for kk, v in base["yards"]["by_polity"].items() if kk != k3 and v["has_yard_tech"]), reverse=True)[:3]
    spare = sum(x for x, _ in others) * 0.1
    out.append(dict(id="shipyard_damaged", text="повреждена верфь «Гиперион» (Когниум): −60 % выпуска на 3 года",
                    metrics=dict(lost_dry_t_y=round(lost), spare_capacity_elsewhere_dry_t_y=round(spare), backlog_after_3y_dry_t=round(max(0.0, lost - spare) * 3),
                                 takers=[kk for _, kk in others], fleet_shortfall_share_after_3y=round(max(0.0, lost - spare) * 3 / max(1.0, sum(
                                     v["active"] * I.SE["ship_classes"][c]["dry_mass_t"] for c, v in base["census"][k3].items() if I.SE["ship_classes"][c]["family"] == "civil")), 4)),
                    changes=[], recovery=dict(years=3, text="заказы уходят к другим верфям (свободно ~10 % их мощности); остальное — очередь: старые корабли летают дольше, фрахт растёт")))
    # 4. habitat disaster: 2 M people displaced in Cognium's rings
    k4 = "tau_ceti_cognium"
    need_t = dict(food=2e6 * 0.55 * 0.5, medical=2e6 * 0.004 * 3, construction=2e6 * 55.0 * 0.25)
    R4 = compute(I, dict(extra_demand={k4: need_t}))
    diff(R4, "habitat_disaster", "авария кольца Нового Шанхая: 2 млн человек без жилья", dict(
        relief_t=round(sum(need_t.values())), support=support_program(I, base, "eps_eri_harmony", k4, need_t, motive="trade_dependence"),
        years=4, text="эвакуация в соседние кольца, помощь соседей по цепочке SupportProgram, затем верфи хабитатов"), [k4])
    # 5. external aid to a polity at war
    aid = support_program(I, base, "eps_eri_harmony", "hd40307_firmament", dict(weapons=2.0e4, machinery=4.0e4, jump_charge=5.0e3), motive="alliance_obligation")
    out.append(dict(id="external_aid", text="Гармония помогает своей провинции Небесная Твердыня в обострении с Маланд",
                    support_program=aid, changes=[], recovery=dict(text="помощь вычитается из экономики донора и проходит через логистику")))
    # 6. war + sanctions + neutral re-export
    war = {("hd40307_maland", "hd40307_firmament"), ("eps_eri_harmony", "hd40307_maland")}
    sup = {}
    for f in M0.flows:
        if f["buyer"] == "hd40307_maland" and f["channel"] == "official" and f["seller"] not in ("eps_eri_harmony", "hd40307_firmament"):
            sup[f["seller"]] = sup.get(f["seller"], 0.0) + f["value_cr"]
    top = [kk for kk, _ in sorted(sup.items(), key=lambda kv: (-kv[1], kv[0]))[:2]]
    sanc = {(kk, "hd40307_maland") for kk in top}
    R6 = compute(I, dict(war=war, sanctions=sanc))
    re = [dict(seller=f["seller"], buyer=f["buyer"], via=f["via"], commodity=f["commodity"], tonnes=round(f["tonnes"]), markup=1.25)
          for f in R6["M"].flows if f["channel"] == "re_export"]
    diff(R6, "war_sanctions_reexport", "война Маланд — Твердыня/Гармония; два крупнейших поставщика Маланд (" + ", ".join(top) + ") вводят санкции", dict(sanctioning=top,
        re_export_flows=re[:20], mobilisation=mobilisation(base, "hd40307_maland"), years=6,
        text="законные каналы закрыты: поставки идут через нейтральные узлы с наценкой 25 % и задержкой; страховка растёт; военная конверсия съедает гражданский выпуск"),
        ["hd40307_maland", "hd40307_firmament", "eps_eri_harmony"])
    return out


def support_program(I, base, donor, recipient, goods, motive):
    """pledged -> allocated (what the donor's economy really has) -> shipped (transport capacity) ->
    arrived (losses / interception) -> absorbed (what the recipient can use)."""
    M = base["M"]
    path = M.T.route(M.pols[donor]["system"], M.pols[recipient]["system"])
    rows = []
    for c, t in sorted(goods.items()):
        stock = M.prod[donor][c] * 30.0 / 365.0 + M.prod[donor][c] * 0.1          # a month of output + spare 10 %
        allocated = min(t, stock)
        cls = M.T.klass(c, allocated)
        sc = I.SE["ship_classes"][cls]
        spare = max(1.0, base["census"][donor].get(cls, {}).get("reserve", 0))
        tr = M.T.per_tonne(cls, c, path) if path else None
        trips_per_ship = 365.0 * sc["utilisation"] / (2 * tr["trip"]["trip_days"]) / 4 if tr else 0     # a quarter
        cap = spare * trips_per_ship * (tr["load"] if tr else 0)
        shipped = min(allocated, cap)
        loss = 0.02 if path and len(path) > 2 else 0.01
        arrived = shipped * (1 - loss)
        absorb = 0.85 if c in ("weapons", "machinery") else 0.95
        rows.append(dict(commodity=c, pledged_t=round(t), allocated_t=round(allocated), shipped_t=round(shipped), arrived_t=round(arrived),
                         absorbed_t=round(arrived * absorb), ship_class=cls, route=path, transport_cr=round(shipped * (tr["cr_per_t"] if tr else 0))))
    return dict(donor=donor, recipient=recipient, motive=motive, budget_cr=round(sum(goods[c] * I.cat[c]["cost_cr_per_t"] for c in goods)),
                status_counts=rows, statuses=["pledged", "allocated", "shipped", "arrived", "absorbed"])


def mobilisation(base, k):
    M = base["M"]
    p = M.pols[k]
    yd = base["yards"]["by_polity"][k]
    civ = yd.get("civil_dry_t_y", 0.0)
    return dict(normal_defence_share=base["defence"][k]["share_of_output"], emergency_share=round(min(0.15, base["defence"][k]["share_of_output"] * 4), 3),
                conversion=[dict(from_="civil yard", to="military transports / escorts", dry_t_y=round(civ * 0.6), months=9),
                            dict(from_="electronics", to="radars / guidance", share=0.4, months=6),
                            dict(from_="machinery", to="weapons / repair parts", share=0.35, months=4)],
                costs=["гражданский выпуск −20-35 %", "жилищное строительство встаёт", "цены потребительских товаров +30-60 %",
                       "нехватка транспорта: поезда реквизированы", "торговля падает, растут нейтральные посредники"])


# ---------------------------------------------------------------- banking and player base (data contracts)
def banking_model():
    return dict(currency="Cr", note="одна валюта, без валютного рынка",
                accounts=["account", "transfer", "loan", "interest", "collateral", "credit_limit", "default", "repossession"],
                rate_formula="rate = base 4 % + risk premium; premium = 2 % x (1 - income score) + 3 % x route risk + 2 % x legal risk + 4 % x prior defaults + 1 % x debt/income",
                limit_formula="limit = min(LTV x collateral value, 3 x annual net income)",
                ltv={"new_ship": 0.7, "used_ship": 0.55, "cargo": 0.3, "base_module": 0.4},
                examples=[credit_offer(dict(income_net_y=120000, ship_value=1.6e6, route_risk=0.2, legal_risk=0.0, defaults=0, debt=0, used=True)),
                          credit_offer(dict(income_net_y=60000, ship_value=1.6e6, route_risk=0.6, legal_risk=0.5, defaults=1, debt=200000, used=True)),
                          credit_offer(dict(income_net_y=400000, ship_value=3.39e6, route_risk=0.1, legal_risk=0.0, defaults=0, debt=0, used=False))],
                default_chain=["просрочка 30 дней: штраф 2 %", "90 дней: арест корабля в порту", "180 дней: изъятие и аукцион", "долг остаётся за вычетом выручки"])


def credit_offer(a):
    income_score = clamp(a["income_net_y"] / 300000.0)
    dti = a["debt"] / max(1.0, a["income_net_y"])
    premium = 0.02 * (1 - income_score) + 0.03 * a["route_risk"] + 0.02 * a["legal_risk"] + 0.04 * min(1, a["defaults"]) + 0.01 * min(2.0, dti)
    ltv = 0.55 if a["used"] else 0.7
    limit = min(ltv * a["ship_value"], 3 * a["income_net_y"])
    rate = 0.04 + premium
    n = 10
    pay = limit * rate / (1 - (1 + rate) ** -n) if limit > 0 else 0
    return dict(inputs=a, rate=round(rate, 4), limit_cr=round(limit), annual_payment_10y_cr=round(pay))


def player_base_model(I):
    cal = I.cal
    stages = [("survey", 6, 2.0e5, 0, "геологоразведка, заявка на участок"),
              ("claim", 1, 5.0e4, 0, "разрешение / аренда / нелегальная заявка"),
              ("construction_kit", 0, 9.0e5, 180, "каркас, шлюз, герметичные модули (C20)"),
              ("power", 0, 6.0e5, 60, "малый реактор 20 МВт и радиаторы"),
              ("habitation", 0, 4.0e5, 90, "жилой модуль на 12 человек"),
              ("dock", 0, 7.0e5, 120, "причал для Cobrett, топливный узел"),
              ("storage", 0, 2.0e5, 120, "склад на 2 000 т"),
              ("refinery_workshop", 0, 1.8e6, 300, "опционально: мини-обогатитель и мастерская"),
              ("defence", 0, 2.5e6, 80, "опционально: точечная оборона и сенсоры — повод для внимания государства")]
    rows = []
    for name, months, cost, tonnes, text in stages:
        rows.append(dict(stage=name, months=months or round(1 + tonnes / 120.0, 1), cost_cr=cost, tonnes=tonnes,
                         cobrett_trips=math.ceil(tonnes / 56.0) if tonnes else 0, text=text))
    total = sum(r["cost_cr"] for r in rows if r["stage"] not in ("refinery_workshop", "defence"))
    signals = [("beacon_transponder", "регулярные рейсы к участку видны в сети маяков"), ("inspections", "таможня видит строительные грузы"),
               ("trade_records", "закупки реактора и оружия"), ("banking", "кредит под залог базы"), ("informants", "экипажи и подрядчики говорят")]
    return dict(stages=rows, minimum_cost_cr=total, attention_signals=[dict(id=a, text=b) for a, b in signals],
                escalation=["разрешения", "проверки", "налоговый аудит", "наблюдение", "заморозка активов", "полиция", "военное вмешательство"],
                triggers=["вооружённый анклав", "нелегальная верфь", "стратегическая добыча", "собственные вооружённые силы"],
                charge_cost_per_mean_jump_cr=round(cal["mean_jump_cost_cr"]))


# ---------------------------------------------------------------- outputs
def assemble(I, R, scen):
    M = R["M"]
    SE = I.SE["ship_classes"]
    vul = vulnerabilities(R)
    eco = {}
    infra_out = {}
    fleet_out = {}
    def_out = {}
    for k in M.order:
        p = M.pols[k]
        struct = settlement_structure(I, p)
        rows, tot = asset_derived(I, R["infra"][k])
        cty, cstat = cities(I, p, struct, tot["workers"])
        eco[k] = dict(name=p["name"], polity=p["polity"], system=p["system"], regime=p["regime"], status="PROPOSAL",
                      population=round(p["pop"]), target=p["target"], target_status="AUTHOR_TARGET" if p["target"] else None,
                      settlement_structure={a: round(b) for a, b in struct.items()}, cities=cty, city_stats=cstat,
                      income_index=r3(p["income"]), gdp_cr_y=round(p["gdp"]), gdp_per_capita_cr=round(p["gdp"] / max(p["pop"], 1)),
                      growth_per_year=round(p["growth"], 5),
                      balance={c: {a: round(b) for a, b in v.items()} for c, v in M.bal[k].items()},
                      imports_cr_y=round(sum(f["value_cr"] for f in M.flows if f["buyer"] == k)),
                      exports_cr_y=round(sum(f["value_cr"] for f in M.flows if f["seller"] == k)),
                      fuel=R["fuel"][k], vulnerabilities=vul[k],
                      motives=motives(I, M, R, k))
        infra_out[k] = dict(assets=rows, totals=tot, named=major_and_named(I, M, k, R["infra"][k]),
                            mines=mines_detail(I, M, k, R["infra"][k]), shipyards=R["yards"]["by_polity"][k])
        fleet_out[k] = R["census"][k]
        d = R["defence"][k]
        def_out[k] = dict(share_of_output=d["share_of_output"], components=d["components"], budget_cr_y=round(d["budget_cr_y"]), why=d["why"],
                          roles=d["roles"], hostile=d["hostile"], task_ships=d["task_ships"],
                          forces={c: round(v, 1) for c, v in d["forces"].items()}, under_resourced=d["under_resourced"], structure=d["structure"],
                          bases=d["bases"], personnel=d["personnel"], risky_train_trips=d["risky_train_trips"], arrivals_per_year=d["arrivals_per_year"],
                          annual_cost_per_ship={c: round(v) for c, v in d["annual_cost_per_ship"].items()})
    flows = [dict(seller=f["seller"], buyer=f["buyer"], commodity=f["commodity"], tonnes_y=round(f["tonnes"]), value_cr_y=round(f["value_cr"]),
                  path=f["path"], jumps=len(f["path"]) - 1, ship_class=f["ship_class"], ships=round(f.get("ships", 0.0), 2),
                  loaded_trips_y=round(f.get("trips_per_year", 0.0), 1), channel=f["channel"], via=f["via"],
                  transport_cr_per_t=round(f["transport_cr_per_t"], 1), delivered_cr_per_t=round(f["delivered_cr_per_t"], 1),
                  unbeaconed_hops=f["unbeaconed_hops"], status="PROPOSAL")
             for f in sorted(M.flows, key=lambda f: (-f["value_cr"], f["seller"], f["buyer"], f["commodity"]))]
    totals = world_totals(I, R, eco, infra_out)
    return dict(
        economy=dict(status="PROPOSAL", year=YEAR, currency="Cr", polities=eco, world=totals, banking=banking_model(),
                     player_base=player_base_model(I), commodities=[dict(id=c["id"], name=c["name"]) for c in I.C["commodities"]],
                     part_catalog=I.C["part_catalog"]),
        infrastructure=dict(status="PROPOSAL", polities=infra_out, totals=totals["infrastructure"]),
        trade_flows=dict(status="PROPOSAL", flows=flows, routes=R["routes"]),
        fleet_census=dict(status="PROPOSAL", polities=fleet_out, totals=totals["fleet"], yards=dict(imports=R["yards"]["imports"], core_makers=R["yards"]["core_makers"],
                                                                                                 jump_drives_total_y=R["yards"]["jump_drives_total_y"])),
        defence=dict(status="PROPOSAL", polities=def_out, doctrine=doctrine_text()),
        beacons=dict(status="PROPOSAL", **R["beacons"]),
        scenarios=dict(status="PROPOSAL", scenarios=scen))


def motives(I, M, R, k):
    """Reasons of behaviour (not one relation number): derived where data exists."""
    p = M.pols[k]
    imp = sum(f["value_cr"] for f in M.flows if f["buyer"] == k)
    single = sum(1 for v in R_vul_single(M, k))
    hostile = [g for (a, b), g in I.rel.items() if k in (a, b)]
    fam = I.C["regime_family"].get(p["regime"], "open")
    return dict(security=round(min(1.0, max(hostile or [0]) / 80.0), 2), trade_dependence=round(min(1.0, 20 * imp / max(p["gdp"], 1)), 2),
                resource_dependence=round(min(1.0, single / 4.0), 2), alliance_obligation=0.6 if k in ("eps_eri_harmony", "hd40307_firmament") else 0.2,
                prestige_ideology={"faith": 0.8, "military": 0.7, "strict": 0.5, "open": 0.3}[fam],
                domestic_legitimacy=0.7 if fam in ("faith", "military") else 0.5, elite_interest=0.6 if p["regime"] in ("corporate_board", "shareholder_agora", "grandees_and_church", "security_oligarchy") else 0.4,
                industrial_interest=round(min(1.0, sum(M.exports[k].values()) / max(1.0, sum(M.prod[k].values())) * 5), 2), status="PROPOSAL")


def R_vul_single(M, k):
    by = {}
    for f in M.flows:
        if f["buyer"] == k:
            by.setdefault(f["commodity"], set()).add(f["seller"])
    return [c for c, s in by.items() if len(s) == 1]


def world_totals(I, R, eco, infra_out):
    M = R["M"]
    earth = eco[SOL]["settlement_structure"]["Земля: поверхность"]
    sol = sum(v for kk, v in eco[SOL]["settlement_structure"].items() if kk.startswith("Сол:"))
    off = sum(e["population"] for k, e in eco.items() if k != SOL)
    infra = {}
    for k, v in infra_out.items():
        for a, r in v["assets"].items():
            e = infra.setdefault(a, dict(count=0, major=0, named=0, name=r["name"]))
            e["count"] += r["count"]
            e["major"] += v["named"].get(a, {}).get("major", 0)
            e["named"] += len(v["named"].get(a, {}).get("named", []))
    fleet = {}
    for k, rows in R["census"].items():
        for c, r in rows.items():
            e = fleet.setdefault(c, dict(role=r["role"], family=r["family"], active=0, reserve=0, repair=0, new_per_year=0.0, lost_per_year=0.0, retired_per_year=0.0))
            for kk in ("active", "reserve", "repair"):
                e[kk] += r[kk]
            for kk in ("new_per_year", "lost_per_year", "retired_per_year"):
                e[kk] = round(e[kk] + r[kk], 1)
    goods = {}
    for k in M.pols:
        for c in I.cats:
            g = goods.setdefault(c, dict(production=0.0, consumption=0.0, interstellar=0.0))
            g["production"] += M.prod[k][c]
            g["consumption"] += M.bal[k][c]["consumption"]
            g["interstellar"] += M.exports[k][c]
    return dict(population=dict(earth=round(earth), sol_outside_earth=round(sol), off_sol=round(off), total=round(earth + sol + off)),
                gdp_cr_y=round(sum(p["gdp"] for p in M.pols.values())), infrastructure=infra, fleet=fleet,
                goods={c: {a: round(b) for a, b in v.items()} for c, v in goods.items()},
                beacons=R["beacons"]["totals"], interstellar_flows=len(M.flows),
                interstellar_tonnes_y=round(sum(f["tonnes"] for f in M.flows)), interstellar_value_cr_y=round(sum(f["value_cr"] for f in M.flows)))


def doctrine_text():
    return dict(
        principles=["почти нет укрытий: кто видит первым, тот выбирает бой; сенсоры и пикеты важнее брони",
                    "время до перехвата = f(ускорение, дальность обнаружения, дальность оружия, запас топлива), а не максимальная скорость",
                    "хабы и планеты движутся: оборона держит точки, а не линии; маршруты — коридоры, а не рельсы",
                    "логистика определяет выносливость: тендер и депо — первые цели, уничтожение снабжения важнее уничтожения линкора",
                    "выход из прыжка имеет ошибку: засада у точки выхода работает только у маяка, без маяка противник появляется в эллипсе",
                    "маяк — военный объект: даёт точную навигацию и картину трафика; его захват или выключение меняет театр",
                    "противоастероидная оборона — та же техника (буксиры, перехватчики, сенсоры): флот существует и в мирное время"],
        roles={"patrol": "патрульный катер: таможня, полиция, SAR, охрана маяков (единая береговая служба)",
               "interceptor": "перехватчик: быстрый ответ внутри системы, короткая автономность",
               "escort": "эскортный фрегат: конвои поездов, точечная оборона, выносливость",
               "military": "линейный ракетный корабль: сдерживание и война; разведчик/РЭБ-пикет; тендер флота",
               "impact_defence": "буксир противоастероидной обороны: обнаружить, перехватить, увести или разбить опасный камень"})


# ---------------------------------------------------------------- checks
def checks(I, R, out):
    M = R["M"]
    res = []

    def add(name, ok, detail):
        res.append(dict(check=name, ok=bool(ok), detail=detail))
    # demographic totals
    ph = I.PH["world_targets"]
    pt = out["economy"]["world"]["population"]
    for key, nm in (("earth", "earth"), ("sol_outside_earth", "sol_outside_earth"), ("off_sol", "off_sol")):
        lo, hi = ph[nm]["range"]
        add(f"population_{key}_in_author_range", lo <= pt[key] <= hi, f"{pt[key] / 1e9:.3f} bn in {lo / 1e9:.2f}-{hi / 1e9:.2f}")
    bad = []
    for k, e in out["economy"]["polities"].items():
        s = sum(e["settlement_structure"].values())
        if abs(s - e["population"]) > max(10, 1e-6 * e["population"]):
            bad.append(k)
        c = sum(x["pop"] for x in e["cities"])
        if abs(c - e["population"]) > max(10, 1e-6 * e["population"]):
            bad.append(k + ":cities")
    add("settlement_structure_sums_to_population", not bad, ",".join(bad) or "all polities")
    # cargo balance
    worst = 0.0
    for k in M.pols:
        for c in I.cats:
            b = M.bal[k][c]
            lhs = b["production"] + b["imports"]
            rhs = b["consumption"] + b["exports"] + b["losses"] + b["stock_change"]
            worst = max(worst, abs(lhs - rhs) / max(lhs, 1.0))
    add("cargo_balance_identity", worst < 1e-9, f"max relative residual {worst:.2e}")
    neg = [(k, c) for k in M.pols for c in I.cats if M.bal[k][c]["stock_change"] < -1.0]
    add("no_goods_from_thin_air", not neg, f"negative stock changes: {len(neg)}")
    # infrastructure covers production
    short = []
    A = I.PE["asset_types"]
    for k in M.pols:
        for c, a in OUTPUT_ASSET.items():
            cap = R["infra"][k][a]["count"] * A[a]["capacity_t_y"]
            if M.prod[k][c] > cap + 1.0:
                short.append(f"{k}:{c}")
    add("infrastructure_covers_production", not short, ",".join(short[:10]) or "ok")
    # transport capacity >= flows
    cap_t = {}
    for k, rows in R["census"].items():
        for cls, r in rows.items():
            cap_t[(k, cls)] = r["active"]
    under = []
    for f in M.flows:
        pass
    need = {}
    for f in M.flows:
        need[(f["seller"], f["ship_class"])] = need.get((f["seller"], f["ship_class"]), 0.0) + f.get("ships", 0.0)
    for key, n in sorted(need.items()):
        if cap_t.get(key, 0) + 1e-9 < n:
            under.append(f"{key[0]}:{key[1]}")
    add("cargo_flows_within_transport_capacity", not under, ",".join(under[:10]) or "ok")
    # fuel demand <= production + imports + stock withdrawal
    fb = []
    for k, f in R["fuel"].items():
        if M.pols[k]["refuses"]:
            continue
        for kind in ("reactor_fuel", "jump_charge"):
            need = M.need[k][kind]
            have = M.prod[k][kind] - M.exports[k][kind] + M.imports[k][kind]
            rsv = f["strategic_reserve_reactor_t"] if kind == "reactor_fuel" else f["strategic_reserve_charge_t"]
            if need > have + 1.0:
                fb.append(f"{k}:{kind} short {round(need - have)} t/y (reserve {round(rsv)} t)")
    add("fuel_demand_covered", not fb, ",".join(fb) or "ok")
    # fleet replacement <= shipyard output + imports
    yb = []
    A = I.PE["asset_types"]
    for k, y in R["yards"]["by_polity"].items():
        inf = R["infra"][k]
        cap_c = inf["civil_shipyard"]["count"] * A["civil_shipyard"]["dry_t_y"] + inf["small_yard"]["count"] * A["small_yard"]["dry_t_y"]
        cap_m = inf["military_shipyard"]["count"] * A["military_shipyard"]["dry_t_y"]
        if y["civil_dry_t_y"] > cap_c + 1 or y["military_dry_t_y"] > cap_m + 1:
            yb.append(k)
    add("fleet_replacement_within_shipyard_output_plus_imports", not yb, ",".join(yb) or "ok")
    # statuses
    st = {"PROPOSAL", "AUTHOR_TARGET", "CANON"}
    badst = [k for k in ("economy", "infrastructure", "trade_flows", "fleet_census", "defence", "beacons", "scenarios") if out[k].get("status") not in st]
    add("status_present", not badst, ",".join(badst) or "ok")
    return res


# ---------------------------------------------------------------- markdown
def fmt_num(x, d=0):
    if x is None:
        return "—"
    if isinstance(x, float) and d:
        return f"{x:,.{d}f}".replace(",", " ")
    return f"{int(round(x)):,}".replace(",", " ")


def md_report(I, out, chk):
    eco = out["economy"]
    W = eco["world"]
    L = ["# Баланс мира 3126 — экономика, логистика, флоты, оборона", "",
         "Сгенерировано `tools/colonization_lab/world_economy.py` (машинные данные — `out/world_3126/*.json`). "
         "Статус всех чисел — PROPOSAL; численность Земли, Сола и колоний — AUTHOR_TARGET (модель объясняет её эпохами, см. ДЕМОГРАФИЯ).", "",
         "## Человечество", "",
         f"- Земля: {W['population']['earth'] / 1e9:.2f} млрд; Сол вне Земли: {W['population']['sol_outside_earth'] / 1e9:.2f} млрд; вне Сола: {W['population']['off_sol'] / 1e9:.2f} млрд.",
         f"- Выпуск: {W['gdp_cr_y'] / 1e12:.0f} трлн Cr/год. Межзвёздная торговля: {fmt_num(W['interstellar_tonnes_y'] / 1e6, 1)} млн т/год на {W['interstellar_value_cr_y'] / 1e12:.2f} трлн Cr/год ({W['interstellar_flows']} потоков).", "",
         "## Инфраструктура человечества", "", "| Тип | Всего | Крупных | Именованных |", "|---|---:|---:|---:|"]
    for a, v in sorted(W["infrastructure"].items(), key=lambda kv: (-kv[1]["count"], kv[0])):
        L.append(f"| {v['name']} | {fmt_num(v['count'])} | {fmt_num(v['major'])} | {v['named']} |")
    L += ["", f"Маяки: " + ", ".join(f"{k} {v}" for k, v in sorted(W["beacons"].items())), "",
          "## Флот человечества", "", "| Класс | Роль | Активных | Резерв | В ремонте | Новых/год | Потерь/год | Списано/год |", "|---|---|---:|---:|---:|---:|---:|---:|"]
    for c, v in sorted(W["fleet"].items(), key=lambda kv: (-kv[1]["active"], kv[0])):
        L.append(f"| {c} | {v['role']} | {fmt_num(v['active'])} | {fmt_num(v['reserve'])} | {fmt_num(v['repair'])} | {fmt_num(v['new_per_year'])} | {fmt_num(v['lost_per_year'])} | {fmt_num(v['retired_per_year'])} |")
    L += ["", "## Товары (млн т/год)", "", "| Категория | Производство | Потребление | Межзвёздно |", "|---|---:|---:|---:|"]
    for c, v in W["goods"].items():
        L.append(f"| {I.cat[c]['name']} | {v['production'] / 1e6:,.2f} | {v['consumption'] / 1e6:,.2f} | {v['interstellar'] / 1e6:,.3f} |".replace(",", " "))
    L += ["", "## Оборона и безопасность по политиям", "", "| Полития | Доля выпуска | Бюджет, млрд Cr | Почему | Кораблей | Недофинансирование |", "|---|---:|---:|---|---:|---|"]
    for k, d in out["defence"]["polities"].items():
        L.append(f"| {eco['polities'][k]['name']} | {100 * d['share_of_output']:.2f} % | {d['budget_cr_y'] / 1e9:,.1f} | {d['why']} | {fmt_num(sum(d['forces'].values()))} | {'да' if d['under_resourced'] else ''} |".replace(",", " "))
    L += ["", "## Проверки", ""] + [f"- {'OK' if c['ok'] else 'FAIL'} `{c['check']}` — {c['detail']}" for c in chk]
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------- main
def run_all():
    I = Inputs()
    R = compute(I)
    scen = scenarios(I, R)
    out = assemble(I, R, scen)
    chk = checks(I, R, out)
    out["checks"] = dict(status="PROPOSAL", checks=chk)
    return I, out, chk


FILES = {"economy": "economy_3126.json", "infrastructure": "infrastructure_3126.json", "trade_flows": "trade_flows_3126.json",
         "fleet_census": "fleet_census_3126.json", "defence": "defence_3126.json", "beacons": "beacons_3126.json",
         "scenarios": "scenarios_3126.json", "checks": "checks_3126.json"}


def serialise(I, out, chk):
    texts = {fn: json.dumps(out[k], ensure_ascii=False, indent=1, sort_keys=True) + "\n" for k, fn in FILES.items()}
    texts["WORLD_BALANCE_3126.md"] = md_report(I, out, chk)
    return texts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=OUT)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    I, out, chk = run_all()
    texts = serialise(I, out, chk)
    failed = [c for c in chk if not c["ok"]]
    if a.check:
        stale = [fn for fn, t in texts.items() if not os.path.exists(os.path.join(a.out, fn)) or open(os.path.join(a.out, fn), encoding="utf-8").read() != t]
        for c in chk:
            print(("OK   " if c["ok"] else "FAIL ") + c["check"] + ": " + c["detail"])
        for fn in stale:
            print("STALE", fn)
        ok = not stale and not failed
        print("world_economy check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    os.makedirs(a.out, exist_ok=True)
    for fn, t in texts.items():
        with open(os.path.join(a.out, fn), "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
    for c in chk:
        print(("OK   " if c["ok"] else "FAIL ") + c["check"] + ": " + c["detail"])
    W = out["economy"]["world"]
    print(f"population earth {W['population']['earth'] / 1e9:.3f} sol {W['population']['sol_outside_earth'] / 1e9:.3f} off {W['population']['off_sol'] / 1e9:.3f}; "
          f"flows {W['interstellar_flows']} {W['interstellar_tonnes_y'] / 1e6:.1f} Mt/y")
    return 0 if not failed else 1


if __name__ == "__main__":
    sys.exit(main())
