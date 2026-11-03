"""The World's Ass (月工门) economy: causal model of the Lotus (NIRVANA-1) region.

Called by world_sites.py. Everything follows from canon/world_sites.json ->
lotus (Developer World Truth + parameters) and the physical catalog:

  finite primary stock (crew cohorts x exposure x transformation x recovery x
  decay - removed) -> demand (medical from the income tail of every polity,
  research budgets, phase stabilisers of the ships that really work in
  anomalies) -> free entry of raid segments on rumoured prices (corridor maps,
  cordon, crowding of the hulk) -> supply and price -> trafficking from
  incentives -> quarantine capacity vs traffic -> corruption pressure by role
  and zone -> blockade strictness sweep (non-linear) -> lotus fever outbreaks.

Lotusite is not fuel and does not extend the normal jump range (author,
fourth pass): the stabiliser only holds the jump field in anomalous space.

Numbers are outputs, never inputs: nothing here is fixed by hand except the
parameters in the canon block.
"""
import math

TAG = "PROPOSAL"


def r3(x):
    return round(x, 3)


def clamp(x, lo=0.0, hi=1.0):
    return max(lo, min(hi, x))


# ---------------------------------------------------------------- primary stock
def primary_stock(L, year=3126):
    M = L["material"]
    crew = L["truth"]["crew"]
    rows = []
    total_pot = 0.0
    for c in crew["cohorts"]:
        died = sum(c["died"]) / 2.0
        alive = c["alive_in_field_years"]
        pre = M["pre_exposure_years"] * M["pre_exposure_intensity"]       # weak shell exposure since 2450
        t_eff = alive + pre
        frac = 1.0 - math.exp(-t_eff / M["tau_years"])
        post = M["post_mortem_growth"] * (1.0 - math.exp(-(year - died) / 150.0))
        per_body = M["potential_g_fully_transformed_body"] * frac * (1.0 + post)
        decay = 0.5 ** ((year - died) / M["half_life_years_in_field"])
        g = c["people"] * per_body * decay
        total_pot += g
        rows.append(dict(cohort=f"{c['died'][0]}-{c['died'][1]}", people=c["people"], exposure_years=r3(t_eff), transformed_share=r3(frac),
                         g_per_body=r3(per_body * decay), potential_g=r3(g)))
    people = sum(c["people"] for c in crew["cohorts"]) + crew.get("born_after", 0)
    mean_body = total_pot / max(1, sum(c["people"] for c in crew["cohorts"]))
    total_pot += crew.get("born_after", 0) * mean_body * 0.6
    recoverable = total_pot * M["recoverable_fraction"]
    removed = M["removed_bodies_by_3126"]
    removed_g = removed * mean_body * M["recoverable_fraction"]
    return dict(bodies=people, cohorts=rows, mean_g_per_body=r3(mean_body), potential_g=r3(total_pot), recoverable_g_total=r3(recoverable),
                removed_bodies=removed, removed_recoverable_g=r3(removed_g), bodies_left=people - removed,
                recoverable_g_left=r3(recoverable - removed_g), upper_estimate=True,
                note="верхняя оценка: все 2 160 тел, полная экспозиция по когортам, извлекаемая доля 35 %, распад в поле; вне поля сырьё распадается быстрее (40 лет)")


# ---------------------------------------------------------------- demand and price
def stabiliser_fleet(L, zones_by_kind, legal_bodies_y):
    """Ships that really need a phase stabiliser: cordon units in the field, SAR, research, anomaly patrols, licensed expeditions."""
    S = L["phase_stabilizer"]
    n_zones = sum(zones_by_kind.get(k, 0) for k in S["zone_kinds"])
    rows = []
    for u in S["users"]:
        src = u.get("ships_from")
        if src == "security_units":
            for su in L["security_units"]:
                if su["works_in_field"] and su["mandate"] != "SAR":
                    rows.append(dict(user=u["id"], who=su["name"], ships=su["ships"], ship_class=su["ship_class"], exposure_days_y=u["exposure_days_y"]))
        elif src == "security_units:SAR":
            n = sum(su["ships"] for su in L["security_units"] if su["mandate"] == "SAR") + u.get("extra_ships", 0)
            rows.append(dict(user=u["id"], who=u["who"], ships=n, ship_class=u["class"], exposure_days_y=u["exposure_days_y"]))
        elif "ships_per_zone" in u:
            rows.append(dict(user=u["id"], who=u["who"], ships=r3(u["ships_per_zone"] * n_zones), ship_class=u["class"], exposure_days_y=u["exposure_days_y"]))
        else:
            rows.append(dict(user=u["id"], who=u["who"], ships=r3(u["ships_per_legal_body"] * legal_bodies_y), ship_class=u["class"],
                             exposure_days_y=u["exposure_days_y"]))
    for r in rows:
        r["g_per_ship"] = S["g_per_unit"][S["class_size"].get(r["ship_class"], "small")]
        r["installed_g"] = r3(r["ships"] * r["g_per_ship"])
        wear = 1.0 - (1.0 - S["degradation_per_heavy_exposure_day"]) ** r["exposure_days_y"]
        r["wear_share_y"] = r3(wear)
        r["replacement_g_y"] = r3(r["installed_g"] * (wear + 1.0 / S["service_life_years_fleet_turnover"]))
    return dict(zones=n_zones, rows=rows, installed_g=r3(sum(r["installed_g"] for r in rows)),
                replacement_g_y=r3(sum(r["replacement_g_y"] for r in rows)))


def stabiliser_adoption(L, fleet, price, class_price):
    """A ship is fitted if the hull losses it avoids outweigh the insert's wear, turnover and recalibration."""
    S = L["phase_stabilizer"]
    U = S["effects"]["unstable_navigation"]
    d_loss_per_jump = U["abort_or_misjump_per_jump_without"] - U["abort_or_misjump_per_jump_with"]
    out = []
    for r in fleet["rows"]:
        hull = class_price.get(r["ship_class"], 3.0e6)
        jumps_y = r["exposure_days_y"] / 10.0                    # a jump into or out of disturbed space every ~10 exposure days
        avoided = jumps_y * d_loss_per_jump * 0.25 * hull           # a quarter of misjumps end the hull
        cost_y = r["g_per_ship"] * price * (r["wear_share_y"] + 1.0 / S["service_life_years_fleet_turnover"]) + \
            S["recalibration_cost_cr"] * r["exposure_days_y"] / S["recalibration_every_exposure_days"]
        out.append(dict(r, avoided_loss_cr_y=round(avoided), cost_cr_y=round(cost_y), fitted=avoided >= cost_y))
    return out


def medical_demand_g(L, p, pols):
    """Buyers = Pareto income tail of every polity that can spend wtp share of a year's income on a course, times access."""
    M = L["demand"]["medical"]
    course = L["medical"]["course_g"]
    y_star = p * course / M["wtp_income_share"]
    buyers = 0.0
    rows = {}
    for k, v in pols.items():
        y_top = M["top_income_multiple_of_gdp_pc"] * v["gdp_pc"]
        share = M["top_share"] * ((y_star / y_top) ** (-M["pareto_alpha"]) if y_star > y_top else 1.0)
        access = M["access_base"] * math.exp(-v["ly_from_origin"] / M["access_reach_ly"])
        rows[k] = v["pop"] * share * access
        buyers += rows[k]
    return buyers / M["repeat_years"] * course, buyers, rows


def research_demand_g(L, p):
    return sum(L["demand"]["research"]["budgets_cr_y"].values()) / p


def demand_g(L, p, ctx):
    fit = stabiliser_adoption(L, ctx["fleet"], p, ctx["class_price"])
    return medical_demand_g(L, p, ctx["pols"])[0], research_demand_g(L, p), sum(r["replacement_g_y"] for r in fit if r["fitted"])


def clearing_price(L, q, ctx):
    lo, hi = 1e4, 1e11
    for _ in range(120):
        mid = math.sqrt(lo * hi)
        if sum(demand_g(L, mid, ctx)) > q:
            lo = mid
        else:
            hi = mid
    return math.sqrt(lo * hi)


# ---------------------------------------------------------------- raids: free entry on rumoured prices
SEGMENTS = [
    dict(id="naive", ru="наивные (без карты, без связей)", corridor="unknown", bribe=0.0, sell="inner_anomaly", haul=1.2),
    dict(id="informed", ru="с купленной картой (часть карт ложные)", corridor="estimated", bribe=0.0, sell="grey_frontier", haul=2.0),
    dict(id="connected", ru="со связями (свежая карта, взятка, своя лаборатория)", corridor="known_safe", bribe=1.0, sell="outer_perimeter", haul=3.0),
    dict(id="cover_story", ru="старатели «минерала» у планеты c (легенда)", corridor=None, bribe=0.0, sell=None, haul=0.0),
]


def _p_loss(L, corridor):
    C = L["corridor"]
    if corridor == "estimated":
        return (1 - C["fake_share"]) * C["states"]["estimated"]["p_loss"] + C["fake_share"] * C["states"]["unknown"]["p_loss"]
    return C["states"][corridor]["p_loss"]


def raid_economics(L, stock, price, hull, run_cost, n_by_seg, strict):
    """Realised economics of every segment at the real tier prices, and what each segment believes before it flies."""
    R = L["runs"]
    C = L["corridor"]
    tiers = L["markets"]["tier_share_of_civilised_price"]
    left = stock["bodies_left"] / max(1, stock["bodies"])
    n_tot = sum(n_by_seg.values())
    # the hulk gives ~cap bodies a year; whoever knows the open compartments gets there first (connected -> informed -> naive)
    room = R["hulk_extraction_capacity_bodies_y"] * math.sqrt(left)
    crowd = {}
    for sid in ("connected", "informed", "naive"):
        want = n_by_seg[sid] * next(sg["haul"] for sg in SEGMENTS if sg["id"] == sid)
        crowd[sid] = 1.0 if want <= room else room / want
        room = max(0.0, room - want)
    p_caught_base = 0.45 * strict * min(1.0, R["cordon_capacity_runs"] / max(1.0, n_tot))
    g_body = stock["mean_g_per_body"] * L["material"]["recoverable_fraction"]
    rows = []
    for sg in SEGMENTS:
        n = n_by_seg[sg["id"]]
        if sg["corridor"] is None:
            ev = -(run_cost + 30000) - 0.06 * 0.6 * hull
            rows.append(dict(segment=sg["id"], ru=sg["ru"], runs_y=r3(n), p_ship_lost=0.06, p_caught=0.0, bodies=0.0, price_per_body=0, ev_cr=round(ev),
                             perceived_ev_cr=None, stage1_risk=0.0))
            continue
        p_loss = _p_loss(L, sg["corridor"])
        map_cost = C["map_price_cr"].get(sg["corridor"], 0)
        per_body = tiers[sg["sell"]] * price * g_body
        bribe_cr = sg["bribe"] * 0.12 * sg["haul"] * per_body      # the picket's price follows the value of the cargo
        p_caught = p_caught_base * (0.3 if sg["bribe"] else 1.0)
        bodies = sg["haul"] * crowd[sg["id"]]
        ok = (1 - p_loss) * (1 - p_caught)
        losses = p_loss * 0.6 * hull + (1 - p_loss) * p_caught * (250000 + 0.3 * 0.6 * hull)
        ev = ok * bodies * per_body - run_cost - map_cost - bribe_cr - losses
        bel = R["segment_perception"][sg["id"]]
        b_loss = _p_loss(L, bel["believed_corridor"])
        b_body = tiers[bel["believed_tier"]] * bel.get("believed_discount", 1.0) * price * g_body
        learn = bel.get("learning", 1.0)                      # rumour of the full haul vs what returning crews tell
        b_bodies = sg["haul"] * ((1 - learn) + learn * crowd[sg["id"]])
        perceived = (1 - b_loss) * (1 - p_caught) * b_bodies * b_body - run_cost - map_cost - bribe_cr - b_loss * 0.6 * hull
        rows.append(dict(segment=sg["id"], ru=sg["ru"], runs_y=r3(n), corridor=sg["corridor"], p_ship_lost=r3(p_loss), p_caught=r3(p_caught),
                         bodies=r3(ok * bodies), price_per_body=round(per_body), bribe_cr=round(bribe_cr), map_cr=map_cost, ev_cr=round(ev),
                         perceived_ev_cr=round(perceived), stage1_risk=0.13))
    avg = sum(r["runs_y"] * r["ev_cr"] for r in rows) / max(1e-9, n_tot)
    return rows, avg


def entry(L, stock, price, hull, run_cost, strict):
    """Each segment enters while the economics it BELIEVES stay above -optimism x run cost (crowding of the hulk closes entry);
    connected crews are capped by the pickets and labs that take them; cover-story prospectors follow the naive rumour."""
    R = L["runs"]
    floor = -R["optimism_bias"] * run_cost
    n = dict(naive=0.0, informed=0.0, connected=0.0, cover_story=0.0)

    def perceived(sid, x):
        m = dict(n, **{sid: x})
        m["cover_story"] = R["cover_story_per_naive"] * m["naive"]
        rows, _ = raid_economics(L, stock, price, hull, run_cost, m, strict)
        return next(r for r in rows if r["segment"] == sid)["perceived_ev_cr"]
    for _ in range(40):
        before = dict(n)
        for sid in ("connected", "informed", "naive"):
            hi = R["connected_capacity_runs_y"] if sid == "connected" else 3000.0
            if perceived(sid, hi) >= floor:
                n[sid] = hi
            elif perceived(sid, 1e-6) < floor:
                n[sid] = 0.0
            else:
                lo = 0.0
                for _ in range(50):
                    mid = (lo + hi) / 2
                    if perceived(sid, mid) >= floor:
                        lo = mid
                    else:
                        hi = mid
                n[sid] = (lo + hi) / 2
        n["cover_story"] = R["cover_story_per_naive"] * n["naive"]
        if all(abs(n[k] - before[k]) < 1e-4 for k in n):
            break
    return n


def equilibrium(L, stock, hull, run_cost, strict, ctx):
    """Price clears supply (raids + legal recovery + trafficking) against demand; raids react to price through entry."""
    legal_bodies = 15.0
    g_body = stock["mean_g_per_body"] * L["material"]["recoverable_fraction"]
    price = sum(L["demand"]["target_price_cr_per_g"]) / 2.0
    for _ in range(100):
        traf = trafficking(L, price)
        n = entry(L, stock, price, hull, run_cost, strict)
        rows, avg = raid_economics(L, stock, price, hull, run_cost, n, strict)
        bodies = sum(r["runs_y"] * r["bodies"] for r in rows)
        q = (bodies + legal_bodies) * g_body + traf["g_y"]
        new_price = clearing_price(L, max(q, 1e-6), ctx)
        if abs(new_price / price - 1) < 1e-5:
            break
        price = math.sqrt(price * new_price)
    return dict(runs_y=sum(n.values()), runs_by_segment={k: r3(v) for k, v in n.items()}, rows=rows, avg_ev=avg, bodies_y=bodies,
                legal_bodies_y=legal_bodies, refined_g_y=q, price=price, trafficking=traf)


# ---------------------------------------------------------------- trafficking from incentives
def trafficking(L, price, garden_pop=180):
    T = L["trafficking"]
    capacity = garden_pop / T["exposure_years"] * 1.2
    g = L["material"]["potential_g_fully_transformed_body"] * (1 - math.exp(-T["exposure_years"] / L["material"]["tau_years"])) * \
        L["material"]["recoverable_fraction"] * T["useful_yield_share"]
    # organised networks with captured officials sell through their own labs and clinics, close to retail
    revenue = T["survival_to_harvest"] * g * price * L["markets"][T["sells_at"]]
    margin = revenue / T["cost_per_victim_cr"]
    victims = capacity * clamp((margin - 1.0) / margin) if margin > 1 else 0.0
    return dict(victims_y=r3(victims), capacity_y=r3(capacity), g_per_victim=r3(T["survival_to_harvest"] * g), margin=r3(margin),
                g_y=r3(victims * T["survival_to_harvest"] * g), revenue_per_victim_cr=round(revenue),
                note="объём следует из выгоды (маржа) и ёмкости преступных сетей, а не задан фракцией")


# ---------------------------------------------------------------- quarantine capacity
def quarantine_capacity(L, runs_y, legit_ships_y):
    Q = L["quarantine"]
    out = {}
    for site, res in Q["resources"].items():
        insp = Q["inspectors"].get(site, 0)
        per_day = insp * 8 * 60 / Q["inspection_minutes_per_ship"] * 0.8
        share = {"site.041.holding": 1.0, "dso.quarantine_picket": 0.6, "dso.far_berth": 0.0}[site]
        traffic_day = (legit_ships_y + runs_y) * share / 365.0
        susp = traffic_day * Q["suspect_share_of_traffic"]
        holding_need = susp * Q["hold_days"]["suspect"] * 0.8 + susp * 0.1 * Q["hold_days"]["contaminated"]
        out[site] = dict(inspections_per_day=r3(per_day), arrivals_per_day=r3(traffic_day), inspection_utilisation=r3(traffic_day / per_day) if per_day else None,
                         holding_slots=res["holding_berths"] + res["isolated_anchorage"], holding_needed=r3(holding_need),
                         holding_utilisation=r3(holding_need / max(1, res["holding_berths"] + res["isolated_anchorage"])), resources=res)
    return out


# ---------------------------------------------------------------- corruption pressure
ROLES = [  # role, illicit approaches per year, share of the cargo value a bribe buys, legal salary Cr/y
    ("досмотровый инспектор", 6, 0.01, 60000),
    ("офицер изъятия", 4, 0.05, 80000),
    ("клерк навигационных данных", 20, 0.002, 45000),
    ("лаборант-аналитик", 5, 0.01, 55000),
    ("командир патруля", 3, 0.03, 120000),
    ("выдача карантинных справок", 10, 0.005, 50000),
]


def corruption_pressure(L, price, g_body):
    cargo = 3.0 * g_body * price * L["markets"]["tier_share_of_civilised_price"]["outer_perimeter"]     # one connected haul
    out = {}
    for zid, z in L["zones"].items():
        rows = []
        for role, opp, share, salary in ROLES:
            illicit = opp * share * cargo
            frag = 1.0 + 0.15 * (z["fragmentation"] - 1)
            pressure = illicit / salary * (1.0 - z["audit"]) * z["isolation"] * frag
            corrupt = pressure / (1.0 + pressure)
            rows.append(dict(role=role, illicit_cr_y=round(illicit), legal_salary_cr_y=salary, pressure=r3(pressure), expected_corrupt_share=r3(corrupt),
                             cost_of_honesty_cr_y=round(illicit * (1.0 - z["audit"]))))
        out[zid] = rows
    return dict(formula="pressure = illicit income per year / legal salary per year x (1 - audit) x isolation x fragmentation; corrupt share = p / (1 + p)",
                cargo_cr=round(cargo), by_zone=out,
                note="не все продажны: доля — ожидание по роли; честные офицеры, врачи и следователи есть, но ближе к полю честность стоит дороже")


def blockade_sweep(L, base_runs, price, p0):
    """Non-linear: stricter control raises the black premium and with it the bribe a controller is offered;
    leakage follows the same corruption pressure as the inspectors (p0 = outer-perimeter inspector now)."""
    rows = []
    s0 = L["cordon"]["strictness"]
    prem0 = 1.0 + 2.0 * s0 * s0
    for s in (0.2, 0.4, 0.6, 0.8, 1.0):
        premium = 1.0 + 2.0 * s * s
        attempts = base_runs * (0.6 + 0.4 * premium)
        pressure = p0 * (premium / prem0) ** 2 * (s / s0)
        leak = pressure / (1.0 + pressure)
        interdicted = attempts * s * 0.7 * (1 - leak)
        rows.append(dict(strictness=s, black_premium=r3(premium), attempts_y=r3(attempts), leakage=r3(leak), interdicted_y=r3(interdicted),
                         smuggled_y=r3(attempts - interdicted)))
    best = min(rows, key=lambda r: r["smuggled_y"])
    return dict(rows=rows, least_smuggling_at=best["strictness"], actual=L["cordon"]["strictness"],
                note="жёстче контроль ≠ меньше контрабанды: премия и взятки растут, контролёров покупают")


# ---------------------------------------------------------------- lotus fever outbreaks
def outbreaks(L):
    F = L["fever"]
    out = []
    for o in L["outbreaks"]:
        cases = 0.0
        for kind, n in o["exposed"].items():
            ar = F["attack_rate"][kind]
            if o["repeat_exposure_years"]:
                ar = 1 - (1 - ar) ** (1 + o["repeat_exposure_years"])
            cases += n * ar
        treated = min(cases, o["beds"] * 12.0)        # a bed turns over ~12 times in an outbreak window
        cfr = (treated * F["cfr_treated"] + (cases - treated) * F["cfr_untreated"]) / max(cases, 1.0)
        out.append(dict(id=o["id"], site=o["site"], year=o["year"], population=o["population"], exposed=sum(o["exposed"].values()), cases=round(cases),
                        deaths=round(cases * cfr), beds=o["beds"], treated=round(treated), cfr=r3(cfr), cause_chain=o["cause_chain"], consequence=o["consequence"], status=TAG))
    return out


# ---------------------------------------------------------------- stabiliser politics (no map effect)
def politics(fit):
    fitted = [r for r in fit if r["fitted"]]
    return ["обычная дальность прыжка не меняется: новых маячных рёбер и новых достижимых систем нет (+3 св. года отклонено автором)",
            "Когниум — единственный производитель вставок и переотжига: решает, чьи SAR, наука и патрули могут работать в аномалиях",
            f"стабилизаторы стоят на {round(sum(r['ships'] for r in fitted), 1)} кораблях; без них кордон не досматривает внутри поля, а SAR не вытаскивает застрявших",
            "у рейдеров вставок почти нет (несовместимые ядра, учёт Когниума): поэтому они гибнут чаще кордона"]


def compute(L, pols, class_price, zones_by_kind, hull, run_cost):
    """pols: {polity: dict(pop, gdp_pc, ly_from_origin)}; class_price: list price per ship class; zones_by_kind: counted hazard zones."""
    stock = primary_stock(L)
    ctx = dict(pols=pols, class_price=class_price, fleet=stabiliser_fleet(L, zones_by_kind, 15.0))
    eq = equilibrium(L, stock, hull, run_cost, L["cordon"]["strictness"], ctx)
    price = eq["price"]
    med, res, tech = demand_g(L, price, ctx)
    _, buyers, by_pol = medical_demand_g(L, price, pols)
    fit = stabiliser_adoption(L, ctx["fleet"], price, class_price)
    qc = quarantine_capacity(L, eq["runs_y"], 260)
    ob = outbreaks(L)
    S = L["phase_stabilizer"]
    n_runs = max(1e-9, eq["runs_y"])
    p_loss = sum(r["runs_y"] * r["p_ship_lost"] for r in eq["rows"]) / n_runs
    years_left = stock["recoverable_g_left"] / max(1e-9, (eq["bodies_y"] + eq["legal_bodies_y"]) * stock["mean_g_per_body"] * L["material"]["recoverable_fraction"])
    # when the dead run out: supply = trafficking only; price and victims settle together
    p2 = price
    for _ in range(200):
        t2 = trafficking(L, p2)
        p_new = clearing_price(L, max(t2["g_y"] + eq["legal_bodies_y"] * 0.0, 1e-3), ctx)
        if abs(p_new / p2 - 1) < 1e-6:
            break
        p2 = math.sqrt(p2 * p_new)
    t2 = trafficking(L, p2)
    after = dict(price_cr_per_g=round(p2), victims_y=t2["victims_y"], refined_g_y=t2["g_y"],
                 note="после исчерпания тел NIRVANA-1 материал даёт только живое сырьё: цена растёт, ёмкость «Сада» становится узким местом — давление строить новые «Сады»")
    corr = corruption_pressure(L, price, stock["mean_g_per_body"] * L["material"]["recoverable_fraction"])
    # bribes actually paid: connected crews buy the picket, networks buy permits and certificates
    bribe_pool = sum(r["runs_y"] * r.get("bribe_cr", 0) for r in eq["rows"]) + 0.1 * eq["trafficking"]["victims_y"] * eq["trafficking"]["revenue_per_victim_cr"]
    weights = {u["id"]: u["corruption"] * (1 - u["audit_risk"]) * u["ships"] for u in L["security_units"] if u["category"] != "lawful"}
    wsum = sum(weights.values()) or 1.0
    units = [dict(u, unofficial_income_cr_y=round(bribe_pool * weights.get(u["id"], 0.0) / wsum)) for u in L["security_units"]]
    tgt = L["demand"]["target_price_cr_per_g"]
    return dict(
        stock=stock, price_cr_per_g=round(price), price_target_cr_per_g=tgt, price_target_status=L["demand"]["target_status"],
        price_in_target=tgt[0] <= price <= tgt[1],
        price_vs_cobrett=r3(price / class_price["cobrett_i_n1"]),
        demand_at_price=dict(medical_g_y=r3(med), research_g_y=r3(res), stabiliser_g_y=r3(tech), medical_buyers=round(buyers),
                             medical_buyers_top=sorted(((k, round(v)) for k, v in by_pol.items()), key=lambda x: -x[1])[:6]),
        supply=dict(raids_y=r3(eq["runs_y"]), raids_by_segment=eq["runs_by_segment"], bodies_stolen_y=r3(eq["bodies_y"]), legal_bodies_y=eq["legal_bodies_y"],
                    refined_g_y=r3(eq["refined_g_y"]), years_until_primary_exhausted=r3(years_left)),
        raids=dict(segments=eq["rows"], average_ev_cr=round(eq["avg_ev"]), ships_lost_y=r3(eq["runs_y"] * p_loss), run_cost_cr=round(run_cost),
                   body_value_retail_cr=round(stock["mean_g_per_body"] * L["material"]["recoverable_fraction"] * price)),
        trafficking=eq["trafficking"], after_primary_exhausted=after, quarantine=qc, corruption=corr,
        blockade=blockade_sweep(L, eq["runs_y"], price, corr["by_zone"]["outer_perimeter"][0]["pressure"]),
        outbreaks=ob,
        stabilizer=dict(name=S["name"], function=S["function"], g_per_unit=S["g_per_unit"], effects=S["effects"], not_absolute=S["not_absolute"],
                        consumption=S["consumption"], fleet=fit, installed_g=ctx["fleet"]["installed_g"], anomaly_zones=ctx["fleet"]["zones"],
                        small_insert_price_cr=round(S["g_per_unit"]["small"] * price), politics=politics(fit),
                        map_effect=dict(new_direct_beacon_edges=[], newly_reachable_systems=[], note="обычная дальность прыжка не меняется")),
        medical=dict(course_g=L["medical"]["course_g"], course_price_cr=round(L["medical"]["course_g"] * price), courses_y=round(med / L["medical"]["course_g"]),
                     useless=L["medical"]["market_useless_share"], dangerous=L["medical"]["market_dangerous_share"], fraud=L["medical"]["fraud_share"]),
        security_units=units, bribe_pool_cr_y=round(bribe_pool), status=TAG)
