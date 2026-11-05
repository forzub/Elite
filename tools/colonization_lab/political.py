"""Political order of 3126 (offline, deterministic, stdlib only).

Single reader of canon/political_order.json. Separates four things that used to be one
"bloc" label in canon/industry.json:
  1. political / ideological bloc (or non-alignment, province, special zone);
  2. economic integration (bloc.integration, membership full / associate);
  3. technology and trade access per category (0 open .. 4 closed, directional);
  4. sanctions overlay derived from world.json relations_anchor.

Other generators import this module: industry.py (supply channels), economy_live.py
(hub blocs + runtime headline matrix), ship_designer.py (supplier channels),
world_economy.py (yard partners), lore_*.py (reference pages).

    python political.py            # write out/world_3126/political_order_3126.json and POLITICAL_ORDER_3126.md, print the listing
    python political.py --list     # print the programmatic listing only
    python political.py --check    # recompute, compare with out/, run the checks
"""
import argparse
import json
import os
import sys

import lab

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out", "world_3126")
SOL = "sol_authority"
CHANNEL = ("open", "licensed", "restricted", "black", "closed")


class Order:
    def __init__(self, P=None, W=None):
        self.P = P or lab.load("political_order.json")
        self.W = W or lab.load("world.json")
        P = self.P
        self.cats = P["categories"]["order"]
        self.actors = P["actors"]
        self.blocs = P["blocs"]
        self.units = [c["id"] for c in self.W["colonies"]] + [SOL]
        self.unit_actor = {}
        self.unit_kind = {}
        for a, v in sorted(self.actors.items()):
            for u in v["units"]:
                self.unit_actor[u] = a
                self.unit_kind[u] = "seat" if u == v["seat"] else P["units"].get(u, {}).get("kind", "core")
        for u, v in P["units"].items():
            self.unit_kind[u] = v["kind"]
            if v["kind"] == "special_zone":
                self.unit_actor[u] = None
        self.exceptions = P["exceptions"]
        self.sanctions = P["sanctions"]
        self.jump = {j["unit"]: j for j in self.W["jump_technology"]}
        self.licences = []
        for L in P.get("licences", []):
            holders = {j["unit"] for j in self.W["jump_technology"] if j["how"] in L["holders_from_jump_how"]}
            holders |= set(L.get("extra_holders", {}))
            holders |= set(self.actors[L["licensor"]]["units"])
            self.licences.append(dict(L, holders=holders))
        self._cache = {}

    # ------------------------------------------------------------ membership
    def actor_of(self, unit):
        return self.unit_actor.get(unit)

    def bloc_of_actor(self, a):
        return self.actors[a]["bloc"] if a else None

    def bloc_of_unit(self, unit):
        """Political bloc of an economic unit (province inherits; zone / non-aligned -> None)."""
        return self.bloc_of_actor(self.actor_of(unit))

    def access_key(self, unit):
        """Group key for the runtime headline matrix: bloc id, else actor / zone id."""
        b = self.bloc_of_unit(unit)
        if b:
            return b
        a = self.actor_of(unit)
        return f"nonaligned:{a}" if a else f"zone:{unit}"

    def bloc_members(self, b):
        return sorted(a for a, v in self.actors.items() if v["bloc"] == b)

    def bloc_units(self, b):
        return sorted(u for u in self.units if self.bloc_of_unit(u) == b)

    def nonaligned(self):
        return sorted(a for a, v in self.actors.items() if not v["bloc"])

    def zones(self):
        return sorted(u for u, v in self.P["units"].items() if v["kind"] == "special_zone")

    def provinces(self):
        return sorted(u for u, v in self.P["units"].items() if v["kind"] == "province")

    # ------------------------------------------------------------ access
    def _profile(self, unit, side):
        if unit in self.zones():
            return self.P["zone_profiles"][unit][side]
        a = self.actor_of(unit)
        if not self.actors[a]["bloc"]:
            return self.P["nonaligned"][a][side]
        return self.blocs[self.actors[a]["bloc"]][side]

    def _in_scope(self, ex, partner_unit):
        sc = ex["scope"]
        pa = self.actor_of(partner_unit)
        if partner_unit in self.zones():
            return isinstance(sc, dict) and partner_unit in sc.get("zones", [])
        if sc == "all_actors":
            return True
        if sc.get("all_actors"):
            return pa not in sc.get("except_actors", [])
        return pa in sc.get("actors", []) or self.bloc_of_actor(pa) in sc.get("blocs", [])

    def _sanction_hits(self, s, seller, buyer):
        sa, ba = self.actor_of(seller), self.actor_of(buyer)

        def match(fr, to, fu, tu):
            return fr == s["from"] and to == s["to"] and (not s.get("units_from") or fu in s["units_from"]) and (not s.get("units_to") or tu in s["units_to"])
        if match(sa, ba, seller, buyer):
            return True
        if s.get("mutual"):
            # the same measure seen from the other side: from/to and their unit scopes swap
            return ba == s["from"] and sa == s["to"] and (not s.get("units_from") or buyer in s["units_from"]) and (not s.get("units_to") or seller in s["units_to"])
        return False

    def licence_holder(self, licence_id, unit):
        L = next(x for x in self.licences if x["id"] == licence_id)
        return unit in L["holders"]

    def access(self, seller, buyer, trace=False):
        """Directional access seller unit -> buyer unit per category (0..4)."""
        if not trace:
            k = (seller, buyer)
            if k not in self._cache:
                self._cache[k] = self._access(seller, buyer, False)
            return dict(self._cache[k])
        return self._access(seller, buyer, True)

    def _access(self, seller, buyer, trace=False):
        why = []
        if seller == buyer or (self.actor_of(seller) and self.actor_of(seller) == self.actor_of(buyer)):
            lv = {c: 0 for c in self.cats}
            why.append("same actor")
            return (lv, why) if trace else lv
        sa, ba = self.actor_of(seller), self.actor_of(buyer)
        sb, bb = self.bloc_of_actor(sa), self.bloc_of_actor(ba)
        if sb and sb == bb:
            assoc = self.actors[sa].get("membership") == "associate" or self.actors[ba].get("membership") == "associate"
            prof = self.blocs[sb]["associate_internal" if assoc and "associate_internal" in self.blocs[sb] else "internal"]
            lv = dict(prof)
            why.append(f"{sb}.{'associate_internal' if assoc else 'internal'}")
        else:
            ex_, im_ = self._profile(seller, "export"), self._profile(buyer, "import")
            lv = {c: max(ex_[c], im_[c]) for c in self.cats}
            why.append("max(export seller, import buyer)")
        for ex in self.exceptions:
            d = ex["direction"]
            for me, other, role in ((seller, buyer, "seller"), (buyer, seller, "buyer")):
                if self.actor_of(me) != ex["actor"] or (d != "both" and d != role) or not self._in_scope(ex, other):
                    continue
                for c, cap in ex["cap"].items():
                    if lv[c] > cap:
                        lv[c] = cap
                        why.append(f"{ex['id']} caps {c}={cap}")
        # licence layer: the licensor sells its licensed category only to licence holders
        for L in self.licences:
            if self.actor_of(seller) == L["licensor"] and buyer not in L["holders"] and lv[L["category"]] < L["non_holder_level"]:
                lv[L["category"]] = L["non_holder_level"]
                why.append(f"{L['id']}: buyer not a holder -> {L['category']}={L['non_holder_level']}")
        raised_closed = set()
        for s in self.sanctions:
            if not self._sanction_hits(s, seller, buyer):
                continue
            for c, l in s["level"].items():
                if l > lv[c]:
                    lv[c] = l
                    why.append(f"{s['id']} {c}={l}")
                    if l >= 4:
                        raised_closed.add(c)
        for ex in self.exceptions:
            if ex["kind"] != "transit_hub":
                continue
            hub = self.actors[ex["actor"]]["seat"]
            if hub in (seller, buyer):
                continue
            for c in ex.get("transit_categories", []):
                if c in raised_closed and lv[c] >= 4 and self.access(seller, hub)[c] <= 1 and self.access(hub, buyer)[c] <= 1:
                    lv[c] = 3
                    why.append(f"{ex['id']} transit {c}=3")
        return (lv, why) if trace else lv

    def channel(self, seller, buyer, category):
        return CHANNEL[self.access(seller, buyer)[category]]

    def headline(self, a, b):
        """One symmetric channel per pair for the runtime board (headline_channel_rule)."""
        x, y = self.access(a, b), self.access(b, a)
        civ = max(x["civilian_goods"], y["civilian_goods"])
        ind = max(x["industrial"], y["industrial"])
        dual = max(x["dual_use"], y["dual_use"])
        if civ >= 4:
            return "closed"
        if civ == 3:
            return "black"
        if ind >= 2:
            return "restricted"
        if ind == 1 or dual >= 2:
            return "licensed"
        return "open"

    def headline_matrix(self):
        """Runtime matrix keyed by access_key; a group pair takes the seats' headline (unit-level local conflicts stay in the per-unit matrix)."""
        rep = {}
        for u in self.units:
            k = self.access_key(u)
            a = self.actor_of(u)
            seat_first = (0 if a and self.actors[a]["seat"] == u else 1, u)
            if k not in rep or seat_first < rep[k][0]:
                rep[k] = (seat_first, u)
        keys = sorted(rep)
        m = {}
        for i, ka in enumerate(keys):
            m[ka] = {}
            for kb in keys[i:]:
                m[ka][kb] = "open" if ka == kb and not ka.startswith(("nonaligned:", "zone:")) else self.headline(rep[ka][1], rep[kb][1])
        return m

    def unit_matrix(self):
        """seller -> buyer -> six digits (levels in categories order), compact for the runtime file."""
        return {a: {b: "".join(str(self.access(a, b)[c]) for c in self.cats) for b in self.units if b != a} for a in self.units}


# ---------------------------------------------------------------- listing, checks, files
def listing(O):
    L = []
    actors = sorted(O.actors)
    L.append(f"actors_3126 ({len(actors)}): " + ", ".join(actors))
    L.append(f"economic_units ({len(O.units)}): " + ", ".join(O.units))
    L.append(f"provinces ({len(O.provinces())}): " + ", ".join(f"{u} -> {O.P['units'][u]['actor']}" for u in O.provinces()))
    L.append(f"special_zones ({len(O.zones())}): " + ", ".join(f"{u} ({O.P['units'][u]['zone_type']})" for u in O.zones()))
    for b in O.blocs:
        mem = O.bloc_members(b)
        L.append(f"bloc {b} [{O.blocs[b]['kind']}, integration {O.blocs[b]['integration']}] members {len(mem)}: " +
                 ", ".join(m + (" (associate)" if O.actors[m].get("membership") == "associate" else "") for m in mem) +
                 f"; units: {', '.join(O.bloc_units(b))}")
    L.append(f"nonaligned ({len(O.nonaligned())}): " + ", ".join(f"{a} ({O.actors[a]['nonaligned']})" for a in O.nonaligned()))
    for a in actors:
        L.append(f"  actor {a}: bloc {O.actors[a]['bloc'] or 'NONALIGNED:' + O.actors[a]['nonaligned']}, units {O.actors[a]['units']}, status {O.actors[a]['status']}")
    return L


def checks(O):
    res = []

    def add(n, ok, d=""):
        res.append(dict(check=n, ok=bool(ok), detail=d))
    covered = set(O.unit_actor)
    add("every_unit_mapped_once", set(O.units) == covered, f"missing {sorted(set(O.units) - covered)}, extra {sorted(covered - set(O.units))}")
    seen = [u for v in O.actors.values() for u in v["units"]]
    add("unit_in_at_most_one_actor", len(seen) == len(set(seen)), str(len(seen)))
    add("actor_seat_is_own_unit", all(v["seat"] in v["units"] for v in O.actors.values()))
    add("actor_bloc_known", all(v["bloc"] is None or v["bloc"] in O.blocs for v in O.actors.values()))
    add("nonaligned_have_profile_and_type", all(a in O.P["nonaligned"] and O.actors[a]["nonaligned"] in O.P["nonaligned_types"] for a in O.nonaligned()))
    add("no_autarkies_bloc", "autarkies" not in O.blocs)
    add("zones_not_actors", all(u not in O.actors and O.actor_of(u) is None for u in O.zones()))
    add("provinces_inherit_bloc", all(O.bloc_of_unit(u) == O.bloc_of_actor(O.P["units"][u]["actor"]) and u in O.actors[O.P["units"][u]["actor"]]["units"] for u in O.provinces()))
    full_sets = all(set(p) == set(O.cats) for b in O.blocs.values() for k in ("internal", "export", "import") for p in [b[k]]) and \
        all(set(O.P["nonaligned"][a][k]) == set(O.cats) for a in O.nonaligned() for k in ("export", "import"))
    add("profiles_cover_all_categories", full_sets)
    ok_ex = all(e["actor"] in O.actors and set(e["cap"]) <= set(O.cats) for e in O.exceptions)
    add("exceptions_reference_actors", ok_ex)
    rel = {tuple(sorted((r["a"], r["b"]))) for r in O.W["relations_anchor"]}
    bad = []
    for s in O.sanctions:
        fu = s.get("units_from") or O.actors[s["from"]]["units"]
        tu = s.get("units_to") or O.actors[s["to"]]["units"]
        if not any(tuple(sorted((x, y))) in rel for x in fu for y in tu):
            bad.append(s["id"])
    add("sanctions_derived_from_relations_anchor", not bad, ",".join(bad) or f"{len(O.sanctions)} measures")
    # behaviour checks the order is meant to have
    a = O.access
    add("citadel_strategic_closed_outside_pact", all(a("wolf359_citadel", u)["strategic"] == 4 for u in O.units if O.bloc_of_unit(u) != "citadel_compact"))
    add("tauranga_strategic_for_southern_circle", all(a("lalande_mecca", u)["strategic"] <= 1 for u in O.bloc_units("southern_circle")))
    add("cognium_cores_reach_sol_league_pact", all(a("tau_ceti_cognium", u)["strategic"] <= 2 for u in ("sol_authority", "lalande_hellas", "gj581_europa", "eps_eri_harmony")))
    add("kern_finance_crosses_blocs", all(a("cyg61_kern", u)["people_finance_data"] <= 1 for u in O.units if u not in O.zones() and u not in ("delta_pav_karma", "hd114783_hellas3", "cyg61_kern")),
        "Kern finance <= licensed with everyone except Karma (isolation), Third Hellas (claims freeze), zones")
    add("agora_more_open_than_league_peer", sum(a("lalande_hellas", "wolf359_citadel").values()) < sum(a("gj581_europa", "wolf359_citadel").values()))
    add("firmament_local_conflict_only", a("hd40307_maland", "hd40307_firmament")["military"] == 4 and a("hd40307_maland", "eps_eri_harmony")["military"] < 4)
    add("hellas3_less_integrated_than_core", sum(a("lalande_hellas", "hd114783_hellas3").values()) > sum(a("lalande_hellas", "gj581_europa").values()))
    add("southern_circle_less_integrated_than_pact", sum(O.blocs["southern_circle"]["internal"].values()) > sum(O.blocs["heavenly_pact"]["internal"].values()))
    add("zone_closed_to_ordinary_trade", all(a(u, z)["civilian_goods"] >= 3 and a(z, u)["civilian_goods"] == 4 for z in O.zones() for u in O.units if u != z))
    add("nonaligned_not_mutual_allies", all(O.access_key(x) != O.access_key(y) for x in O.nonaligned() for y in O.nonaligned() if x != y))
    # jump cores: who may legally obtain a strategic product from whom (licence + own technology + sanctions)
    strat_ok = {(s, b) for s in O.units for b in O.units if s != b and a(s, b)["strategic"] <= 2}
    bad = []
    for s, b in sorted(strat_ok):
        jt = O.jump.get(s)
        if O.actor_of(s) == "tau_ceti_cognium" and O.jump.get(b, {}).get("how") in ("own", "stolen", None) and b not in O.licences[0]["extra_holders"]:
            bad.append(f"{s}->{b}")
    add("cognium_cores_only_to_licence_holders", not bad, ",".join(bad[:8]) or "ok")
    refusers = [u for u in O.units if O.jump.get(u, {}).get("how") == "stolen" or u == "delta_pav_karma"]
    add("no_cognium_cores_for_refusers_and_copiers", all(a("tau_ceti_cognium", u)["strategic"] >= 4 for u in refusers if u != "tau_ceti_cognium"), ",".join(refusers))
    own = [u for u in O.units if O.jump.get(u, {}).get("how") in ("own", "state_secret")]
    add("own_core_makers_not_licence_buyers", all(a("tau_ceti_cognium", u)["strategic"] >= 4 for u in own if u != "tau_ceti_cognium"), ",".join(own))
    cat = O.P["categories"]
    mil_goods = [g for g, c in cat["goods_override"].items() if c == "military"] + [g for g, c in cat["goods_group"].items() if c == "military" and g != "weapons"]
    add("controlled_goods_not_military", not mil_goods, ",".join(mil_goods) or "only the weapons group is military; narcotics = civilian_goods + legality layer")
    add("transit_never_strategic", all("strategic" not in e.get("transit_categories", []) and "military" not in e.get("transit_categories", []) for e in O.exceptions))
    add("finance_bridge_no_strategic_cap", all(c not in e["cap"] for e in O.exceptions if e["kind"] in ("finance_bridge", "transit_hub", "multivector_neutral", "fuel_supplier") for c in ("strategic", "military")))
    add("kern_cannot_sell_strategic_to_citadel", a("cyg61_kern", "wolf359_citadel")["strategic"] >= 3 and a("cyg61_kern", "wolf359_citadel")["military"] >= 2)
    add("providence_does_not_lift_sanctions", all(a("gj876_next", u)["strategic"] >= 3 for u in O.units if u != "gj876_next"))
    add("citadel_civilian_open_strategic_closed", all(a(u, "wolf359_citadel")["civilian_goods"] <= 1 and a("wolf359_citadel", u)["strategic"] == 4 for u in O.units
                                                      if O.bloc_of_unit(u) != "citadel_compact" and u not in O.zones() and u != "delta_pav_karma"))
    no_role = [f"bloc:{b}" for b, v in O.blocs.items() if not v.get("player_role")] + [f"nonaligned:{x}" for x in O.nonaligned() if not O.P["nonaligned"][x].get("player_role")] +         [f"unit:{u}" for u, v in O.P["units"].items() if not v.get("player_role")] + [f"exception:{e['id']}" for e in O.exceptions if not e.get("player_role")] +         [f"licence:{L['id']}" for L in O.licences if not L.get("player_role")] + ([] if O.P.get("sanctions_player_role") else ["sanctions"])
    add("order_accepted_canon", O.P["status"] == "CANON" and "PROPOSAL_PENDING_AUTHOR_REVIEW" not in json.dumps({k: v for k, v in O.P.items() if k != "status_legend"}, ensure_ascii=False),
        "top-level CANON; no structural element pending review")
    add("no_open_design_questions", not O.P["open_questions"] and {u["id"] for u in O.P.get("author_unknowns", [])} == {"leo83_crisis_outcome", "sol_own_jump_cores"})
    mecca = O.jump.get("lalande_mecca", {})
    add("tauranga_own_math_canon", mecca.get("how") == "own" and mecca.get("status") == "CANON" and "PROPOSAL" not in O.P["licences"][0]["basis"].split("Тауранга")[1][:80])
    hs = next(s for s in O.sanctions if s["id"] == "harmony_strife_embargo")
    add("harmony_strife_dual_use_grey_not_closed", hs["level"]["dual_use"] == 3 and O.access("eps_eri_harmony", "eps_eri_strife")["dual_use"] == 3)
    add("every_element_has_player_role", not no_role, ",".join(no_role) or "Chekhov's gun: all blocs, non-aligned, units, exceptions, licences, sanctions")
    leaky = []
    for sc in O.sanctions:
        why = sc.get("full_closure_reason", {})
        for c, l in sc["level"].items():
            if c in ("civilian_goods", "industrial", "dual_use") and l >= 4 and c not in why:
                leaky.append(f"{sc['id']}:{c}")
            if c == "civilian_goods" and l >= 3 and c not in why:
                leaky.append(f"{sc['id']}:{c}")
    add("sanctions_realistically_leaky", not leaky and all(sc.get("evasion") for sc in O.sanctions), ",".join(leaky) or "civilian / industrial / dual-use never fully closed without a stated reason; every measure has an evasion route")
    add("no_pool_wide_embargo_on_karma", [sc["from"] for sc in O.sanctions if "delta_pav_karma" in (sc["from"], sc["to"])] == ["delta_pav_karma"],
        "only Free Karma's own measure; pool coordinates strategic / dual-use / IP, civilian trade is national")
    ia = O.P.get("interim_arrangements", [])
    add("thebaid_kern_interim_open_no_sanctions", ia and ia[0]["outcome"].startswith("UNKNOWN") and ia[0].get("player_hooks")
        and not [sc for sc in O.sanctions if "leo83_zion" in (sc["from"], sc["to"])])
    acc = O.licences[0].get("accession", {})
    add("newcomers_not_licensed_yet", all(u not in O.licences[0]["holders"] for u in acc) and acc.get("hd114783_hellas3", {}).get("status") != acc.get("leo83_zion", {}).get("status"))
    add("free_belts_military_only_grey", all(a(u, b)["military"] >= 3 for u in O.bloc_units("free_belts") for b in O.units if O.bloc_of_unit(b) != "free_belts"))
    return res


def compute(O=None):
    O = O or Order()
    rows = {}
    for s in O.units:
        for b in O.units:
            if s != b:
                lv, why = O.access(s, b, trace=True)
                rows[f"{s}|{b}"] = dict(levels=[lv[c] for c in O.cats], why=why)
    doc = dict(schema="political_order_3126/1", generated_by="tools/colonization_lab/political.py", do_not_edit=True, status=O.P["status"],
               categories=O.cats, scale={k: v["id"] for k, v in O.P["access_scale"].items()},
               actors={a: dict(bloc=v["bloc"], membership=v.get("membership"), nonaligned=v.get("nonaligned"), units=v["units"], status=v["status"]) for a, v in sorted(O.actors.items())},
               units={u: dict(actor=O.actor_of(u), kind=O.unit_kind.get(u, "core"), bloc=O.bloc_of_unit(u), access_key=O.access_key(u)) for u in O.units},
               blocs={b: dict(kind=v["kind"], members=O.bloc_members(b), units=O.bloc_units(b), integration=v["integration"]) for b, v in O.blocs.items()},
               nonaligned=O.nonaligned(), provinces=O.provinces(), special_zones=O.zones(),
               headline_matrix=O.headline_matrix(), access=rows)
    return O, doc


def md(O, doc, chk):
    LV = {int(k): v["ru"] for k, v in O.P["access_scale"].items()}
    L = [f"# Политический порядок 3126 ({O.P['status']})", "", "Сгенерировано `tools/colonization_lab/political.py` из `canon/political_order.json`.", "", "```text"] + listing(O) + ["```", "",
         "## Доступ между столицами блоков и неприсоединившимися (продавец → покупатель)", "",
         "Шкала: " + "; ".join(f"{k} {v}" for k, v in LV.items()) + ". Порядок категорий: " + ", ".join(O.cats) + ".", ""]
    seats = [next(a for a, v in O.actors.items() if v["bloc"] == b) for b in O.blocs] + O.nonaligned() + O.zones()
    L += ["| продавец \\ покупатель | " + " | ".join(seats) + " |", "|---|" + "---|" * len(seats)]
    for s in seats:
        L.append(f"| {s} | " + " | ".join("—" if s == b else "".join(str(x) for x in doc["access"][f"{s}|{b}"]["levels"]) for b in seats) + " |")
    L += ["", "## Проверки", ""] + [f"- {'OK' if c['ok'] else 'FAIL'} {c['check']}: {c['detail']}" for c in chk]
    return "\n".join(L) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--list", action="store_true")
    a = ap.parse_args()
    O, doc = compute()
    chk = checks(O)
    if a.list:
        print("\n".join(listing(O)))
        return 0
    files = {"political_order_3126.json": json.dumps(dict(doc, checks=chk), ensure_ascii=False, indent=1, sort_keys=True) + "\n",
             "POLITICAL_ORDER_3126.md": md(O, doc, chk)}
    bad = [c for c in chk if not c["ok"]]
    if a.check:
        ok = not bad
        for n, t in files.items():
            p = os.path.join(OUT, n)
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != t:
                print(f"STALE: {p}")
                ok = False
        for c in bad:
            print(f"FAIL {c['check']}: {c['detail']}")
        print("political check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    os.makedirs(OUT, exist_ok=True)
    for n, t in files.items():
        with open(os.path.join(OUT, n), "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
    print("\n".join(listing(O)))
    for c in chk:
        print(("OK  " if c["ok"] else "FAIL"), c["check"], "|", c["detail"])
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
