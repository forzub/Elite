"""Interstellar economy 3126 on the causal populations (replaces the per-capita toy market).

Differences from the legacy market (kept in lab.py as LegacyEconomy for comparison):
  * production and demand scale with population AND income (automation), not with headcount;
  * every good has a mass per value unit; interstellar freight is paid per mass and route
    cost (jumps, handling, beacon fees), so bulk only travels when the buyer has NO local
    source or the local source is dearer than the landed import (the rule is checked per
    flow and the break-even freight is reported);
  * high-value goods: precision equipment, biotech/catalysts, rare isotopes, jump cores
    (a state secret of the Beacon Authority; own designs at New Mecca and the Citadel;
    pirate copies after the 2862 leak);
  * corridors carry reason, alternatives, choke points (intermediate beacons) and controllers;
  * political access (political.py, canon/political_order.json): every flow seller -> buyer is
    gated by the access level of the good's category (licence, own technology, sanctions,
    exceptions): level 4 = no flow, 1..3 = price markup (licence, checks / brokers, grey market);
    end-use-controlled categories (sealed jump cores) are exported only by their producer.
Deterministic: sorted iteration only.
"""
import json
import math

GOODS = ["volatiles", "he3", "metals", "food", "components", "electronics", "hulls", "munitions",
         "precision", "biotech", "isotopes", "drive_cores"]
BULK = ("volatiles", "metals", "food")
BASE_PRICE = {"volatiles": 1.0, "he3": 6.0, "metals": 2.0, "food": 1.5, "components": 5.0, "electronics": 12.0, "hulls": 40.0,
              "munitions": 15.0, "precision": 25.0, "biotech": 20.0, "isotopes": 30.0, "drive_cores": 300.0}
# tonnes-equivalent per value unit (assumption F11): water and ore are heavy per value, cores are not
MASS = {"volatiles": 1.0, "he3": 0.03, "metals": 1.0, "food": 0.8, "components": 0.25, "electronics": 0.02, "hulls": 0.05,
        "munitions": 0.15, "precision": 0.04, "biotech": 0.02, "isotopes": 0.01, "drive_cores": 0.005}
PER_CAP = {"volatiles": 0.6, "he3": 0.15, "metals": 0.25, "food": 1.0, "components": 0.15, "electronics": 0.05, "hulls": 0.006,
           "munitions": 0.004, "precision": 0.03, "biotech": 0.03, "isotopes": 0.006, "drive_cores": 0.0012}
INPUTS = {"food": {"volatiles": 0.3}, "components": {"metals": 0.8, "he3": 0.05}, "electronics": {"metals": 0.3, "components": 0.2},
          "hulls": {"metals": 6.0, "components": 3.0, "electronics": 0.8, "he3": 0.5, "drive_cores": 0.02},
          "munitions": {"metals": 1.0, "components": 0.6, "electronics": 0.2},
          "precision": {"metals": 0.3, "electronics": 0.3, "components": 0.4}, "biotech": {"food": 0.2, "volatiles": 0.1},
          "drive_cores": {"isotopes": 3.0, "electronics": 5.0, "precision": 4.0}}
SECTOR_CAP = {"volatiles": "extraction", "he3": "extraction", "metals": "metallurgy", "food": "bio", "components": "manufacturing",
              "electronics": "electronics", "hulls": "shipbuilding", "munitions": "military", "precision": "manufacturing",
              "biotech": "bio", "isotopes": "extraction", "drive_cores": "research"}
ENDOW_GOOD = {"volatiles": lambda e: 0.15 + 0.6 * e["volatiles"] + 0.4 * e["water"], "he3": lambda e: 1.2 * e["he3"],
              "metals": lambda e: 0.1 + 0.9 * e["metals"] + 0.3 * e["strategic"], "food": lambda e: 0.55 + 0.45 * e["agri"],
              "isotopes": lambda e: 0.02 + 1.0 * e["strategic"] ** 2}
ASSET_GOOD = {"gas_platform": "he3", "ice_export": "volatiles", "mine": "metals", "refinery": "metals", "food_habitat": "food",
              "factory": "components", "electronics_fab": "electronics", "shipyard": "hulls", "arsenal_shipyard": "munitions",
              "jump_infrastructure": "drive_cores", "research": "precision", "strategic_mine": "isotopes"}
FREIGHT = 1.5        # price units per tonne-equivalent per unit of route cost (1 + d/10 per jump), assumption F11
HANDLING = 0.03      # share of the base price per shipment (insurance, brokerage)


def clamp(x, lo=0.0, hi=100.0):
    return lo if x < lo else hi if x > hi else x


def route(edges, src, dst, blocked=frozenset()):
    """Dijkstra with predecessors: (cost, hops, path) or (None, None, None)."""
    if src == dst:
        return 0.0, 0, [src]
    dist = {src: 0.0}
    prev = {}
    todo = [src]
    while todo:
        todo.sort(key=lambda n: (dist[n], n))
        u = todo.pop(0)
        if u == dst:
            break
        for v, d in sorted(edges.get(u, {}).items()):
            if v in blocked:
                continue
            nd = dist[u] + 1.0 + d / 10.0
            if v not in dist or nd < dist[v] - 1e-12:
                dist[v] = nd
                prev[v] = u
                todo.append(v)
    if dst not in dist:
        return None, None, None
    path = [dst]
    while path[-1] != src:
        path.append(prev[path[-1]])
    path.reverse()
    return dist[dst], len(path) - 1, path


class Economy:
    """actors: list of dicts {id, system, pop, income, assets, regime, core_maker (0..1)};
    prof: id -> {caps}; E: endowment; graph: (nodes, edges, hop)."""

    def __init__(self, actors, prof, E, graph, access=None):
        """access: optional object with level(seller_id, buyer_id, good) -> 0..4 and end_use(good) -> bool."""
        self.access = access
        self.cols = actors
        self.prof = prof
        self.E = E
        self.nodes, self.edges, _ = graph
        self.mult = {c["id"]: {g: 1.0 for g in GOODS} for c in actors}
        self.demand_mult = {c["id"]: {g: 1.0 for g in GOODS} for c in actors}
        self.blocked = set()
        self.risk = {c["id"]: 0.0 for c in actors}
        self._rt = {}
        self._byid = {c["id"]: c for c in actors}
        for c in actors:
            for a in c["assets"]:
                g = ASSET_GOOD.get(a["type"])
                if g:
                    self.mult[c["id"]][g] += 0.6 * a["importance"]
            self.mult[c["id"]]["drive_cores"] *= c.get("core_maker", 0.0)
        self.mult0 = json.loads(json.dumps(self.mult))
        self.calibrate()
        self.stock = {c["id"]: {g: self.consumption(c)[g] * 1.5 for g in GOODS} for c in actors}

    # --- sizes
    def L(self, c):
        return c["pop"] / 1e5 * (c["income"] / 2.0)                 # output scales with automation-driven income

    def consumption(self, c):
        L = c["pop"] / 1e5 * (0.4 + 0.6 * c["income"] / 2.0)
        out = {g: PER_CAP[g] * L * self.demand_mult[c["id"]][g] for g in GOODS}
        mil = self.prof[c["id"]]["caps"]["military"] / 100
        out["munitions"] *= (0.5 + 2.0 * mil)
        out["hulls"] *= (0.7 + 0.8 * mil)
        out["drive_cores"] *= (0.7 + 0.8 * mil)
        return out

    def eff(self, c, base=False):
        caps = self.prof[c["id"]]["caps"]
        e = self.E[c["system"]]["idx"]
        m = self.mult0[c["id"]] if base else self.mult[c["id"]]
        return {g: (caps[SECTOR_CAP[g]] / 60.0) ** 1.5 * (ENDOW_GOOD[g](e) if g in ENDOW_GOOD else 1.0) * m[g] for g in GOODS}

    def transport(self, a_sys, b_sys):
        if a_sys == b_sys:
            return 0.0, 0, [a_sys]
        if a_sys in self.blocked or b_sys in self.blocked:
            return None, None, None
        key = (a_sys, b_sys, frozenset(self.blocked))
        if key not in self._rt:
            self._rt[key] = route(self.edges, a_sys, b_sys, frozenset(self.blocked))
        return self._rt[key]

    def freight(self, g, cost):
        return MASS[g] * FREIGHT * cost + HANDLING * BASE_PRICE[g]

    def unit_cost(self, cid, g, effs=None):
        ef = (effs or {}).get(cid) or self.eff(self._byid[cid], base=True)
        if ef[g] <= 1e-6:
            return None
        return BASE_PRICE[g] * self.mean_eff[g] / ef[g]

    def calibrate(self):
        effs = {c["id"]: self.eff(c, base=True) for c in self.cols}
        self.mean_eff = {g: max(1e-9, sum(effs[c][g] for c in effs) / len(effs)) for g in GOODS}
        tot_cons = {g: sum(self.consumption(c)[g] for c in self.cols) for g in GOODS}
        vt = sum(tot_cons[g] * BASE_PRICE[g] for g in GOODS)
        self.dshare = {g: tot_cons[g] * BASE_PRICE[g] / vt for g in GOODS}
        # local-source rule: a bulk good is produced at home when the home source, however poor,
        # is cheaper than the cheapest landed import; otherwise the colony specialises away from it
        self.self_supply = {}
        self.local_vs_import = {}
        for c in self.cols:
            cid = c["id"]
            for g in GOODS:
                loc = self.unit_cost(cid, g, effs)
                best = None
                for s in self.cols:
                    if s["id"] == cid:
                        continue
                    cost, hops, _ = self.transport(s["system"], c["system"])
                    sc = self.unit_cost(s["id"], g, effs)
                    mk = self.markup(s["id"], cid, g)
                    if cost is None or sc is None or mk is None:
                        continue
                    landed = sc * (1 + mk) + self.freight(g, cost)
                    if best is None or landed < best[0]:
                        best = (landed, s["id"], cost)
                self.local_vs_import[(cid, g)] = (loc, best)
                self.self_supply[(cid, g)] = loc is not None and (best is None or loc <= best[0])
        self.alloc = {}
        for c in self.cols:
            cid = c["id"]
            ef = effs[cid]
            w = {g: (ef[g] / self.mean_eff[g]) ** 3 * self.dshare[g] for g in GOODS}
            tw = sum(w.values()) or 1.0
            self.alloc[cid] = {g: w[g] / tw for g in GOODS}
        self.k = {g: 1.0 for g in GOODS}
        self.ca = {cid: dict(w) for cid, w in self.alloc.items()}   # comparative-advantage weights
        for it in range(10):
            prod = {g: sum(self.raw(c)[g] for c in self.cols) for g in GOODS}
            need = dict(tot_cons)
            for g, ins in INPUTS.items():
                for inp, q in ins.items():
                    need[inp] += prod[g] * q
            self.k = {g: self.k[g] * (1.06 * need[g] / prod[g]) ** 0.7 if prod[g] > 0 else self.k[g] for g in GOODS}
            # self-supply of bulk goods (and He-3 where a giant exists) when home is cheaper than the
            # landed import: the labour share needed to cover home demand is reserved first
            for c in self.cols:
                cid = c["id"]
                nd = self.need(c)
                L = self.L(c)
                ef = effs[cid]
                fixed = {}
                for g in BULK + ("he3",):
                    if self.self_supply[(cid, g)] and ef[g] > 0:
                        per = L * ef[g] * self.k[g] / BASE_PRICE[g] * 4.0
                        fixed[g] = min(0.5, nd[g] * 1.08 / max(per, 1e-12))
                tf = sum(fixed.values())
                if tf > 0.8:
                    fixed = {g: v * 0.8 / tf for g, v in fixed.items()}
                    tf = 0.8
                rest = {g: self.ca[cid][g] for g in GOODS}
                tr = sum(rest.values()) or 1.0
                self.alloc[cid] = {g: max(fixed.get(g, 0.0), (1 - tf) * rest[g] / tr) for g in GOODS}
                t = sum(self.alloc[cid].values())
                self.alloc[cid] = {g: v / t for g, v in self.alloc[cid].items()}

    def markup(self, seller, buyer, g):
        """Political markup on a flow seller -> buyer of good g; None = no legal or grey channel."""
        if self.access is None:
            return 0.0
        return self.access.markup(seller, buyer, g)

    def raw(self, c):
        L = self.L(c)
        ef = self.eff(c)
        return {g: L * self.alloc[c["id"]][g] * ef[g] * self.k[g] / BASE_PRICE[g] * 4.0 for g in GOODS}

    def need(self, c):
        nc = getattr(self, "_need", None)
        if nc is not None and c["id"] in nc:
            return nc[c["id"]]
        out = dict(self.consumption(c))
        r = self.raw(c)
        for g, ins in INPUTS.items():
            for inp, q in ins.items():
                out[inp] += r[g] * q
        return out

    def price(self, cid, g):
        c = self._byid[cid]
        target = self.need(c)[g] * 1.15 + 1e-6
        src = getattr(self, "pre", None) or self.stock
        s = src[cid][g]
        return BASE_PRICE[g] * clamp((target / max(s, target * 0.05)) ** 0.7, 0.35, 8.0) * (1 + self.risk[cid])

    def step(self, relations):
        flows = []
        made = {}
        self._need = None
        self._need = {c["id"]: self.need(c) for c in self.cols}
        for c in self.cols:
            cid = c["id"]
            cap = self.raw(c)
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
        for g in GOODS:
            need = []
            for c in self.cols:
                gap = self.need(c)[g] * 1.3 - self.stock[c["id"]][g]
                if gap > 0:
                    need.append((-self.price(c["id"], g), c["id"], gap))
            for _, buyer, gap in sorted(need):
                bsys = self._byid[buyer]["system"]
                offers = []
                for s in self.cols:
                    sid = s["id"]
                    if sid == buyer:
                        continue
                    surplus = self.stock[sid][g] - self.need(s)[g] * 1.1
                    if surplus <= 0:
                        continue
                    cost, hops, _ = self.transport(s["system"], bsys)
                    if cost is None:
                        continue
                    griev = relations.get(tuple(sorted((buyer, sid))), 0)
                    if griev >= 85:
                        continue
                    mk = self.markup(sid, buyer, g)
                    if mk is None:
                        continue                  # closed by political access (licence, sanctions, own policy)
                    if self.access is not None and self.access.end_use(g) and self.unit_cost(sid, g) is None:
                        continue                  # end-use controlled: only the producer exports
                    delivered = self.price(sid, g) * (1 + griev / 200) * (1 + mk) + self.freight(g, cost)
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
        self.pre = {cid: dict(v) for cid, v in self.stock.items()}
        for c in self.cols:
            cons = self.consumption(c)
            for g in GOODS:
                self.stock[c["id"]][g] = max(0.0, self.stock[c["id"]][g] - cons[g]) * 0.97
        return made, flows

    def run(self, years, relations):
        return [self.step(relations) for _ in range(years)]

    def snapshot(self, hist, years=5):
        made = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        imp = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        exp = {c["id"]: {g: 0.0 for g in GOODS} for c in self.cols}
        partners = {c["id"]: {} for c in self.cols}
        pair = {}
        for m, flows in hist[-years:]:
            for cid, p in m.items():
                for g in GOODS:
                    made[cid][g] += p[g] / years
            for g, s, b, q, _ in flows:
                imp[b][g] += q / years
                exp[s][g] += q / years
                partners[b][s] = partners[b].get(s, 0.0) + q * BASE_PRICE[g] / years
                partners[s][b] = partners[s].get(b, 0.0) + q * BASE_PRICE[g] / years
                pair[(s, b, g)] = pair.get((s, b, g), 0.0) + q / years
        prices = {c["id"]: {g: round(self.price(c["id"], g), 2) for g in GOODS} for c in self.cols}
        return dict(made=made, imp=imp, exp=exp, prices=prices, partners=partners, pair=pair)

    # --- corridors
    def corridors(self, snap, operators, min_value=0.0):
        out = []
        for (s, b, g), q in sorted(snap["pair"].items()):
            val = q * BASE_PRICE[g]
            if val <= min_value:
                continue
            S, B = self._byid[s], self._byid[b]
            cost, hops, path = self.transport(S["system"], B["system"])
            loc, best = self.local_vs_import.get((b, g), (None, None))
            sup = self.unit_cost(s, g)
            fr = self.freight(g, cost or 0.0)
            if hops == 0:
                reason = "same_system"
            elif g in BULK or g == "he3":
                if loc is None:
                    reason = "no_local_source"
                elif sup is not None and loc > sup + fr:
                    reason = "local_source_dearer"
                else:
                    reason = "residual_shortfall"      # home source is cheaper; model residual, not a real corridor
            elif g == "drive_cores":
                reason = "state_monopoly_or_own_design"
            elif g in ("hulls", "munitions"):
                reason = "industrial_specialisation_military"
            else:
                reason = "specialisation_high_value"
            # break-even freight: the FREIGHT rate above which the buyer would rather use its own source
            be = None
            if loc is not None and sup is not None and cost:
                be = round(max(0.0, (loc - sup - HANDLING * BASE_PRICE[g]) / (MASS[g] * cost)), 2)
            alts = []
            for o in self.cols:
                if o["id"] in (s, b):
                    continue
                oc = self.unit_cost(o["id"], g)
                c2, h2, _ = self.transport(o["system"], B["system"])
                if oc is None or c2 is None or sup is None:
                    continue
                if oc + self.freight(g, c2) <= (sup + fr) * 1.2:
                    alts.append(o["id"])
            mids = [n for n in (path or [])[1:-1]]
            ctrl = sorted({operators.get(n, "?") for n in mids})
            out.append(dict(seller=s, buyer=b, good=g, value=round(val, 3), mass=round(q * MASS[g], 3), hops=hops,
                            route=path, choke_points=mids, controllers=ctrl, reason=reason, alternatives=sorted(alts)[:4],
                            break_even_freight=be, local_cost=None if loc is None else round(loc, 2),
                            landed_cost=None if sup is None else round(sup + fr, 2)))
        return out


def volume_class(v, top):
    if top <= 0:
        return "none"
    r = v / top
    return "major" if r > 0.25 else "medium" if r > 0.05 else "minor"
