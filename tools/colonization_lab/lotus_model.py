"""The World's Ass (月工门) economy: causal model of the Lotus (NIRVANA-1) region.

Called by world_sites.py. Everything follows from canon/world_sites.json ->
lotus (Developer World Truth + parameters) and the physical catalog:

  finite primary stock (crew cohorts x exposure x transformation x recovery x
  decay - removed) -> demand (medical + jump stabiliser) -> free-entry raids
  (segments, corridor maps, cordon, crowding of the hulk) -> supply and price ->
  trafficking from incentives -> quarantine capacity vs traffic -> corruption
  pressure by role and zone -> blockade strictness sweep (non-linear) ->
  lotus fever outbreaks from site population and beds -> +3 ly map effect.

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
def demand_g(L, p):
    D = L["demand"]
    ref = D["ref_price_cr_per_g"]
    return D["medical_g_y_at_ref"] * (p / ref) ** (-D["medical_elasticity"]), D["jump_g_y_at_ref"] * (p / ref) ** (-D["jump_elasticity"])


def clearing_price(L, q):
    lo, hi = 1e5, 1e12
    for _ in range(200):
        mid = math.sqrt(lo * hi)
        m, j = demand_g(L, mid)
        if m + j > q:
            lo = mid
        else:
            hi = mid
    return math.sqrt(lo * hi)


# ---------------------------------------------------------------- raids: free entry
SEGMENTS = [
    dict(id="naive", ru="наивные (без карты, без связей)", share=0.45, corridor="unknown", bribe=0.0, sell="inner_anomaly", haul=1.2),
    dict(id="informed", ru="с купленной картой (часть карт ложные)", share=0.32, corridor="estimated", bribe=0.0, sell="grey_frontier", haul=2.0),
    dict(id="connected", ru="со связями (свежая карта, взятка, своя лаборатория)", share=0.05, corridor="known_safe", bribe=1.0, sell="outer_perimeter", haul=3.0),
    dict(id="cover_story", ru="старатели «минерала» у планеты c (легенда)", share=0.18, corridor=None, bribe=0.0, sell=None, haul=0.0),
]


def raid_economics(L, stock, price, hull, run_cost, n_runs, strict):
    R = L["runs"]
    C = L["corridor"]
    tiers = L["markets"]["tier_share_of_civilised_price"]
    left = stock["bodies_left"] / max(1, stock["bodies"])
    base_bodies = n_runs * sum(sg["share"] * sg["haul"] for sg in SEGMENTS)
    crowd = min(1.0, R["hulk_extraction_capacity_bodies_y"] * math.sqrt(left) / max(base_bodies, 1e-9))
    p_caught_base = 0.45 * strict * min(1.0, R["cordon_capacity_runs"] / max(1.0, n_runs))
    g_body = stock["mean_g_per_body"] * L["material"]["recoverable_fraction"]
    rows = []
    for sg in SEGMENTS:
        if sg["corridor"] is None:
            ev = -(run_cost + 30000) - 0.06 * 0.6 * hull
            rows.append(dict(segment=sg["id"], ru=sg["ru"], share=sg["share"], p_ship_lost=0.06, p_caught=0.0, bodies=0.0, price_per_body=0, ev_cr=round(ev),
                             stage1_risk=0.0))
            continue
        cs = C["states"][sg["corridor"]]
        p_loss = cs["p_loss"]
        if sg["corridor"] == "estimated":
            p_loss = (1 - C["fake_share"]) * cs["p_loss"] + C["fake_share"] * C["states"]["unknown"]["p_loss"]
        map_cost = C["map_price_cr"].get(sg["corridor"], 0)
        per_body = tiers[sg["sell"]] * price * g_body
        bribe_cr = sg["bribe"] * 0.12 * sg["haul"] * per_body      # the picket's price follows the value of the cargo
        p_caught = p_caught_base * (0.3 if sg["bribe"] else 1.0)
        bodies = sg["haul"] * crowd
        per_body = tiers[sg["sell"]] * price * g_body
        ok = (1 - p_loss) * (1 - p_caught)
        losses = p_loss * 0.6 * hull + (1 - p_loss) * p_caught * (250000 + 0.3 * 0.6 * hull)
        ev = ok * bodies * per_body - run_cost - map_cost - bribe_cr - losses
        rows.append(dict(segment=sg["id"], ru=sg["ru"], share=sg["share"], corridor=sg["corridor"], p_ship_lost=r3(p_loss), p_caught=r3(p_caught),
                         bodies=r3(ok * bodies), price_per_body=round(per_body), bribe_cr=round(bribe_cr), map_cr=map_cost, ev_cr=round(ev), stage1_risk=0.13))
    avg = sum(r["share"] * r["ev_cr"] for r in rows)
    return rows, avg


def equilibrium(L, stock, hull, run_cost, strict):
    """Free entry: raiders keep coming while the average raid loses no more than the optimism bias.
    Price clears supply (raids + legal recovery + trafficking) against demand; raids react to price."""
    bias = L["runs"]["optimism_bias"]
    legal_bodies = 15.0
    g_body = stock["mean_g_per_body"] * L["material"]["recoverable_fraction"]
    price = L["demand"]["ref_price_cr_per_g"]
    n = 100.0
    for _ in range(80):
        traf = trafficking(L, price)
        lo, hi = 1.0, 5000.0
        for _ in range(60):
            mid = (lo + hi) / 2
            rows, avg = raid_economics(L, stock, price, hull, run_cost, mid, strict)
            if avg > -bias * run_cost:
                lo = mid
            else:
                hi = mid
        n = (lo + hi) / 2
        rows, avg = raid_economics(L, stock, price, hull, run_cost, n, strict)
        bodies = n * sum(r["share"] * r["bodies"] for r in rows)
        q = (bodies + legal_bodies) * g_body + traf["g_y"]
        new_price = clearing_price(L, max(q, 1e-6))
        if abs(new_price / price - 1) < 1e-6:
            break
        price = math.sqrt(price * new_price)
    return dict(runs_y=n, rows=rows, avg_ev=avg, bodies_y=bodies, legal_bodies_y=legal_bodies, refined_g_y=q, price=price, trafficking=traf)


# ---------------------------------------------------------------- trafficking from incentives
def trafficking(L, price, garden_pop=180):
    T = L["trafficking"]
    capacity = garden_pop / T["exposure_years"] * 1.2
    g = L["material"]["potential_g_fully_transformed_body"] * (1 - math.exp(-T["exposure_years"] / L["material"]["tau_years"])) * \
        L["material"]["recoverable_fraction"] * T["useful_yield_share"]
    # organised networks with captured officials sell through their own labs at the outer-perimeter price
    revenue = T["survival_to_harvest"] * g * price * L["markets"]["tier_share_of_civilised_price"]["outer_perimeter"]
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


def corruption_pressure(L, price):
    g_body = 1.0 * L["material"]["recoverable_fraction"]
    cargo = 2.0 * g_body * price * L["markets"]["tier_share_of_civilised_price"]["outer_perimeter"]
    out = {}
    for zid, z in L["zones"].items():
        rows = []
        for role, opp, share, salary in ROLES:
            illicit = opp * share * cargo
            lifetime = salary * 30.0
            frag = 1.0 + 0.15 * (z["fragmentation"] - 1)
            pressure = illicit / lifetime * (1.0 - z["audit"]) * z["isolation"] * frag
            corrupt = pressure / (1.0 + pressure)
            rows.append(dict(role=role, illicit_cr_y=round(illicit), legal_salary_cr_y=salary, pressure=r3(pressure), expected_corrupt_share=r3(corrupt),
                             cost_of_honesty_cr_y=round(illicit * (1.0 - z["audit"]))))
        out[zid] = rows
    return dict(formula="pressure = illicit profit / legal lifetime income x (1 - audit) x isolation x fragmentation", by_zone=out,
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


# ---------------------------------------------------------------- +3 ly on the map
def hop_bonus_effect(E, net_edges, rel_pairs, pol_systems, hop, bonus):
    P = {k: v["pos"] for k, v in E.items()}
    nodes = set(net_edges)

    def d(a, b):
        return math.dist(P[a], P[b])
    new_edges = sorted((a, b, r3(d(a, b))) for a in nodes for b in nodes if a < b and hop < d(a, b) <= hop + bonus)

    def reach(h):
        best = {n: 0.0 for n in nodes}
        frontier = sorted(nodes)
        done = set()
        while frontier:
            u = min(frontier, key=lambda x: best[x])
            frontier.remove(u)
            done.add(u)
            for v in P:
                if v in done:
                    continue
                w = max(best[u], d(u, v))
                if w <= h and w < best.get(v, 1e9):
                    best[v] = w
                    if v not in frontier:
                        frontier.append(v)
        return set(best)
    gained = sorted(reach(hop + bonus) - reach(hop))
    hostile_new = []
    for (a, b), g in sorted(rel_pairs.items()):
        if g < 30 or a not in pol_systems or b not in pol_systems:
            continue
        sa, sb = pol_systems[a], pol_systems[b]
        if sa != sb and hop < d(sa, sb) <= hop + bonus:
            hostile_new.append(dict(a=a, b=b, grievance=g, ly=r3(d(sa, sb))))
    return dict(bonus_ly=bonus, status="AUTHOR_TARGET", new_direct_beacon_edges=[dict(a=a, b=b, ly=l, names=[E[a]["name"], E[b]["name"]]) for a, b, l in new_edges],
                newly_reachable_systems=[dict(id=s, name=E[s]["name"]) for s in gained],
                hostile_pairs_brought_into_one_jump=hostile_new)


def politics(effect):
    out = ["Когниум (единственный производитель демпферов и переотжига) получает рычаг: дальние прыжки только для лицензиатов"]
    if effect["newly_reachable_systems"]:
        out.append("открываются без промежуточных точек: " + ", ".join(s["name"] for s in effect["newly_reachable_systems"]) + " — их ценность (9 Кита — пояс ~2.8 массы Земли) зависит от доступа к лотоситу")
    if effect["new_direct_beacon_edges"]:
        out.append(f"{len(effect['new_direct_beacon_edges'])} новых прямых маячных рёбер сокращают маршруты в обход узлов-посредников (Агора, Новая Индия как реэкспортёры теряют часть ренты)")
    if effect["hostile_pairs_brought_into_one_jump"]:
        out.append("враждебные пары оказываются в одном прыжке: " + ", ".join(f"{h['a']}–{h['b']}" for h in effect["hostile_pairs_brought_into_one_jump"]))
    else:
        out.append("враждебных пар, сближающихся до одного прыжка, нет: военный эффект — скорость рейдов и обход застав, а не новые фронты")
    out.append("страх распространения +3 св. лет — главная негласная причина кордона")
    return out


def compute(L, E, net_edges, rel_pairs, pol_systems, hop, hull, run_cost):
    stock = primary_stock(L)
    eq = equilibrium(L, stock, hull, run_cost, L["cordon"]["strictness"])
    price = eq["price"]
    med, jmp = demand_g(L, price)
    qc = quarantine_capacity(L, eq["runs_y"], 260)
    ob = outbreaks(L)
    effect = hop_bonus_effect(E, net_edges, rel_pairs, pol_systems, hop, L["jump_component"]["max_hop_bonus_ly"])
    jc = L["jump_component"]
    p_loss = sum(r["share"] * r["p_ship_lost"] for r in eq["rows"])
    years_left = stock["recoverable_g_left"] / max(1e-9, (eq["bodies_y"] + eq["legal_bodies_y"]) * stock["mean_g_per_body"] * L["material"]["recoverable_fraction"])
    # when the dead run out: supply = trafficking only; price and victims settle together
    p2 = price
    for _ in range(200):
        t2 = trafficking(L, p2)
        p_new = clearing_price(L, max(t2["g_y"], 1e-3))
        if abs(p_new / p2 - 1) < 1e-7:
            break
        p2 = math.sqrt(p2 * p_new)
    after = dict(price_cr_per_g=round(p2), victims_y=trafficking(L, p2)["victims_y"], refined_g_y=trafficking(L, p2)["g_y"],
                 note="после исчерпания тел NIRVANA-1 материал даёт только живое сырьё: цена растёт, ёмкость «Сада» становится узким местом — давление строить новые «Сады»")
    corr = corruption_pressure(L, price)
    units = []
    for u in L["security_units"]:
        units.append(dict(u, unofficial_income_cr_y=round(u["corruption"] * (1 - u["audit_risk"]) * 0.02 * price * (1.0 if u["category"] != "lawful" else 0.0))))
    return dict(
        stock=stock, price_cr_per_g=round(price), demand_at_price=dict(medical_g_y=r3(med), jump_g_y=r3(jmp)),
        supply=dict(raids_y=r3(eq["runs_y"]), bodies_stolen_y=r3(eq["bodies_y"]), legal_bodies_y=eq["legal_bodies_y"], refined_g_y=r3(eq["refined_g_y"]),
                    years_until_primary_exhausted=r3(years_left)),
        raids=dict(segments=eq["rows"], average_ev_cr=round(eq["avg_ev"]), ships_lost_y=r3(eq["runs_y"] * p_loss), run_cost_cr=round(run_cost)),
        trafficking=eq["trafficking"], after_primary_exhausted=after, quarantine=qc, corruption=corr, blockade=blockade_sweep(L, eq["runs_y"], price, corr["by_zone"]["outer_perimeter"][0]["pressure"]),
        outbreaks=ob, jump=dict(component=jc["name"], g_per_class=jc["g_per_class"],
                                cobrett_inserts_possible_from_stock=round(stock["recoverable_g_left"] / jc["g_per_class"]["cobrett_i_n1"]),
                                cobrett_insert_price_cr=round(jc["g_per_class"]["cobrett_i_n1"] * price), effect=effect, politics=politics(effect)),
        medical=dict(course_g=L["medical"]["course_g"], course_price_cr=round(L["medical"]["course_g"] * price), courses_y=round(med / L["medical"]["course_g"]),
                     useless=L["medical"]["market_useless_share"], dangerous=L["medical"]["market_dangerous_share"], fraud=L["medical"]["fraud_share"]),
        security_units=units, status=TAG)
