"""Colonization history / economy lab (offline, deterministic).

usage:
  python lab.py [--seed N] [--out DIR]          run canon -> DIR (default ./out)
  python lab.py --check-dataset                 validate src/assets/data/galaxy/generation_ships.json against canon

Input:  canon/cultures.json, canon/missions.json, canon/world.json + the physical
        catalog (endowment.py). Output: DIR/lab_state_3126.json, DIR/lab_report.md.
Same input + seed -> byte-identical output (no clocks, no unseeded RNG, sorted
iteration). Anchors (written history) are applied as given; the simulation only
fills gaps between them and reports where its own tendency disagrees with canon.
"""
import argparse
import hashlib
import json
import math
import os
import random
import sys
import zlib

import endowment as endow
import physics

HERE = os.path.dirname(os.path.abspath(__file__))
YEAR_END = 3126


def load(name):
    with open(os.path.join(HERE, "canon", name), encoding="utf-8") as f:
        return json.load(f)


def rng_for(seed, key):
    return random.Random(seed * 1000003 + zlib.crc32(key.encode("utf-8")))


def clamp(x, lo=0.0, hi=100.0):
    return lo if x < lo else hi if x > hi else x


# ---------------------------------------------------------------- culture
CUL = load("cultures.json")
AXES = CUL["axes"]
POWER0 = {"patricians": 3.0, "command": 4.0, "security": 3.0, "engineers": 2.0, "scientists": 2.0,
          "faithful": 2.0, "workers": 1.0, "settlers": 0.7, "coerced": 0.3}


def vec(name):
    return dict(zip(AXES, CUL["cultures"][name]))


def blend(weights):
    tot = sum(weights.values())
    out = {a: 0.0 for a in AXES}
    for k in sorted(weights):
        v = vec(k)
        for a in AXES:
            out[a] += v[a] * weights[k] / tot
    return out


def filtered(base, cohort):
    out = dict(base)
    for a, d in CUL["cohort_filters"].get(cohort, {}).items():
        if a in out:
            out[a] = clamp(out[a] + d)
    return out


def wavg(items):
    tot = sum(w for w, _ in items) or 1.0
    return {a: sum(w * v[a] for w, v in items) / tot for a in AXES}


def cdist(a, b, axes=None):
    axes = axes or AXES
    return math.sqrt(sum((a[x] - b[x]) ** 2 for x in axes) / len(axes))


# ---------------------------------------------------------------- voyage
ERA_CRYO = {"I": (0.0006, 0.020), "II": (0.0003, 0.010), "III": (0.00015, 0.005)}  # static loss/yr, loss per revival
ERA_CAP = {"I": 0.10, "II": 0.12, "III": 0.165}   # max average speed incl. accel/brake [c]
LIGHT_ARK_CAP = 0.125                               # wave I light ark (cryo >= 0.8)


def required_watch(pop, births):
    """Minimum awake functions for a cryo ark (command, navigation, reactor/drive,
    maintenance, life support, cryo/revival medicine, security, governance)."""
    k = 0.3 + 0.7 * min(1.0, pop / 10000.0)
    roles = dict(command=12 * k, navigation=10 * k, drive_reactor=40 * k, maintenance=60 * k + pop * 0.004, life_support=25 * k,
                 cryo_medical=10 * k + pop * 0.0012, security=10 * k + pop * 0.0005, governance=8 * k)
    if births:
        roles["teachers_childcare"] = 30 + pop * 0.001
    return roles


class Voyage:
    def __init__(self, m, E, seed):
        self.m = m
        self.rng = rng_for(seed, m["id"])
        self.events = []
        self.flags = []
        base = blend(m["cultures"]) if m["cultures"] else blend({"earth_late": 1.0})
        self.cohorts = {}
        for c in sorted(m["cohorts"]):
            spec = m["cohorts"][c]
            cv = filtered(blend(spec["culture"]) if "culture" in spec else base, c)
            pw = POWER0.get(c, 1.0) * (1.5 if (c == "faithful" and m["motives"].get("religion", 0) > 0.3) else 1.0)
            self.cohorts[c] = dict(share=spec["share"], awake=spec["awake"], power=pw, grievance=10.0 if c != "coerced" else 35.0,
                                   cohesion=60.0, culture=cv)
        self.base = base
        self.pop = float(m["population"])
        self.knowledge = 1.0
        self.legitimacy = 60.0
        self.keeper = 0.0
        self.owner_sleep = 0.0
        self.scarcity = 10.0
        self.drift = {a: 0.0 for a in AXES}
        self.era = physics.era_of(m["launch_year"])
        # route geometry
        src = m["source_system"]
        tgt = m["final_system"] if isinstance(m.get("final_system"), int) else m.get("target_system")
        self.dist = None
        if isinstance(src, int) and isinstance(tgt, int) and src in E and tgt in E:
            self.dist = endow.distance(E[src], E[tgt])
            if m["id"] == "NEW-JERUSALEM-1":
                self.dist = math.hypot(endow.distance(E[0], E[45]), 1.5)  # early-turn route, ~58 ly
        self.arrival = next((a for a in m["anchors"] if a["type"] == "arrival"), None)
        self.end_year = self.arrival["year"] if self.arrival else next(
            (a["year"] for a in m["anchors"] if a["type"] in ("lost",)), YEAR_END)
        if self.dist and self.arrival and self.end_year > m["launch_year"]:
            v = self.dist / (self.end_year - m["launch_year"])
            cap = LIGHT_ARK_CAP if (self.era == "I" and m["cryo_share"] >= 0.8) else ERA_CAP[self.era]
            self.avg_c = v
            if v > cap * 1.001:
                self.flags.append(f"средняя скорость {v:.3f} c выше потолка эпохи {cap:.3f} c")
            auth = self.arrival.get("authored")
            if auth:
                self.flags.append(f"дата прибытия исправлена {auth} -> {self.end_year} (было {self.dist / (auth - m['launch_year']):.3f} c)")
        else:
            self.avg_c = None

    # --- helpers
    def culture_now(self, by_power=False):
        items = [((c["share"] * (c["power"] if by_power else 1.0)), c["culture"]) for c in self.cohorts.values()]
        cv = wavg(items) if items else dict(self.base)
        return {a: clamp(cv[a] + self.drift[a]) for a in AXES}

    def command_clock(self):
        top = max(self.cohorts.items(), key=lambda kv: (kv[1]["power"], kv[0])) if self.cohorts else None
        return top[1]["culture"]["clock_time"] if top else 50.0

    def apply_effects(self, eff):
        for k, d in (eff or {}).items():
            if "." in k:
                who, attr = k.split(".", 1)
                targets = self.cohorts.keys() if who == "*" else [who] if who in self.cohorts else []
                for t in targets:
                    self.cohorts[t][attr] = clamp(self.cohorts[t][attr] + d, 0.0, 300.0 if attr == "power" else 100.0)
            elif k in AXES:
                self.drift[k] += d
            elif k == "cohesion":
                for c in self.cohorts.values():
                    c["cohesion"] = clamp(c["cohesion"] + d)
            elif k == "state_trust":
                self.drift["state_trust"] += d

    def log(self, year, kind, text, src, deaths=0):
        self.events.append(dict(year=year, type=kind, text=text, source=src, deaths=int(deaths), pop=int(round(self.pop))))

    # --- simulation
    def run(self):
        m = self.m
        L = m["launch_year"]
        births = m["birth_policy"] not in ("none", "forbidden")
        self.log(L, "launch", f"старт: {m['population']} чел., крио {int(m['cryo_share'] * 100)} %", "canon")
        if m["population"] == 0:
            for a in m["anchors"]:
                self.log(a["year"], a["type"], a["text"], "anchor")
            return self
        req = required_watch(m["population"], births)
        self.req_watch = int(sum(req.values()))
        if m["cryo_share"] > 0 and m["awake_watch"] < self.req_watch:
            self.flags.append(f"бодрствующая вахта {m['awake_watch']} меньше минимума функций {self.req_watch}")
        st_loss, rev_loss = ERA_CRYO[self.era]
        sleepers = m["population"] * m["cryo_share"]
        anchors = sorted(m["anchors"], key=lambda a: a["year"])
        ai = 0
        y = L
        T = max(1, self.end_year - L)
        tours = (T / m["watch_years"]) if m["watch_years"] else 0
        self.cycles_needed = 0.0
        if m["cryo_share"] > 0 and m["cryo_share"] < 1 and tours:
            pool = max(1.0, m["population"] * m["cryo_share"])
            self.cycles_needed = tours * m["awake_watch"] / pool * 2.0  # each tour = sleep->wake->sleep
        self.hereditary_watch = births and self.cycles_needed > m["safe_cycles"]
        owners = self.cohorts.get("patricians")
        clock_cmd = self.command_clock()
        unrecorded = 0.0
        while y < self.end_year:
            y2 = min(self.end_year, y + 5)
            dt = y2 - y
            # cryo attrition (static) + revival losses of rotation
            if sleepers > 0:
                lost = sleepers * (1 - (1 - st_loss) ** dt)
                if m["watch_years"] and not self.hereditary_watch:
                    lost += m["awake_watch"] * rev_loss * dt / m["watch_years"]
                lost = min(lost, sleepers)
                sleepers -= lost
                self.pop -= lost
                unrecorded += lost
            # awake ageing/mortality is offset by policy births; generation ships drift
            if m["birth_policy"] in ("generation", "encouraged_awake"):
                cap = sleepers + (m["awake_watch"] * 1.25 if m["cryo_share"] > 0 else m["population"] * 1.1)
                self.pop = min(cap, self.pop * (1 + 0.0015 * dt))
            # social dynamics
            awake_share = 1 - m["cryo_share"] * (sleepers / max(1.0, m["population"] * m["cryo_share"])) if m["cryo_share"] else 1.0
            if self.hereditary_watch:
                self.keeper = clamp(self.keeper + dt * 0.45 * (1 - (owners["awake"] if owners else 1.0)))
            if owners and owners["awake"] < 0.3:
                self.owner_sleep += dt
                owners["power"] = max(0.5, owners["power"] * (0.995 ** dt))
            for name in sorted(self.cohorts):
                c = self.cohorts[name]
                mismatch = max(0.0, clock_cmd - c["culture"]["clock_time"]) + 0.5 * max(0.0, c["culture"]["midday_pause"] - 40)
                c["grievance"] = clamp(c["grievance"] + dt * (0.02 * mismatch + (0.25 if name == "coerced" else 0.05) * self.scarcity / 10
                                                              - 0.06 * c["culture"]["gallows_humor"] / 10))
                c["culture"]["clock_time"] += dt * 0.04 * (clock_cmd - c["culture"]["clock_time"])  # the ship clock wins slowly
            # closed-world drift (GENERATION_SHIP_SOCIAL_ADAPTATION.md)
            for a, d in (("order_love", 0.05), ("privacy", 0.04), ("outsider_trust", -0.03), ("formalism", 0.02), ("thrift", 0.03)):
                self.drift[a] += d * dt
            self.knowledge *= (1 - 0.0006 * dt * (1.6 if not births and m["cryo_share"] > 0.7 else 1.0))
            # anchors in window
            had_anchor = False
            while ai < len(anchors) and anchors[ai]["year"] < y2 + (1 if y2 == self.end_year else 0):
                a = anchors[ai]
                ai += 1
                if a["type"] == "arrival":
                    continue
                had_anchor = True
                d = a.get("deaths", 0) + a.get("pop_loss", 0)
                self.pop -= d
                self.apply_effects(a.get("effects"))
                if a["type"] in ("class_conflict", "mutiny", "factional_conflict", "religious_conflict", "clan_fragmentation", "purge", "security_takeover"):
                    self.legitimacy = clamp(self.legitimacy - 10)
                if a["type"] in ("reform", "reconciliation", "democratisation", "constitution"):
                    self.legitimacy = clamp(self.legitimacy + 8)
                if a["type"] == "shortage":
                    self.scarcity = clamp(self.scarcity + 25)
                self.log(a["year"], a["type"], a["text"], "anchor", d)
            if not had_anchor and dt >= 5:
                self.emergent(y2)
            self.scarcity = clamp(self.scarcity * 0.9 + (5 if self.pop > m["population"] * 1.1 else 0))
            y = y2
        # arrival / end
        if self.arrival:
            target = self.arrival["pop_after"]
            gap = self.pop - target
            self.unrecorded_losses = int(round(gap))
            self.pop = float(target)
            self.log(self.arrival["year"], "arrival", self.arrival["text"], "anchor")
            self.arrival_tendency = self.regime_tendency()
        for a in anchors[ai:]:
            if a["type"] != "arrival" and a["year"] > self.end_year:
                self.log(a["year"], a["type"], a["text"], "anchor (после прибытия/потери)")
        return self

    def emergent(self, year):
        """Gap filler. Minor events only: canon decides the big turns."""
        cv = self.culture_now()
        coh = self.cohorts
        g = max((c["grievance"] for c in coh.values()), default=0)
        sec = coh.get("security", {}).get("power", 0)
        eng = coh.get("engineers", {}).get("power", 0)
        fai = coh.get("faithful", {}).get("share", 0)
        age = year - self.m["launch_year"]
        H = {
            "black_market": 0.02 + 0.10 * cv["rule_bending"] / 100 * self.scarcity / 50,
            "sabotage": 0.01 + 0.06 * g / 100 * coh.get("coerced", {}).get("share", 0) * 3,
            "mutiny_attempt": 0.01 + 0.08 * max(0, g - 50) / 50 * (1 - sec / 10),
            "technical_meritocracy": 0.02 + 0.05 * (1 - self.knowledge) * 10 * eng / 3,
            "religious_revival": 0.01 + 0.08 * fai + 0.02 * (age > 100),
            "mission_fatigue": 0.0 if age < 60 else 0.03 + 0.04 * (1 - cv["collective_duty"] / 100),
            "purge": 0.01 + 0.05 * cv["power_distance"] / 100 * g / 100 * (sec + 1) / 4,
            "reconciliation": 0.02 + 0.04 * cv["hospitality"] / 100 * (g > 40),
            "keeper_caste": 0.06 * self.keeper / 100,
            "joke_cycle": 0.03 * cv["gallows_humor"] / 100,
        }
        r = self.rng.random()
        acc = 0.0
        for k in sorted(H):
            acc += H[k]
            if r < acc:
                self.apply_emergent(year, k)
                return

    def apply_emergent(self, year, k):
        text = {
            "black_market": "чёрный рынок пайков и запчастей",
            "sabotage": "мелкий саботаж в секторе подневольных",
            "mutiny_attempt": "попытка мятежа вахты, подавлена/разрешена переговорами",
            "technical_meritocracy": "инженеры получают право вето на технические решения",
            "religious_revival": "религиозное оживление, новые обряды",
            "mission_fatigue": "усталость от миссии: «зачем лететь»",
            "purge": "чистка «неблагонадёжных» в администрации",
            "reconciliation": "примирение секторов, общие праздники",
            "keeper_caste": "вахта становится наследственной кастой «хранителей»; спящих зовут «грузом»",
            "joke_cycle": "цикл анекдотов о капитане и о Земле",
        }[k]
        c = self.cohorts
        if k == "technical_meritocracy" and "engineers" in c:
            c["engineers"]["power"] += 0.4
        if k == "purge":
            self.pop -= 10
            for x in c.values():
                x["grievance"] = clamp(x["grievance"] + 4)
        if k == "reconciliation":
            for x in c.values():
                x["grievance"] = clamp(x["grievance"] - 8)
        if k == "black_market":
            self.drift["rule_bending"] += 2
        if k == "keeper_caste":
            self.keeper = clamp(self.keeper + 10)
        if k == "religious_revival":
            self.drift["collective_duty"] += 2
        if k == "joke_cycle":
            for x in c.values():
                x["grievance"] = clamp(x["grievance"] - 3)
        self.log(year, k, text, "simulated")

    def regime_tendency(self):
        c = self.cohorts
        cv = self.culture_now(by_power=True)
        P = {k: v["power"] * (0.6 + v["share"]) for k, v in c.items()}
        rel = self.m["motives"].get("religion", 0)
        cand = {
            "technocratic": (P.get("engineers", 0) + P.get("scientists", 0)) * (1 + self.keeper / 100) * (0.3 + cv["say_no"] / 100) * (1.2 - cv["collective_duty"] / 150),
            "security": P.get("security", 0) * (1 + cv["power_distance"] / 100),
            "oligarchic": P.get("patricians", 0) * (0.5 + cv["status_display"] / 100) + 2.0 * cv["kin_first"] / 100 * (cv["power_distance"] / 100),
            "theocratic": P.get("faithful", 0) * 1.5 + 6.0 * rel,
            "assembly": (P.get("workers", 0) + P.get("settlers", 0) + 0.5 * P.get("engineers", 0)) * cv["say_no"] / 50 * (1 - cv["power_distance"] / 150),
            "party": (P.get("command", 0) + 0.5 * P.get("engineers", 0) + 0.5 * P.get("settlers", 0)) * cv["collective_duty"] / 60 * cv["power_distance"] / 60,
            "clan": (P.get("workers", 0) + P.get("patricians", 0)) * cv["kin_first"] / 100 * (100 - cv["state_trust"]) / 70 * (cv["rule_bending"] / 70),
        }
        return max(sorted(cand), key=lambda k: cand[k])


REGIME_FAMILY = {
    "technocratic_meritocracy": "technocratic", "security_oligarchy": "security", "party_technocracy": "party",
    "shareholder_agora": "assembly", "eco_assembly": "assembly", "caste_theocracy": "theocratic",
    "scholar_theocracy": "theocratic", "machine_oracle": "technocratic", "parliamentary_bureaucracy": "assembly",
    "corporate_board": "oligarchic", "corporate_democracy": "assembly", "synod_theocracy": "theocratic",
    "council_of_chiefs": "assembly", "litigating_city_states": "assembly", "grandees_and_church": "oligarchic",
    "clan_contracts": "clan", "merit_caste_republic": "technocratic", "sage_council": "technocratic",
}


# ---------------------------------------------------------------- colonies
CAPS = ["energy", "extraction", "metallurgy", "manufacturing", "electronics", "bio", "shipbuilding", "military", "research"]
CAP_CULTURE = {
    "energy": {"order_love": 0.5, "clock_time": 0.5},
    "extraction": {"improvisation": 0.4, "risk": 0.3, "hardship_pride": 0.3},
    "metallurgy": {"clock_time": 0.4, "thrift": 0.3, "order_love": 0.3},
    "manufacturing": {"collective_duty": 0.35, "clock_time": 0.3, "thrift": 0.2, "power_distance": 0.15},
    "electronics": {"clock_time": 0.35, "order_love": 0.3, "say_no": 0.2, "formalism": 0.15},
    "bio": {"collective_duty": 0.4, "kin_first": 0.3, "hardship_pride": 0.3},
    "shipbuilding": {"clock_time": 0.35, "order_love": 0.3, "improvisation": 0.35},
    "military": {"hardship_pride": 0.3, "power_distance": 0.25, "collective_duty": 0.2, "grievance_memory": 0.25},
    "research": {"say_no": 0.3, "irony_to_power": 0.2, "risk": 0.2, "outsider_trust": 0.3},
}
CAP_ENDOW = {"extraction": ("volatiles", "metals", "he3"), "metallurgy": ("metals", "strategic"),
             "bio": ("agri", "water"), "energy": ("he3",), "shipbuilding": ("metals",)}
ERA_K = {"I": 0.55, "II": 0.62, "III": 0.70, "IV": 0.72, "hyper": 0.9}


def colony_profile(col, voyages, E, missions):
    founders = [voyages[f] for f in col["founders"] if f in voyages and voyages[f].arrival]
    if not founders:
        founders = [voyages[f] for f in col["founders"] if f in voyages]
    items = []
    for v in founders:
        items.append((v.pop or 1.0, v.culture_now()))
    cv = wavg(items) if items else blend({"earth_late": 1.0})
    sysid = col["system"]
    e = E[sysid]["idx"]
    # colony-era drift from environment and history
    hab = E[sysid]["settle"]
    adj = {}
    if hab < 0.5:
        adj.update(order_love=6, privacy=4, thrift=4)
    if col["regime_3126"] in ("security_oligarchy",):
        adj.update(outsider_trust=-10, collective_duty=6)
    if any(a["type"] == "logistics_hub" for a in col["assets"]):
        adj.update(outsider_trust=10, hospitality=5, rule_bending=3)
    if col.get("hyper_year") is None:
        adj.update(outsider_trust=-8, grievance_memory=5)
    for a, dv in col.get("culture_drift", {}).items():   # authored colony-era history (reason in world.json)
        adj[a] = adj.get(a, 0) + dv
    cv = {a: clamp(cv[a] + adj.get(a, 0)) for a in AXES}
    # knowledge: founding era, voyage retention, hyper-era convergence
    if founders:
        k0 = sum(ERA_K.get(missions[v.m["id"]]["wave"], 0.6) * v.knowledge for v in founders) / len(founders)
    else:
        k0 = 0.6
    hy = col.get("hyper_year")
    conv = 0.0 if hy is None else min(1.0, (YEAR_END - hy) / 300.0)
    know = k0 + (0.95 - k0) * conv * 0.8
    pop = col["pop_3126"]
    scale = 8.0 * math.log10(max(pop, 1e3) / 1e4)
    caps = {}
    for cap in CAPS:
        cult = sum(w * cv[a] for a, w in CAP_CULTURE[cap].items())
        end = sum(e[k] for k in CAP_ENDOW.get(cap, ())) / max(1, len(CAP_ENDOW.get(cap, ()))) if cap in CAP_ENDOW else 0.5
        val = 35 * know + 0.25 * cult + 18 * end + scale + col["bias"].get(cap, 0)
        caps[cap] = round(clamp(val), 1)
    return dict(culture=cv, knowledge=round(know, 3), caps=caps)


def fit_growth(p0, y0, p1, y1):
    if not p0 or y1 <= y0:
        return None
    return math.log(p1 / p0) / (y1 - y0)


# ---------------------------------------------------------------- network
def beacon_graph(world, E, year):
    hop = 0.0
    for y, h in world["max_hop_ly"]:
        if year >= y:
            hop = h
    nodes = {b["system"]: b for b in world["beacons"] if b["year"] <= year and not (b.get("quarantined_from") and year >= b["quarantined_from"])}
    for x in world.get("extra_links", []):
        if year >= x["year"]:
            for sid in (x["a"], x["b"]):
                nodes.setdefault(sid, {"system": sid, "year": x["year"], "operator": "lease"})
    edges = {}
    for x in world.get("extra_links", []):
        if year >= x["year"]:
            d = endow.distance(E[x["a"]], E[x["b"]]) * x.get("cost_factor", 1.0)
            edges.setdefault(x["a"], {})[x["b"]] = d
            edges.setdefault(x["b"], {})[x["a"]] = d
    for a in sorted(nodes):
        for b in sorted(nodes):
            if a < b:
                d = endow.distance(E[a], E[b])
                if d <= hop:
                    edges.setdefault(a, {})[b] = d
                    edges.setdefault(b, {})[a] = d
    return nodes, edges, hop


def shortest(edges, src, blocked=frozenset()):
    dist = {src: 0.0}
    hops = {src: 0}
    todo = [src]
    while todo:
        todo.sort(key=lambda n: (dist[n], n))
        u = todo.pop(0)
        for v, d in sorted(edges.get(u, {}).items()):
            if v in blocked:
                continue
            nd = dist[u] + 1.0 + d / 10.0  # per-hop handling + distance
            if v not in dist or nd < dist[v] - 1e-12:
                dist[v] = nd
                hops[v] = hops[u] + 1
                todo.append(v)
    return dist, hops


# ---------------------------------------------------------------- economy
GOODS = ["volatiles", "he3", "metals", "food", "components", "electronics", "hulls", "munitions"]
BASE_PRICE = {"volatiles": 1.0, "he3": 6.0, "metals": 2.0, "food": 1.5, "components": 5.0, "electronics": 12.0, "hulls": 40.0, "munitions": 15.0}
PER_CAP = {"volatiles": 0.6, "he3": 0.15, "metals": 0.25, "food": 1.0, "components": 0.15, "electronics": 0.05, "hulls": 0.006, "munitions": 0.004}
INPUTS = {"food": {"volatiles": 0.3}, "components": {"metals": 0.8, "he3": 0.05}, "electronics": {"metals": 0.3, "components": 0.2},
          "hulls": {"metals": 6.0, "components": 3.0, "electronics": 0.8, "he3": 0.5}, "munitions": {"metals": 1.0, "components": 0.6, "electronics": 0.2}}
SECTOR_CAP = {"volatiles": "extraction", "he3": "extraction", "metals": "metallurgy", "food": "bio", "components": "manufacturing",
              "electronics": "electronics", "hulls": "shipbuilding", "munitions": "military"}
ENDOW_GOOD = {"volatiles": lambda e: 0.15 + 0.6 * e["volatiles"] + 0.4 * e["water"], "he3": lambda e: 1.2 * e["he3"],
              "metals": lambda e: 0.1 + 0.9 * e["metals"] + 0.3 * e["strategic"], "food": lambda e: 0.55 + 0.45 * e["agri"]}
ASSET_GOOD = {"gas_platform": "he3", "ice_export": "volatiles", "mine": "metals", "refinery": "metals", "food_habitat": "food",
              "factory": "components", "electronics_fab": "electronics", "shipyard": "hulls", "arsenal_shipyard": "munitions"}


class Economy:
    def __init__(self, cols, profiles, E, world, links_year=YEAR_END):
        self.cols = [c for c in cols if c.get("economic_actor", True)]
        self.prof = profiles
        self.E = E
        self.world = world
        self.nodes, self.edges, _ = beacon_graph(world, E, links_year)
        self.mult = {c["id"]: {g: 1.0 for g in GOODS} for c in self.cols}
        self.demand_mult = {c["id"]: {g: 1.0 for g in GOODS} for c in self.cols}
        self.blocked = set()
        self.risk = {c["id"]: 0.0 for c in self.cols}
        self.extra_edges = []
        self._sp = {}
        for c in self.cols:
            for a in c["assets"]:
                g = ASSET_GOOD.get(a["type"])
                if g:
                    self.mult[c["id"]][g] += 0.6 * a["importance"]
        self.mult0 = json.loads(json.dumps(self.mult))
        self.calibrate()
        self.stock = {c["id"]: {g: self.consumption(c)[g] * 1.5 for g in GOODS} for c in self.cols}

    def consumption(self, c):
        L = c["pop_3126"] / 1e5
        out = {g: PER_CAP[g] * L * self.demand_mult[c["id"]][g] for g in GOODS}
        mil = self.prof[c["id"]]["caps"]["military"] / 100
        out["munitions"] *= (0.5 + 2.0 * mil)
        out["hulls"] *= (0.7 + 0.8 * mil)
        return out

    def eff(self, c, base=False):
        caps = self.prof[c["id"]]["caps"]
        e = self.E[c["system"]]["idx"]
        m = self.mult0[c["id"]] if base else self.mult[c["id"]]
        return {g: (caps[SECTOR_CAP[g]] / 60.0) ** 1.5 * (ENDOW_GOOD[g](e) if g in ENDOW_GOOD else 1.0) * m[g] for g in GOODS}

    def calibrate(self):
        """Labour follows comparative advantage against the world mean (fixed at
        baseline); k_g scales absolute output so that the 3126 baseline world is
        roughly balanced (units are arbitrary, differences are what matter)."""
        effs = {c["id"]: self.eff(c, base=True) for c in self.cols}
        self.mean_eff = {g: sum(effs[c][g] for c in effs) / len(effs) for g in GOODS}
        tot_cons = {g: sum(self.consumption(c)[g] for c in self.cols) for g in GOODS}
        val = {g: tot_cons[g] * BASE_PRICE[g] for g in GOODS}
        vt = sum(val.values())
        self.dshare = {g: val[g] / vt for g in GOODS}
        self.alloc = {}
        for c in self.cols:
            ef = effs[c["id"]]
            w = {g: (ef[g] / self.mean_eff[g]) ** 3 * self.dshare[g] for g in GOODS}
            for g in ("food", "volatiles"):   # nobody lets its own life support depend entirely on imports
                w[g] = max(w[g], 0.25 * self.dshare[g])
            tw = sum(w.values())
            self.alloc[c["id"]] = {g: w[g] / tw for g in GOODS}
        self.k = {g: 1.0 for g in GOODS}
        for _ in range(4):
            prod = {g: sum(self.raw(c)[g] for c in self.cols) for g in GOODS}
            need = dict(tot_cons)
            for g, ins in INPUTS.items():
                for inp, q in ins.items():
                    need[inp] += prod[g] * q
            self.k = {g: self.k[g] * 1.06 * need[g] / max(prod[g], 1e-9) for g in GOODS}

    def raw(self, c):
        L = c["pop_3126"] / 1e5
        ef = self.eff(c)
        return {g: L * self.alloc[c["id"]][g] * ef[g] * self.k[g] / BASE_PRICE[g] * 4.0 for g in GOODS}

    def capacity(self, c):
        return self.raw(c)

    def need(self, c):
        """Final consumption plus inputs for this colony's own planned output."""
        nc = getattr(self, "_need", None)
        if nc is not None and c["id"] in nc:
            return nc[c["id"]]
        out = dict(self.consumption(c))
        r = self.raw(c)
        for g, ins in INPUTS.items():
            for inp, q in ins.items():
                out[inp] += r[g] * q
        return out

    def price(self, cid, g, stock=None):
        c = self._byid[cid] if getattr(self, "_byid", None) else next(x for x in self.cols if x["id"] == cid)
        target = self.need(c)[g] * 1.15 + 1e-6
        src = getattr(self, "pre", None) or self.stock
        s = src[cid][g] if stock is None else stock
        return BASE_PRICE[g] * clamp((target / max(s, target * 0.05)) ** 0.7, 0.35, 8.0) * (1 + self.risk[cid])

    def transport(self, a_sys, b_sys):
        if a_sys == b_sys:
            return 0.0, 0
        if a_sys in self.blocked or b_sys in self.blocked:
            return None, None
        key = (a_sys, frozenset(self.blocked), len(self.extra_edges))
        if key not in self._sp:
            self._sp[key] = shortest(self._edges(), a_sys, frozenset(self.blocked))
        dist, hops = self._sp[key]
        if b_sys not in dist:
            return None, None
        return dist[b_sys], hops[b_sys]

    def _edges(self):
        if not self.extra_edges:
            return self.edges
        e = {k: dict(v) for k, v in self.edges.items()}
        for a, b, d in self.extra_edges:
            e.setdefault(a, {})[b] = d
            e.setdefault(b, {})[a] = d
        return e

    def step(self, relations):
        flows = []
        made = {}
        self._need = None
        self._need = {c["id"]: self.need(c) for c in self.cols}
        self._byid = {c["id"]: c for c in self.cols}
        for c in self.cols:
            cid = c["id"]
            cap = self.capacity(c)
            prod = {}
            for g in GOODS:
                q = cap[g]
                for inp, k in INPUTS.get(g, {}).items():
                    avail = self.stock[cid][inp] + cap.get(inp, 0) * 0.5
                    q = min(q, avail / k) if k > 0 else q
                prod[g] = max(0.0, q)
                for inp, k in INPUTS.get(g, {}).items():
                    self.stock[cid][inp] -= prod[g] * k
            for g in GOODS:
                self.stock[cid][g] += prod[g]
            made[cid] = prod
        # trade: deficits buy from cheapest reachable surplus (deterministic order)
        tc = {}
        for c in self.cols:
            for d in self.cols:
                if c is d:
                    continue
                cost, hops = self.transport(c["system"], d["system"])
                tc[(c["id"], d["id"])] = cost
        for g in GOODS:
            need = []
            for c in self.cols:
                cons = self.need(c)[g]
                gap = cons * 1.3 - self.stock[c["id"]][g]
                if gap > 0:
                    need.append((-self.price(c["id"], g), c["id"], gap))
            for _, buyer, gap in sorted(need):
                bsys = next(x["system"] for x in self.cols if x["id"] == buyer)
                offers = []
                for s in self.cols:
                    sid = s["id"]
                    if sid == buyer:
                        continue
                    surplus = self.stock[sid][g] - self.need(s)[g] * 1.1
                    if surplus <= 0:
                        continue
                    cost = tc.get((buyer, sid)) if s["system"] != bsys else 0.0
                    if cost is None:
                        continue
                    griev = relations.get(tuple(sorted((buyer, sid))), 0)
                    if griev >= 85:
                        continue  # embargo
                    delivered = self.price(sid, g) + 0.25 * cost * (1 + griev / 100) + 0.08 * BASE_PRICE[g]
                    offers.append((delivered, sid, surplus))
                for delivered, sid, surplus in sorted(offers):
                    if gap <= 0:
                        break
                    if delivered > self.price(buyer, g) * 1.15:
                        break
                    q = min(gap, surplus)
                    self.stock[sid][g] -= q
                    self.stock[buyer][g] += q
                    gap -= q
                    flows.append((g, sid, buyer, q, delivered))
        # consumption and losses (prices read the pre-consumption availability)
        self.pre = {cid: dict(v) for cid, v in self.stock.items()}
        for c in self.cols:
            cons = self.consumption(c)
            for g in GOODS:
                self.stock[c["id"]][g] = max(0.0, self.stock[c["id"]][g] - cons[g]) * 0.97
        return made, flows

    def run(self, years, relations):
        hist = []
        for _ in range(years):
            hist.append(self.step(relations))
        return hist

    def snapshot(self, hist, years=5):
        made = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        imp = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        exp = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        partners = {c["id"]: {} for c in self.cols}
        for m, flows in hist[-years:]:
            for cid, p in m.items():
                for g in GOODS:
                    made[cid][g] += p[g] / years
            for g, s, b, q, _ in flows:
                imp[b][g] += q / years
                exp[s][g] += q / years
                partners[b][s] = partners[b].get(s, 0.0) + q * BASE_PRICE[g] / years
                partners[s][b] = partners[s].get(b, 0.0) + q * BASE_PRICE[g] / years
        prices = {c["id"]: {g: round(self.price(c["id"], g), 2) for g in GOODS} for c in self.cols}
        return dict(made=made, imp=imp, exp=exp, prices=prices, partners=partners)


SHOCKS = [
    dict(id="mine_destroyed", text="главный рудник/комбинат уничтожен (металлы −70 %)", kind="asset", good="metals", factor=0.3),
    dict(id="gas_platform_captured", text="газовая платформа захвачена (He-3 −80 %)", kind="asset", good="he3", factor=0.2),
    dict(id="food_blight", text="грибок в агрокуполах (пища −45 %)", kind="asset", good="food", factor=0.55),
    dict(id="shipyard_destroyed", text="верфь уничтожена (корпуса −85 %)", kind="asset", good="hulls", factor=0.15),
    dict(id="route_blocked", text="маяк системы отключён (блокада)", kind="block"),
    dict(id="earth_fleet", text="Земля объявляет «Возвращение»: угроза войны", kind="war"),
]


def apply_shock(econ, shock, cid):
    col = next(c for c in econ.cols if c["id"] == cid)
    if shock["kind"] == "asset":
        econ.mult[cid][shock["good"]] *= shock["factor"]
    elif shock["kind"] == "block":
        econ.blocked.add(col["system"])
    elif shock["kind"] == "war":
        for c in econ.cols:
            econ.demand_mult[c["id"]]["munitions"] *= 2.5
            econ.demand_mult[c["id"]]["hulls"] *= 1.6
            econ.risk[c["id"]] += 0.15


# ---------------------------------------------------------------- social response
RESPONSES = {
    "ration_mobilize": ({"collective_duty": 0.35, "hardship_pride": 0.3, "power_distance": 0.2, "state_trust": 0.15}, "пайки, мобилизация, «всё для фронта»"),
    "buy_from_neighbors": ({"outsider_trust": 0.5, "hospitality": 0.2, "thrift": -0.1, "say_no": 0.1}, "спокойно докупить у соседей"),
    "market_profiteering": ({"rule_bending": 0.35, "status_display": 0.25, "risk": 0.2, "state_trust": -0.2}, "бизнес взвинчивает цены, государство теряет контроль"),
    "scapegoat_purge": ({"grievance_memory": 0.3, "outsider_trust": -0.3, "power_distance": 0.2, "irony_to_power": -0.2, "order_love": 0.2}, "найти внутреннего врага и устроить чистку"),
    "revenge_strike": ({"grievance_memory": 0.35, "open_conflict": 0.25, "risk": 0.2, "hardship_pride": 0.2}, "месть и удар по виновнику"),
    "committee_procedure": ({"formalism": 0.5, "clock_time": 0.2, "state_trust": 0.2, "risk": -0.1}, "комиссия, регламент, решение через годы"),
    "informal_fix": ({"improvisation": 0.4, "rule_bending": 0.3, "kin_first": 0.2, "formalism": -0.2}, "обойти, починить, достать «по знакомству»"),
    "joke_and_endure": ({"gallows_humor": 0.4, "complaint_ritual": 0.3, "hardship_pride": 0.3}, "пошутить, поворчать и терпеть"),
    "exodus": ({"risk": 0.4, "kin_first": 0.2, "collective_duty": -0.3, "state_trust": -0.1}, "уйти: семьи и кланы уезжают"),
    "pray_ritual": ({"collective_duty": 0.4, "order_love": 0.2, "irony_to_power": -0.3}, "молитва, пост, обряд очищения"),
}
REGIME_RESP = {
    "security_oligarchy": {"ration_mobilize": 8, "scapegoat_purge": 10, "revenge_strike": 6},
    "party_technocracy": {"ration_mobilize": 12, "scapegoat_purge": 4, "committee_procedure": 4},
    "sage_council": {"ration_mobilize": 5, "committee_procedure": 6, "buy_from_neighbors": 6},
    "technocratic_meritocracy": {"buy_from_neighbors": 6, "informal_fix": 4},
    "parliamentary_bureaucracy": {"committee_procedure": 14},
    "shareholder_agora": {"buy_from_neighbors": 8, "market_profiteering": 6},
    "corporate_board": {"market_profiteering": 12},
    "corporate_democracy": {"market_profiteering": 6, "buy_from_neighbors": 4},
    "scholar_theocracy": {"pray_ritual": 8, "committee_procedure": 4},
    "caste_theocracy": {"pray_ritual": 10, "scapegoat_purge": 8},
    "synod_theocracy": {"pray_ritual": 14},
    "machine_oracle": {"committee_procedure": 6, "informal_fix": -6},
    "council_of_chiefs": {"ration_mobilize": 4, "buy_from_neighbors": 4},
    "grandees_and_church": {"informal_fix": 6, "joke_and_endure": 4, "pray_ritual": 4},
    "clan_contracts": {"market_profiteering": 8, "exodus": 8},
    "merit_caste_republic": {"ration_mobilize": 4, "informal_fix": 4},
    "exile_republic": {"ration_mobilize": 6},
    "litigating_city_states": {"committee_procedure": 8, "market_profiteering": 4},
}


SHOCK_BIAS = {
    "mine_destroyed": {"committee_procedure": 4, "informal_fix": 4, "scapegoat_purge": 3},
    "gas_platform_captured": {"revenge_strike": 10, "ration_mobilize": 6},
    "food_blight": {"pray_ritual": 6, "informal_fix": 4, "ration_mobilize": 4, "scapegoat_purge": 3},
    "shipyard_destroyed": {"committee_procedure": 4, "scapegoat_purge": 4, "revenge_strike": 3},
    "route_blocked": {"ration_mobilize": 6, "informal_fix": 6, "exodus": 2},
    "earth_fleet": {"ration_mobilize": 6, "revenge_strike": 2},
}


def social_response(col, prof, shock, ctx):
    """Same shock, different society: culture (deviation from 50) + institutions
    (regime) + situation (alternative suppliers, severity, wealth, isolation)."""
    cv = prof["culture"]
    sev = ctx["severity"]
    scores = {}
    why = {}
    for r, (w, _) in RESPONSES.items():
        terms = {f"{a}{'↑' if cv[a] >= 50 else '↓'}": k * (cv[a] - 50) for a, k in w.items()}
        terms["режим"] = REGIME_RESP.get(col["regime_3126"], {}).get(r, 0)
        terms["тип шока"] = SHOCK_BIAS.get(shock["id"], {}).get(r, 0)
        if r == "buy_from_neighbors":
            terms["альтернативные поставщики"] = 4.0 * ctx["alternatives"] - (15 if ctx["isolated"] else 0)
            terms["богатство"] = 6.0 * ctx["wealth"]
        if r == "ration_mobilize":
            terms["тяжесть"] = 5.0 * sev
            terms["можно купить"] = -2.5 * ctx["alternatives"] * ctx["wealth"]
        if r == "market_profiteering":
            terms["дефицит"] = 4.0 * sev
        if r == "scapegoat_purge":
            terms["тяжесть"] = 2.5 * sev
        if r == "revenge_strike":
            terms["военный потенциал"] = 0.2 * (prof["caps"]["military"] - 60)
        if r == "exodus":
            terms["некуда бежать"] = -10 if ctx["isolated"] else 0
        scores[r] = sum(terms.values())
        why[r] = sorted(terms.items(), key=lambda kv: -abs(kv[1]))[:3]
    order = sorted(scores, key=lambda r: (-scores[r], r))
    return [(r, round(scores[r], 1), RESPONSES[r][1], [(k, round(v, 1)) for k, v in why[r]]) for r in order[:2]]


EARTH_OPTIONS = ["submission", "negotiation", "mobilisation", "alliance", "neutrality", "profiteering", "betrayal", "internal_split", "preemptive_strike"]


def earth_reaction(col, prof, E, ctx):
    cv = prof["culture"]
    d_sol = E[col["system"]]["dist"]
    near = max(0.0, 1.0 - d_sol / 25.0)
    mil = prof["caps"]["military"] / 100
    s = {
        "submission": 20 * near * (1 - mil) + 0.2 * (cv["power_distance"] - 50) - 0.3 * (cv["grievance_memory"] - 50) + (8 if col["regime_3126"] == "none_quiet" else 0),
        "negotiation": 0.3 * (cv["outsider_trust"] - 50) + 0.2 * (cv["formalism"] - 50) + 6 * ctx["hub"] + 4,
        "mobilisation": 25 * near * mil + 0.3 * (cv["collective_duty"] - 50) + 0.2 * (cv["hardship_pride"] - 50) + 8 * mil,
        "alliance": 6 * ctx["n_partners"] / 4 + 0.2 * (cv["outsider_trust"] - 50) + 10 * near * (1 - mil),
        "neutrality": 12 * (1 - near) + 0.2 * (cv["privacy"] - 50) - 4 * ctx["n_partners"] / 4 + (10 if col.get("hyper_year") is None else 0),
        "profiteering": 0.35 * (cv["rule_bending"] - 50) + 0.25 * (cv["status_display"] - 50) + 8 * ctx["exports_war_goods"] + (8 if col["regime_3126"] in ("corporate_board", "clan_contracts") else 0),
        "betrayal": 0.3 * (cv["grievance_memory"] - 50) + 12 * ctx["max_grievance"] / 100 + 0.2 * (cv["rule_bending"] - 50) - 0.2 * (cv["collective_duty"] - 50),
        "internal_split": 25 * ctx["internal_split"] + 0.2 * (cv["open_conflict"] - 50),
        "preemptive_strike": 30 * near * mil * (cv["risk"] / 100) + 0.2 * (cv["grievance_memory"] - 50) + (10 if col["regime_3126"] == "security_oligarchy" else 0) - 10 * (1 - mil),
    }
    order = sorted(s, key=lambda k: (-s[k], k))
    return [(k, round(s[k], 1)) for k in order[:3]]


# ---------------------------------------------------------------- run
def run(seed):
    E = endow.load_endowment()
    MS = load("missions.json")
    W = load("world.json")
    missions = {m["id"]: m for m in MS["missions"]}
    voyages = {mid: Voyage(m, E, seed).run() for mid, m in sorted(missions.items())}
    sanity = {}
    for mid, m in sorted(missions.items()):
        v = voyages[mid]
        if m["population"] <= 0 or m["id"] in ("IRON-SHIELD-2", "HOMEWARD-1", "HYPERION-1"):
            continue
        # average speed incl. accel/brake -> cruise ~8 % higher; ships without an arrival
        # anchor use the canonical average from the registry (lost ships: distance to loss)
        avg = v.avg_c or {"DEEP-SEEK-1": 38.4 / 280, "KEPLER-1": 0.10, "FINAL-ARK-1": 0.058, "FAREWELL-1": 0.05,
                          "UBUNTU-1": 0.09, "NIRVANA-1": 0.014, "NOMAD-1": 0.0075}.get(m["id"], 0.08)
        cruise = avg * 1.08
        d = physics.design(m["population"], m["cryo_share"], cruise, m["launch_year"],
                           awake_override=m["awake_watch"] if m["cryo_share"] > 0 else None,
                           claimed_full_mt=m["claimed_mass_mt"].get("full"), claimed_dry_mt=m["claimed_mass_mt"].get("dry"))
        verdict, reasons = physics.verdict(d)
        sanity[mid] = dict(design={k: (round(x, 4) if isinstance(x, float) else x) for k, x in d.items()}, verdict=verdict, reasons=reasons, cruise_c=round(cruise, 4))
    # colonies
    cols = W["colonies"]
    profiles = {c["id"]: colony_profile(c, voyages, E, missions) for c in cols}
    growth = {}
    for c in cols:
        fv = [voyages[f] for f in c["founders"] if f in voyages and voyages[f].arrival]
        p0 = sum(v.arrival["pop_after"] for v in fv) if fv else None
        g = fit_growth(p0, c["founded"], c["pop_3126"], YEAR_END) if p0 else None
        flag = None
        if g is not None and g > 0.012 and (c.get("hyper_year") is None or c["founded"] > 2700):
            flag = "рост выше 1.2 %/год без гипер-миграции — проверить"
        if g is not None and g > 0.03:
            flag = "рост неправдоподобен (> 3 %/год)"
        growth[c["id"]] = dict(p0=p0, rate=None if g is None else round(g * 100, 3), flag=flag,
                               path={y: (int(p0 * math.exp(g * (y - c["founded"]))) if (p0 and g is not None and y >= c["founded"]) else None)
                                     for y in (2300, 2500, 2750, 2900, 3000, 3126)})
    relations = {}
    for r in W["relations_anchor"]:
        relations[tuple(sorted((r["a"], r["b"])))] = r["grievance"]
    # baseline economy
    econ = Economy(cols, profiles, E, W)
    hist = econ.run(30, relations)
    base = econ.snapshot(hist)
    # wealth / derived
    wealth = {}
    for c in econ.cols:
        cid = c["id"]
        val = sum(base["made"][cid][g] * BASE_PRICE[g] for g in GOODS) + 0.5 * sum(base["exp"][cid][g] * BASE_PRICE[g] for g in GOODS)
        hubs = sum(a["importance"] for a in c["assets"] if a["type"] in ("logistics_hub", "legal_hub", "jump_infrastructure"))
        val += hubs * c["pop_3126"] / 1e5 * 6.0
        wealth[cid] = round(val / (c["pop_3126"] / 1e5), 2)
    # relations matrix (multi-dimensional)
    rel = {}
    eco = [c for c in cols if c.get("economic_actor", True)]
    for i, a in enumerate(eco):
        for b in eco[i + 1:]:
            key = tuple(sorted((a["id"], b["id"])))
            tr = base["partners"][a["id"]].get(b["id"], 0.0)
            tot_a = sum(base["partners"][a["id"]].values()) or 1.0
            cult = 100 - 2.2 * cdist(profiles[a["id"]]["culture"], profiles[b["id"]]["culture"])
            same_sys = a["system"] == b["system"]
            griev = relations.get(key, 0)
            rel["|".join(key)] = dict(trade=round(100 * tr / tot_a, 1), culture=round(cult, 1), grievance=griev,
                                      competition=60 if same_sys else 0,
                                      stance=("враг-сосед" if griev >= 60 else "соперник" if (same_sys or griev >= 30) else
                                              "партнёр" if tr > 0.15 * tot_a else "нейтрал"))
    # shocks
    shock_out = {}
    targets = [c["id"] for c in econ.cols if c["pop_3126"] >= 40000]

    def scenario(sh, cid):
        e2 = Economy(cols, profiles, E, W)
        e2.stock = json.loads(json.dumps(econ.stock))
        e2.run(5, relations)
        if sh:
            apply_shock(e2, sh, cid)
        return e2.snapshot(e2.run(8, relations))

    ctrl = scenario(None, None)   # same procedure without the shock: deltas are causal, not drift
    for sh in SHOCKS:
        for cid in (targets if sh["kind"] != "war" else ["_all"]):
            s2 = scenario(sh, cid if cid != "_all" else targets[0])
            deltas = {}
            for c in econ.cols:
                d = {}
                for g in GOODS:
                    p0, p1 = ctrl["prices"][c["id"]][g], s2["prices"][c["id"]][g]
                    m0, m1 = ctrl["made"][c["id"]][g], s2["made"][c["id"]][g]
                    if abs(p1 - p0) / p0 > 0.08 or (m0 > 0.05 and abs(m1 - m0) / m0 > 0.08):
                        d[g] = dict(price=round(p1 / p0 - 1, 3), output=round((m1 - m0) / m0, 3) if m0 > 0.05 else None)
                if d:
                    deltas[c["id"]] = d
            shock_out[f"{sh['id']}@{cid}"] = deltas
    # social responses: same shock for every major colony
    social = {}
    for sh in SHOCKS:
        for c in eco:
            cid = c["id"]
            sys_ = c["system"]
            key = f"{sh['id']}@{cid}" if f"{sh['id']}@{cid}" in shock_out else f"{sh['id']}@_all"
            own = shock_out.get(key, {}).get(cid, {})
            hit = sh.get("good")
            pj = abs(own.get(hit, {}).get("price", 0)) if hit else max([abs(v.get("price", 0)) for v in own.values()] or [0])
            if hit:
                sup = sum(1 for s2 in eco if s2["id"] != cid and base["exp"][s2["id"]][hit] > 0.05
                          and relations.get(tuple(sorted((cid, s2["id"]))), 0) < 60)
            else:
                sup = sum(1 for v in base["partners"][cid].values() if v > 0.5)
            isolated = (sys_ not in econ.nodes) or sh["kind"] == "block"
            ctx = dict(alternatives=0 if isolated else min(4, sup), isolated=isolated,
                       wealth=min(1.5, wealth[cid] / 25.0), severity=min(3.0, pj))
            social[f"{sh['id']}@{cid}"] = social_response(c, profiles[cid], sh, ctx)
    earth = {}
    for c in cols:
        cid = c["id"]
        if not c.get("economic_actor", True):
            continue
        mg = max([v for k, v in relations.items() if cid in k] or [0])
        same = [x for x in cols if x["system"] == c["system"] and x["id"] != cid]
        ctx = dict(n_partners=sum(1 for v in base["partners"][cid].values() if v > 0.05),
                   hub=sum(a["importance"] for a in c["assets"] if a["type"] in ("logistics_hub", "legal_hub")),
                   exports_war_goods=1.0 if (base["exp"][cid]["munitions"] + base["exp"][cid]["hulls"]) > 0.05 else 0.0,
                   max_grievance=mg, internal_split=0.6 if (same and mg >= 50) else (0.4 if c["regime_3126"] in ("shareholder_agora", "corporate_democracy", "litigating_city_states") else 0.1))
        earth[cid] = earth_reaction(c, profiles[cid], E, ctx)
    # network chronology
    net = {}
    for y in (2760, 2800, 2850, 2900, 3000, 3126):
        nodes, edges, hop = beacon_graph(W, E, y)
        comp = []
        seen = set()
        for n in sorted(nodes):
            if n in seen:
                continue
            reach, _ = shortest(edges, n)
            comp.append(sorted(reach))
            seen |= set(reach)
        net[y] = dict(hop=hop, nodes=sorted(nodes), components=comp)
    return dict(seed=seed, E=E, missions=missions, voyages=voyages, sanity=sanity, cols=cols, profiles=profiles, growth=growth,
                base=base, wealth=wealth, rel=rel, shocks=shock_out, social=social, earth=earth, net=net, world=W)


def to_json(R):
    out = dict(seed=R["seed"], missions={}, colonies={}, economy={}, relations=R["rel"], shocks=R["shocks"],
               social=R["social"], earth_3126=R["earth"], network={str(k): v for k, v in R["net"].items()}, sanity=R["sanity"])
    for mid, v in sorted(R["voyages"].items()):
        out["missions"][mid] = dict(events=v.events, flags=v.flags, avg_c=None if v.avg_c is None else round(v.avg_c, 4),
                                    required_watch=getattr(v, "req_watch", None), cycles_needed=round(getattr(v, "cycles_needed", 0.0), 2),
                                    hereditary_watch=getattr(v, "hereditary_watch", False), keeper_index=round(v.keeper, 1),
                                    owner_sleep_years=round(v.owner_sleep, 1), unrecorded_losses=getattr(v, "unrecorded_losses", None),
                                    arrival_tendency=getattr(v, "arrival_tendency", None), knowledge=round(v.knowledge, 3),
                                    culture={a: round(x, 1) for a, x in v.culture_now().items()},
                                    cohorts={k: dict(power=round(c["power"], 2), grievance=round(c["grievance"], 1)) for k, c in sorted(v.cohorts.items())})
    for c in R["cols"]:
        p = R["profiles"][c["id"]]
        out["colonies"][c["id"]] = dict(name=c["name"], system=c["system"], pop_3126=c["pop_3126"], regime=c["regime_3126"],
                                        knowledge=p["knowledge"], caps=p["caps"], culture={a: round(x, 1) for a, x in p["culture"].items()},
                                        growth=R["growth"][c["id"]], wealth_per_100k=R["wealth"].get(c["id"]))
    b = R["base"]
    for cid in sorted(b["made"]):
        out["economy"][cid] = dict(made={g: round(x, 3) for g, x in b["made"][cid].items()}, imports={g: round(x, 3) for g, x in b["imp"][cid].items()},
                                   exports={g: round(x, 3) for g, x in b["exp"][cid].items()}, prices=b["prices"][cid],
                                   partners={k: round(x, 2) for k, x in sorted(b["partners"][cid].items(), key=lambda kv: -kv[1])[:5]})
    return out


def fmt_report(R, J):
    E = R["E"]
    L = ["# Colonization lab — отчёт прогона", "", f"seed = {R['seed']}; вход: canon/*.json + каталог galaxy_details; детерминированно.", ""]
    L += ["## 1. Миссии: скорость, вахта, крио", "",
          "| миссия | старт | прибытие/конец | ср. скорость c | мин. вахта | вахта | циклов/чел | наследств. вахта | хранители | сон владельцев, лет | неучтённые потери | тенденция режима | канон режима | флаги |",
          "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for mid, v in sorted(R["voyages"].items(), key=lambda kv: kv[1].m["launch_year"]):
        m = v.m
        canon_reg = m.get("arrival_regime", "")
        fam = REGIME_FAMILY.get(canon_reg, "")
        tend = getattr(v, "arrival_tendency", "") or ""
        agree = "" if not tend or not fam else (" ✓" if tend == fam else " ≠")
        L.append(f"| {mid} | {m['launch_year']} | {v.end_year} | {'' if v.avg_c is None else f'{v.avg_c:.3f}'} | {getattr(v, 'req_watch', '')} | {m['awake_watch']} | "
                 f"{getattr(v, 'cycles_needed', 0):.1f} / {m['safe_cycles']} | {'да' if getattr(v, 'hereditary_watch', False) else 'нет'} | {v.keeper:.0f} | {v.owner_sleep:.0f} | "
                 f"{'' if getattr(v, 'unrecorded_losses', None) is None else v.unrecorded_losses} | {tend}{agree} | {canon_reg} | {'; '.join(v.flags)} |")
    L += ["", "## 2. Хроники миссий (anchor = канон, simulated = заполнение пробелов)", ""]
    for mid, v in sorted(R["voyages"].items(), key=lambda kv: kv[1].m["launch_year"]):
        L.append(f"### {mid}")
        for e in v.events:
            L.append(f"- {e['year']} [{e['source']}] {e['type']}: {e['text']}" + (f" (погибло {e['deaths']})" if e['deaths'] else "") + f" — нас. {e['pop']}")
        L.append("")
    L += ["## 3. Инженерная проверка (physics.py)", "", "| миссия | эра | ср. скорость c | сухая Мт (расчёт / заявлено) | полная Мт (расчёт / заявлено) | отношение масс | He-3, Мт | атмосфер гиганта, т | ускорение g | мощность струи, ПВт | радиаторы разгона, км² | вердикт |", "|---|---|---|---|---|---|---|---|---|---|---|---|"]
    for mid, s in sorted(R["sanity"].items(), key=lambda kv: R["missions"][kv[0]]["launch_year"]):
        d = s["design"]
        L.append(f"| {mid} | {d['era']} | {s['cruise_c']:.3f} | {d['dry_mt']:.1f} / {d.get('claimed_dry_mt', '-')} | {d['full_mt']:.0f} / {d.get('claimed_full_mt', '-')} | {d['mass_ratio']:.1f} | "
                 f"{d['he3_mt']:.1f} | {d['giant_atmo_processed_t']:.1e} | {d['accel_g']:.4f} | {d['jet_power_pw']:.1f} | {d['boost_radiator_km2']:.0f} | "
                 f"{'возможно' if s['verdict'] == 'possible' else 'исправить: ' + '; '.join(s['reasons'])} |")
    L += ["", "## 4. Колонии 3126", "", "| колония | система | нас. 3126 | основание (нас.) | рост %/год | режим | знание | богатство /100k | " + " | ".join(CAPS) + " | флаг |", "|" + "---|" * (9 + len(CAPS))]
    for c in R["cols"]:
        p = R["profiles"][c["id"]]
        g = R["growth"][c["id"]]
        L.append(f"| {c['name']} | {E[c['system']]['name']} | {c['pop_3126']:,} | {c['founded']} ({g['p0'] or '-'}) | {g['rate'] if g['rate'] is not None else '-'} | {c['regime_3126']} | {p['knowledge']} | "
                 f"{R['wealth'].get(c['id'], '-')} | " + " | ".join(f"{p['caps'][k]:.0f}" for k in CAPS) + f" | {g['flag'] or ''} |")
    L += ["", "## 5. Экономика 3126 (условные единицы/год; цены относительно базовых)", ""]
    b = R["base"]
    L += ["| колония | производит (топ) | импорт (топ) | экспорт (топ) | цены выше 1.5× базы | главные партнёры |", "|---|---|---|---|---|---|"]
    for c in R["cols"]:
        cid = c["id"]
        if cid not in b["made"]:
            continue
        top = lambda d: ", ".join(f"{g} {d[g]:.2f}" for g in sorted(d, key=lambda g: -d[g] * BASE_PRICE[g])[:3] if d[g] > 0.01)
        hi = ", ".join(f"{g}×{b['prices'][cid][g] / BASE_PRICE[g]:.1f}" for g in GOODS if b["prices"][cid][g] > 1.5 * BASE_PRICE[g])
        pn = ", ".join(k for k, _ in sorted(b["partners"][cid].items(), key=lambda kv: -kv[1])[:3])
        L.append(f"| {c['name']} | {top(b['made'][cid])} | {top(b['imp'][cid])} | {top(b['exp'][cid])} | {hi} | {pn} |")
    L += ["", "## 6. Шоки: последствия через 8 лет (своя колония + 4 самых задетых соседа; |Δ| ≥ 15 %)", ""]

    def line(d, cid):
        items = [(g, v) for g, v in sorted(d[cid].items()) if abs(v["price"]) >= 0.15 or (v["output"] is not None and abs(v["output"]) >= 0.15)]
        return "; ".join(f"{g} цена {v['price']:+.0%}" + (f", выпуск {v['output']:+.0%}" if v["output"] is not None and abs(v["output"]) >= 0.05 else "")
                         for g, v in items)

    for k in sorted(R["shocks"]):
        d = R["shocks"][k]
        cid0 = k.split("@")[1]
        L.append(f"### {k}")
        if cid0 in d:
            L.append(f"- **{cid0}**: {line(d, cid0) or '—'}")
        others = sorted([c for c in d if c != cid0], key=lambda c: (-max(abs(v["price"]) for v in d[c].values()), c))[:4]
        for cid in others:
            ln = line(d, cid)
            if ln:
                L.append(f"- {cid}: {ln}")
        L.append("")
    L += ["## 7. Социальный ответ на одинаковый шок", ""]
    for sh in SHOCKS:
        L.append(f"### {sh['id']} — {sh['text']}")
        L.append("| колония | 1-й ответ | 2-й ответ | почему (главные члены) |")
        L.append("|---|---|---|---|")
        tops = set()
        for c in R["cols"]:
            k = f"{sh['id']}@{c['id']}"
            if k not in R["social"]:
                continue
            r = R["social"][k]
            tops.add(r[0][0])
            L.append(f"| {c['name']} | {r[0][2]} ({r[0][1]}) | {r[1][2]} ({r[1][1]}) | {', '.join(f'{a} {v:+}' for a, v in r[0][3])} |")
        L.append(f"\nразных первых ответов: {len(tops)}\n")
    L += ["## 8. Реакция на Землю-3126 (топ-3)", ""]
    for c in R["cols"]:
        if c["id"] in R["earth"]:
            L.append(f"- {c['name']}: " + ", ".join(f"{k} ({v})" for k, v in R["earth"][c["id"]]))
    L += ["", "## 9. Отношения (торговля % оборота / культурная близость / обида / стойка)", ""]
    for k in sorted(R["rel"]):
        r = R["rel"][k]
        if r["trade"] > 10 or r["grievance"] or r["competition"]:
            L.append(f"- {k}: торговля {r['trade']} %, культура {r['culture']}, обида {r['grievance']}, конкуренция {r['competition']} → {r['stance']}")
    L += ["", "## 10. Сеть маяков по годам", ""]
    for y, n in sorted(R["net"].items()):
        L.append(f"- {y}: прыжок ≤ {n['hop']} св. лет; компоненты: " + " | ".join(",".join(str(x) for x in comp) for comp in n["components"]))
    return "\n".join(L) + "\n"


def check_dataset():
    """Runtime JSON must not silently diverge from the canon: every runtime ship
    id must be a canon mission (or explicitly retired), and canon fields that the
    runtime file carries must match."""
    path = os.path.normpath(os.path.join(HERE, "..", "..", "src", "assets", "data", "galaxy", "generation_ships.json"))
    with open(path, encoding="utf-8") as f:
        data = json.load(f)
    MS = load("missions.json")
    canon = {m["id"]: m for m in MS["missions"]}
    retired = {r["id"] for r in MS["retired"]}
    problems = []
    seen = set()
    for s in data["ships"]:
        c = s.get("canon")
        if not c:
            problems.append(f"{s['id']}: нет блока canon")
            continue
        sid = c["id"]
        seen.add(sid)
        if sid not in canon:
            if c.get("status") != "retired" or sid not in retired:
                problems.append(f"{sid}: нет в каноне и не помечен retired")
            continue
        m = canon[sid]
        for k_rt, k_c in (("launch_year", "launch_year"), ("population", "population"), ("final_system", "final_system"), ("cryo_share", "cryo_share")):
            if c.get(k_rt) != m.get(k_c):
                problems.append(f"{sid}: canon.{k_rt}={c.get(k_rt)} != {m.get(k_c)}")
    for mid in canon:
        if mid not in seen:
            problems.append(f"{mid}: канонической миссии нет в runtime-файле")
    print("dataset check:", "OK" if not problems else f"{len(problems)} проблем")
    for p in problems:
        print("  -", p)
    return 0 if not problems else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=3126)
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--check-dataset", action="store_true")
    a = ap.parse_args()
    if a.check_dataset:
        sys.exit(check_dataset())
    R = run(a.seed)
    J = to_json(R)
    os.makedirs(a.out, exist_ok=True)
    js = json.dumps(J, ensure_ascii=False, indent=1, sort_keys=True)
    md = fmt_report(R, J)
    with open(os.path.join(a.out, "lab_state_3126.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(js)
    with open(os.path.join(a.out, "lab_report.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(md)
    h = hashlib.sha256((js + md).encode("utf-8")).hexdigest()
    print(f"seed={a.seed} missions={len(R['voyages'])} colonies={len(R['cols'])} sha256={h}")


if __name__ == "__main__":
    main()
