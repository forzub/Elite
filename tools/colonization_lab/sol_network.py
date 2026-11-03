"""Sol 3126: the hub network of the Solar System, derived from population, mining, traffic and orbit physics.

Called by world_sites.py (it replaces the generic Sol hubs of build_sites) and runnable alone:

    python sol_network.py            # print the network summary and checks (needs out/world_3126 from world_economy.py / world_sites.py)

Inputs: canon/sol_network.json (names, history, owners, orbits, rules - the only authored part),
Sol population split (economy settlement_structure), the 60 Earth agglomerations (world_sites.earth_settlements),
Sol infrastructure asset counts (infrastructure_3126.json), passenger rates (canon/world_sites.json -> passengers),
ship classes (canon/ship_economics.json). Outputs: Sol sites (one id = one world object, legacy generic ids kept as
aliases), Earth orbital network (orbits, surface allocation, docks, utilisation), inter-hub and interplanetary routes
(aggregate, persistent route ids), local relays, capacity checks, runtime orbital hubs for initial_world_state.json.
"""
import json
import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
TAG = "PROPOSAL"
MU_EARTH = 398600.4418          # km^3/s^2
R_EARTH = 6371.0
MU_SUN = 1.32712440018e11       # km^3/s^2
AU_KM = 1.495978707e8
R_SUN = 695700.0
G0 = 9.80665
DAY = 86400.0
SOL = "sol_authority"
REGION_OF_KEY = {}
FUNC_RU = {"interstellar_gateway": "межзвёздные ворота", "customs": "таможня", "passenger": "пассажиры", "administration": "администрация", "cargo": "грузы",
           "heavy_lift": "тяжёлые грузы", "shipyard_service": "обслуживание верфей", "interplanetary": "межпланетные рейсы", "fuel": "топливо",
           "power": "энергосеть", "industry": "промышленность", "comms": "связь", "science": "наука", "observation": "наблюдение Земли",
           "polar_service": "полярная связь", "transfer": "пересадка", "ice": "лёд", "mining": "добыча", "defence": "оборона", "quarantine": "карантин",
           "medical": "медицина", "shipyard": "верфь", "agro": "агро", "logistics": "логистика", "metallurgy": "металлургия", "high_inclination": "высокие наклонения",
           "black_market": "чёрный рынок", "energy": "энергия", "atmosphere": "атмосфера", "trade": "торговля", "chemicals": "химия", "gas": "газ", "outer_relay": "ретранслятор"}


def r3(x):
    return round(x, 3)


def load_canon():
    return json.load(open(os.path.join(HERE, "canon", "sol_network.json"), encoding="utf-8"))


# ---------------------------------------------------------------- orbits
def circ_period_s(alt_km, mu=MU_EARTH, R=R_EARTH):
    r = R + alt_km
    return 2.0 * math.pi * math.sqrt(r ** 3 / mu)


def circ_speed_km_s(alt_km, mu=MU_EARTH, R=R_EARTH):
    return math.sqrt(mu / (R + alt_km))


def hub_pos_km(h, t):
    """Circular orbit in the parent-equatorial frame: node about the pole, inclination about the node line."""
    r = R_EARTH + h["altitude_km"]
    u = math.radians(h["phase_deg"]) + 2.0 * math.pi * t / circ_period_s(h["altitude_km"])
    x, y = r * math.cos(u), r * math.sin(u)
    i, O = math.radians(h["inclination_deg"]), math.radians(h["lan_deg"])
    xi, yi, zi = x, y * math.cos(i), y * math.sin(i)
    return (xi * math.cos(O) - yi * math.sin(O), xi * math.sin(O) + yi * math.cos(O), zi)


def min_separation(hubs, days=2.0, step=20.0):
    worst = (1e18, None)
    n = int(days * DAY / step)
    pos = {h["id"]: [hub_pos_km(h, k * step) for k in range(n)] for h in hubs}
    ids = [h["id"] for h in hubs]
    for a in range(len(ids)):
        for b in range(a + 1, len(ids)):
            pa, pb = pos[ids[a]], pos[ids[b]]
            d = min(math.dist(pa[k], pb[k]) for k in range(n))
            if d < worst[0]:
                worst = (d, (ids[a], ids[b]))
    return worst


# ---------------------------------------------------------------- Earth: surface -> orbit allocation
def allocate_surface(SN, earth_rows, legs_y):
    """Every agglomeration splits its lifter legs over the hubs whose plane it reaches directly (i >= |lat|),
    logit on the lost Earth-rotation boost; the rest of Earth follows the agglomerations' latitude mix."""
    ST = SN["surface_traffic"]
    hubs = [h for h in SN["earth_hubs"] if any(f in h["function"] for f in ("passenger", "cargo", "heavy_lift"))
            and h["altitude_km"] < 2000]
    cities = [c for c in earth_rows if c.get("surface")]
    pop60 = sum(c["population"] for c in cities)
    rest = sum(c["population"] for c in earth_rows if not c.get("surface"))
    load = {h["id"]: 0.0 for h in hubs}
    per_city = {}
    for it in range(6):          # congestion iterations: loads relative to the mean share
        mean = sum(load.values()) / max(1, len(load)) or 1.0
        new = {h["id"]: 0.0 for h in hubs}
        for c in cities:
            lat = abs(c["surface"]["lat_deg"])
            legs = legs_y * c["population"] / pop60   # rest of Earth assumed to share the cities' latitude mix
            w = {}
            for h in hubs:
                if h["inclination_deg"] + ST["dogleg_tolerance_deg"] < lat:
                    continue
                excess = max(0.0, h["inclination_deg"] - lat)
                cong = (load[h["id"]] / mean) if it else 0.0
                w[h["id"]] = math.exp(-ST["inclination_penalty_per_deg"] * excess - ST["congestion_weight"] * max(0.0, cong - 1.0))
            s = sum(w.values())
            per_city[c["id"]] = {k: legs * v / s for k, v in w.items()}
            for k, v in per_city[c["id"]].items():
                new[k] += v
        load = new
    return load, per_city, dict(cities=len(cities), pop_cities=pop60, pop_rest=rest,
                                note="остальная Земля (города вне 60 агломераций) летает с той же смесью широт")


def dock_sizing(SN, movements_day, cycle_min):
    D = SN["dock_model"]
    per_pad_day = D["operating_hours_per_day"] * 60.0 / cycle_min
    target = sum(D["target_utilisation"]) / 2.0
    sectors = max(1, math.ceil(movements_day / (D["pads_per_sector"] * per_pad_day * target)))
    util = movements_day / (sectors * D["pads_per_sector"] * per_pad_day)
    return sectors, util, per_pad_day


# ---------------------------------------------------------------- Sol regions and their drivers
def region_population(I, SN):
    st = I.pol[SOL]["settlement_structure"]
    out = {}
    for rid, r in SN["regions"].items():
        k = r["population_key"]
        out[rid] = 0 if k is None else next((v for a, v in st.items() if a.endswith(k)), 0)
    return out


def asset_by_region(I, SN, pops):
    A = {a: v["count"] for a, v in I.X["infrastructure"]["polities"][SOL]["assets"].items()}
    S = SN["asset_region_shares"]
    out = {r: {} for r in SN["regions"]}
    for asset in ("mine", "ice_mine", "gas_platform", "research_station", "military_base"):
        for r, sh in S[asset].items():
            out[r][asset] = A.get(asset, 0) * sh
    off = sum(pops.values())
    ind = A.get("metallurgy", 0) + A.get("factory", 0) + A.get("electronics_fab", 0) + A.get("chemical_plant", 0)
    for r in out:
        out[r]["industry"] = ind * 0.25 * pops[r] / off    # a quarter of Sol industry sits off Earth, by population
    return out, A


def required_hubs(SN, pops, assets):
    D = SN["hub_drivers"]
    req = {}
    for r in SN["regions"]:
        a = assets[r]
        req[r] = dict(passenger=math.ceil(pops[r] / D["passenger_port_per_population"]) if pops[r] else 0,
                      mining=math.ceil((a.get("mine", 0) + a.get("ice_mine", 0)) / D["mining_hub_per_mines"]),
                      gas=math.ceil(a.get("gas_platform", 0) / D["gas_hub_per_platforms"]),
                      science=math.ceil(a.get("research_station", 0) / D["science_hub_per_stations"]),
                      industry=math.ceil(a.get("industry", 0) / 900.0) if a.get("industry", 0) > 300 else 0,
                      military=1 if a.get("military_base", 0) >= 0.9 else 0)
    return req


# ---------------------------------------------------------------- the network
LEO_SHUTTLE_MIN = 75.0       # hub <-> hub in low Earth orbit: an Orbital Transfer of ~1 h + docking


def region_anchor_au(SN, I, rid):
    a = {"earth_cislunar": 1.0, "mars": 1.524, "belt": 2.77, "mercury": 0.387, "venus": 0.723, "jupiter": 5.203, "saturn": 9.537, "outer": 19.19}
    return a[rid]


def brachistochrone_days(d_au, accel_g):
    return 2.0 * math.sqrt(d_au * AU_KM * 1000.0 / (accel_g * G0)) / DAY


def compute(I, earth_rows, pax_model):
    SN = load_canon()
    SE = I.SE["ship_classes"]
    WSP = I.WS["passengers"]
    D = SN["dock_model"]
    pops = region_population(I, SN)
    earth_pop = sum(c["population"] for c in earth_rows)
    assets, A = asset_by_region(I, SN, pops)
    req = required_hubs(SN, pops, assets)

    # ---------------- Earth orbital network
    legs_y = earth_pop * WSP["trips_per_capita_y_surface_orbit"] * 2.0
    lf = SN["surface_traffic"]["load_factor"]
    seats = SE["surface_lifter"]["passengers"]
    load, per_city, alloc_note = allocate_surface(SN, earth_rows, legs_y)
    pax_y = pax_model
    sol_interstellar = sum(x["pax_y"] for x in pax_y["od_top"] if SOL in x["link"])
    off_earth = sum(pops.values())
    TS = SN["transfer_shares"]
    interplanetary_y = off_earth * TS["interplanetary_per_capita_off_earth_y"] + earth_pop * TS["interplanetary_per_capita_earth_y"]
    earth_hubs = []
    total_surface = sum(load.values())
    gateway = "earth_orbital_hub"
    interplanetary_hubs = [h["id"] for h in SN["earth_hubs"] if "interplanetary" in h["function"]]
    hub_hub = {}
    for h in SN["earth_hubs"]:
        surf_legs = load.get(h["id"], 0.0)
        pax_flights_day = surf_legs / (seats * lf) / 365.0
        cargo_day = pax_flights_day * SN["surface_traffic"]["cargo_flights_per_pax_flight"] * (2.0 if h["profile"] == "cargo" else 1.0)
        service_day = pax_flights_day * SN["surface_traffic"]["service_flights_per_pax_flight"]
        earth_hubs.append(dict(h, surface_legs_y=round(surf_legs), surface_pax_flights_day=r3(pax_flights_day), cargo_lifter_flights_day=r3(cargo_day),
                               service_flights_day=r3(service_day)))
    # hub <-> hub: transfer share by gravity of surface loads; interstellar connections to the gateway; interplanetary departures
    eh = {h["id"]: h for h in earth_hubs}
    leo = [h for h in earth_hubs if h["altitude_km"] < 2000]
    for a in leo:
        for b in leo:
            if a["id"] >= b["id"]:
                continue
            g = a["surface_legs_y"] * b["surface_legs_y"] / max(1.0, total_surface)
            hub_hub[(a["id"], b["id"])] = dict(transfer=TS["hub_to_hub_transfer_share"] * g)
    for h in leo:
        if h["id"] == gateway:
            continue
        key = tuple(sorted((h["id"], gateway)))
        hub_hub.setdefault(key, dict(transfer=0.0))
        hub_hub[key]["interstellar"] = sol_interstellar * h["surface_legs_y"] / max(1.0, total_surface)
        for ip in interplanetary_hubs:
            if ip == h["id"]:
                continue
            k2 = tuple(sorted((h["id"], ip)))
            hub_hub.setdefault(k2, dict(transfer=0.0))
            hub_hub[k2]["interplanetary"] = hub_hub[k2].get("interplanetary", 0.0) + \
                interplanetary_y * 0.6 * h["surface_legs_y"] / max(1.0, total_surface) / len(interplanetary_hubs)
    geo = next(h for h in earth_hubs if h["altitude_km"] > 30000)
    for h in leo:
        if h["profile"] in ("cargo",) or "heavy_lift" in h["function"]:
            k = tuple(sorted((h["id"], geo["id"])))
            hub_hub.setdefault(k, dict(transfer=0.0))
            hub_hub[k]["cargo_t"] = 2.0e6
    aur = next(h for h in earth_hubs if "science" in h["function"])
    k = tuple(sorted((aur["id"], gateway)))
    hub_hub.setdefault(k, dict(transfer=0.0))
    hub_hub[k]["service"] = 6000.0
    # movements per hub
    for h in earth_hubs:
        hh_flights = 0.0
        for (a, b), f in hub_hub.items():
            if h["id"] in (a, b):
                pax = f.get("transfer", 0) + f.get("interstellar", 0) + f.get("interplanetary", 0) + f.get("service", 0)
                hh_flights += pax / (seats * lf) / 365.0 + f.get("cargo_t", 0) / SE["surface_lifter"]["cargo_t"] / 365.0
        inter = 0.0
        if h["id"] == gateway:
            inter = sol_interstellar / (SE["liner"]["passengers"] * lf) / 365.0
        if h["id"] in interplanetary_hubs:
            inter += interplanetary_y * 0.6 / (SE["liner"]["passengers"] * lf) / 365.0 / len(interplanetary_hubs)
        h["hub_shuttle_flights_day"] = r3(hh_flights)
        h["interstellar_or_interplanetary_departures_day"] = r3(inter)
        mov = 2.0 * (h["surface_pax_flights_day"] + h["cargo_lifter_flights_day"] + h["service_flights_day"] + h["hub_shuttle_flights_day"]) + 2.0 * inter
        if mov < 1.0:
            mov = 6.0 if "science" in h["function"] else 12.0
        weighted_cycle = (D["pad_cycle_minutes"]["surface_lifter"] * (h["surface_pax_flights_day"] + h["service_flights_day"]) +
                          D["pad_cycle_minutes"]["cargo_lifter"] * h["cargo_lifter_flights_day"] +
                          D["pad_cycle_minutes"]["hub_shuttle"] * h["hub_shuttle_flights_day"] + D["pad_cycle_minutes"]["interplanetary"] * inter) / \
            max(1e-9, h["surface_pax_flights_day"] + h["service_flights_day"] + h["cargo_lifter_flights_day"] + h["hub_shuttle_flights_day"] + inter)
        if weighted_cycle <= 0:
            weighted_cycle = 45.0
        sectors, util, _ = dock_sizing(SN, mov, weighted_cycle)
        h["movements_day"] = round(mov)
        h["pad_cycle_min_mean"] = r3(weighted_cycle)
        h["dock_sectors"] = sectors
        h["pads_per_sector"] = D["pads_per_sector"]
        h["docks"] = sectors * D["pads_per_sector"]
        h["utilisation"] = r3(util)
        h["peak_utilisation"] = r3(util * D["peak_to_mean"])
        rho = min(0.99, util)
        h["queue_wait_min"] = r3(weighted_cycle * rho / (2.0 * (1.0 - rho)) / (D["pads_per_sector"] * sectors) ** 0.5)
        h["population"] = round(1500 + 9.0 * mov + (25000 if h["id"] == gateway else 0) + (8000 if "industry" in h["function"] else 0))
        h["period_min"] = r3(circ_period_s(h["altitude_km"]) / 60.0)
        h["speed_km_s"] = r3(circ_speed_km_s(h["altitude_km"]))
        h["arrivals_day"] = round(mov / 2.0)
    sep = min_separation(earth_hubs)
    # ---------------- other Sol hubs
    hubs = []
    for s in SN["hubs"]:
        r = s["region"]
        hubs.append(dict(s))
    by_region = {}
    for s in hubs:
        by_region.setdefault(s["region"], []).append(s)
    # region passenger and cargo flows (gravity over off-Earth population; Earth counted by its trip rate)
    routes = []
    ore_off = I.pol[SOL]["balance"]["ore"]["production"] * 0.6        # heavy mining moved off Earth by 3126 (PROPOSAL)
    mines_total = sum(assets[r].get("mine", 0) + assets[r].get("ice_mine", 0) for r in assets)
    gas_total = sum(assets[r].get("gas_platform", 0) for r in assets)
    for rid in SN["regions"]:
        main = next((h for h in by_region.get(rid, []) if h.get("driver") == "passenger"), None) or (by_region.get(rid) or [None])[0]
        if main is None:
            continue
        d_au = abs(region_anchor_au(SN, I, rid) - 1.0) + (0.0026 if rid == "earth_cislunar" else 0.5 * min(1.0, region_anchor_au(SN, I, rid)))
        pax = pops[rid] * TS["interplanetary_per_capita_off_earth_y"] * 2.0 if rid != "venus" else 2000.0
        mined = (assets[rid].get("mine", 0) + assets[rid].get("ice_mine", 0)) / max(1.0, mines_total) * ore_off * 0.25
        gas = assets[rid].get("gas_platform", 0) / max(1.0, gas_total) * I.pol[SOL]["balance"]["reactor_fuel"]["production"] * 50.0
        cls = "liner"
        days = brachistochrone_days(max(d_au, 0.0026), SE[cls]["burn_accel_g"]) if rid != "earth_cislunar" else 1.5
        dest = "earth_hub.kourou" if rid != "earth_cislunar" else "earth_orbital_hub"
        routes.append(dict(id=f"route.sol.pax.{rid}", type="passenger", origin=dest, destination=main["id"], pax_y=round(pax),
                           flights_day=r3(pax / (SE[cls]["passengers"] * lf) / 365.0), ship_class=cls, travel_days=r3(days),
                           distance_au=r3(d_au)))
        if mined > 0:
            cargo_hub = next((h for h in by_region.get(rid, []) if h.get("driver") == "mining"), main)
            fdays = brachistochrone_days(max(d_au, 0.0026), SE["medium_freighter"]["burn_accel_g"]) if rid != "earth_cislunar" else 2.5
            routes.append(dict(id=f"route.sol.cargo.{rid}", type="cargo", origin=cargo_hub["id"], destination="earth_hub.wenchang", cargo_t_y=round(mined),
                               flights_day=r3(mined / SE["medium_freighter"]["cargo_t"] / SE["medium_freighter"]["utilisation"] / 365.0),
                               ship_class="medium_freighter", travel_days=r3(fdays), distance_au=r3(d_au)))
        if gas > 0:
            gh = next((h for h in by_region.get(rid, []) if h.get("driver") == "gas"), main)
            gdays = brachistochrone_days(d_au, SE["gas_tanker"]["burn_accel_g"])
            routes.append(dict(id=f"route.sol.gas.{rid}", type="cargo", origin=gh["id"], destination="earth_hub.kourou", cargo_t_y=round(gas),
                               flights_day=r3(gas / SE["gas_tanker"]["cargo_t"] / 365.0), ship_class="gas_tanker", travel_days=r3(gdays), distance_au=r3(d_au)))
    # belt internal mining routes: Vesta / Pallas / Hygiea -> Ceres (ore tugs), Ceres -> Mars (metals)
    for sid, d in (("site.000.sol.vesta", 0.45), ("site.000.sol.pallas", 1.2), ("site.000.sol.hygiea", 0.5)):
        t = ore_off * 0.25 * 0.15
        routes.append(dict(id=f"route.sol.mining.{sid.split('.')[-1]}", type="mining", origin=sid, destination="site.000.sol.ceres", cargo_t_y=round(t),
                           flights_day=r3(t / SE["ore_tug"]["cargo_t"] / 365.0), ship_class="ore_tug", travel_days=r3(brachistochrone_days(d, SE["ore_tug"]["burn_accel_g"])),
                           distance_au=d))
    routes.append(dict(id="route.sol.cargo.ceres_mars", type="cargo", origin="site.000.sol.ceres", destination="site.000.sol.mars_phobos", cargo_t_y=round(ore_off * 0.25 * 0.2),
                       flights_day=r3(ore_off * 0.25 * 0.2 / SE["medium_freighter"]["cargo_t"] / 365.0), ship_class="medium_freighter",
                       travel_days=r3(brachistochrone_days(1.25, SE["medium_freighter"]["burn_accel_g"])), distance_au=1.25))
    # Earth hub <-> hub routes (persistent ids; aggregate)
    for (a, b), f in sorted(hub_hub.items()):
        pax = f.get("transfer", 0) + f.get("interstellar", 0) + f.get("interplanetary", 0) + f.get("service", 0)
        typ = "shuttle" if pax else "cargo"
        routes.append(dict(id=f"route.earth.{a.split('.')[-1]}--{b.split('.')[-1]}", type=typ, origin=a, destination=b, pax_y=round(pax),
                           cargo_t_y=round(f.get("cargo_t", 0)), flights_day=r3(pax / (seats * lf) / 365.0 + f.get("cargo_t", 0) / SE["surface_lifter"]["cargo_t"] / 365.0),
                           ship_class="surface_lifter", travel_days=r3(LEO_SHUTTLE_MIN / 1440.0), mix={k: round(v) for k, v in f.items()},
                           transfer="orbital_transfer"))
    # Moon local: Selena <-> Shackleton / Imbrium surface; EML1 <-> Selena
    for a, b, p in (("site.000.sol.luna_orbit", "site.000.sol.shackleton", 0.5), ("site.000.sol.luna_orbit", "site.000.sol.imbrium", 0.2),
                    ("site.000.sol.eml1", "site.000.sol.luna_orbit", 0.8)):
        pax = pops["earth_cislunar"] * 0.6 * p
        routes.append(dict(id=f"route.luna.{a.split('.')[-1]}--{b.split('.')[-1]}", type="shuttle", origin=a, destination=b, pax_y=round(pax),
                           flights_day=r3(pax / (seats * lf) / 365.0), ship_class="surface_lifter", travel_days=0.2 if "eml1" not in a else 1.0))
    for r in routes:
        r["disruption"] = "none"
        r["hazard"] = "none"
        r["status"] = TAG
    # per-site traffic and sizing for the other Sol hubs
    flights = {}
    for r in routes:
        for end in (r["origin"], r["destination"]):
            flights[end] = flights.get(end, 0.0) + r["flights_day"]
    for s in hubs:
        r = s["region"]
        share = {"passenger": 0.55, "mining": 0.25, "gas": 0.15, "industry": 0.2, "science": 0.004, "military": 0.02, "quarantine": 0.02,
                 "piracy": 0.003, "outer": 0.3}[s["driver"]]
        res = pops[r] * share / max(1, sum(1 for x in by_region[r] if x["driver"] == s["driver"])) if s["driver"] in ("passenger", "outer") else 0.0
        staff = {"mining": 3500, "gas": 2200, "industry": 9000, "science": 600, "military": 5000, "quarantine": 900, "piracy": 675, "passenger": 4000, "outer": 1500}[s["driver"]]
        if s["type"] in ("planetary_surface_port",):
            pop = res if res else pops[r] * 0.1
        else:
            pop = staff + 0.004 * res
        mov = 2.0 * flights.get(s["id"], 0.0) + {"military": 30, "science": 4, "quarantine": 10, "piracy": 6}.get(s["driver"], 8)
        cyc = D["pad_cycle_minutes"]["interplanetary"] if s["driver"] in ("mining", "gas") else D["pad_cycle_minutes"]["surface_lifter"]
        sectors, util, _ = dock_sizing(SN, mov, cyc)
        s.update(population=round(pop), movements_day=round(mov), arrivals_day=round(mov / 2.0), dock_sectors=sectors, pads_per_sector=D["pads_per_sector"],
                 docks=sectors * D["pads_per_sector"], utilisation=r3(util))
    # ---------------- relays (real world objects)
    R = SN["relays"]
    relays = []
    for rid, names in R["names_by_region"].items():
        n_hubs = len(by_region.get(rid, [])) + (len(earth_hubs) if rid == "earth_cislunar" else 0)
        need = 2 if rid == "transit" else min(len(names), 2 + n_hubs // 6) if (pops.get(rid) or rid == "venus") else 1
        for i, nm in enumerate(names[:need]):
            relays.append(dict(id=f"relay.sol.{rid}.{i + 1}", name=nm, region=rid, kind="nav_relay",
                               anchor={"transit": "lagrange:Солнце-Земля:L4" if i == 0 else "lagrange:Солнце-Земля:L5"}.get(rid) if rid == "transit" else None,
                               reason="резерв района" if i < 2 else "покрытие плотного узла", status=TAG))
    # ---------------- checks
    chk = []

    def add(name, ok, detail):
        chk.append(dict(check=name, ok=bool(ok), detail=detail))
    add("earth_hub_count_is_author_target", len(earth_hubs) == SN["earth_hub_count_target"]["value"], f"{len(earth_hubs)} hubs")
    ids = [h["id"] for h in earth_hubs] + [h["id"] for h in hubs]
    add("unique_sol_hub_ids", len(ids) == len(set(ids)), f"{len(ids)} ids")
    names = [h["name"] for h in earth_hubs] + [h["name"] for h in hubs]
    add("unique_sol_hub_names", len(names) == len(set(names)), f"{len(names)} names")
    add("earth_hubs_orbital_separation", sep[0] > 20.0, f"min {round(sep[0])} km between {sep[1]} over 2 days")
    add("earth_hubs_above_atmosphere", all(h["altitude_km"] >= 400 for h in earth_hubs), "all >= 400 km")
    cities_ok = [c for c in earth_rows if c.get("spaceport") and not any(h["inclination_deg"] + SN["surface_traffic"]["dogleg_tolerance_deg"] >= abs(c["surface"]["lat_deg"])
                                                                           for h in earth_hubs if "passenger" in h["function"])]
    add("every_spaceport_reaches_a_passenger_plane", not cities_ok, ",".join(c["name"] for c in cities_ok) or "ok")
    cap_legs = 0.0
    for h in earth_hubs:
        if h["id"] in load:
            cap_legs += h["dock_sectors"] * D["pads_per_sector"] * D["operating_hours_per_day"] * 60.0 / h["pad_cycle_min_mean"] * \
                h["surface_pax_flights_day"] / max(1e-9, h["movements_day"] / 2.0) * seats * lf * 365.0 / 2.0
    add("earth_demand_within_orbital_capacity", legs_y <= cap_legs * 0.8, f"demand {round(legs_y):,} legs/y <= 80 % of capacity {round(cap_legs):,}".replace(",", " "))
    major = [h for h in earth_hubs if h["movements_day"] >= 500]
    band = D["target_utilisation"]
    bad = [f"{h['name']} {h['utilisation']}" for h in major if not (band[0] - 0.1 <= h["utilisation"] <= band[1])]
    add("major_hub_utilisation_in_band", not bad, ", ".join(bad) or f"{len(major)} major hubs within {band[0] - 0.1}-{band[1]}")
    alloc_sum = sum(load.values())
    add("surface_traffic_conserved", abs(alloc_sum - legs_y) / legs_y < 1e-6, f"{round(alloc_sum):,} of {round(legs_y):,}".replace(",", " "))
    known = set(ids) | {r["id"] for r in relays}
    badr = [r["id"] for r in routes if r["origin"] not in known or r["destination"] not in known or r["origin"] == r["destination"]]
    add("routes_have_real_ends", not badr, ",".join(badr) or f"{len(routes)} routes")
    serves = {"passenger": {"passenger"}, "mining": {"mining", "ice"}, "gas": {"gas"}, "science": {"science"},
              "industry": {"industry", "shipyard", "metallurgy", "chemicals"}, "military": {"defence"}}
    gaps = []
    for r, q in req.items():
        for drv, n in q.items():
            if n and not any(serves[drv] & set(s["function"]) for s in hubs if s["region"] == r):
                gaps.append(f"{r}:{drv}")
    add("every_driver_has_a_hub", not gaps, ", ".join(gaps) or "ok")
    idle = [s["name"] for s in hubs + earth_hubs if s["arrivals_day"] < 1]
    add("every_hub_has_traffic", not idle, ", ".join(idle) or "ok")
    legacy = [x for s in hubs for x in s.get("legacy_ids", [])]
    add("legacy_generic_ids_mapped_once", len(legacy) == len(set(legacy)), f"{len(legacy)} legacy ids")
    return dict(earth_hubs=earth_hubs, hubs=hubs, routes=routes, relays=relays, belt_anchors=SN["belt_anchors"], regions=SN["regions"],
                region_population=pops, region_assets={r: {k: r3(v) for k, v in a.items()} for r, a in assets.items()}, required_hubs=req,
                surface=dict(legs_y=round(legs_y), legs_day=round(legs_y / 365.0), lifter_seats=seats, load_factor=lf,
                             lifter_flights_day=round(legs_y / (seats * lf) / 365.0), allocation={k: round(v) for k, v in load.items()}, **alloc_note,
                             by_city={c: {h: round(v) for h, v in m.items()} for c, m in per_city.items()}),
                interstellar_pax_y=round(sol_interstellar), interplanetary_pax_y=round(interplanetary_y),
                min_separation_km=round(sep[0]), min_separation_pair=sep[1], dock_model=D, checks=chk, status=TAG)


# ---------------------------------------------------------------- runtime exports
def runtime_hubs(net, template):
    """initial_world_state.orbital_hubs: the 12 Earth hubs + the Ceres hub (one station ModelAsset family, many instances)."""
    out = []
    base_mod = template["modules"][0]
    for h in net["earth_hubs"]:
        mod_id = "earth_high_orbital" if h["id"] == "earth_orbital_hub" else h["id"].replace("earth_hub.", "") + "_station"
        out.append(dict(id=h["id"], name=h["name"], owner="Sol Authority", operator=h["operator"], role=h["role_ru"], system_id=0,
                        parent_body_id="system_0.Sol.Земля", map_object_module_id=mod_id,
                        model_family="command_station",
                        traffic_profile=dict(profile=h["profile"], arrivals_day=h["arrivals_day"], dock_sectors=h["dock_sectors"],
                                             surface_pax_flights_day=h["surface_pax_flights_day"], hub_shuttle_flights_day=h["hub_shuttle_flights_day"]),
                        motion=dict(type="parent_orbit", altitude_km=h["altitude_km"], orbital_period_seconds=0,
                                    self_rotation_period_seconds=180, inclination_deg=h["inclination_deg"], longitude_of_ascending_node_deg=h["lan_deg"],
                                    argument_of_periapsis_deg=0, initial_phase_deg=h["phase_deg"], epoch_seconds=0, orbital_period_mode="kepler"),
                        modules=[dict(base_mod, id=mod_id, name=h["name"])]))
    ce = next(s for s in net["hubs"] if s["id"] == "site.000.sol.ceres")
    anc = next(a for a in net["belt_anchors"] if a["id"] == ce["belt_anchor"])
    lan, inc, phase = heliocentric_motion(anc)
    out.append(dict(id=ce["id"], name=ce["name"], owner="Sol Authority", operator=ce["operator"], role=ce["role_ru"], system_id=0,
                    parent_body_id="system_0.Sol", map_object_module_id="ceres_station", model_family="command_station",
                    traffic_profile=dict(profile="mining", arrivals_day=ce["arrivals_day"], dock_sectors=ce["dock_sectors"]),
                    motion=dict(type="parent_orbit", altitude_km=round(anc["a_au"] * AU_KM - R_SUN), orbital_period_seconds=0,
                                self_rotation_period_seconds=240, inclination_deg=r3(inc), longitude_of_ascending_node_deg=r3(lan),
                                argument_of_periapsis_deg=0, initial_phase_deg=r3(phase), epoch_seconds=0, orbital_period_mode="kepler"),
                    modules=[dict(base_mod, id="ceres_station", name=ce["name"])],
                    note="co-orbits the Ceres anchor on its circular approximation (e omitted); see canon/sol_network.json belt_anchor_note"))
    return out


def anchor_engine_pos(anc, M_deg):
    """Engine frame of the catalog (keplerPerifocalToEngine): standard (x, y, z_north) -> engine (x, z, -y); circular (e omitted)."""
    O, i, w = (math.radians(anc[k]) for k in ("node_deg", "i_deg", "peri_deg"))
    u = w + math.radians(M_deg)
    x = math.cos(O) * math.cos(u) - math.sin(O) * math.sin(u) * math.cos(i)
    y = math.sin(O) * math.cos(u) + math.cos(O) * math.sin(u) * math.cos(i)
    z = math.sin(u) * math.sin(i)
    return (x, z, -y)


def orbital_motion_pos(lan, inc, phase_deg):
    """OrbitalMotion convention: R_y(lan) R_x(inc) (cos p, 0, sin p) (src/world/orbits/OrbitalMotion.h)."""
    p = math.radians(phase_deg)
    v = (math.cos(p), 0.0, math.sin(p))
    i = math.radians(inc)
    v = (v[0], v[1] * math.cos(i) - v[2] * math.sin(i), v[1] * math.sin(i) + v[2] * math.cos(i))
    L = math.radians(lan)
    return (v[0] * math.cos(L) + v[2] * math.sin(L), v[1], -v[0] * math.sin(L) + v[2] * math.cos(L))


def heliocentric_motion(anc):
    """Find OrbitalMotion (lan, inclination, phase) that traces the anchor's circular orbit in the same direction."""
    p0 = anchor_engine_pos(anc, anc["mean_anomaly_epoch_deg"])
    p1 = anchor_engine_pos(anc, anc["mean_anomaly_epoch_deg"] + 1.0)
    best = None
    for lan10 in range(0, 3600, 5):
        for inc10 in range(0, 1800, 5):
            lan, inc = lan10 / 10.0, inc10 / 10.0
            # phase that matches p0 best, then check direction with p1
            for ph10 in range(0, 3600, 20):
                q0 = orbital_motion_pos(lan, inc, ph10 / 10.0)
                d0 = math.dist(q0, p0)
                if d0 > 0.08:
                    continue
                q1 = orbital_motion_pos(lan, inc, ph10 / 10.0 + 1.0)
                err = d0 + math.dist(q1, p1)
                if best is None or err < best[0]:
                    best = (err, lan, inc, ph10 / 10.0)
    _, lan, inc, ph = best
    for step in (0.1, 0.01, 0.001):    # local refinement
        improved = True
        while improved:
            improved = False
            for dl, di, dp in ((step, 0, 0), (-step, 0, 0), (0, step, 0), (0, -step, 0), (0, 0, step), (0, 0, -step)):
                c = (lan + dl, inc + di, ph + dp)
                e = math.dist(orbital_motion_pos(*c), p0) + math.dist(orbital_motion_pos(c[0], c[1], c[2] + 1.0), p1)
                if e < best[0] - 1e-12:
                    best = (e, *c)
                    lan, inc, ph = c
                    improved = True
    return lan % 360.0, inc, ph % 360.0


if __name__ == "__main__":
    import world_sites as ws
    I = ws.Inputs()
    net = compute(I, ws.earth_settlements(I), ws.passengers(I, ws.route_graph(I)))
    for h in net["earth_hubs"]:
        print(f"{h['name']:12s} i={h['inclination_deg']:6.2f} alt={h['altitude_km']:6d} legs/y={h['surface_legs_y']:>12,} mov/d={h['movements_day']:>6} "
              f"sect={h['dock_sectors']:3d} util={h['utilisation']:.2f} pop={h['population']:>7,}")
    for c in net["checks"]:
        print("OK  " if c["ok"] else "FAIL", c["check"], "|", c["detail"])
