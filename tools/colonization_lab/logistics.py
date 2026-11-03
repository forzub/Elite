"""Logistics cost reference (PROPOSAL data, deterministic, stdlib only).

Answers, from the real 3126 beacon network and the ship-class data in
canon/ship_economics.json:

  * what a "mean interstellar jump" is in this network (edge statistics and
    the hops that real shortest-path trips actually use);
  * the two-term jump rule  fuel_t = q * d_ly * (alpha + M_total), alpha =
    kappa * hull envelope area (field term) + mass term, with q and kappa
    calibrated so a full Cobrett tank covers 3.0 mean jumps empty and 2.5 at
    its full 120 t payload; consumable, charging energy and peak discharge
    power per class; the derived jump-charge price that makes one mean jump
    of a half-loaded Cobrett cost ~2.5x one in-system leg;
  * in-system propulsion per class: burn profile, reactor fuel, thermal load;
  * a ship's build cost from its components;
  * the cost of moving one tonne A -> B (fuel, maintenance, wear, crew,
    insurance, fees, tolls, customs, expected loss, depreciation);
  * replacement-driven shipyard demand and fleet-from-flow sizing.

Usage:  python logistics.py [--out DIR]   (writes logistics.json + logistics_report.md)
        python logistics.py --check      (recompute, compare with out/, no writes)
"""

import argparse
import json
import math
import os
import sys

import endowment as endow
import lab

HERE = os.path.dirname(os.path.abspath(__file__))
YEAR = 3126
G = 9.80665
AU_M = 1.495978707e11


def load_ship_economics():
    with open(os.path.join(HERE, "canon", "ship_economics.json"), encoding="utf-8") as f:
        econ = json.load(f)
    cm = econ["cargo_model"]
    for sc in econ["ship_classes"].values():
        sc["_c20_payload_t"] = cm["container_payload_t"]
        sc["_c20_tare_t"] = cm["container_tare_t"]
    return econ


def cargo_gross_t(sc, net_t):
    """Gross cargo mass = net payload + tare of the C20 containers it occupies (dense packing:
    20 t per container). Ships without container places carry bulk / modules: gross = net."""
    slots = sc.get("container_slots", 0)
    if net_t <= 0.0 or not slots:
        return max(0.0, net_t)
    used = min(slots, math.ceil(net_t / sc.get("_c20_payload_t", 20.0) - 1e-9))
    return net_t + used * sc.get("_c20_tare_t", 1.7)


# ---------------------------------------------------------------- network
def network_stats(world, E, year=YEAR):
    nodes, edges, hop = lab.beacon_graph(world, E, year)
    lengths = sorted(d for a in edges for b, d in edges[a].items() if a < b)
    n = len(lengths)
    edge_mean = sum(lengths) / n
    edge_median = (lengths[n // 2] + lengths[(n - 1) // 2]) / 2.0
    # Hops actually used by trips: shortest path (lab cost: 1 + d/10 per hop)
    # between every ordered pair of reachable beacon systems.
    used, trip_ly, trip_hops = [], [], []
    for src in sorted(nodes):
        dist = {src: 0.0}
        prev = {}
        todo = [src]
        while todo:
            todo.sort(key=lambda x: (dist[x], x))
            u = todo.pop(0)
            for v, d in sorted(edges.get(u, {}).items()):
                nd = dist[u] + 1.0 + d / 10.0
                if v not in dist or nd < dist[v] - 1e-12:
                    dist[v] = nd
                    prev[v] = u
                    todo.append(v)
        for dst in sorted(nodes):
            if dst == src or dst not in dist:
                continue
            ly, h, x = 0.0, 0, dst
            while x != src:
                d = edges[prev[x]][x]
                used.append(d)
                ly += d
                h += 1
                x = prev[x]
            trip_ly.append(ly)
            trip_hops.append(h)
    nearest = []
    for a in sorted(nodes):
        ds = [d for b, d in edges.get(a, {}).items()]
        if ds:
            nearest.append(min(ds))
    return {
        "year": year,
        "max_hop_ly": hop,
        "systems": len(nodes),
        "edges": n,
        "edge_mean_ly": round(edge_mean, 3),
        "edge_median_ly": round(edge_median, 3),
        "edge_min_ly": round(lengths[0], 3),
        "edge_max_ly": round(lengths[-1], 3),
        "trip_hop_mean_ly": round(sum(used) / len(used), 3),
        "trip_mean_ly": round(sum(trip_ly) / len(trip_ly), 3),
        "trip_mean_hops": round(sum(trip_hops) / len(trip_hops), 3),
        "trip_max_hops": max(trip_hops),
        "nearest_neighbour_mean_ly": round(sum(nearest) / len(nearest), 3),
        "_edges": edges,
        "_nodes": sorted(nodes),
    }


# ---------------------------------------------------------------- ships
def build_cost(sc):
    parts = sum(sc["components_cr"].values())
    labour = parts * sc["labour_share"]
    production = parts + labour
    margin = production * sc["yard_margin"]
    tax = (production + margin) * sc["sales_tax"]
    return {
        "components_cr": round(parts),
        "labour_cr": round(labour),
        "production_cost_cr": round(production),
        "yard_margin_cr": round(margin),
        "sales_tax_cr": round(tax),
        "list_price_cr": round(production + margin + tax),
    }


def envelope_m2(sc):
    """Hull envelope area: the box of envelope_m, or an explicit envelope_m2 (trains)."""
    if sc.get("envelope_m2"):
        return float(sc["envelope_m2"])
    L, W, H = sc["envelope_m"]
    return 2.0 * (L * W + L * H + W * H)


def insystem_leg(sc, rule, cargo_t=None):
    """One brachistochrone leg at the class burn acceleration (loaded):
    duration (s) and reactor fuel (kg). Drive power scales with the thrust
    fraction the acceleration needs."""
    d = rule["mean_leg_au"] * AU_M
    a = sc.get("burn_accel_g", rule["burn_accel_g"]) * G
    t = 2.0 * math.sqrt(d / a)
    mass_kg = (sc["dry_mass_t"] + sc["jump_tank_t"] + cargo_gross_t(sc, sc["cargo_t"] if cargo_t is None else cargo_t)) * 1000.0
    thrust_fraction = min(1.0, mass_kg * a / (sc["thrust_mn"] * 1e6))
    energy = sc["drive_input_mw"] * 1e6 * thrust_fraction * t
    fuel_kg = energy / (rule["fuel_energy_j_per_kg"] * rule["plant_efficiency"])
    return t, fuel_kg


def calibrate(econ, net):
    """q and kappa of fuel_t = q * d * (kappa * A + M) from the reference class targets (fuel burn-off
    during one tank is ~1 % and is ignored here; range_jumps integrates it exactly)."""
    jr = econ["jump_rule"]
    ref = econ["ship_classes"][jr["reference_class"]]
    mean_jump = net["trip_hop_mean_ly"]
    m_empty = ref["dry_mass_t"] + ref["jump_tank_t"]
    m_full = m_empty + cargo_gross_t(ref, ref["cargo_t"])
    r = jr["target_mean_jumps_empty"] / jr["target_mean_jumps_full"]
    alpha = (m_full - r * m_empty) / (r - 1.0)
    kappa = alpha / envelope_m2(ref)
    q = ref["jump_tank_t"] / (jr["target_mean_jumps_empty"] * mean_jump * (alpha + m_empty))
    m_ref = m_empty + cargo_gross_t(ref, 0.5 * ref["cargo_t"])
    f_ref = q * mean_jump * (alpha + m_ref)
    leg_t, leg_fuel_kg = insystem_leg(ref, econ["insystem_rule"], 0.5 * ref["cargo_t"])
    leg_cost = leg_fuel_kg * econ["anchors"]["reactor_fuel_cr_per_kg"]
    ratio = econ["anchors"]["jump_vs_insystem_cost_ratio"]
    charge_price = ratio * leg_cost / (f_ref * 1000.0)  # Cr per kg
    e_ref = jr["charge_power_share_of_reactor"] * ref["reactor_mwe"] * 1e6 * jr["charge_hours_per_jump"] * 3600.0
    return {
        "reference_mass_t": m_ref,
        "mean_jump_ly": mean_jump,
        "q_per_ly": q,
        "kappa_t_per_m2": kappa,
        "alpha_reference_t": alpha,
        "jump_fuel_per_mean_jump_t": f_ref,
        "jump_energy_per_charge_t_j": e_ref / f_ref,
        "jump_energy_reference_j": e_ref,
        "insystem_leg_hours": leg_t / 3600.0,
        "insystem_leg_fuel_kg": leg_fuel_kg,
        "insystem_leg_cost_cr": leg_cost,
        "jump_charge_cr_per_kg": charge_price,
        "mean_jump_cost_cr": f_ref * 1000.0 * charge_price,
    }


def jump_fuel_t(cal, econ, sc, total_mass_t, d_ly):
    return cal["q_per_ly"] * d_ly * (cal["kappa_t_per_m2"] * envelope_m2(sc) + total_mass_t)


def jump_profile(cal, econ, sc):
    """Consumable, energy and power of one network-mean jump at half load, kept apart."""
    jr = econ["jump_rule"]
    m = sc["dry_mass_t"] + sc["jump_tank_t"] + cargo_gross_t(sc, 0.5 * sc["cargo_t"])
    f = jump_fuel_t(cal, econ, sc, m, cal["mean_jump_ly"])
    energy = f * cal["jump_energy_per_charge_t_j"]
    charge_w = jr["charge_power_share_of_reactor"] * sc["reactor_mwe"] * 1e6
    return {
        "alpha_field_term_t": round(cal["kappa_t_per_m2"] * envelope_m2(sc), 1),
        "mass_half_load_t": round(m, 1),
        "charge_per_mean_jump_t": round(f, 4),
        "kg_per_ly_per_1000t": round(f * 1000.0 / cal["mean_jump_ly"] / (m / 1000.0), 3),
        "kg_per_ly_per_1000t_cargo": round(f * 1000.0 / cal["mean_jump_ly"] / (0.5 * sc["cargo_t"] / 1000.0), 2) if sc["cargo_t"] > 0 else None,
        "energy_tj": round(energy / 1e12, 2),
        "charge_hours": round(energy / charge_w / 3600.0, 2),
        "peak_discharge_tw": round(energy / jr["discharge_seconds"] / 1e12, 2),
        "cost_cr": round(f * 1000.0 * cal["jump_charge_cr_per_kg"]),
    }


def insystem_profile(econ, sc):
    """Typical in-system leg (half load): burn time, reactor fuel and its cost, thermal load."""
    rule = econ["insystem_rule"]
    t, kg = insystem_leg(sc, rule, 0.5 * sc["cargo_t"])
    m_kg = (sc["dry_mass_t"] + sc["jump_tank_t"] + cargo_gross_t(sc, 0.5 * sc["cargo_t"])) * 1000.0
    a = sc.get("burn_accel_g", rule["burn_accel_g"]) * G
    frac = min(1.0, m_kg * a / (sc["thrust_mn"] * 1e6))
    p_mw = sc["drive_input_mw"] * frac
    heat = p_mw * rule["waste_heat_share"]
    return {
        "thrust_mn": sc["thrust_mn"], "burn_accel_g": sc.get("burn_accel_g"), "drive_power_mw": round(p_mw),
        "leg_au": rule["mean_leg_au"], "leg_hours": round(t / 3600.0, 1), "reactor_fuel_kg": round(kg, 2),
        "fuel_cost_cr": round(kg * econ["anchors"]["reactor_fuel_cr_per_kg"]),
        "thermal_load_mw": round(heat), "radiator_mw": sc.get("radiator_mw"),
        "thermal_margin": round(sc.get("radiator_mw", 0) / max(1.0, heat), 2),
    }


def range_jumps(cal, econ, sc, cargo_fraction):
    """Mean jumps a full tank gives at a cargo load (fuel mass burns off)."""
    fuel = sc["jump_tank_t"]
    if fuel <= 0.0:
        return 0.0
    jumps = 0.0
    m = sc["dry_mass_t"] + fuel + cargo_gross_t(sc, cargo_fraction * sc["cargo_t"])
    while True:
        f = jump_fuel_t(cal, econ, sc, m, cal["mean_jump_ly"])
        if f > fuel:
            return round(jumps + fuel / f, 2)
        fuel -= f
        m -= f
        jumps += 1.0


# ---------------------------------------------------------------- delivery
def trip_cost(econ, cal, sc_key, hops_ly, cargo_t, cargo_value_cr):
    """Cost of one loaded trip along the given jump lengths (+ two in-system legs)."""
    sc = econ["ship_classes"][sc_key]
    fees = econ["fees"]
    price = build_cost(sc)["list_price_cr"]
    leg_s, leg_fuel = insystem_leg(sc, econ["insystem_rule"], cargo_t)
    leg_h = leg_s / 3600.0
    jumps = len(hops_ly)
    m = sc["dry_mass_t"] + sc["jump_tank_t"] + cargo_gross_t(sc, cargo_t)
    jump_fuel = 0.0
    for d in hops_ly:
        f = jump_fuel_t(cal, econ, sc, m, d)
        jump_fuel += f
        m -= f
    charge_h = jump_profile(cal, econ, sc)["charge_hours"] if jumps else 0.0
    hours = 2.0 * leg_h + jumps * charge_h + 8.0  # + 2 x 4 h port turnaround
    days = hours / 24.0
    year_share = days / 365.0
    c = {}
    c["jump_fuel"] = jump_fuel * 1000.0 * cal["jump_charge_cr_per_kg"]
    c["insystem_fuel"] = 2.0 * leg_fuel * econ["anchors"]["reactor_fuel_cr_per_kg"]
    c["maintenance_reserve"] = price * sc["maintenance_share_per_year"] * year_share / sc["utilisation"]
    c["crew"] = sc["crew"] * econ["anchors"]["pilot_wage_cr_per_month"] * 12.0 * year_share / sc["utilisation"]
    c["hull_insurance"] = price * sc["insurance_hull_rate_per_year"] * year_share / sc["utilisation"]
    c["cargo_insurance"] = cargo_value_cr * fees["cargo_insurance_rate_per_trip"]
    c["port_fees"] = 2.0 * (fees["dock_cr_per_call_base"] + fees["dock_cr_per_tonne_dry"] * sc["dry_mass_t"])
    c["cargo_handling"] = 2.0 * fees["cargo_handling_cr_per_tonne"] * cargo_t
    c["beacon_tolls"] = jumps * fees["beacon_toll_cr_per_jump"]
    c["customs"] = cargo_value_cr * fees["customs_share_of_cargo_value"] if jumps > 0 else 0.0
    c["expected_loss"] = jumps * fees["route_loss_probability_per_jump"] * (price + cargo_value_cr)
    c["depreciation"] = price / sc["service_life_years"] * year_share / sc["utilisation"]
    total = sum(c.values())
    return {
        "ship": sc_key,
        "jumps": jumps,
        "ly": round(sum(hops_ly), 2),
        "cargo_t": cargo_t,
        "cargo_value_cr": cargo_value_cr,
        "trip_days": round(days, 2),
        "costs_cr": {k: round(v) for k, v in c.items()},
        "total_cr": round(total),
        "cr_per_tonne": round(total / cargo_t, 1),
        "jump_fuel_t": round(jump_fuel, 3),
        "insystem_fuel_kg": round(2.0 * leg_fuel, 2),
    }


def route_hops(net, a, b):
    edges = net["_edges"]
    dist = {a: 0.0}
    prev = {}
    todo = [a]
    while todo:
        todo.sort(key=lambda x: (dist[x], x))
        u = todo.pop(0)
        for v, d in sorted(edges.get(u, {}).items()):
            nd = dist[u] + 1.0 + d / 10.0
            if v not in dist or nd < dist[v] - 1e-12:
                dist[v] = nd
                prev[v] = u
                todo.append(v)
    if b not in dist:
        return None
    hops, x = [], b
    while x != a:
        hops.append(edges[prev[x]][x])
        x = prev[x]
    return list(reversed(hops))


# ---------------------------------------------------------------- fleet
def replacement(fleet, life_years, loss_rate):
    return {
        "fleet": fleet,
        "service_life_years": life_years,
        "natural_replacement_per_year": round(fleet / life_years, 1),
        "losses_per_year": round(fleet * loss_rate, 1),
        "yard_demand_per_year_no_growth": round(fleet / life_years + fleet * loss_rate, 1),
    }


def fleet_for_flow(econ, cal, sc_key, tonnes_per_year, hops_ly):
    sc = econ["ship_classes"][sc_key]
    one = trip_cost(econ, cal, sc_key, hops_ly, sc["cargo_t"], 0.0)
    round_trip_days = 2.0 * one["trip_days"]
    trips_per_year = 365.0 * sc["utilisation"] / round_trip_days
    ships = tonnes_per_year / (sc["cargo_t"] * trips_per_year)
    return {"ship": sc_key, "tonnes_per_year": tonnes_per_year, "round_trip_days": round(round_trip_days, 2),
            "loaded_trips_per_ship_year": round(trips_per_year, 1), "ships_needed": math.ceil(ships)}


# ---------------------------------------------------------------- report
def compute():
    world = lab.load("world.json")
    E = endow.load_endowment()
    econ = load_ship_economics()
    net = network_stats(world, E)
    cal = calibrate(econ, net)
    cob = econ["ship_classes"]["cobrett_i_n1"]
    examples = []
    pairs = [(0, n) for n in net["_nodes"] if n != 0]
    # Earth (system 0) to its nearest, a median and the farthest reachable beacon.
    reach = [(a, b, route_hops(net, a, b)) for a, b in pairs]
    lens = sorted(((sum(h), b) for a, b, h in reach if h), key=lambda x: (x[0], x[1]))
    picks = [lens[0][1], lens[len(lens) // 2][1], lens[-1][1]]
    typical = round(econ["cargo_model"]["typical_mixed_density_t_m3"] * cob["cargo_m3"])
    for b in picks:
        hops = route_hops(net, 0, b)
        # a typical mixed (volume-limited) load of cheap goods, the same load of high-value goods
        # and a full dense 120 t load of metals
        for load, value in ((typical, typical * 3000.0), (typical, 2000000.0), (cob["cargo_t"], cob["cargo_t"] * 1100.0)):
            r = trip_cost(econ, cal, "cobrett_i_n1", hops, load, value)
            r["route"] = f"0->{b}"
            examples.append(r)
    for key in ("medium_freighter", "jump_train"):
        sc = econ["ship_classes"][key]
        hops = route_hops(net, 0, picks[1])
        r = trip_cost(econ, cal, key, hops, sc["cargo_t"], sc["cargo_t"] * 300.0)
        r["route"] = f"0->{picks[1]}"
        examples.append(r)
    insys = trip_cost(econ, cal, "ocs4_lastochka", [], 450.0, 450.0 * 300.0)
    insys["route"] = "in-system (no jump)"
    out = {
        "status": "PROPOSAL",
        "network": {k: v for k, v in net.items() if not k.startswith("_")},
        "calibration": {k: round(v, 6) if isinstance(v, float) else v for k, v in cal.items()},
        "build_cost": {k: build_cost(v) for k, v in sorted(econ["ship_classes"].items())},
        "cobrett_cargo": {"cargo_t_max": cob["cargo_t"], "cargo_m3": cob["cargo_m3"], "container_slots": cob["container_slots"],
                          "typical_mixed_load_t": typical, "container": econ["cargo_model"]["container"],
                          "net_payload_t": cob["cargo_t"], "container_tare_t": round(cargo_gross_t(cob, cob["cargo_t"]) - cob["cargo_t"], 2),
                          "gross_cargo_t": round(cargo_gross_t(cob, cob["cargo_t"]), 2),
                          "typical_gross_t": round(typical + cob["container_slots"] * econ["cargo_model"]["container_tare_t"], 2)},
        "range_mean_jumps": {
            k: {"empty": range_jumps(cal, econ, v, 0.0), "half": range_jumps(cal, econ, v, 0.5),
                "full": range_jumps(cal, econ, v, 1.0)}
            for k, v in sorted(econ["ship_classes"].items()) if v["jump_tank_t"] > 0
        },
        "jump_profile": {k: jump_profile(cal, econ, v) for k, v in sorted(econ["ship_classes"].items()) if v["jump_tank_t"] > 0},
        "insystem_profile": {k: insystem_profile(econ, v) for k, v in sorted(econ["ship_classes"].items())},
        "trip_examples": examples + [insys],
        "replacement_examples": [replacement(12000, 35, cob["loss_rate_per_year"]),
                                 replacement(12000, 35, 0.03)],
        "fleet_from_flow_examples": [
            fleet_for_flow(econ, cal, "jump_train", 100e6, route_hops(net, 0, picks[1])),
            fleet_for_flow(econ, cal, "cobrett_i_n1", 100e3, route_hops(net, 0, picks[1])),
        ],
    }
    return out


def fmt(out):
    n, c = out["network"], out["calibration"]
    cc = out["cobrett_cargo"]
    L = ["# Logistics cost reference (PROPOSAL)", "",
         "Generated by `tools/colonization_lab/logistics.py` from `canon/world.json` (beacon network) and",
         "`canon/ship_economics.json`. Every money / fuel number is a PROPOSAL; physical ship facts come from",
         "the runtime descriptors.", "",
         f"## Network {n['year']} (beacons, max hop {n['max_hop_ly']} ly)", "",
         f"- systems {n['systems']}, edges {n['edges']}",
         f"- edge length: mean {n['edge_mean_ly']} ly, median {n['edge_median_ly']}, min {n['edge_min_ly']}, max {n['edge_max_ly']}",
         f"- **hops real trips use** (all-pairs shortest paths): mean **{n['trip_hop_mean_ly']} ly**",
         f"- trip: mean {n['trip_mean_ly']} ly in {n['trip_mean_hops']} jumps (max {n['trip_max_hops']})",
         f"- nearest-neighbour mean {n['nearest_neighbour_mean_ly']} ly", "",
         "## Cobrett cargo (authoritative)", "",
         f"- {cc['container_slots']} x {cc['container']} places, {cc['cargo_m3']} m3, structural limit {cc['cargo_t_max']} t; "
         f"typical mixed load {cc['typical_mixed_load_t']} t (volume-limited)",
         f"- mass: net payload {cc['net_payload_t']} t + container tare {cc['container_tare_t']} t = gross {cc['gross_cargo_t']} t "
         f"(typical mixed: {cc['typical_mixed_load_t']} t net, {cc['typical_gross_t']} t gross); the jump and burn equations use gross", "",
         "## Jump fuel", "",
         f"- rule: `fuel_t = q * d_ly * (kappa * A_envelope + M_total)`; q = {c['q_per_ly']} 1/ly, kappa = {c['kappa_t_per_m2']} t/m2 "
         f"(Cobrett field term {round(c['alpha_reference_t'], 1)} t)",
         f"- mean jump {c['mean_jump_ly']} ly of a half-loaded Cobrett ({c['reference_mass_t']} t): {round(c['jump_fuel_per_mean_jump_t'], 4)} t of charge",
         f"- in-system leg: {round(c['insystem_leg_hours'], 1)} h, {round(c['insystem_leg_fuel_kg'], 1)} kg reactor fuel = {round(c['insystem_leg_cost_cr'])} Cr",
         f"- derived jump charge price {round(c['jump_charge_cr_per_kg'], 2)} Cr/kg -> mean jump {round(c['mean_jump_cost_cr'])} Cr",
         "", "| ship | mean jumps on a full tank: empty | half cargo | full cargo |", "|---|---:|---:|---:|"]
    for k, v in out["range_mean_jumps"].items():
        L.append(f"| {k} | {v['empty']} | {v['half']} | {v['full']} |")
    L += ["", "Consumable / energy / power of one mean jump at half load (kept apart: the energy comes from the reactor "
          "while charging, the peak from the core's store during the discharge):", "",
          "| ship | field term t | mass t | charge t | kg/(ly*1000 t ship) | kg/(ly*1000 t cargo) | energy TJ | charge h | peak TW | Cr |",
          "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for k, v in out["jump_profile"].items():
        L.append(f"| {k} | {v['alpha_field_term_t']} | {v['mass_half_load_t']} | {v['charge_per_mean_jump_t']} | {v['kg_per_ly_per_1000t']} | "
                 f"{v['kg_per_ly_per_1000t_cargo']} | {v['energy_tj']} | {v['charge_hours']} | {v['peak_discharge_tw']} | {v['cost_cr']} |")
    L += ["", "In-system propulsion (typical leg, half load):", "",
          "| ship | thrust MN | burn g | drive MW | leg h | reactor fuel kg | fuel Cr | heat MW | radiators MW | margin |",
          "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|"]
    for k, v in out["insystem_profile"].items():
        L.append(f"| {k} | {v['thrust_mn']} | {v['burn_accel_g']} | {v['drive_power_mw']} | {v['leg_hours']} | {v['reactor_fuel_kg']} | "
                 f"{v['fuel_cost_cr']} | {v['thermal_load_mw']} | {v['radiator_mw']} | {v['thermal_margin']} |")
    L += ["", "## Build cost (production cost -> list price)", "", "| ship | components | labour | production | margin | tax | list |",
          "|---|---:|---:|---:|---:|---:|---:|"]
    for k, v in out["build_cost"].items():
        L.append(f"| {k} | {v['components_cr']} | {v['labour_cr']} | {v['production_cost_cr']} | {v['yard_margin_cr']} | {v['sales_tax_cr']} | {v['list_price_cr']} |")
    L += ["", "## Trip cost examples (one loaded trip, two in-system legs + jumps)", ""]
    for r in out["trip_examples"]:
        L.append(f"### {r['ship']} {r['route']}: {r['jumps']} jumps, {r['ly']} ly, {r['cargo_t']} t worth {int(r['cargo_value_cr'])} Cr, {r['trip_days']} days")
        L.append("")
        L.append("| item | Cr |")
        L.append("|---|---:|")
        for k, v in r["costs_cr"].items():
            L.append(f"| {k} | {v} |")
        L.append(f"| **total** | **{r['total_cr']}** |")
        L.append(f"| per tonne | {r['cr_per_tonne']} |")
        L.append("")
    L += ["## Replacement-driven yard demand", ""]
    for r in out["replacement_examples"]:
        L.append(f"- fleet {r['fleet']}, life {r['service_life_years']} y: {r['natural_replacement_per_year']} natural + {r['losses_per_year']} losses = "
                 f"**{r['yard_demand_per_year_no_growth']} hulls/year** before growth or war")
    L += ["", "## Fleet from flow", ""]
    for r in out["fleet_from_flow_examples"]:
        L.append(f"- {r['tonnes_per_year']:.0f} t/year by {r['ship']}: round trip {r['round_trip_days']} d, "
                 f"{r['loaded_trips_per_ship_year']} loaded trips/ship/year -> **{r['ships_needed']} ships**")
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------- runtime passports
RUNTIME_PASSPORTS = {"cobrett_i_n1": "CobrettIN1Cargo", "ocs4_lastochka": "Ocs4Cargo"}


def passport_check(econ=None):
    """Runtime cargo passports (src/game/catalog/ships/ShipCargoPassports.h) and ship_economics.json
    must state the same physical facts: dry mass, net payload, volume, C20 places and the C20 container."""
    import re
    econ = econ or load_ship_economics()
    h = open(os.path.join(endow.ROOT, "src", "game", "catalog", "ships", "ShipCargoPassports.h"), encoding="utf-8").read()
    errors = []
    cm = econ["cargo_model"]
    for name, key in (("C20PayloadTonnes", "container_payload_t"), ("C20VolumeM3", "container_volume_m3"), ("C20TareTonnes", "container_tare_t")):
        m = re.search(r"%s\s*=\s*([0-9.]+)" % name, h)
        if not m or abs(float(m.group(1)) - cm[key]) > 1e-9:
            errors.append(f"{name} != cargo_model.{key}")
    for cls, const in RUNTIME_PASSPORTS.items():
        m = re.search(r"ShipCargoPassport\s+%s\s*\{\s*([0-9.]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*,\s*([0-9]+)\s*\}" % const, h)
        if not m:
            errors.append(f"{const} not found")
            continue
        sc = econ["ship_classes"][cls]
        got = (float(m.group(1)) / 1000.0, int(m.group(2)), int(m.group(3)), int(m.group(4)))
        want = (float(sc["dry_mass_t"]), int(sc["cargo_t"]), int(sc["cargo_m3"]), int(sc["container_slots"]))
        if got != want:
            errors.append(f"{cls}: runtime {got} != ship_economics {want}")
    return errors


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=os.path.join(HERE, "out"))
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    out = compute()
    js = json.dumps(out, ensure_ascii=False, indent=2, sort_keys=True) + "\n"
    md = fmt(out)
    if a.check:
        ok = True
        for name, text in (("logistics.json", js), ("logistics_report.md", md)):
            p = os.path.join(a.out, name)
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != text:
                print(f"STALE: {p}")
                ok = False
        for e in passport_check():
            print(f"PASSPORT: {e}")
            ok = False
        print("logistics check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    os.makedirs(a.out, exist_ok=True)
    with open(os.path.join(a.out, "logistics.json"), "w", encoding="utf-8", newline="\n") as f:
        f.write(js)
    with open(os.path.join(a.out, "logistics_report.md"), "w", encoding="utf-8", newline="\n") as f:
        f.write(md)
    print(md)
    return 0


if __name__ == "__main__":
    sys.exit(main())
