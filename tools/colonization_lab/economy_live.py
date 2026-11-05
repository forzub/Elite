"""Live economy layer 3126: hub module composition, trade-goods markets, price-information delays
(offline, deterministic, stdlib only).

Inputs (canon, PROPOSAL): canon/hub_modules.json (module catalog and derivation rules),
canon/trade_goods.json (37 trade goods mapped onto the 15 commodities.json categories),
canon/economy_live.json (ticks, events, quarantine, board, price-info, blocs, customs, patrols),
canon/commodities.json, canon/political_order.json (blocs, access, via political.py), canon/world.json (regimes),
canon/world_sites.json (deep-space objects), canon/sol_network.json (Sol function codes).
GENERATED inputs (run world_economy.py and world_sites.py first): the runtime sites document
src/assets/data/galaxy_details/world_sites/sites_3126.json, out/world_3126/beacons_3126.json,
out/world_3126/comms_3126.json, out/world_3126/economy_3126.json.

Outputs: out/world_3126/hub_modules_3126.json, market_state_3126.json, price_info_delay_3126.json
and runtime copies (GENERATED, never edited by hand) in src/assets/data/economy/:
trade_goods.json, economy_rules.json, hub_modules_3126.json, market_state_3126.json,
price_info_delay_3126.json.

    python economy_live.py            # compute, write out/ and the runtime copies
    python economy_live.py --check    # recompute, compare with the files, run the consistency checks
"""
import political
import argparse
import hashlib
import heapq
import json
import math
import os
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CANON = os.path.join(HERE, "canon")
OUT = os.path.join(HERE, "out", "world_3126")
RUNTIME_SITES = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json")
RUNTIME_ECONOMY = os.path.join(ROOT, "src", "assets", "data", "economy")
TAG = "PROPOSAL"
SPLIT_KEYS = ["shuttle", "liner", "cargo", "refuel", "service", "naval"]


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def canon(name):
    return load(os.path.join(CANON, name + ".json"))


def jitter(key, amp):
    """Deterministic +-amp from crc32 of the key (no RNG state)."""
    return 1.0 + amp * ((zlib.crc32(key.encode("utf-8")) % 2001) / 1000.0 - 1.0)


def r3(x):
    return round(x, 3)


def nice_t(x):
    if x >= 100:
        return int(round(x))
    if x >= 1:
        return round(x, 1)
    return round(x, 3)


def fmt_int(n):
    return f"{int(n):,}".replace(",", " ")


def plural(n, one, few, many):
    """Russian noun form after a number: 1 площадка, 2 площадки, 5 площадок."""
    n = abs(int(n))
    if n % 10 == 1 and n % 100 != 11:
        w = one
    elif 2 <= n % 10 <= 4 and not 12 <= n % 100 <= 14:
        w = few
    else:
        w = many
    return f"{fmt_int(n)} {w}"


class Inputs:
    def __init__(self):
        self.HM = canon("hub_modules")
        self.TG = canon("trade_goods")
        self.EL = canon("economy_live")
        self.CM = canon("commodities")
        self.IN = canon("industry")
        self.W = canon("world")
        self.WS = canon("world_sites")
        self.SN = canon("sol_network")
        self.sites_doc = load(RUNTIME_SITES)
        self.beacons = load(os.path.join(OUT, "beacons_3126.json"))
        self.comms = load(os.path.join(OUT, "comms_3126.json"))
        self.economy = load(os.path.join(OUT, "economy_3126.json"))
        self.cat = {c["id"]: c for c in self.CM["commodities"]}
        self.goods = self.TG["goods"]
        self.PO = political.Order(W=self.W)
        # runtime "bloc" of a hub = access group: political bloc, else nonaligned:<actor> / zone:<unit> (political.access_key)
        self.bloc_of = {u: self.PO.access_key(u) for u in self.PO.units}
        self.regime_of = {c["id"]: c.get("regime_3126") for c in self.W["colonies"]}
        self.regime_of["sol_authority"] = "sol_authority"
        self.polity_name = {}
        for s in self.sites_doc["sites"]:
            if s.get("polity") and s.get("polity_name"):
                self.polity_name.setdefault(s["polity"], s["polity_name"])
        self.systems = {s["system_id"]: s for s in self.sites_doc["systems"]}
        self.system_name = {k: v["name"] for k, v in self.systems.items()}
        for s in self.sites_doc["sites"]:
            sid = s["system_id"]
            if sid not in self.system_name and s.get("parent_body_id", "").startswith("system_"):
                self.system_name[sid] = s["parent_body_id"].split(".")[1].replace("_", " ")
        self.sol_function = {h["id"]: h.get("function", []) for h in self.SN["earth_hubs"] + self.SN["hubs"]}
        self.light_lag = {r["site"]: r["light_lag_s"] for r in self.comms["sites"]}
        self.coverage = {r["site"]: r["coverage"] for r in self.comms["sites"]}


def regime_family(I, polity):
    reg = I.regime_of.get(polity)
    return I.CM["regime_family"].get(reg, "open") if reg else "open"


# ---------------------------------------------------------------- hubs (sites + beacons + deep space)
def hub_records(I):
    """Every hub that exists in the data, normalised to one record shape."""
    hubs = []
    for s in I.sites_doc["sites"]:
        if s.get("docks") is None:  # Earth agglomerations (cities) are not hubs
            continue
        hubs.append(dict(id=s["id"], name=s["name"], source="sites_3126", type=s["type"], type_ru=s.get("type_ru"), system_id=s["system_id"],
                         polity=s.get("polity"), owner=s.get("owner") or s.get("polity"), operator=s.get("operator"), level=s.get("level"),
                         map_level=s.get("map_level"), purpose=s.get("purpose"), functions=s.get("function") or [], roles=s.get("roles") or [],
                         docks=s.get("docks") or 0, storage_t=s.get("storage_t") or 0, population=s.get("population") or 0,
                         repair=s.get("repair"), security=s.get("security"), medical_beds=s.get("medical_beds") or 0,
                         quarantine_beds=s.get("quarantine_beds") or 0, arrivals_day=s.get("arrivals_day"),
                         dock_sectors=s.get("dock_sectors"), dock_modules=s.get("dock_modules"),
                         passenger_terminals=s.get("passenger_terminals"), cargo_terminals=s.get("cargo_terminals"),
                         service_berths=s.get("service_berths"), legal=s.get("legal"), hidden=False,
                         known_to_player=s.get("known_to_player")))
    st = I.WS["site_types"]
    for d in I.WS["deep_space_objects"]:
        t = st[d["site_type"]]
        hubs.append(dict(id=d["id"], name=d["name"], source="deep_space_objects", type=d["site_type"], type_ru=t["ru"], system_id=None,
                         polity=d.get("polity"), owner=d.get("polity"), operator=d.get("operator"), level="C", map_level="galaxy",
                         purpose=d.get("purpose"), functions=[], roles=[], docks=t["docks"], storage_t=t["storage_t"],
                         population=d.get("population") or 0, repair=t["repair"], security=t["security"],
                         medical_beds=round(t["medical_beds_per_1000"] * (d.get("population") or 0) / 1000.0, 1),
                         quarantine_beds=round(t["quarantine_beds_per_1000"] * (d.get("population") or 0) / 1000.0, 1),
                         arrivals_day=None, dock_sectors=None, dock_modules=None, passenger_terminals=None, cargo_terminals=None,
                         service_berths=None, legal="illegal" if d.get("hidden") else None, hidden=bool(d.get("hidden")),
                         known_to_player="hidden" if d.get("hidden") else "public", stocks=d.get("stocks"),
                         beacon_uptime=(d.get("beacon") or {}).get("uptime")))
    bb = I.HM["beacons"]["bridge"]
    for b in I.beacons["bridge"]:
        sname = I.system_name.get(b["system"], f"система {b['system']}")
        hubs.append(dict(id=f"beacon.{b['id']}", name=f"Мост-маяк {b['id']} ({sname})", source="beacons_3126.bridge", type="beacon_bridge",
                         type_ru=bb["type_ru"], system_id=b["system"], polity=b["operator"], owner=b["operator"], operator=b["operator"],
                         level="C", map_level="system", purpose=f"мост-маяк системы: {b['motion']}; трафик {fmt_int(b['transponder_tracks'])} транспондеров",
                         functions=["маяк", "связь", "эфемериды"], roles=[], docks=bb["docks"], storage_t=bb["storage_t"],
                         population=b["staff"], repair=bb["repair"], security=bb["security"], medical_beds=bb["medical_beds"],
                         quarantine_beds=bb["quarantine_beds"], arrivals_day=None, dock_sectors=None, dock_modules=None,
                         passenger_terminals=None, cargo_terminals=None, service_berths=None, legal=None, hidden=False,
                         known_to_player="public", coverage_ls=b["coverage_ls"]))
    br = I.HM["beacons"]["relay"]
    for r in I.sites_doc["relays"]:
        hubs.append(dict(id=r["id"], name=r["name"], source="sites_3126.relays", type="beacon_relay", type_ru=br["type_ru"],
                         system_id=r["system_id"], polity="sol_authority", owner="sol_authority", operator="Управление маяков Сола",
                         level="C", map_level="system", purpose=f"навигационный ретранслятор Сола ({r['reason']})", functions=["маяк"],
                         roles=[], docks=br["docks"], storage_t=br["storage_t"], population=br["staff"], repair=br["repair"],
                         security=br["security"], medical_beds=0, quarantine_beds=0, arrivals_day=None, dock_sectors=None,
                         dock_modules=None, passenger_terminals=None, cargo_terminals=None, service_berths=None, legal=None,
                         hidden=False, known_to_player="public"))
    return hubs


def hub_staff(I, h):
    """People living ON the hub. A surface / moon port carries the population of its city in the site data;
    the port itself houses only its staff: staff_per_dock x docks + 1 per staff_per_storage_t of storage."""
    rule = I.HM["port_staff"]
    pop = int(h["population"] or 0)
    if h["type"] in rule["types"]:
        return min(pop, int(rule["staff_per_dock"] * h["docks"] + h["storage_t"] / rule["storage_t_per_staff"]))
    return pop


def split_docks(n, fractions):
    """Largest-remainder split of n pads over the SPLIT_KEYS fractions (sum = n)."""
    tot = sum(fractions)
    if n <= 0 or tot <= 0:
        return {k: 0 for k in SPLIT_KEYS}
    raw = [n * f / tot for f in fractions]
    base = [int(math.floor(x)) for x in raw]
    rest = n - sum(base)
    order = sorted(range(len(raw)), key=lambda i: (-(raw[i] - base[i]), i))
    for i in order[:rest]:
        base[i] += 1
    return dict(zip(SPLIT_KEYS, base))


def text_of(h):
    return " ".join([h.get("purpose") or "", h.get("name") or ""] + list(h.get("functions") or []) + list(h.get("roles") or [])).lower()


def has_kw(I, h, key):
    t = text_of(h)
    return any(k in t for k in I.HM["function_keywords"][key])


def compose(I, h):
    """Blocks and modules of one hub (counts and capacities)."""
    MT = I.HM["module_types"]
    rule = I.HM["type_rules"][h["type"]]
    level_a = h.get("level") == "A"
    ruined = h["type"] == "abandoned_hub"
    mods = []

    def add(t, count=1, **cap):
        if count <= 0:
            return
        m = dict(type=t, block=MT[t]["block"], count=int(count))
        m.update({k: v for k, v in cap.items() if v is not None})
        if ruined:
            m["state"] = "ruined"
        mods.append(m)

    sol_detail = (h.get("dock_modules") or 0) > 0
    sd = I.HM["sol_detail_rule"]
    if sol_detail:
        fn = I.sol_function.get(h["id"], [])
        gateway = "interstellar_gateway" in fn
        pads = h["docks"]
        hexes = h["dock_modules"]
        pterm = h.get("passenger_terminals") or 0
        cterm = h.get("cargo_terminals") or 0
        sberths = h.get("service_berths") or 0
        refuel = sd["refuel_docks_per_3_sectors"] * int(math.ceil((h.get("dock_sectors") or 1) / 3.0)) * (2 if "fuel" in fn else 1)
        liner = pterm * (sd["liner_platforms_per_terminal_gateway"] if gateway else sd["liner_platforms_per_terminal"])
        split = dict(shuttle=pads, liner=liner, cargo=cterm * MT["cargo_terminal"]["docks"], refuel=refuel, service=sberths, naval=0)
        add("shuttle_dock", hexes, pads=pads, sectors=h.get("dock_sectors"), scope="lifters")
        add("hotel", pterm, beds=pterm * MT["hotel"]["beds_per_sol_terminal"])
        add("liner_platform", liner, platforms=liner, scope="interstellar" if gateway else "interplanetary")
        add("cargo_terminal", cterm, docks=split["cargo"], train_berths=cterm if level_a else 0, teu=split["cargo"] * MT["cargo_terminal"]["teu_per_dock"])
        add("refuel_dock", int(math.ceil(refuel / 6.0)), docks=refuel)
        add("service_dock", int(math.ceil(sberths / 6.0)) if sberths else 0, docks=sberths, repair=h.get("repair"))
    else:
        fr = I.HM["dock_split"].get(h["type"])
        if h["type"] in ("beacon_bridge", "beacon_relay"):
            fr = I.HM["beacons"]["bridge" if h["type"] == "beacon_bridge" else "relay"]["dock_split"]
        split = split_docks(h["docks"], fr)
        if split["shuttle"]:
            add("shuttle_dock", int(math.ceil(split["shuttle"] / 6.0)), pads=split["shuttle"])
        if split["liner"]:
            beds = split["liner"] * MT["hotel"]["beds_per_liner_platform"]
            add("hotel", 1, beds=beds)
            add("liner_platform", 1, platforms=split["liner"], scope="interstellar")
        if split["cargo"]:
            n = int(math.ceil(split["cargo"] / float(MT["cargo_terminal"]["docks"])))
            add("cargo_terminal", n, docks=split["cargo"], train_berths=1 if level_a and h["type"] in ("trade_hub", "orbital_hub") else 0,
                teu=split["cargo"] * MT["cargo_terminal"]["teu_per_dock"])
        if split["refuel"]:
            add("refuel_dock", int(math.ceil(split["refuel"] / 6.0)), docks=split["refuel"])
        if split["service"]:
            add("service_dock", int(math.ceil(split["service"] / 6.0)), docks=split["service"], repair=h.get("repair"))
        if split["naval"]:
            add("naval_berth", int(math.ceil(split["naval"] / 4.0)), berths=split["naval"])

    # fuel / gas terminal
    ft = rule["fuel_terminal"]
    want_ft = ft == "yes" or (ft == "level_a" and level_a) or has_kw(I, h, "fuel_terminal")
    if want_ft and h["type"] not in ("beacon_relay", "abandoned_hub", "derelict_hub"):
        berths = MT["fuel_terminal"]["tanker_berths"] * (2 if level_a else 1)
        add("fuel_terminal", 1, tanker_berths=berths, tank_t=berths * MT["fuel_terminal"]["tank_t_per_berth"])

    # holding area ("отстойник")
    non_shuttle = sum(v for k, v in split.items() if k != "shuttle")
    slots = max(rule["holding_min"], int(math.ceil(0.25 * non_shuttle)))
    if rule["customs"]:
        slots = int(math.ceil(slots * 1.5))
    if slots and not ruined:
        add("holding_area", 1, slots=slots)

    # storage
    if h["storage_t"] > 0:
        add("warehouse", max(1, int(math.ceil(h["storage_t"] / MT["warehouse"]["storage_t"]))), storage_t=int(h["storage_t"]))

    # admin block: station, customs, habitat, medical, quarantine, market
    staff_admin = MT["admin_station"]["staff_level_a" if level_a else "staff"]
    if h["type"] != "beacon_relay":
        add("admin_station", 1, staff=min(staff_admin, max(4, h["population"])) if h["population"] else 0)
    if rule["customs"] and not ruined:
        add("customs_post", 1, inspection_berths=MT["customs_post"]["inspection_berths_level_a" if level_a else "inspection_berths"])
    staff = hub_staff(I, h)
    if staff > 0:
        add("habitat", max(1, int(math.ceil(staff / float(MT["habitat"]["beds"])))), beds=staff)
    scale = staff / float(h["population"]) if h["population"] else 1.0  # a city port: only the port's share of the city beds
    med, qua = h["medical_beds"] * scale, h["quarantine_beds"] * scale
    if med >= 1:
        add("medical_bay", 1, beds=int(round(med)))
    if qua >= 1 or has_kw(I, h, "quarantine_ward") and staff > 0:
        add("quarantine_ward", 1, beds=max(1, int(round(qua))))
    market = rule["market"]
    if h.get("legal") == "illegal" and market != "none":
        market = "black"
    if ruined:
        market = "none"
    if market != "none":
        add("market_exchange", 1, tier=market)

    # specialised modules
    special = set(rule.get("special", []))
    for key in ("ore_yard", "gas_platform_link", "shipyard_slip", "lab_module", "naval_berth", "production_plant"):
        if has_kw(I, h, key):
            special.add(key)
    if h["type"] in ("beacon_relay", "abandoned_hub"):
        special -= {"lab_module", "production_plant", "ore_yard"}
    if "naval_berth" in special and not any(m["type"] == "naval_berth" for m in mods) and h["type"] not in ("beacon_relay",):
        add("naval_berth", 1, berths=2)
    if "ore_yard" in special:
        add("ore_yard", 2 if level_a else 1, tug_berths=MT["ore_yard"]["tug_berths"] * (2 if level_a else 1))
    if "production_plant" in special:
        add("production_plant", 3 if level_a else 1)
    if "gas_platform_link" in special:
        add("gas_platform_link", 1)
    if "shipyard_slip" in special:
        add("shipyard_slip", 4 if level_a else 1)
    if "lab_module" in special:
        add("lab_module", 2 if level_a else 1)
    if "beacon_core" in special:
        add("beacon_core", 1, coverage_ls=h.get("coverage_ls"))

    order = list(I.HM["blocks"])
    blocks = []
    for b in order:
        ms = [m for m in mods if m["block"] == b]
        if ms:
            blocks.append(dict(block=b, ru=I.HM["blocks"][b], modules=[{k: v for k, v in m.items() if k != "block"} for m in ms]))
    totals = dict(pads=split["shuttle"], liner_platforms=split["liner"], cargo_docks=split["cargo"], refuel_docks=split["refuel"],
                  service_docks=split["service"], naval_berths=split["naval"] + (2 if "naval_berth" in special and not split["naval"] else 0),
                  holding_slots=slots if not ruined else 0, hotel_beds=sum(m.get("beds", 0) for m in mods if m["type"] == "hotel"),
                  residents=staff, city_population=int(h["population"]) if staff != int(h["population"]) else None, storage_t=int(h["storage_t"]), modules=sum(m["count"] for m in mods))
    return blocks, totals, market, mods


PADS = ("площадка", "площадки", "площадок")
BERTHS = ("причал", "причала", "причалов")
DOCKS = ("док", "дока", "доков")
PLACES = ("место", "места", "мест")
BEDS = ("койка", "койки", "коек")
PEOPLE = ("чел.", "чел.", "чел.")


def times(m):
    return f" ×{m['count']}" if m["count"] > 1 else ""


MOD_LABEL = {
    "shuttle_dock": lambda m: f"челночный док{times(m)} ({plural(m.get('pads', 0), *PADS)})",
    "hotel": lambda m: f"гостиница{times(m)} ({plural(m.get('beds', 0), *PLACES)})",
    "liner_platform": lambda m: f"платформы лайнеров ({plural(m.get('platforms', 0), 'платформа', 'платформы', 'платформ')})",
    "cargo_terminal": lambda m: f"грузовой терминал{times(m)} ({plural(m.get('docks', 0), *BERTHS)}" +
                                (f", {plural(m['train_berths'], 'причал', 'причала', 'причалов')} поезда" if m.get("train_berths") else "") + ")",
    "refuel_dock": lambda m: f"заправочный док ({plural(m.get('docks', 0), *DOCKS)})",
    "service_dock": lambda m: f"сервисный док ({plural(m.get('docks', 0), *DOCKS)})",
    "fuel_terminal": lambda m: f"топливный терминал ({plural(m.get('tanker_berths', 0), 'танкерный причал', 'танкерных причала', 'танкерных причалов')})",
    "holding_area": lambda m: f"отстойник ({plural(m.get('slots', 0), *PLACES)})",
    "warehouse": lambda m: f"склады ({fmt_int(m.get('storage_t', 0))} т)",
    "admin_station": lambda m: "станция-администрация",
    "customs_post": lambda m: "таможенный пост",
    "habitat": lambda m: f"жилые модули{times(m)} ({plural(m.get('beds', 0), *PEOPLE)})" if m["count"] > 1 else f"жилой модуль ({plural(m.get('beds', 0), *PEOPLE)})",
    "medical_bay": lambda m: f"медблок ({plural(m.get('beds', 0), *BEDS)})",
    "quarantine_ward": lambda m: f"карантинный блок ({plural(m.get('beds', 0), *PLACES)})",
    "market_exchange": lambda m: {"full": "биржа", "local": "местный рынок", "supply": "лавка снабжения", "black": "рынок без вопросов"}[m["tier"]],
    "ore_yard": lambda m: "рудный двор" + times(m),
    "production_plant": lambda m: "производственный модуль" + times(m),
    "gas_platform_link": lambda m: "узел газовых платформ",
    "shipyard_slip": lambda m: "стапель" + times(m),
    "lab_module": lambda m: "лаборатория" + times(m),
    "naval_berth": lambda m: f"военные причалы ({m.get('berths', 0)})",
    "beacon_core": lambda m: "ядро маяка",
}


def describe(I, h, blocks, supply):
    owner = I.polity_name.get(h.get("owner") or "", None) or h.get("owner") or "нет владельца"
    operator = I.polity_name.get(h.get("operator") or "", h.get("operator"))
    head = f"{h['name']} — {h.get('type_ru') or h['type']}"
    if h.get("level"):
        head += f" класса {h['level']}"
    parts = [head + f". Владелец: {owner}" + (f"; оператор: {operator}" if operator and operator != owner else "") + "."]
    if h.get("purpose"):
        parts.append(h["purpose"][0].upper() + h["purpose"][1:] + ("" if h["purpose"].endswith(".") else "."))
    for b in blocks:
        parts.append(b["ru"] + ": " + ", ".join(MOD_LABEL[m["type"]](m) for m in b["modules"]) + ".")
    if supply and supply["remote"]:
        s = "Снабжение"
        if supply["shift_size"]:
            s = f"Вахта {supply['shift_size']} чел. раз в {supply['rotation_days']} сут; снабжение"
        if supply.get("maintenance_visit"):
            s = "Без экипажа: техобслуживание раз в год; снабжение"
        s += f" ~{nice_t(supply['supply_t_per_30d'])} т на 30 сут"
        s += f" через {supply['supply_parent_name']}." if supply.get("supply_parent_name") else " — своим рейсом оператора."
        parts.append(s)
    return " ".join(parts)


# ---------------------------------------------------------------- comms graph and delays
def comms_graph(I):
    g = {}
    for l in I.comms["links"]:
        a, b = l["a"], l["b"]
        g.setdefault(a, {})[b] = l["routine_hop_h"]
        g.setdefault(b, {})[a] = l["routine_hop_h"]
    return g


def dijkstra(g, src, factor, refuse):
    dist = {src: 0.0}
    pq = [(0.0, str(src), src)]
    while pq:
        d, _, u = heapq.heappop(pq)
        if d > dist.get(u, 1e18):
            continue
        if u in refuse and u != src:
            continue
        for v, w in sorted(g.get(u, {}).items(), key=lambda kv: str(kv[0])):
            nd = d + w * factor
            if nd < dist.get(v, 1e18) - 1e-9:
                dist[v] = nd
                heapq.heappush(pq, (nd, str(v), v))
    return dist


def price_info_delay(I, hubs):
    g = comms_graph(I)
    refuse = set(I.comms.get("disconnected", []))
    bulk = I.WS["comms"]["priority_factor"]["bulk"]
    proc = I.WS["comms"]["relay_hop_processing_h"]
    nodes = sorted(g, key=lambda x: (isinstance(x, str), str(x)))
    matrix = []
    for a in nodes:
        if a in refuse:
            matrix.append([None if b != a else 0.0 for b in nodes])
            continue
        d = dijkstra(g, a, bulk, refuse)
        matrix.append([None if (b in refuse and b != a) or b not in d else round(d[b] + (proc if b != a else 0.0), 2) for b in nodes])
    hub_rows = {}
    for h in hubs:
        node = h["system_id"] if h["system_id"] is not None else h["id"]
        if node not in g and h["system_id"] is None:
            node = None
        lag = I.light_lag.get(h["id"], 0.0)
        cov = I.coverage.get(h["id"], "full" if node is not None else "none")
        if h["system_id"] in refuse:
            cov = "none"
        hub_rows[h["id"]] = dict(node=node, light_lag_s=lag, coverage=cov)
    return dict(status=TAG, generated_by="tools/colonization_lab/economy_live.py", do_not_edit=True, priority="bulk",
                priority_factor=bulk, market_period_h=I.EL["tick"]["market_period_h"],
                rule="D_h(hub H -> observer O) = light_lag(H)/3600 + bulk_h[node(H)][node(O)] + light_lag(O)/3600; "
                     "age ~ D + market_period_h/2; null = no network path (disconnected) -> age from the last ship arrival",
                nodes=[str(n) for n in nodes], bulk_h=matrix, hubs=hub_rows), g


def nearest_parent(I, h, hubs_by_system, g, markets):
    """Hub whose board carries the supply / crew requests of a remote hub or beacon."""
    bloc = I.bloc_of.get(h.get("polity"))

    def best(cands):
        cands = [c for c in cands if c["id"] != h["id"] and markets.get(c["id"]) == "full"]
        if not cands:
            return None
        cands.sort(key=lambda c: (c.get("polity") != h.get("polity"), I.bloc_of.get(c.get("polity")) != bloc, -c["docks"], c["id"]))
        return cands[0]

    if h["system_id"] is not None:
        p = best(hubs_by_system.get(h["system_id"], []))
        if p:
            return p
    start = h["system_id"] if h["system_id"] is not None else h["id"]
    if start not in g:
        return None
    d = dijkstra(g, start, 1.0, set(I.comms.get("disconnected", [])))
    scored = []
    for node, dist in d.items():
        if isinstance(node, int) and node != h["system_id"]:
            p = best(hubs_by_system.get(node, []))
            if p:
                w = 1.0 if p.get("polity") == h.get("polity") else (1.5 if bloc and I.bloc_of.get(p.get("polity")) == bloc else 3.0)
                scored.append((dist * w, str(node), p))
    if scored:
        return min(scored, key=lambda x: (x[0], x[1]))[2]
    return None


def supply_profile(I, h, totals, market, hubs_by_system, g, markets):
    S = I.HM["supply"]
    sys_has_full = h["system_id"] is not None and any(markets.get(c["id"]) == "full" for c in hubs_by_system.get(h["system_id"], []))
    remote = (h["type"] in S["remote_types"] and h.get("level") != "A") or h["system_id"] is None or not sys_has_full
    if h["type"] in ("abandoned_hub",) or (h["population"] == 0 and h["type"] not in ("beacon_relay",)):
        remote = remote and h["type"] == "beacon_relay"
    if not remote:
        return dict(remote=False)
    rot = S["rotation"].get(h["type"], {"share": 0.0, "period_days": 0})
    staff = hub_staff(I, h)
    kit = {k: r3(v * staff) for k, v in S["kit_per_person_30d_t"].items() if v * staff > 0}
    for k, v in S["kit_per_module_30d_t"].items():
        kit[k] = r3(kit.get(k, 0.0) + v * totals["modules"])
    parent = nearest_parent(I, h, hubs_by_system, g, markets)
    out = dict(remote=True, staff=staff, rotation_days=rot["period_days"], shift_size=int(math.ceil(staff * rot["share"])),
               supply_t_per_30d=r3(sum(kit.values())), kit_30d_t=kit, supply_parent=parent["id"] if parent else None,
               supply_parent_name=parent["name"] if parent else None, supply_mode="board" if parent else "own_ship")
    if rot.get("maintenance_visit"):
        out["maintenance_visit"] = True
    return out


# ---------------------------------------------------------------- markets
LOCAL_GROUPS = {"food", "water_gas", "ore", "metals", "chemicals", "construction", "fuel", "machinery", "medical", "consumer"}
SUPPLY_GOODS = {"food_staples", "food_rations", "water", "oxygen", "tech_gases", "reactor_fuel", "jump_charge", "spare_parts",
                "life_support", "pharmaceuticals", "household_goods", "clothing"}
PRODUCER_RULES = [
    ("mining_hub", None, ["ore_common", "ore_rare", "water"]),
    ("belt_hub", None, ["ore_common", "water", "tech_gases"]),
    ("industrial_hub", None, ["steel", "light_alloys", "construction_bulk", "industrial_machinery"]),
    (None, "газ", ["reactor_fuel", "tech_gases"]),
    (None, "he-3", ["reactor_fuel"]),
    (None, "хим", ["polymers", "industrial_chem", "fertilizers"]),
    (None, "металлург", ["steel", "light_alloys"]),
    (None, "аграр", ["food_staples", "food_fresh"]),
    (None, "агро", ["food_staples", "food_fresh"]),
    (None, "верф", ["spare_parts"]),
    (None, "столица", ["consumer_electronics", "household_goods"]),
    (None, "крупный город", ["household_goods", "clothing"]),
    (None, "заряд прыжка", ["jump_charge"]),
]


def market_state(I, hubs, markets, remote_of):
    goods = I.goods
    gid = [g["id"] for g in goods]
    # world tonnage weight per good: commodity consumption share x good share, compressed ^0.5
    tot = {}
    for p, v in I.economy["polities"].items():
        for c, b in v["balance"].items():
            tot[c] = tot.get(c, 0.0) + b["consumption"]
    world_t = sum(tot.values())
    weight = {g["id"]: math.sqrt(max(1e-9, tot.get(g["commodity"], 0.0) / world_t * g["share"])) for g in goods}
    cname = {c["id"]: c["name"] for c in I.CM["commodities"]}
    LM = I.EL["tick"]["local_mult"]
    exp = I.EL["tick"]["price_rule"]["stock_exponent"]
    spread = I.EL["tick"]["price_rule"]["spread_by_market"]
    rows = []
    for h in hubs:
        tier = markets.get(h["id"], "none")
        if tier == "none":
            continue
        fam = regime_family(I, h.get("polity"))
        sysrow = I.systems.get(h["system_id"], {}) if h["system_id"] is not None else {}
        producer = set()
        t = text_of(h)
        for typ, kw, gl in PRODUCER_RULES:
            if (typ and h["type"] == typ) or (kw and kw in t):
                producer.update(gl)
        if h["type"] == "military_hub":
            traded_extra = {"munitions", "small_arms"}
        else:
            traded_extra = set()
        price, stock, target, flow = [], [], [], []
        traded = []
        for g in goods:
            leg = (g.get("legality") or I.cat[g["commodity"]]["legality"]).get(fam, "legal")
            grp = g["group"]
            ok = {"full": True, "black": True, "local": grp in LOCAL_GROUPS, "supply": g["id"] in SUPPLY_GOODS}[tier]
            ok = ok or g["id"] in producer or g["id"] in traded_extra
            if leg == "banned" and tier != "black":
                ok = False
            if g.get("licence") and tier not in ("black",) and "лотосит" not in t:
                ok = False
            if not ok:
                price.append(None); stock.append(None); target.append(None); flow.append(None)
                continue
            traded.append(g["id"])
            cn = cname[g["commodity"]]
            s = 1.0
            if cn in sysrow.get("shortages", []):
                s *= LM["system_shortage"]
            if cn in sysrow.get("imports", []):
                s *= LM["system_import"]
            if cn in sysrow.get("exports", []):
                s *= LM["system_export"]
            if cn in sysrow.get("industries", []):
                s *= LM["system_industry"]
            if g["id"] in producer:
                s *= LM["hub_producer"]
            structural = jitter(h["id"] + "/" + g["id"], LM["jitter"])
            if remote_of.get(h["id"]):
                structural *= LM["remote_hub"]
            if h["system_id"] is None:
                structural *= LM["deep_space"]
            lm = I.EL["tick"]["legality_mult"]
            legm = lm["legal"] if leg == "legal" else (lm["restricted"] if leg == "restricted" else lm["banned_black"])
            base = I.cat[g["commodity"]]["cost_cr_per_t"] * g["price_factor"]
            w = weight[g["id"]] * (3.0 if g["id"] in producer else 1.0) * g.get("stock_scale", 1.0)
            target.append(w)  # normalised below
            stock.append(s)
            price.append(base * structural * s * legm)
            flow.append(None)
        wsum = sum(x for x in target if x is not None) or 1.0
        cap = max(h["storage_t"], 50) * 0.5
        for i in range(len(goods)):
            if target[i] is None:
                continue
            tg = cap * target[i] / wsum
            st = tg * stock[i] ** (-1.0 / exp)
            target[i] = nice_t(tg)
            stock[i] = nice_t(min(3.0 * tg, max(0.05 * tg, st)))
            flow[i] = nice_t(tg / I.EL["tick"]["cover_days"])
            price[i] = int(round(price[i])) if price[i] >= 10 else round(price[i], 2)
        rows.append(dict(hub=h["id"], market=tier, spread=spread[tier], regime_family=fam, producer=sorted(producer & set(traded)),
                         price_cr=price, stock_t=stock, target_t=target, flow_t_day=flow))
    return dict(status=TAG, generated_by="tools/colonization_lab/economy_live.py", do_not_edit=True, epoch_year=3126, tick=0,
                goods_order=gid, note="arrays follow goods_order; null = not traded here. price_cr = mid price (buy/sell = price x (1 -/+ spread/2)). "
                                      "Initial stock encodes the scarcity seed so that the tick formula reproduces the initial price.",
                hubs=rows)


# ---------------------------------------------------------------- build
def build(I):
    hubs = hub_records(I)
    hubs_by_system = {}
    for h in hubs:
        if h["system_id"] is not None:
            hubs_by_system.setdefault(h["system_id"], []).append(h)
    composed = {}
    markets = {}
    for h in hubs:
        blocks, totals, market, mods = compose(I, h)
        composed[h["id"]] = (blocks, totals, market)
        markets[h["id"]] = market
    delay, g = price_info_delay(I, hubs)
    records = []
    remote_of = {}
    for h in hubs:
        blocks, totals, market = composed[h["id"]]
        sup = supply_profile(I, h, totals, market, hubs_by_system, g, markets)
        remote_of[h["id"]] = sup.get("remote", False)
        bloc = I.bloc_of.get(h.get("polity"))
        rec = dict(id=h["id"], name=h["name"], source=h["source"], type=h["type"], type_ru=h.get("type_ru"), system_id=h["system_id"],
                   system_name=I.system_name.get(h["system_id"]) if h["system_id"] is not None else None,
                   polity=h.get("polity"), owner=h.get("owner"), owner_name=I.polity_name.get(h.get("owner") or ""), operator=h.get("operator"),
                   bloc=bloc, political_bloc=I.PO.bloc_of_unit(h["polity"]) if h.get("polity") in I.bloc_of else None,
                   actor=I.PO.actor_of(h["polity"]) if h.get("polity") in I.bloc_of else None, level=h.get("level"), map_level=h.get("map_level"), hidden=h.get("hidden", False),
                   functions=h.get("functions") or [], purpose=h.get("purpose"), market=market, hosts_board=h["docks"] > 0 and market != "none" or h["type"] in ("orbital_hub", "trade_hub"),
                   blocks=blocks, totals=totals, supply=sup)
        rec["description_ru"] = describe(I, h, blocks, sup)
        records.append(rec)
    records.sort(key=lambda r: (r["system_id"] is None, r["system_id"] if r["system_id"] is not None else 0, r["source"], r["id"]))
    hub_doc = dict(schema="hub_modules_3126/1", status=TAG, generated_by="tools/colonization_lab/economy_live.py", do_not_edit=True, epoch_year=3126,
                   module_types=I.HM["module_types"], blocks=I.HM["blocks"],
                   counts=dict(hubs=len(records), by_source=count_by(records, "source"), by_type=count_by(records, "type"),
                               remote=sum(1 for r in records if r["supply"].get("remote"))),
                   hubs=records)
    mkt = market_state(I, hubs, markets, remote_of)
    return hub_doc, mkt, delay


def count_by(rows, key):
    out = {}
    for r in rows:
        out[r[key]] = out.get(r[key], 0) + 1
    return dict(sorted(out.items()))


def dumps(o, indent=1):
    return json.dumps(o, ensure_ascii=False, indent=indent, sort_keys=True) + "\n"


def files_of(I, hub_doc, mkt, delay):
    out = {os.path.join(OUT, "hub_modules_3126.json"): json.dumps(hub_doc, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
           os.path.join(OUT, "market_state_3126.json"): json.dumps(mkt, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n",
           os.path.join(OUT, "price_info_delay_3126.json"): json.dumps(delay, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"}
    rt = {os.path.join(RUNTIME_ECONOMY, "hub_modules_3126.json"): out[os.path.join(OUT, "hub_modules_3126.json")],
          os.path.join(RUNTIME_ECONOMY, "market_state_3126.json"): out[os.path.join(OUT, "market_state_3126.json")],
          os.path.join(RUNTIME_ECONOMY, "price_info_delay_3126.json"): out[os.path.join(OUT, "price_info_delay_3126.json")]}
    tg = dict(I.TG)
    tg.update(generated_by="tools/colonization_lab/economy_live.py (copy of canon/trade_goods.json + base prices)", do_not_edit=True)
    tg["goods"] = [dict(g, base_cr_t=round(I.cat[g["commodity"]]["cost_cr_per_t"] * g["price_factor"], 2),
                        density_t_m3=g.get("density_t_m3", I.cat[g["commodity"]]["density_t_m3"]),
                        hazard=g.get("hazard", I.cat[g["commodity"]]["hazard"]),
                        legality=g.get("legality", I.cat[g["commodity"]]["legality"])) for g in I.TG["goods"]]
    tg["container"] = I.CM["container"]
    tg["regime_family"] = I.CM["regime_family"]
    rt[os.path.join(RUNTIME_ECONOMY, "trade_goods.json")] = dumps(tg, indent=1)
    el = dict(I.EL)
    keys = sorted({I.PO.access_key(u) for u in I.PO.units})
    el.update(generated_by="tools/colonization_lab/economy_live.py (copy of canon/economy_live.json + blocs derived from canon/political_order.json)", do_not_edit=True,
              blocs_members={k: sorted(u for u in I.PO.units if I.PO.access_key(u) == k) for k in keys})
    el["blocs"] = dict(I.EL["blocs"], channels_order=list(political.CHANNEL), matrix=I.PO.headline_matrix(),
                       status=I.PO.P["status"], rule=I.PO.P["headline_channel_rule"])
    el["political_access"] = dict(categories=I.PO.cats, scale=list(political.CHANNEL), note="seller unit -> buyer unit -> six digits = levels 0..4 in categories order (0 open, 1 licensed, 2 restricted, 3 black / sanctions, 4 closed)",
                                  matrix=I.PO.unit_matrix())
    rt[os.path.join(RUNTIME_ECONOMY, "economy_rules.json")] = dumps(el, indent=1)
    out.update(rt)
    return out


# ---------------------------------------------------------------- checks
def checks(I, hub_doc, mkt, delay):
    res = []

    def ok(name, cond, detail=""):
        res.append((name, bool(cond), detail))

    goods = I.goods
    ok("goods_count_30_40", 30 <= len(goods) <= 40, str(len(goods)))
    ok("goods_ids_unique", len({g["id"] for g in goods}) == len(goods))
    ok("goods_commodity_known", all(g["commodity"] in I.cat for g in goods))
    ok("goods_group_known", all(g["group"] in {x["id"] for x in I.TG["groups"]} for g in goods))
    bad = []
    for c in I.cat:
        gs = [g for g in goods if g["commodity"] == c]
        sh = sum(g["share"] for g in gs)
        wp = sum(g["share"] * g["price_factor"] for g in gs)
        if not gs or abs(sh - 1.0) > 1e-6 or abs(wp - 1.0) > 0.06:
            bad.append(f"{c}: share {sh:.3f} price {wp:.3f}")
    ok("every_commodity_mapped_share1_price1", not bad, "; ".join(bad))
    hubs = hub_doc["hubs"]
    ok("hub_ids_unique", len({h["id"] for h in hubs}) == len(hubs))
    site_hubs = [s for s in I.sites_doc["sites"] if s.get("docks") is not None]
    ok("all_site_hubs_covered", {s["id"] for s in site_hubs} <= {h["id"] for h in hubs}, f"{len(site_hubs)} site hubs")
    ok("all_beacons_covered", all(f"beacon.{b['id']}" in {h['id'] for h in hubs} for b in I.beacons["bridge"]))
    ok("all_dso_covered", all(d["id"] in {h['id'] for h in hubs} for d in I.WS["deep_space_objects"]))
    pads_bad = []
    sites = {s["id"]: s for s in site_hubs}
    for h in hubs:
        if h["id"] in sites and not (sites[h["id"]].get("dock_modules") or 0):
            t = h["totals"]
            if t["pads"] + t["liner_platforms"] + t["cargo_docks"] + t["refuel_docks"] + t["service_docks"] + t["naval_berths"] - \
                    (2 if any(m["type"] == "naval_berth" for b in h["blocks"] for m in b["modules"]) and not sites[h["id"]].get("docks") else 0) < sites[h["id"]]["docks"]:
                pads_bad.append(h["id"])
        if h["id"] in sites and (sites[h["id"]].get("dock_modules") or 0):
            if h["totals"]["pads"] != sites[h["id"]]["docks"]:
                pads_bad.append(h["id"])
    ok("dock_totals_match_site_docks", not pads_bad, ",".join(pads_bad[:5]))
    ok("every_live_hub_has_admin", all(any(m["type"] == "admin_station" for b in h["blocks"] for m in b["modules"])
                                      for h in hubs if h["type"] not in ("beacon_relay",)))
    ok("every_hub_has_description", all(h.get("description_ru") for h in hubs))
    rem = [h for h in hubs if h["supply"].get("remote")]
    ok("remote_hubs_have_supply_mode", all((h["supply"]["supply_mode"] == "board") == bool(h["supply"].get("supply_parent")) for h in rem),
       "own_ship: " + ",".join(h["id"] for h in rem if h["supply"]["supply_mode"] == "own_ship")[:300])
    ids = {h["id"] for h in hubs}
    exs = I.EL["board"]["examples"]
    ok("board_examples_reference_existing_hubs", all(e["origin_hub"] in ids and e["destination_hub"] in ids for e in exs),
       ",".join(x for e in exs for x in (e["origin_hub"], e["destination_hub"]) if x not in ids))
    ok("board_examples_goods_known", all(e["goods"] is None or e["goods"] in {g["id"] for g in goods} for e in exs))
    keys = set(I.EL["board"]["entry_schema"])
    ok("board_examples_follow_schema", all(set(e) == keys for e in exs), "")
    blocs = {I.PO.access_key(u) for u in I.PO.units}
    m = I.PO.headline_matrix()
    full = all((m.get(a, {}).get(b) or m.get(b, {}).get(a)) for a in blocs for b in blocs)
    ok("bloc_matrix_complete", full and set(m) == blocs)
    ok("hub_blocs_are_access_keys", all(h["bloc"] in blocs for h in hubs if h.get("polity") in I.bloc_of))
    n = len(mkt["goods_order"])
    ok("market_arrays_aligned", all(len(r[k]) == n for r in mkt["hubs"] for k in ("price_cr", "stock_t", "target_t", "flow_t_day")))
    ok("market_no_negative", all((x is None or x >= 0) for r in mkt["hubs"] for k in ("price_cr", "stock_t", "target_t") for x in r[k]))
    ok("delay_matrix_square", all(len(row) == len(delay["nodes"]) for row in delay["bulk_h"]))
    fin = [x for row in delay["bulk_h"] for x in row if x is not None]
    ok("delay_max_plausible_days", max(fin) / 24.0 < 40.0, f"max {max(fin) / 24.0:.1f} d")
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    I = Inputs()
    hub_doc, mkt, delay = build(I)
    files = files_of(I, hub_doc, mkt, delay)
    res = checks(I, hub_doc, mkt, delay)
    # determinism: a second build is byte-identical
    files2 = files_of(I, *build(I))
    res.append(("deterministic_rebuild", files == files2, ""))
    if a.check:
        stale = [p for p, txt in files.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != txt]
        res.append(("files_up_to_date", not stale, ",".join(os.path.relpath(p, ROOT) for p in stale)))
    else:
        os.makedirs(OUT, exist_ok=True)
        os.makedirs(RUNTIME_ECONOMY, exist_ok=True)
        for p, txt in files.items():
            with open(p, "w", encoding="utf-8", newline="\n") as f:
                f.write(txt)
    failed = [r for r in res if not r[1]]
    for name, good, detail in res:
        print(f"{'OK  ' if good else 'FAIL'} {name}" + (f"  ({detail})" if detail else ""))
    h = hashlib.sha256("".join(files[k] for k in sorted(files)).encode("utf-8")).hexdigest()
    c = hub_doc["counts"]
    print(f"hubs={c['hubs']} remote={c['remote']} by_source={c['by_source']} markets={len(mkt['hubs'])} goods={len(mkt['goods_order'])} sha256={h}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
