"""Fuel stations 3126: where a ship can refill its jump charge, legally or not (offline, deterministic, stdlib only).

    python fuel_stations.py            # write src/assets/data/fuel/fuel_stations.json + docs/FUEL_STATIONS.md
    python fuel_stations.py --check    # recompute, compare with the files, run the consistency checks

Inputs (all generated / canon data, nothing invented here):
  canon/fuel_stations.json                         - pump rates, stock and price rules, illegal-spot rules (PROPOSAL)
  src/assets/data/economy/hub_modules_3126.json    - every hub with a market: type, refuel docks, system
  src/assets/data/economy/market_state_3126.json   - jump_charge / reactor_fuel price and stock per hub
  src/assets/data/galaxy_details/world_sites/sites_3126.json - sites (refuelling stations) and hidden_sites (pirates,
                                                     smugglers) of the placement pass (outposts.py)
  out/world_3126/outposts_3126.json, frontier_3126.json - jump-point depot demand, rejected smugglers, piracy index,
                                                     hazard zones
  src/assets/data/galaxy/jump_approach_shells.json - corridor mouths
  src/assets/data/galaxy_details/systems/*/generated  - belts (sumps) and small bodies
  src/assets/data/initial_world_state.json         - runtime-only demo hubs of Sol

Output: one record per place keyed by the RUNTIME hub id (= the world-site id; Sol's hubs keep their own ids), with
kind (hub / refuel_point / small / grey / illegal), legality, `illegal` flag, `world_object` (does the runtime spawn
it - the hidden and proposed places are data only), stock, pump rate, coupling times, price hint / factor, risk.
"""
import argparse
import glob
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
DATA = os.path.join(ROOT, "src", "assets", "data")
OUT_JSON = os.path.join(DATA, "fuel", "fuel_stations.json")
OUT_MD = os.path.join(ROOT, "docs", "FUEL_STATIONS.md")
SCHEMA = "elite.fuel_stations.v1"


def jload(*parts):
    with open(os.path.join(*parts), encoding="utf-8") as f:
        return json.load(f)


def h01(s):
    return int(hashlib.sha1(s.encode("utf-8")).hexdigest()[:8], 16) / float(0xFFFFFFFF)


def r2(x):
    return None if x is None else round(float(x), 2)


class Inputs:
    def __init__(self):
        self.C = jload(HERE, "canon", "fuel_stations.json")
        self.hm = jload(DATA, "economy", "hub_modules_3126.json")["hubs"]
        m = jload(DATA, "economy", "market_state_3126.json")
        self.gi = m["goods_order"].index("jump_charge")
        self.ri = m["goods_order"].index("reactor_fuel")
        self.market = {h["hub"]: h for h in m["hubs"]}
        ws = jload(DATA, "galaxy_details", "world_sites", "sites_3126.json")
        self.sites = {s["id"]: s for s in ws["sites"]}
        self.hidden = ws["hidden_sites"]
        self.outposts = jload(HERE, "out", "world_3126", "outposts_3126.json")
        self.frontier = jload(HERE, "out", "world_3126", "frontier_3126.json")
        self.pir = {p["system"]: p for p in self.frontier["piracy_by_system"]}
        self.shells = jload(DATA, "galaxy", "jump_approach_shells.json")["shells"]
        self.iws = jload(DATA, "initial_world_state.json")
        self.sysname = {}
        self.reservoirs = {}
        self.bodies = {}
        for d in sorted(glob.glob(os.path.join(DATA, "galaxy_details", "systems", "*"))):
            sj = os.path.join(d, "system.json")
            if not os.path.exists(sj):
                continue
            s = jload(sj)
            sid = s["id"]
            self.sysname[sid] = s["name"]
            for f in sorted(glob.glob(os.path.join(d, "generated", "reservoirs", "*.json"))):
                r = jload(f)
                self.reservoirs.setdefault(sid, []).append(r)
            for f in sorted(glob.glob(os.path.join(d, "generated", "bodies", "*.json"))):
                b = jload(f)
                self.bodies[b["id"]] = b
        for h in self.hm:
            if h.get("system_id") is not None and h["system_id"] not in self.sysname:
                self.sysname[h["system_id"]] = h.get("system_name")

    def name_of(self, sid):
        if sid is None:
            return "глубокий космос"
        return self.sysname.get(sid, f"система {sid}")


def pump(I, kind):
    p = I.C["pumps"][kind]
    return dict(rate_t_per_min=p["rate_t_per_min"], hookup_s=p["hookup_s"], disconnect_s=p["disconnect_s"])


def market_fuel(I, hub_id):
    mk = I.market.get(hub_id)
    if not mk:
        return None, None, None, None
    return mk["price_cr"][I.gi], mk["stock_t"][I.gi], mk["price_cr"][I.ri], mk["stock_t"][I.ri]


def system_price(I, sid):
    """median market jump_charge price in a system (a remote place's supply source)"""
    xs = sorted(I.market[h["id"]]["price_cr"][I.gi] for h in I.hm if h.get("system_id") == sid and h["id"] in I.market
                and I.market[h["id"]]["price_cr"][I.gi] is not None)
    if not xs:
        xs = sorted(m["price_cr"][I.gi] for m in I.market.values() if m["price_cr"][I.gi] is not None)
    return xs[len(xs) // 2]


def risk_of(I, sid, crooks_scale=1.0):
    """risk to the BUYER at an illegal seller: law (patrols find untaxed restricted charge) and crooks (the seller's
    friends); 0..1 each, combined as independent chances"""
    p = I.pir.get(sid)
    if not p:
        law, crooks = 0.2, 0.5 * crooks_scale
    else:
        law = 0.5 * (1.0 - p["weak_enforcement"]) * (1.0 - 0.5 * p["corruption"])
        crooks = min(1.0, p["index"] * crooks_scale)
    return dict(risk_law=r2(law), risk_crooks=r2(crooks), risk=r2(1.0 - (1.0 - law) * (1.0 - crooks)))


# ---------------------------------------------------------------- legal and grey stations
def hub_layer(I):
    rows = []
    for h in I.hm:
        hid = h["id"]
        if h.get("market") in (None, "none") or hid not in I.market:
            continue
        price, stock, rprice, rstock = market_fuel(I, hid)
        if price is None or not stock:
            continue
        typ = h.get("type")
        site = I.sites.get(hid, {})
        docks = (h.get("totals") or {}).get("refuel_docks") or 0
        black = h.get("market") == "black"
        illegal = black and ("незакон" in (h.get("name") or "").lower() or h.get("hidden"))
        grey = not illegal and (black or typ == "free_port" or site.get("legal") in ("semi_legal",))
        if illegal:
            kind, legality = "illegal", "illegal"
        elif grey:
            kind, legality = "grey", "grey"
        elif typ in ("beacon_bridge",) or hid.startswith("dso.") or docks <= 1 and typ not in ("deep_space_refuel_point",):
            kind, legality = "small", "legal"
        elif typ == "deep_space_refuel_point":
            kind, legality = "refuel_point", "legal"
        else:
            kind, legality = "hub", "legal"
        pk = {"hub": "hub", "refuel_point": "refuel_point", "small": "small", "grey": "hub", "illegal": "illegal"}[kind]
        factor = {"grey": I.C["price"]["grey_factor"], "illegal": I.C["price"]["smuggler_factor"]}.get(kind, 1.0)
        rec = dict(id=hid, name=h.get("name") or site.get("name"), system_id=h.get("system_id"), system=I.name_of(h.get("system_id")),
                   kind=kind, site_type=typ, legality=legality, illegal=legality == "illegal", world_object=None,
                   known_to_player="hidden" if h.get("hidden") else "public",
                   stock_t=dict(jump_charge=r2(stock), reactor_fuel=r2(rstock)), refuel_docks=docks,
                   price_hint_cr_per_t=dict(jump_charge=price, reactor_fuel=rprice), price_factor=factor, **pump(I, pk),
                   source="market_state_3126 + hub_modules_3126")
        if legality == "illegal":
            rec.update(risk_of(I, h.get("system_id")))
        if grey:
            rec["note"] = "без таможни / вне лицензии: заряд прыжка «restricted» во всех режимах, здесь продают без проверки лицензии"
        rows.append(rec)
    return rows


def refuel_point_layer(I, known):
    """the placement pass's refuelling stations (not in the economy files yet: economy_live.py not rerun)"""
    rows = []
    jp = {r["id"]: r for r in I.outposts["refuel"]}
    stock_days = jload(HERE, "canon", "outposts.json")["refuel"]["jump_point"]["stock_days"]
    for sid_, s in sorted(I.sites.items()):
        if s.get("type") != "deep_space_refuel_point" or sid_ in known:
            continue
        o = jp.get(sid_, {})
        if o.get("role") == "jump_point":
            stock = o["charge_t_day"] * stock_days
            why = f"транзит {round(o.get('transit_t_y', 0) / 1e6, 2)} млн т/год, {o['charge_t_day']} т заряда в сутки, запас {stock_days} сут"
        else:
            stock = I.C["stock"]["shell_staging_t"]
            why = "стоянка у устья коридора: запас танкерных поставок"
        rows.append(dict(id=sid_, name=s["name"], system_id=s["system_id"], system=I.name_of(s["system_id"]), kind="refuel_point",
                         site_type=s["type"], legality="legal", illegal=False, world_object=None, known_to_player=s.get("known_to_player", "public"),
                         stock_t=dict(jump_charge=r2(stock), reactor_fuel=None), refuel_docks=s.get("docks"),
                         price_hint_cr_per_t=dict(jump_charge=round(system_price(I, s["system_id"]) * I.C["price"]["remote_markup"]), reactor_fuel=None),
                         price_factor=1.0, **pump(I, "refuel_point"), source="sites_3126 (outposts.py refuel layer)", note=why))
    return rows


# ---------------------------------------------------------------- illegal: the placement pass's hidden sites
def hidden_layer(I):
    rows = []
    P, S = I.C["price"], I.C["stock"]
    for h in I.hidden:
        sid = h.get("system_id")
        kind = h.get("outpost_kind")
        if kind == "pirate_settlement":
            stock = (h.get("ships") or 0) * S["pirate_t_per_ship"]
            factor, crooks = P["pirate_factor"], 1.0
            why = "пиратское логово: заряд снят с захваченных кораблей и скуплен через скупщиков; продают своим и тем, кого не боятся"
        else:
            stock = (h.get("population") or 0) * S["smuggler_t_per_person"]
            factor, crooks = P["smuggler_factor"], 0.5
            why = "тайная стоянка контрабандистов: заряд уведён из лицензионной торговли («restricted» везде), продают без лицензии и без записи"
            if "втридорога" in (h.get("placement_reason") or ""):
                factor = P["vulture_factor"]
                why = "устье коридора без законного хозяина: «заряд втридорога» (канон placement pass)"
        rows.append(dict(id=h["id"], name=h["name"], system_id=sid, system=I.name_of(sid), kind="illegal", site_type=kind,
                         legality="illegal", illegal=True, world_object=False, known_to_player=h.get("known_to_player", "hidden"),
                         where=h.get("parent_body_id"), placement=h.get("placement"),
                         stock_t=dict(jump_charge=r2(stock), reactor_fuel=None), refuel_docks=h.get("docks"),
                         price_hint_cr_per_t=dict(jump_charge=round(system_price(I, sid)), reactor_fuel=None),
                         price_factor=factor, **pump(I, "illegal"), **risk_of(I, sid, crooks),
                         source="sites_3126.hidden_sites (outposts.py)", note=why, reason=h.get("placement_reason")))
    return rows


# ---------------------------------------------------------------- illegal: proposed places (data only)
def proposed(I, rec_id, name, sid, site_type, where, placement, factor, crooks, why, reason):
    return dict(id=rec_id, name=name, system_id=sid, system=I.name_of(sid), kind="illegal", site_type=site_type, legality="illegal",
                illegal=True, world_object=False, proposed=True, known_to_player="rumour", where=where, placement=placement,
                stock_t=dict(jump_charge=float(I.C["stock"]["proposed_tanker_t"]), reactor_fuel=None), refuel_docks=1,
                price_hint_cr_per_t=dict(jump_charge=round(system_price(I, sid)) if sid is not None else round(system_price(I, -1)), reactor_fuel=None),
                price_factor=factor, **pump(I, "illegal"), **risk_of(I, sid, crooks), source="fuel_stations.py rule " + site_type,
                note=why, reason=reason)


def vulture_layer(I, taken_systems):
    R = I.C["illegal_rules"]["sump_vulture"]
    rows = []
    for sid, p in sorted(I.pir.items()):
        if sid in taken_systems or p["index"] < R["min_piracy_index"] or p["weak_enforcement"] < R["min_weak_enforcement"] or p["traffic"] < R["min_traffic"]:
            continue
        res = [r for r in I.reservoirs.get(sid, []) if r.get("kind") in R["reservoir_kinds"]]
        if not res:
            continue
        r = max(res, key=lambda x: (x.get("total_mass_earth") or 0.0, x["id"]))
        rid = f"ill.{sid:03d}.sump"
        rad = r.get("peak_au") or 0.5 * ((r.get("inner_au") or 0.0) + (r.get("outer_au") or 0.0))
        place = dict(belt=dict(radius_au=round(rad, 3), longitude_deg=round(360.0 * h01(rid), 2)))
        reason = (f"индекс пиратства {p['index']} (слабый надзор {p['weak_enforcement']}, трафик {p['traffic']}, коррупция {p['corruption']}); "
                  f"пояс {r['id'].split('.')[-1]} ({r['kind']}, {round(r.get('total_mass_earth') or 0.0, 4)} M⊕) - отстойник прыжков, своего тайного объекта в системе нет")
        rows.append(proposed(I, rid, f"Чёрный танкер у отстойника {I.name_of(sid)}", sid, "sump_vulture", r["id"], place,
                             I.C["price"]["vulture_factor"], 1.0, R["why"], reason))
    return rows


def sargasso_layer(I):
    R = I.C["illegal_rules"]["sargasso_edge"]
    rows = []
    for z in I.frontier["hazard_zones"]:
        if z.get("kind") not in R["zone_kinds"]:
            continue
        anchor = z.get("anchor") or ""
        sid = int(anchor.split("_")[1].split(".")[0]) if anchor.startswith("system_") else None
        rid = "ill." + ("%03d" % sid if sid is not None else "dso") + "." + z["id"].split(".")[-1]
        place = dict(zone=z["id"], anchor=anchor, edge_km=z.get("radius_km"), longitude_deg=round(360.0 * h01(rid), 2))
        reason = f"зона «{z['name']}» ({z['kind']}, радиус {z.get('radius_km')} км у {anchor}): там копятся застрявшие корабли и обломки"
        rows.append(proposed(I, rid, f"Чёрный танкер на краю «{z['name']}»", sid, "sargasso_edge", anchor, place,
                             I.C["price"]["vulture_factor"], 1.0, R["why"], reason))
    return rows


def shell_layer(I):
    R = I.C["illegal_rules"]["shell_mouth"]
    occupied = set()
    for s in list(I.sites.values()) + I.hidden:
        c = ((s.get("placement") or {}).get("corridor") or {}).get("name")
        if c:
            occupied.add(c)
    rows = []
    for sh in I.shells:
        sid = sh["systemId"]
        free = [c for c in sh.get("corridors", []) if c["name"] not in occupied]
        if not free:
            continue
        c = free[0]
        lawful = any(((s.get("placement") or {}).get("corridor") or {}).get("name", "").split("-")[0] == c["name"].split("-")[0]
                     for s in I.sites.values())
        rid = f"ill.{sid:03d}.mouth"
        # only the corridor's name: its axis, width, drift and the shell's depth are tuned in jump_approach_shells.json
        # (the runtime reads them there) - copying them here would only go stale
        place = dict(corridor=dict(name=c["name"], shell=f"jump_approach_shells.json systemId {sid}"), outside_shell="1.5 x outerAu")
        reason = (f"оболочка {sh['name']}: устье «{c['name']}» без поста; "
                  + ("рядом лицензионный кордон на другом коридоре - заправка для тех, кто его обходит" if lawful
                     else "у оболочки нет законного хозяина"))
        rows.append(proposed(I, rid, f"Чёрный танкер у устья «{c['name']}»", sid, "shell_mouth", f"shell.{sid}.{c['name']}", place,
                             I.C["price"]["vulture_factor"], 0.7, R["why"], reason))
    return rows


def covert_layer(I, taken_systems):
    R = I.C["illegal_rules"]["covert_moon"]
    rows = []
    legal_bodies = {s.get("parent_body_id") for s in I.sites.values()} | {h.get("parent_body_id") for h in I.hidden}
    for rej in I.outposts.get("smugglers_rejected", []):
        sid = rej["system"]
        if rej.get("why") != "лимит" or sid in taken_systems:
            continue
        cands = []
        for bid, b in I.bodies.items():
            if not bid.startswith(f"system_{sid}.") or b.get("kind") not in ("moon", "dwarf_planet") or bid in legal_bodies:
                continue
            a = (b.get("orbit") or {}).get("semi_major_axis_au") or 0.0
            parent = I.bodies.get(b.get("parent_id"))
            if b.get("kind") == "moon" and parent:
                a += (parent.get("orbit") or {}).get("semi_major_axis_au") or 0.0
            cands.append((-a, bid))
        if not cands:
            continue
        cands.sort()
        body = cands[0][1]
        rid = f"ill.{sid:03d}.covert"
        b = I.bodies[body]
        alt = round(max(200.0, 0.5 * ((b.get("physical") or {}).get("radius_km") or 400.0)))
        place = dict(orbit=dict(altitude_km=alt, inclination_deg=round(60.0 + 50.0 * h01(rid), 1)))
        reason = f"контрабанда {rej['name']} отсеяна только лимитом (score {rej['score']}); дальнее малое тело {body.split('.')[-1]}"
        rows.append(proposed(I, rid, f"Тайный танкер {rej['name']}", sid, "covert_moon", body, place,
                             I.C["price"]["smuggler_factor"], 0.5, R["why"], reason))
    return rows


def demo_layer(I, known):
    rows = []
    D = I.C["runtime_demo_hubs"]
    hubs = {h["id"]: h for h in I.iws.get("orbital_hubs", [])}
    for hid, stock in sorted(D["hubs"].items()):
        if hid in known or hid not in hubs:
            continue
        rows.append(dict(id=hid, name=hubs[hid].get("name"), system_id=hubs[hid].get("system_id", 0), system=I.name_of(0), kind="hub",
                         site_type="runtime_demo", legality="legal", illegal=False, world_object=True, known_to_player="public",
                         stock_t=dict(jump_charge=float(stock), reactor_fuel=None), refuel_docks=1,
                         price_hint_cr_per_t=dict(jump_charge=system_price(I, 0), reactor_fuel=None), price_factor=1.0, **pump(I, "hub"),
                         source="initial_world_state (runtime demo hub)", note=D["note"]))
    return rows


def compute():
    I = Inputs()
    runtime_ids = {h["id"] for h in I.iws.get("orbital_hubs", [])}
    hubs = hub_layer(I)
    known = {r["id"] for r in hubs}
    pts = refuel_point_layer(I, known)
    known |= {r["id"] for r in pts}
    demo = demo_layer(I, known)
    hidden = hidden_layer(I)
    illegal_systems = {r["system_id"] for r in hidden} | {r["system_id"] for r in hubs if r["illegal"]}
    vult = vulture_layer(I, illegal_systems)
    covert = covert_layer(I, illegal_systems | {r["system_id"] for r in vult})
    sarg = sargasso_layer(I)
    mouth = shell_layer(I)
    stations = hubs + pts + demo + hidden + vult + covert + sarg + mouth
    for r in stations:
        if r.get("world_object") is None:
            # a site the server spawns: the Sol hubs and the authored colony hubs (initial_world_state) and, in the
            # other inhabited systems, one generated station per body (GameServer::materializeWorldSiteHubs) - which
            # site wins a body is decided at runtime, so the data says "site" and the runtime checks the hub exists
            r["world_object"] = "runtime" if r["id"] in runtime_ids else ("site" if r["id"] in I.sites else False)
    stations.sort(key=lambda r: (r["system_id"] if r["system_id"] is not None else 9999, {"hub": 0, "refuel_point": 1, "small": 2, "grey": 3, "illegal": 4}[r["kind"]], r["id"]))
    summary = dict(total=len(stations), legal=sum(1 for r in stations if r["legality"] == "legal"),
                   grey=sum(1 for r in stations if r["legality"] == "grey"), illegal=sum(1 for r in stations if r["illegal"]),
                   illegal_existing=sum(1 for r in stations if r["illegal"] and not r.get("proposed")),
                   illegal_proposed=sum(1 for r in stations if r.get("proposed")),
                   refuel_points=sum(1 for r in stations if r["kind"] == "refuel_point"),
                   systems=len({r["system_id"] for r in stations if r["system_id"] is not None}))
    out = dict(schema=SCHEMA, status="GENERATED by tools/colonization_lab/fuel_stations.py - PROPOSAL numbers (canon/fuel_stations.json)",
               do_not_edit="regenerate: python tools/colonization_lab/fuel_stations.py", generated_by="tools/colonization_lab/fuel_stations.py",
               what_fuel_is=I.C["what_fuel_is"], pumps=I.C["pumps"], price_note=I.C["price"]["note"], summary=summary, stations=stations)
    return I, out


def checks(out):
    bad = []
    ids = [r["id"] for r in out["stations"]]
    if len(ids) != len(set(ids)):
        bad.append("duplicate ids")
    for r in out["stations"]:
        if not (r["stock_t"]["jump_charge"] and r["stock_t"]["jump_charge"] > 0):
            bad.append(f"{r['id']}: no stock")
        if r["rate_t_per_min"] <= 0:
            bad.append(f"{r['id']}: no pump")
        if r["illegal"] != (r["legality"] == "illegal"):
            bad.append(f"{r['id']}: illegal flag")
        if r["illegal"] and r.get("risk") is None:
            bad.append(f"{r['id']}: illegal without risk")
    s = out["summary"]
    for need in ("earth_orbital_hub", "site.003.wolf359_citadel.port01", "fuel.014.jump_point", "plane_demo"):
        if need not in ids:
            bad.append(f"missing {need}")
    if s["refuel_points"] < 6:
        bad.append("refuel points < 6")
    return bad


# ---------------------------------------------------------------- the document
def fmt(x, d=0):
    if x is None:
        return "—"
    return f"{x:,.{d}f}".replace(",", " ")


def markdown(I, out):
    st = out["stations"]
    s = out["summary"]
    P = out["pumps"]
    L = []
    a = L.append
    a("# Заправки 3126: где заправить заряд прыжка, законно и нет")
    a("")
    a("Статус: PROPOSAL (2026-11-19). Файл генерирует `tools/colonization_lab/fuel_stations.py` (правила — "
      "`tools/colonization_lab/canon/fuel_stations.json`); данные для игры — `src/assets/data/fuel/fuel_stations.json`. "
      "Руками не править: перезапустить скрипт (`--check` сверяет).")
    a("")
    a("## Что такое топливо корабля")
    a("")
    a("- **Заряд прыжка** (`jump_charge`, т) — единственный расходник рантайма. Изотопный заряд привода, 1.9 т/м³, класс опасности D, "
      "в любом режиме товаров «restricted» — продаётся по лицензии. Лежит в баках `std.tank.jumpcharge` (1 т; у Cobrett 3 бака = 3 т, "
      "`motion.jChargeCapacityT`). Тратится прыжком (`q·d·(α+M)`, GameSimulationJump.cpp) и полётом J (JDrive.h `jChargeRateTps`).")
    a("- **Реакторное топливо** (D/He-3) в данных станций есть (запас, цена), но рантайм его не тратит: Cobrett сжигает граммы в год.")
    a("- Газ манёвровых двигателей восполняется сам (`manoeuvreGasRechargePerSecond`) — не заправка.")
    a("")
    a("## Как заправляются (сервис)")
    a("")
    a("Сервер решает (GameSimulationRefuel.cpp). Корабль должен стоять у станции: на площадке дока (диспетчер — DOCKED) или "
      "в кадре хаба почти неподвижно (≤ 5 м/с): тогда подходит бункеровщик, как бункеровка судна на рейде, стыковка вдвое дольше — так обслуживается и остановка маршрута. Дальше: "
      "стыковка шланга → перекачка с темпом станции до полного бака (или до конца запаса станции) → отстыковка. Уход из дока / из кадра хаба "
      "прерывает заправку (залитое остаётся). Цена — через интерфейс экономики (цена топлива на станции, списание со счёта); сейчас 0.")
    a("")
    a("| Вид | Темп, т/мин | Стыковка / отстыковка, с | Почему |")
    a("|---|---:|---:|---|")
    for k, ru in (("hub", "хаб (заправочный док)"), ("refuel_point", "заправка (депо)"), ("small", "малая (маяк, склад)"), ("illegal", "нелегальная")):
        a(f"| {ru} | {P[k]['rate_t_per_min']} | {P[k]['hookup_s']} / {P[k]['disconnect_s']} | {P[k]['why']} |")
    a("")
    a("Пустой Cobrett (3 т) у хаба: 30 с + 2.5 мин + 15 с ≈ 3 мин 15 с; у нелегала: 60 с + 7.5 мин + 20 с.")
    a("")
    a("Пилот: в доке **Ctrl+U** — заправиться (ещё раз — прервать); строка «ТОПЛИВО» в терминале дока. DEV-скрипт: `refuel`, "
      "`refuel:cancel`, `waitfor:refueled`. Маршрут: остановка с действием «refuel» идёт через тот же сервис, а планировщик вставляет "
      "заправочную остановку только у хаба, где заряд продают.")
    a("")
    a("## Сводка")
    a("")
    a(f"Всего {s['total']} мест в {s['systems']} системах (+ глубокий космос): законных {s['legal']}, серых {s['grey']}, "
      f"нелегальных {s['illegal']} (уже размещённых проходом placement {s['illegal_existing']}, предложенных здесь {s['illegal_proposed']}). "
      f"Отдельных заправок (депо) — {s['refuel_points']}.")
    a("")
    a("- **Все обитаемые хабы с рынком** продают заряд: у каждого из 609 рынков `jump_charge` торгуется (market_state_3126), "
      "запас — их складской, темп — по заправочному доку (`hub_modules.refuel_dock`). Хаб с одним заправочным доком или "
      "маяк-мост / склад глубокого космоса — «малая».")
    a("- **Серые**: вольные порты (`free_port`, рынок `black`) — без таможни, заряд без проверки лицензии, ×1.3.")
    a("- **В мире сейчас** (`world_object`): `runtime` — хабы initial_world_state (Сол, колонии с городами, демо-города); `site` — "
      "место мира, его станцию сервер ставит по одной на тело (materializeWorldSiteHubs); `false` — только данные (скрытые и предложенные места, "
      "заправки у точек прыжка и у устьев пока не объекты мира).")
    a("")
    a("## По системам")
    a("")
    a("| Система | хабы | депо | малые | серые | нелег. | запас заряда, т | Заправки и нелегалы |")
    a("|---|---:|---:|---:|---:|---:|---:|---|")
    by = {}
    for r in st:
        by.setdefault(r["system_id"], []).append(r)
    for sid in sorted(by, key=lambda x: 9999 if x is None else x):
        rs = by[sid]
        c = {k: sum(1 for r in rs if r["kind"] == k) for k in ("hub", "refuel_point", "small", "grey", "illegal")}
        stock = sum(r["stock_t"]["jump_charge"] or 0 for r in rs if not r["illegal"])
        names = [r["name"] for r in rs if r["kind"] in ("refuel_point",)] + [("⚠ " + r["name"]) for r in rs if r["illegal"]]
        label = ("%d " % sid if sid is not None else "") + I.name_of(sid)
        a(f"| {label} | {c['hub'] or ''} | {c['refuel_point'] or ''} | {c['small'] or ''} | {c['grey'] or ''} | {c['illegal'] or ''} | "
          f"{fmt(stock)} | {'; '.join(names)} |")
    a("")
    a("## Заправки (депо)")
    a("")
    a("| id | Название | Система | Запас, т | Темп, т/мин | Почему здесь |")
    a("|---|---|---|---:|---:|---|")
    for r in st:
        if r["kind"] == "refuel_point":
            a(f"| `{r['id']}` | {r['name']} | {r['system']} | {fmt(r['stock_t']['jump_charge'])} | {r['rate_t_per_min']} | {r.get('note', '')} |")
    a("")
    a("## Нелегальные заправки")
    a("")
    a("Флаг `illegal: true`, `price_factor` — наценка к рыночной цене системы, `risk` = 1 − (1 − закон)(1 − бандиты): "
      "закон = 0.5·(1 − слабый надзор)·(1 − 0.5·коррупция) — патруль найдёт неоплаченный «restricted» заряд; бандиты — индекс пиратства "
      "(у пиратов ×1, у контрабандистов ×0.5). Новых 3D-моделей нет: это данные; в мир не ставятся (`world_object: false`).")
    a("")
    a("### Уже размещённые (placement pass, `hidden_sites`) и чёрные рынки")
    a("")
    a("| id | Где | Система | Запас, т | ×цена | Риск | Почему |")
    a("|---|---|---|---:|---:|---:|---|")
    for r in st:
        if r["illegal"] and not r.get("proposed"):
            a(f"| `{r['id']}` | {r['name']} | {r['system']} | {fmt(r['stock_t']['jump_charge'])} | {r['price_factor']} | {r['risk']} | {r.get('note', '')} |")
    a("")
    a("### Предложенные новые места (`proposed: true`)")
    a("")
    for rule, title in (("sump_vulture", "У отстойников (пояса)"), ("covert_moon", "Тайные танкеры у дальних лун"),
                        ("sargasso_edge", "На краю саргассов"), ("shell_mouth", "У ничьих устьев коридоров")):
        rs = [r for r in st if r.get("proposed") and r["site_type"] == rule]
        if not rs:
            continue
        a(f"**{title}** — {I.C['illegal_rules'][rule]['why']}")
        a("")
        a("| id | Система | Где | ×цена | Риск (закон / бандиты) | Обоснование |")
        a("|---|---|---|---:|---|---|")
        for r in rs:
            a(f"| `{r['id']}` | {r['system']} | `{r['where']}` | {r['price_factor']} | {r['risk']} ({r['risk_law']} / {r['risk_crooks']}) | {r['reason']} |")
        a("")
    a("**Отклонено:** межзвёздные ретрансляторы трассы (21) — корабль не останавливается посреди прыжка; ловят его рои между системами "
      "в случайной точке вдоль линии, танкеру там некого ждать. Если маршрут пересекает ядро роя, место станет кандидатом (правило на потом).")
    a("")
    a("## Что дальше")
    a("")
    a("1. Цена и списание — сервис агента ЭКОНОМИКИ `game::economy::service` (src/game/economy/EconomyService.h: fuelPriceCrPerTonne, chargeAccount, noteFuelDelivered); пока топливо бесплатно — цена 0.")
    a("2. Запас станции сейчас считают двое: насос сервиса (m_fuelStockT, из этих данных) и рынок экономики (noteFuelDelivered). Свести к одному — спрашивать запас у экономики.")
    a("3. Запас станций в рантайме убывает, но не пополняется (танкеры — потом); после перезапуска сервера — снова из данных.")
    a("4. Нелегальные места — объектами мира вместе с `hidden_sites` (знание игрока: слухи, купленные координаты); риск — "
      "в событие (досмотр патрулём, засада).")
    a("5. `economy_live.py` не перезапущен: депо placement pass нет в рынках; после перезапуска они придут из рынка.")
    a("")
    return "\n".join(L) + "\n"


def dump(obj):
    return json.dumps(obj, ensure_ascii=False, indent=1) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    I, out = compute()
    text, md = dump(out), markdown(I, out)
    bad = checks(out)
    if args.check:
        same_j = os.path.exists(OUT_JSON) and open(OUT_JSON, encoding="utf-8").read() == text
        same_m = os.path.exists(OUT_MD) and open(OUT_MD, encoding="utf-8").read() == md
        print("files_up_to_date", "OK" if same_j and same_m else "FAIL")
        print("consistency", "OK" if not bad else "FAIL " + "; ".join(bad[:10]))
        sys.exit(0 if same_j and same_m and not bad else 1)
    if bad:
        print("CHECK FAILED:", "; ".join(bad[:20]))
        sys.exit(1)
    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    with open(OUT_MD, "w", encoding="utf-8", newline="\n") as f:
        f.write(md)
    print(json.dumps(out["summary"], ensure_ascii=False))


if __name__ == "__main__":
    main()
