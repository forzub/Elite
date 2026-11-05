"""Industrial geography and rescue services of 3126 (offline, deterministic, stdlib only).

From canon/industry.json (industrial schools, designs, manufacturers, block groups,
rescue norms) + canon/political_order.json via political.py (blocs, access per category,
sanctions) + world.json (colonies, jump-core lineage, grievances) + the generated
economy, defence and sites (out/world_3126) it derives:

  - per polity and per major block (ship, mining, gas, station): concern /
    assembly / parts / import, and who supplies it through which channel
    (access seller -> buyer in the block's category: open, licensed, restricted,
    black market / sanctions, closed);
  - per site: what is produced, assembled and sold there;
  - fleet uniformity of each bloc (how many hull makers its fleets come from);
  - bills of major blocks of typical products with the makers by bloc;
  - rescue services: SAR, search, fire / damage control, medevac, towing,
    hazmat, disaster response, their organisation by legal family, counts and
    response times to every site.

    python industry.py            # write out/world_3126/industry_3126.json, rescue_3126.json, INDUSTRY_3126.md
    python industry.py --check    # recompute, compare, run the checks
"""
import argparse
import json
import math
import os
import sys

import lab
import logistics as lg
import political

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "out", "world_3126")
SOL = "sol_authority"
TAG = "PROPOSAL"
LEVELS = ("import", "parts", "assembly", "concern")


def jload(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def clamp(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


def r3(x):
    return round(x, 3)


class Inputs:
    def __init__(self):
        self.IN = lab.load("industry.json")
        self.W = lab.load("world.json")
        self.WS = lab.load("world_sites.json")
        self.SE = lg.load_ship_economics()
        self.X = {n: jload(os.path.join(OUT, f"{n}_3126.json")) for n in ("economy", "infrastructure", "defence", "fleet_census", "sites", "law", "comms")}
        self.eco = self.X["economy"]["polities"]
        self.col = {c["id"]: c for c in self.W["colonies"]}
        self.rel = {tuple(sorted((r["a"], r["b"]))): r["grievance"] for r in self.W["relations_anchor"]}
        self.PO = political.Order(W=self.W)
        self.school = self.IN["industrial_schools"]["units"]
        self.bloc_of = {u: self.PO.bloc_of_unit(u) for u in self.PO.units}
        self.cat_of_block = self.PO.P["categories"]["industry_block"]
        self.jt = {j["unit"]: j for j in self.W["jump_technology"]}

    def system(self, k):
        return 0 if k == SOL else self.eco[k]["system"]


# ---------------------------------------------------------------- blocs and channels
def channels(I):
    """Channel per buyer|seller unit pair and access category (political.py: seller -> buyer)."""
    out = {}
    for k in sorted(I.eco):
        for q in sorted(I.eco):
            lv = I.PO.access(q, k)
            out[f"{k}|{q}"] = {c: political.CHANNEL[lv[c]] for c in I.PO.cats}
    return out


def refuses(I, k):
    return bool(I.col.get(k, {}).get("demography", {}).get("refuses_network"))


# ---------------------------------------------------------------- capability
def capability(I):
    IN = I.IN
    inf = I.X["infrastructure"]["polities"]
    makers = {}
    for m in IN["manufacturers"]:
        for g in m["groups"]:
            makers.setdefault((m["polity"], g), []).append(m["id"])
    out = {}
    for k, e in sorted(I.eco.items()):
        gdp_t = e["gdp_cr_y"] / 1e12
        a = inf[k]["assets"]
        yards = (3 * a["civil_shipyard"]["count"] + a["small_yard"]["count"] / 10.0 + a["military_shipyard"]["count"]) / 4.0
        bias = I.col.get(k, {}).get("bias", {})
        if k == SOL:
            bias = {"shipbuilding": 25, "manufacturing": 25, "metallurgy": 20, "electronics": 25, "bio": 15, "extraction": 15, "research": 20}
        design = I.school[k]["design"]
        rows = {}
        for grp in IN["block_groups"].values():
            for blk in grp:
                drv = IN["block_driver"][blk]
                if blk == "jump_core":
                    j = I.jt.get(k)
                    how = j["how"] if j else None
                    if how in ("state_secret", "own"):
                        lvl, why = "concern", f"ядра: {how}"
                    elif how == "licensed" and j["level"] >= 0.6:
                        lvl, why = "assembly", "сборка из запечатанных блоков по лицензии"
                    elif how == "licensed":
                        lvl, why = "import", "ядра по лицензии (готовые)"
                    elif how == "stolen":
                        lvl, why = "parts", "пиратские копии и ремонт (качество pirated_copy)"
                    else:
                        lvl, why = "import", "своих ядер нет" + (" (свои ядра Земли — UNKNOWN у автора)" if k == SOL else "")
                else:
                    score = 0.4 * clamp(gdp_t / 15.0) + 0.3 * clamp(yards) + 0.3 * clamp(bias.get(drv, 0) / 25.0)
                    if blk in ("hull_structure", "pressure_segments", "shielding", "cryo_tanks") and design == "hollowed_asteroid_and_refit":
                        score += 0.25
                    if blk == "weapons" and design == "heavy_military":
                        score += 0.3
                    lvl = "concern" if score >= 0.6 else "assembly" if score >= 0.38 else "parts" if score >= 0.18 else "import"
                    why = f"оценка {score:.2f} (выпуск {gdp_t:.1f} трлн, верфи {yards:.2f}, уклон {drv} {bias.get(drv, 0)})"
                if blk == "jump_core" and refuses(I, k):
                    lvl, why = "import", "отказ от сети: ядер нет и не покупают"
                if (k, blk) in makers:
                    lvl = "concern"
                    why = "концерн: " + ", ".join(makers[(k, blk)])
                if k == SOL and blk != "jump_core" and lvl != "concern":
                    lvl = "concern"
                    why = "Сол: исполинская промышленность (65 % выпуска человечества)"
                rows[blk] = dict(level=lvl, why=why, makers=makers.get((k, blk), []))
        builds = all(LEVELS.index(rows[b]["level"]) >= 2 for b in ("hull_structure", "reactor", "main_drive")) and rows["jump_core"]["level"] != "parts"
        builds_partly = sum(1 for b in ("hull_structure", "reactor", "main_drive", "avionics") if LEVELS.index(rows[b]["level"]) >= 1)
        role = "строит корабли целиком" if builds else "делает узлы и запчасти" if builds_partly >= 2 else "покупает корабли"
        if refuses(I, k):
            role = "строит только внутрисистемный флот (отказ от сети)" if builds_partly >= 2 else "живёт старым флотом (отказ от сети)"
        out[k] = dict(bloc=I.bloc_of[k], actor=I.PO.actor_of(k), design=design, blocks=rows, ship_role=role)
    return out


def plant_site(I, k, kind):
    sys_ = I.system(k)
    ids = {s["id"] for s in I.X["sites"]["sites"] if s.get("polity") == k}
    pref = {"industrial_hub": "industrial_hub01", "mining_hub": "mining_hub01", "military_hub": "military_hub01", "orbital": "port01", "trade": "trade_hub01"}[kind]
    for cand in (f"site.{sys_:03d}.{k}.{pref}", f"site.{sys_:03d}.{k}.industrial_hub01", f"site.{sys_:03d}.{k}.trade_hub01", f"site.{sys_:03d}.{k}.city01"):
        if cand in ids:
            return cand
    if k == SOL:      # Sol: the named hub network (sol_network.py)
        return {"industrial_hub": "earth_hub.wenchang", "mining_hub": "site.000.sol.ceres", "military_hub": "site.000.sol.military_luna"}.get(kind, "earth_orbital_hub")
    return sorted(ids)[0] if ids else None


def supply(I, cap, ch):
    """Who supplies each polity each block and through which channel."""
    out = {}
    for k in sorted(cap):
        rows = {}
        for blk, c in cap[k]["blocks"].items():
            if c["level"] in ("concern", "assembly"):
                rows[blk] = dict(source=k, channel="own", level=c["level"])
                continue
            offers = []
            for q in sorted(cap):
                if q == k or cap[q]["blocks"][blk]["level"] != "concern":
                    continue
                chn = ch[f"{k}|{q}"][I.cat_of_block[blk]]
                if chn == "closed":
                    continue                      # e.g. Citadel cores outside its pact: strategic access 4
                rank = political.CHANNEL.index(chn)
                offers.append((rank, -I.eco[q]["gdp_cr_y"], q, chn))
            if blk == "jump_core":
                src = I.school[k]["core_source"]
                # preferred source: its own maker or the bloc assembler of sealed reference-fab blocks
                if src and src != k and cap[src]["blocks"]["jump_core"]["level"] in ("concern", "assembly"):
                    chn = ch[f"{k}|{src}"]["strategic"]
                    if chn != "closed":
                        offers.append((-1, 0, src, chn))
            offers.sort()
            if refuses(I, k):
                rows[blk] = dict(source=None, channel="none", level=c["level"], note="отказ от сети: межзвёздных закупок нет")
            elif offers:
                _, _, q, chn = offers[0]
                rows[blk] = dict(source=q, channel=chn, category=I.cat_of_block[blk], level=c["level"], via="чёрный рынок / реэкспорт" if chn == "black" else None)
                if (I.bloc_of[k] is None or I.bloc_of[k] != I.bloc_of[q]) and blk != "jump_core":
                    # outside its own bloc a buyer spreads purchases over the three best suppliers
                    top = [o for o in offers if o[0] == offers[0][0]][:3]
                    w = [0.5, 0.3, 0.2][:len(top)]
                    rows[blk]["sources"] = [dict(source=o[2], channel=o[3], share=r3(x / sum(w))) for o, x in zip(top, w)]
            else:
                rows[blk] = dict(source=None, channel="none", level=c["level"])
        out[k] = rows
    return out


def fleet_uniformity(I, cap, sup):
    out = {}
    fc = I.X["fleet_census"]["polities"]
    for b in sorted(I.PO.blocs):
        makers = {}
        for k in I.PO.bloc_units(b):
            if k not in sup:
                continue
            ships = sum(r["active"] for r in fc.get(k, {}).values())
            h = sup[k]["hull_structure"]
            for part in (h.get("sources") or [dict(source=h["source"], share=1.0)]):
                src = part["source"]
                names = [m["name"] for m in I.IN["manufacturers"] if m["polity"] == src and "hull_structure" in m["groups"]] if src else []
                key = names[0] if names else (f"верфи {I.eco[src]['name']}" if src else "старый флот / нет")
                makers[key] = makers.get(key, 0) + ships * part["share"]
        tot = sum(makers.values()) or 1
        top = max(makers.values()) / tot if makers else 0
        seat = next(a for a, v in I.PO.actors.items() if v["bloc"] == b)
        designs = sorted({I.school[u]["design"] for u in I.PO.bloc_units(b)})
        out[b] = dict(hull_makers=len(makers), shares={m: r3(s / tot) for m, s in sorted(makers.items(), key=lambda kv: -kv[1])}, main_maker_share=r3(top),
                      designs=designs, variety=I.IN["designs"][I.school[seat]["design"]]["variety"], design_ru=", ".join(I.IN["designs"][d]["ru"] for d in designs))
    return out


def by_site(I, cap, sup):
    """Per site: produced (concerns), assembled (assembly lines), sold (market channels)."""
    rows = {}
    for m in I.IN["manufacturers"]:
        site = plant_site(I, m["polity"], m["plant"])
        if not site:
            continue
        r = rows.setdefault(site, dict(produced=[], assembled=[], sold=[]))
        r["produced"].append(dict(maker=m["name"], blocks=m["groups"]))
    for k, c in sorted(cap.items()):
        asm = [b for b, x in c["blocks"].items() if x["level"] == "assembly"]
        if asm:
            site = plant_site(I, k, "industrial_hub")
            if site:
                rows.setdefault(site, dict(produced=[], assembled=[], sold=[]))["assembled"].append(dict(polity=k, blocks=asm))
        tsite = plant_site(I, k, "trade")
        if tsite:
            sources = sorted({(x["source"], x["channel"]) for x in sup[k].values() if x["source"] and x["source"] != k})
            rows.setdefault(tsite, dict(produced=[], assembled=[], sold=[]))["sold"].append(
                dict(polity=k, imports_from=[dict(source=s, channel=chn) for s, chn in sources], own_exports=[b for b, x in c["blocks"].items() if x["level"] == "concern"]))
    return rows


BOM = {  # major blocks of typical products, cost shares
    "cobrett_i_n1": None, "medium_freighter": None, "jump_train": None, "ocs4_lastochka": None, "line_combatant": None,
    "station_ring_section": {"rotating_rings": 0.35, "pressure_segments": 0.2, "habitat_cabins": 0.15, "agro_life_modules": 0.12, "radiator_arrays": 0.08, "shielding": 0.1},
    "hex_dock": {"dock_modules": 0.55, "pressure_segments": 0.15, "reactor_modules": 0.12, "radiator_arrays": 0.08, "avionics": 0.1},
    "asteroid_mine_base": {"drill_heads": 0.3, "ore_processors": 0.25, "mass_drivers": 0.15, "reactor_modules": 0.12, "habitat_cabins": 0.1, "shielding": 0.08},
    "gas_platform": {"scoop_platforms": 0.45, "he3_separators": 0.3, "cryo_tanks": 0.15, "reactor_modules": 0.1},
}
COMP_TO_BLOCK = {"hull_structure": "hull_structure", "main_drive": "main_drive", "reactor": "reactor", "jump_drive": "jump_core", "electronics": "avionics",
                 "life_support": "life_support", "rcs_thermal": "thermal", "weapons": "weapons"}


def boms(I, cap):
    out = {}
    makers_by_bloc = {}
    for m in I.IN["manufacturers"]:
        for g in m["groups"]:
            makers_by_bloc.setdefault((I.PO.access_key(m["polity"]), g), []).append(m["name"])
    for prod, shares in BOM.items():
        if shares is None:
            comp = I.SE["ship_classes"][prod]["components_cr"]
            tot = sum(comp.values())
            shares = {COMP_TO_BLOCK[c]: v / tot for c, v in comp.items() if v > 0}
        rows = []
        for blk, sh in sorted(shares.items(), key=lambda kv: -kv[1]):
            keys = sorted({kk for kk, _ in makers_by_bloc})
            rows.append(dict(block=blk, share=r3(sh), makers={b: makers_by_bloc.get((b, blk), []) for b in keys if makers_by_bloc.get((b, blk))}))
        out[prod] = rows
    return out


# ---------------------------------------------------------------- rescue services
def rescue(I, cap):
    R = I.IN["rescue"]
    D = I.X["defence"]["polities"]
    S = I.X["sites"]["sites"]
    law = {r["site"]: r for r in I.X["law"]["sites"]}
    out_p = {}
    for k, e in sorted(I.eco.items()):
        st = e["settlement_structure"]
        surface = sum(v for a, v in st.items() if "поверхность" in a or "подзем" in a or "Земля" in a)
        stations = e["population"] - surface
        fam = I.WS["regime_law"].get(I.col.get(k, {}).get("regime_3126", "sol_authority" if k == SOL else "none_quiet"), "none") if k != SOL else "sol_codified"
        sar_cutters = sum(t["ships"] for t in D[k]["tasks"] if t["task"] == "sar_customs") * 0.5
        arrivals_day = D[k]["arrivals_per_year"] / 365.0
        beacons = 1 if e.get("system") is not None else 0
        out_p[k] = dict(organisation=R["organisation_by_law"].get(fam, "—"), law_family=fam,
                        fire_damage_personnel=round(stations * R["fire_crew_per_1000_on_stations"] / 1000.0 + surface * R["fire_crew_per_1000_surface"] / 1000.0),
                        sar_cutters=r3(sar_cutters), rescue_tugs=r3(max(1.0, arrivals_day * R["rescue_tug_per_100_arrivals_day"] / 100.0 * 10.0)),
                        search_ships=r3(R["search_ships_per_beacon_system"] * beacons + (2 if k in ("cyg61_kern", "tau_ceti_cognium") else 0)),
                        hospital_ships=r3(e["population"] / 1e8 * R["hospital_ships_per_100m"]),
                        hazmat_teams=R["hazmat_teams_per_quarantine_hub"] * sum(1 for s in S if s.get("polity") == k and s["type"] == "quarantine_hub"),
                        private_salvage_share=R["salvage_share_for_private"].get(fam, 0.0), status=TAG)
    # response time to every site: cutter at 1.5 g from the polity's main hub; deep space: jump + charge
    a = 1.5 * 9.80665
    au = 1.495978707e11
    rows = []
    for s in S:
        k = s.get("polity")
        if s["system"] is None:
            resp_h = 6.0 + 24.0 * (2.0 if s["id"] in ("dso.lotus", "dso.garden") else 1.0)
            who = "Kern SAR «Ласка» с «Дальнего Причала»" if s["id"] in ("dso.lotus", "dso.far_berth", "dso.garden", "dso.quarantine_picket") else "ближайший оператор (прыжок + зарядка)"
        else:
            patrol_bases = D.get(k, {}).get("bases", {}).get("patrol", 0) if k else 0
            if s["type"] in ("orbital_hub", "planetary_surface_port", "trade_hub", "quarantine_hub", "military_hub", "moon_hub", "industrial_hub", "science_hub"):
                d_au = 0.002          # same body / same orbital neighbourhood as the polity's rescue base
            elif patrol_bases >= 10:
                d_au = 0.05           # belts and mining hubs with a local patrol base
            else:
                d_au = 1.5            # no local base: help comes across the system
            resp_h = 2.0 * math.sqrt(d_au * au / a) / 3600.0 + 0.5
            who = out_p.get(k, {}).get("organisation", "никто; частники за плату") if k else "никто; частники за плату"
            if s["type"] == "pirate_enclave":
                who, resp_h = "никто (сами)", None
        rows.append(dict(site=s["id"], responder=who, response_h=r3(resp_h) if resp_h is not None else None,
                         fire_crew_on_site=round(s["population"] * (I.IN["rescue"]["fire_crew_per_1000_on_stations"] / 1000.0)) if s["type"] != "planetary_surface_port" else round(s["population"] / 1000.0),
                         law=law.get(s["id"], {}).get("applicable_law")))
    return dict(services=R["services"], polities=out_p, sites=rows, status=TAG,
                note="SAR-катера — половина задачи «SAR и таможня» обороны; пожарные и аварийные команды — по населению станций и поверхности; поиск — межзвёздный, у маячных систем")


# ---------------------------------------------------------------- checks / report
def checks(I, out):
    res = []

    def add(n, ok, d):
        res.append(dict(check=n, ok=bool(ok), detail=d))
    missing = sorted(k for k in I.eco if k not in I.school or k not in I.PO.unit_actor)
    add("every_unit_has_school_and_political_status", not missing, f"missing {missing}")
    add("schools_are_not_blocs", "blocs" not in I.IN, "industry.json holds no political blocs (political_order.json)")
    cores = {k for k, c in out["capability"].items() if c["blocks"]["jump_core"]["level"] == "concern"}
    jt_own = {u for u, j in I.jt.items() if j["how"] in ("state_secret", "own")}
    add("core_makers_match_jump_lineage", cores == jt_own, f"{sorted(cores)} == {sorted(jt_own)}")
    nosrc = [f"{k}:{b}" for k, rows in out["supply"].items() for b, r in rows.items() if r["source"] is None and I.school[k]["design"] != "none" and not refuses(I, k)]
    add("every_block_has_a_source", not nosrc, ",".join(nosrc[:8]) or "ok")
    exp_cit = [k for k, rows in out["supply"].items() if rows["jump_core"]["source"] == "wolf359_citadel" and k != "wolf359_citadel" and out["capability"][k]["bloc"] != "citadel_compact"]
    add("citadel_cores_not_exported", not exp_cit, ",".join(exp_cit) or "ok")
    pact = out["fleet_uniformity"]["heavenly_pact"]
    league = out["fleet_uniformity"]["lalande_league"]
    add("allied_fleets_uniform", pact["main_maker_share"] >= 0.6 and league["main_maker_share"] < pact["main_maker_share"],
        f"Pact main hull maker {pact['main_maker_share']} (uniform), League {league['main_maker_share']} (many vendors)")
    sites = {s["id"] for s in I.X["sites"]["sites"]}
    bad = [s for s in out["by_site"] if s not in sites]
    add("production_sites_exist", not bad, ",".join(bad[:6]) or f"{len(out['by_site'])} sites")
    add("rescue_covers_every_site", len(out["rescue"]["sites"]) == len(sites), f"{len(out['rescue']['sites'])} sites")
    return res


def md(I, out, chk):
    L = ["# Промышленность и спасательные службы 3126 (PROPOSAL)", "", "Сгенерировано `tools/colonization_lab/industry.py`. Блоки — `canon/political_order.json` (political.py); школы — `industry.json industrial_schools`.", "", "## Блоки (политика) и школы (промышленность)", "",
         "| Блок | Единицы | Конструкторские школы | Ядра | Производителей корпусов | Доля главного |", "|---|---|---|---|---:|---:|"]
    for b, v in I.PO.blocs.items():
        u = out["fleet_uniformity"][b]
        cores = sorted({I.school[x]["core_source"] or "—" for x in I.PO.bloc_units(b)})
        L.append(f"| {v['ru']} | {', '.join(I.PO.bloc_units(b))} | {u['design_ru']} | {', '.join(cores)} | {u['hull_makers']} | {u['main_maker_share']} |")
    outside = [u for u in I.PO.units if not I.bloc_of[u]]
    L.append(f"| вне блоков | {', '.join(outside)} | {', '.join(sorted({I.IN['designs'][I.school[u]['design']]['ru'] for u in outside}))} | — | — | — |")
    L += ["", "## Кто строит корабли", "", "| Полития | Блок | Роль | Корпус | Ядро | Реактор | Двигатель | Авионика |", "|---|---|---|---|---|---|---|---|"]
    for k, c in out["capability"].items():
        b = c["blocks"]
        L.append(f"| {k} | {c['bloc'] or 'вне блоков'} | {c['ship_role']} | {b['hull_structure']['level']} | {b['jump_core']['level']} | {b['reactor']['level']} | {b['main_drive']['level']} | {b['avionics']['level']} |")
    L += ["", "## Проверки", ""] + [f"- {'OK' if x['ok'] else 'FAIL'} {x['check']}: {x['detail']}" for x in chk]
    return "\n".join(L) + "\n"


def compute():
    I = Inputs()
    ch = channels(I)
    cap = capability(I)
    sup = supply(I, cap, ch)
    out = dict(blocs={b: dict(ru=v["ru"], kind=v["kind"], members=I.PO.bloc_members(b), units=I.PO.bloc_units(b)) for b, v in I.PO.blocs.items()},
               schools=I.school, designs=I.IN["designs"], channels=ch, capability=cap, supply=sup, fleet_uniformity=fleet_uniformity(I, cap, sup),
               by_site=by_site(I, cap, sup), boms=boms(I, cap), manufacturers=I.IN["manufacturers"], status=TAG)
    out["rescue"] = rescue(I, cap)
    return I, out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    I, out = compute()
    chk = checks(I, out)
    resc = out.pop("rescue")
    files = {"industry_3126.json": json.dumps(dict(out, checks=chk), ensure_ascii=False, indent=1, sort_keys=True) + "\n",
             "rescue_3126.json": json.dumps(resc, ensure_ascii=False, indent=1, sort_keys=True) + "\n",
             "INDUSTRY_3126.md": md(I, out, chk)}
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
        print("industry check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    for n, t in files.items():
        with open(os.path.join(OUT, n), "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
    for c in chk:
        print(("OK  " if c["ok"] else "FAIL"), c["check"], "|", c["detail"])
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
