#!/usr/bin/env python3
"""Ship designs 3126: passports, equipment passports, physics checks, drawings, functional schematics.

Input (authored):  canon/ship_designs.json  - equipment products and ship models
                   canon/ship_economics.json - class targets (mass, payload, thrust, power, price by component)
                   canon/industry.json       - manufacturers, blocs, design schools
                   out/world_3126/industry_3126.json - trade channels between blocs
Output:            out/ships_3126/ship_designs_3126.json  (every derived number and check)
                   out/ships_3126/SHIP_DESIGNS_3126.md     (report)
                   out/ships_3126/svg/<id>.svg             (simplified design: top / side / front)
                   out/ships_3126/svg/<id>.schematic.svg   (functional schematic for integrity / repair)
                   src/assets/data/ships/ship_catalog_3126.json (GENERATED runtime copy for the generic builder)

Physics follows the game universe (src/game/physics/UniverseThermalRadiation.h): hot propulsion pads reject
6 MW/m2, radiating surfaces use Stefan-Boltzmann with the universal x10 effectiveness. A drive's rated input is
not F*v; its waste heat is input * (1 - efficiency). A reactor's waste is mwe * (1/eff - 1).

usage: ship_designer.py [--check]
"""
import argparse
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from lore_ships import CAT_RU  # noqa: E402  (one category vocabulary for drawings and lore)
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CANON = os.path.join(HERE, "canon")
OUT = os.path.join(HERE, "out", "ships_3126")
RUNTIME = os.path.join(ROOT, "src", "assets", "data", "ships", "ship_catalog_3126.json")
SIGMA = 5.670374419e-8
G0 = 9.80665

# functional assemblies (installation code A-index) and the categories they hold
ASSEMBLIES = [
    ("A1", "Двигательная установка", ("drive",)),
    ("A2", "Энергетика", ("reactor", "service_reactor")),
    ("A3", "Охлаждение", ("coolant_loop", "radiator")),
    ("A4", "Прыжок и топливо", ("jump_core", "tank")),
    ("A5", "Маневровая система", ("rcs",)),
    ("A6", "Авионика, сенсоры, связь", ("computer", "radar", "comm", "transponder", "ecm", "sensor_mast")),
    ("A7", "Жизнеобеспечение и спасение", ("life_support", "escape_capsule", "lifeboat", "medbay")),
    ("A8", "Груз и пассажиры", ("container_rack", "passenger_module", "train_module", "cryo_tank", "bulk_hold")),
    ("A9", "Стыковка и ангары", ("docking_port", "boat_bay", "drone_bay", "hangar")),
    ("A10", "Специальное оборудование", ("tow_rig", "workshop", "heat_shield", "landing_gear")),
    ("A11", "Вооружение", ("weapon",)),
    ("A12", "Броня", ("armour",)),
]
CAT_ASSEMBLY = {c: a for a, _, cats in ASSEMBLIES for c in cats}

# repair doctrine by category: level (crew / station / yard / factory), line-replaceable unit, hours to replace
REPAIR = {
    "drive": ("yard", True, 36), "reactor": ("yard", True, 48), "service_reactor": ("station", True, 12),
    "coolant_loop": ("station", True, 8), "radiator": ("crew", True, 1.5), "jump_core": ("factory", False, 0),
    "tank": ("station", True, 6), "rcs": ("crew", True, 2), "computer": ("station", True, 4), "radar": ("station", True, 6),
    "comm": ("crew", True, 2), "transponder": ("crew", True, 0.5), "ecm": ("station", True, 8), "sensor_mast": ("station", True, 10),
    "life_support": ("station", True, 10), "escape_capsule": ("station", True, 6), "lifeboat": ("station", True, 8), "medbay": ("station", True, 6),
    "container_rack": ("crew", True, 3), "passenger_module": ("yard", True, 24), "train_module": ("station", True, 6),
    "cryo_tank": ("yard", True, 30), "bulk_hold": ("yard", False, 0), "docking_port": ("station", True, 8), "boat_bay": ("yard", False, 0),
    "drone_bay": ("station", True, 8), "hangar": ("yard", False, 0), "tow_rig": ("station", True, 16), "workshop": ("station", True, 12),
    "heat_shield": ("yard", True, 40), "landing_gear": ("station", True, 12), "weapon": ("station", True, 10), "armour": ("yard", True, 2),
}
# component-group of the economics price model
PRICE_GROUP = {
    "drive": "main_drive", "reactor": "reactor", "service_reactor": "reactor", "coolant_loop": "rcs_thermal", "radiator": "rcs_thermal",
    "rcs": "rcs_thermal", "jump_core": "jump_drive", "tank": "jump_drive", "computer": "electronics", "radar": "electronics",
    "comm": "electronics", "transponder": "electronics", "ecm": "electronics", "sensor_mast": "electronics",
    "life_support": "life_support", "escape_capsule": "life_support", "lifeboat": "life_support", "medbay": "life_support",
    "weapon": "weapons", "armour": "weapons",
}
SLOT_POS = {   # normalized body positions of the six runtime layouts (src/game/ship/presets/cobra/CobraPropulsionPresets.h)
    "I": {"aft": [(0, 0, 1)], "assisted_aft": [(-1, 0, 1), (1, 0, 1)], "assisted_fore": [(-1, 0, -1), (1, 0, -1)]},
    "Y": {"aft": [(0, .5, 1), (-.43, -.25, 1), (.43, -.25, 1)], "assisted_aft": [(0, 1, 1), (-.87, -.5, 1), (.87, -.5, 1)],
          "assisted_fore": [(0, 1, -1), (-.87, -.5, -1), (.87, -.5, -1)]},
    "X": {"aft": [(-.4, .4, 1), (.4, .4, 1), (-.4, -.4, 1), (.4, -.4, 1)],
          "assisted_aft": [(-.71, .71, 1), (.71, .71, 1), (-.71, -.71, 1), (.71, -.71, 1)],
          "assisted_fore": [(-.71, .71, -1), (.71, .71, -1), (-.71, -.71, -1), (.71, -.71, -1)]},
}
CAT_COLOR = {
    "drive": "#e8762b", "reactor": "#d64545", "service_reactor": "#c76b6b", "coolant_loop": "#3d8fd6", "radiator": "#7fb8e6",
    "jump_core": "#9b59d0", "tank": "#b08ad6", "rcs": "#d6b13d", "computer": "#3dbf9a", "radar": "#2fa58a", "comm": "#57c7b0",
    "transponder": "#57c7b0", "ecm": "#2a8f78", "sensor_mast": "#2a8f78", "life_support": "#5aa04a", "escape_capsule": "#8cc56f",
    "lifeboat": "#8cc56f", "medbay": "#8cc56f", "container_rack": "#8a8a8a", "passenger_module": "#a3a07a", "train_module": "#8a8a8a",
    "cryo_tank": "#6fa6b5", "bulk_hold": "#8a8a8a", "docking_port": "#d0d0d0", "boat_bay": "#bdbdbd", "drone_bay": "#bdbdbd",
    "hangar": "#bdbdbd", "tow_rig": "#a8783d", "workshop": "#a8783d", "heat_shield": "#7a5a3d", "landing_gear": "#7a5a3d",
    "weapon": "#c0392b", "armour": "#5d6d7e",
}


def load(name, base=CANON):
    with open(os.path.join(base, name), encoding="utf-8") as f:
        return json.load(f)


def r(x, n=2):
    return round(x, n) if isinstance(x, float) else x


def compute():
    D = load("ship_designs.json")
    ECON = load("ship_economics.json")
    E = ECON["ship_classes"]
    C20_TARE_T = ECON["cargo_model"]["container_tare_t"]   # canonical empty C20 (runtime: ShipCargoPassports.h C20TareTonnes)
    I = load("industry.json")
    channels = load(os.path.join("..", "out", "world_3126", "industry_3126.json"))["channels"]
    R = D["rules"]
    prods = {p["id"]: p for p in D["products"]}
    mfr = {m["id"]: m for m in I["manufacturers"]}
    bloc_of = {pol: b for b, v in I["blocs"].items() for pol in v["members"]}
    school_of_bloc = {b: v["design"] for b, v in I["blocs"].items()}
    designs = I["designs"]

    def bloc_of_mfr(mid):
        return bloc_of.get(mfr[mid]["polity"]) if mid in mfr else None

    out_ships = []
    for s in D["ships"]:
        chk = []

        def add(name, ok, detail, level="FAIL"):
            chk.append(dict(check=name, ok=bool(ok), level="OK" if ok else level, detail=detail))

        cls = E[s["cls"]]
        bloc = bloc_of.get(s["polity"])
        school = s.get("school_override") or school_of_bloc.get(bloc, "none")
        sx = designs.get(school, {"price_x": 1.0, "life_x": 1.0})
        layout_key = s["law"] + s["layout"]
        lay = R["layouts"][layout_key]

        # ---- installed items with installation codes
        root = s["designation"]
        items = []
        per_asm = {}
        for e in s["eq"]:
            code_letter, pid, qty = e[0], e[1], e[2]
            extra = e[3] if len(e) > 3 else {}
            p = prods[pid]
            asm = CAT_ASSEMBLY[p["cat"]]
            idx = per_asm.get(asm, 0) + 1
            per_asm[asm] = idx
            items.append(dict(key=code_letter, product=pid, cat=p["cat"], qty=qty, code=f"{root}-{asm}A{idx}", assembly=asm, extra=extra))
        by_cat = {}
        for it in items:
            by_cat.setdefault(it["cat"], []).append(it)

        def qty(cat):
            return sum(i["qty"] for i in by_cat.get(cat, []))

        def total(cat, field):
            return sum(prods[i["product"]].get(field, 0.0) * i["qty"] for i in by_cat.get(cat, []))

        persons = s["crew"] + s["passengers"]
        cold = sum(prods[i["product"]].get("seats", 0) * i["qty"] for i in by_cat.get("passenger_module", []) if prods[i["product"]].get("cold_sleep"))
        awake = persons - cold

        # ---- propulsion layout: one drive per main slot; one generator per Newtonian main / Assisted fore-aft pair
        n_drives = qty("drive")
        add("drives_fill_layout_slots", n_drives == lay["aft"] + lay["fore"], f"{n_drives} drives for {layout_key} ({lay['aft']} aft + {lay['fore']} fore)")
        n_groups = lay["aft"]
        add("one_generator_per_group", qty("reactor") == n_groups, f"{qty('reactor')} propulsion reactors for {n_groups} groups")
        drive = prods[by_cat["drive"][0]["product"]]
        reactor = prods[by_cat["reactor"][0]["product"]]
        drives_per_group = (lay["aft"] + lay["fore"]) // n_groups
        # Assisted: the fore and aft units of a pair never fire together at full -> one drive input per group
        group_demand = drive["input_mw"] * (1 if s["law"] == "Assisted" else drives_per_group)
        add("generator_covers_group_demand", reactor["mwe"] >= 0.98 * group_demand, f"{reactor['mwe']} MWe per group vs {group_demand} MW demand")

        # ---- mass budget
        dry = s.get("dry_mass_t", cls["dry_mass_t"])
        eq_mass = sum(prods[i["product"]]["mass_t"] * i["qty"] for i in items)
        fluids = sum(prods[i["product"]].get("capacity_t", 0) * i["qty"] for i in by_cat.get("tank", []))
        ls_days = min((prods[i["product"]]["days"] for i in by_cat.get("life_support", [])), default=0)
        consumables = awake * min(ls_days, max(s["endurance_days"], 1)) * R["consumables_t_per_person_day"]
        structure = dry - eq_mass
        share = structure / dry
        lo, hi = R["structure_share_by_form"].get(s["hull"]["form"], R["structure_share_range"])
        plo, phi = R["structure_share_by_form"].get(s["hull"]["form"], R["structure_share_plausible"])
        add("equipment_fits_dry_mass", lo <= share <= hi, f"hull structure {structure:.0f} t = {share:.0%} of dry {dry} t (possible {lo:.0%}-{hi:.0%})")
        add("hull_structure_share_typical", plo <= share <= phi, f"hull structure {share:.0%} of dry (typical {plo:.0%}-{phi:.0%})", "WARN")
        # Mass ladder (one meaning everywhere: designer, runtime ShipCore, economics):
        #   dry     - hull + installed equipment, empty tanks
        #   service - dry + full tanks + full stores + crew aboard (ready to fly, no payload)
        #   net     - cargo payload (cargo_t) + passengers (90 kg with kit)
        #   tare    - empty C20 boxes in EVERY declared place (a full passport load)
        #   gross   - service + net + tare: everything aboard at a full passport load
        service = dry + fluids + consumables + 0.09 * s["crew"]
        container_tare = s["c20"] * C20_TARE_T
        gross = service + s["cargo_t"] + 0.09 * s["passengers"] + container_tare
        thrust = drive["thrust_mn"] * lay["aft"]
        a_service = thrust * 1e6 / (service * 1e3)
        a_full = thrust * 1e6 / (gross * 1e3)
        target_g = s.get("burn_accel_g", cls["burn_accel_g"])
        add("thrust_meets_class_acceleration", a_full >= 0.85 * target_g * G0,
            f"{thrust:.2f} MN: {a_full / G0:.2f} g loaded, {a_service / G0:.2f} g in service (class burn {target_g} g)")
        tol = R["physics_tolerance"]
        add("thrust_near_class_target", abs(thrust / cls["thrust_mn"] - 1) <= tol, f"{thrust:.1f} MN vs class {cls['thrust_mn']} MN", "WARN")

        # ---- power
        prop_mwe = reactor["mwe"] * qty("reactor")
        drive_input = drive["input_mw"] * (lay["aft"] if s["law"] == "Assisted" else n_drives)
        hotel = persons * R["hotel_mw_per_person"] + sum(prods[i["product"]].get("power_mw", 0) * i["qty"] for i in items
                                                       if i["cat"] not in ("drive", "rcs", "weapon"))
        weapons_power = total("weapon", "power_mw")
        pumps = total("coolant_loop", "pump_mw")
        hotel += pumps + 0.3 * weapons_power
        apu = total("service_reactor", "mwe")
        add("service_power_covers_hotel", apu >= 1.15 * hotel, f"service {apu:.0f} MWe vs hotel {hotel:.1f} MW (+15 %)")
        rcs_mw = total("rcs", "input_mw")
        if s["law"] == "Newtonian":
            # the runtime Newtonian RCS bank: 8 mount nodes x 3 fixed nozzles (24 nozzle units, or k clusters per node)
            add("rcs_fills_newtonian_bank", qty("rcs") == 24 or (qty("rcs") > 0 and qty("rcs") % 8 == 0),
                f"{qty('rcs')} RCS units on the 8-node Newtonian bank")
        rcs_peak = rcs_mw * R["rcs_simultaneous_share"]
        spare = prop_mwe - drive_input
        add("rcs_powered_from_propulsion_spare", rcs_peak <= max(spare, 0) + 1e-9 or rcs_mw == 0,
            f"RCS {rcs_mw:.0f} MW installed, {rcs_peak:.0f} MW simultaneous, from propulsion spare {spare:.0f} MW")
        add("propulsion_power_near_class_target", abs(prop_mwe / cls["reactor_mwe"] - 1) <= tol + 0.2,
            f"{prop_mwe:.0f} MWe vs class {cls['reactor_mwe']} MWe", "WARN")

        # ---- thermal (universe law)
        rx_waste = reactor["mwe"] * (1.0 / reactor["eff"] - 1.0)
        pad_m2 = round(1.3 * rx_waste / R["pad_cooling_mw_per_m2"], 1)
        dr_waste = drive["input_mw"] * (1.0 - drive["eff"])
        emitter_mw_per_m2 = R["radiation_effectiveness"] * 0.9 * 0.9 * SIGMA * 2500.0 ** 4 * 1e-6
        emitter_d = math.sqrt(4.0 * 1.5 * dr_waste / emitter_mw_per_m2 / math.pi)
        apu_p = prods[by_cat["service_reactor"][0]["product"]]
        service_heat = hotel + apu * (1.0 / apu_p["eff"] - 1.0) * (hotel / apu if apu else 0)
        loop_cap = total("coolant_loop", "capacity_mw")
        rad_area = total("radiator", "area_m2")
        rad = prods[by_cat["radiator"][0]["product"]]
        rad_mw = R["radiation_effectiveness"] * rad["eps"] * SIGMA * rad["temp_k"] ** 4 * rad_area * 1e-6
        add("coolant_loops_carry_service_heat", loop_cap >= 1.1 * service_heat, f"loops {loop_cap:.0f} MW vs service heat {service_heat:.1f} MW")
        add("radiators_reject_service_heat", rad_mw >= 1.1 * service_heat,
            f"{rad_area:.0f} m2 at {rad['temp_k']} K = {rad_mw:.0f} MW vs {service_heat:.1f} MW")
        sustainable = min(1.0, (emitter_mw_per_m2 * math.pi * emitter_d ** 2 / 4) / max(dr_waste, 1e-9),
                          (pad_m2 * R["pad_cooling_mw_per_m2"]) / max(rx_waste, 1e-9))

        # ---- jump
        if cls["jump"]:
            core = prods[by_cat["jump_core"][0]["product"]] if "jump_core" in by_cat else None
            add("jump_core_installed", core is not None, "jump class needs a core")
            if core:
                add("jump_core_rating_covers_gross", core["rating_t"] >= gross * R["jump_rating_margin"],
                    f"core {core['rating_t']} t vs gross {gross:.0f} t")
            charge = sum(prods[i["product"]]["capacity_t"] * i["qty"] for i in by_cat.get("tank", []) if prods[i["product"]]["fluid"] == "jump_charge")
            add("jump_charge_capacity", charge >= cls["jump_tank_t"] * 0.99, f"{charge:.0f} t charge vs class {cls['jump_tank_t']} t")
        else:
            add("no_jump_core_on_insystem_class", "jump_core" not in by_cat, "in-system class")

        # ---- life, survival, payload
        ls_persons = total("life_support", "persons") + total("passenger_module", "ls_persons")
        add("life_support_capacity", ls_persons >= awake, f"{ls_persons} places for {awake} awake persons ({cold} in cold sleep)")
        add("life_support_endurance", ls_days >= s["endurance_days"], f"{ls_days} days vs endurance {s['endurance_days']} days")
        seats = (sum(prods[i["product"]].get("seats", 0) * i["qty"] for i in items if i["cat"] in ("escape_capsule", "lifeboat"))
                 + total("passenger_module", "survival_seats"))
        add("survival_seats_for_everyone_awake", seats >= awake * R["survival_seat_share"],
            f"{seats} survival seats for {awake} awake persons (cold-sleep blocks are their own survival cells)")
        racks = total("container_rack", "c20_slots") + total("train_module", "c20_slots")
        add("container_slots_match", racks == s["c20"], f"{racks:.0f} C20 places vs {s['c20']}")
        pax_seats = total("passenger_module", "seats")
        alt_pax = all(i["extra"].get("alt_load") for i in by_cat.get("passenger_module", [])) if "passenger_module" in by_cat else False
        add("passenger_places", pax_seats >= s["passengers"], f"{pax_seats:.0f} places vs {s['passengers']} passengers" + (" (alternative load to cargo)" if alt_pax else ""))
        tm = total("train_module", "payload_t") + total("cryo_tank", "capacity_t")
        if tm:
            add("module_payload_matches_cargo", abs(tm - s["cargo_t"]) <= 0.02 * s["cargo_t"], f"{tm:.0f} t in modules vs cargo {s['cargo_t']} t")
        towed = total("tow_rig", "tow_t") if cls["role"] == "tug" else 0
        add("cargo_near_class_target", abs(max(s["cargo_t"], towed) / max(cls["cargo_t"], 1) - 1) <= tol or s.get("variant_of"),
            f"{s['cargo_t']} t vs class {cls['cargo_t']} t", "WARN")

        # ---- suppliers through bloc channels
        bad = []
        grey = []
        for it in items:
            p = prods[it["product"]]
            pb = bloc_of_mfr(p["mfr"])
            if pb is None or pb == bloc:
                continue
            ch = channels.get(f"{bloc}|{pb}", "restricted")
            military = p["cat"] in ("weapon", "jump_core", "armour")
            note = it["extra"].get("supplier_note", "")
            if ch == "black" or (ch == "restricted" and military):
                (grey if note else bad).append(f"{p['model']} ({pb}, {ch}{'; ' + note if note else ''})")
        add("suppliers_reachable_through_channels", not bad, "; ".join(bad) or "all suppliers inside open / licensed / restricted-civil channels")
        if grey:
            add("grey_supply_declared", True, "; ".join(grey))

        # ---- price by component group vs economics x school
        groups = {}
        for it in items:
            g = PRICE_GROUP.get(it["cat"])
            if g:
                groups[g] = groups.get(g, 0) + prods[it["product"]]["price_cr"] * it["qty"]
        target = {g: v * sx["price_x"] for g, v in cls["components_cr"].items() if g != "hull_structure"}
        dev = {g: (groups.get(g, 0) / v - 1) if v else None for g, v in target.items()}
        worst = max((abs(v) for v in dev.values() if v is not None), default=0)
        add("component_prices_near_class_model", worst <= R["price_tolerance"] or bool(s.get("variant_of")),
            ", ".join(f"{g} {groups.get(g, 0) / 1e6:.2f}/{v / 1e6:.2f} M" for g, v in target.items() if v), "WARN")
        hull_cr = cls["components_cr"]["hull_structure"] * sx["price_x"] * (dry / cls["dry_mass_t"])
        parts = sum(prods[i["product"]]["price_cr"] * i["qty"] for i in items)
        labour = cls.get("labour_share", 0.16)
        list_price = (parts + hull_cr) * (1 + labour) * (1 + cls.get("yard_margin", 0.09)) * (1 + cls.get("sales_tax", 0.05))

        # ---- functional schematic (nodes = installation positions, edges = networks)
        nodes, edges = [], []
        for it in items:
            p = prods[it["product"]]
            lvl, lru, hrs = REPAIR[it["cat"]]
            nodes.append(dict(code=it["code"], product=it["product"], name=p["name"], cat=it["cat"], qty=it["qty"], assembly=it["assembly"],
                              repair_level=lvl, line_replaceable=lru, replace_hours=hrs, overhaul_h=p.get("overhaul_h"), life_h=p.get("life_h"),
                              condition=it["extra"].get("condition", "new")))
        code = {i["key"]: i["code"] for i in items}
        cat_codes = lambda c: [i["code"] for i in by_cat.get(c, [])]

        def link(net, a, b):
            if a and b:
                edges.append(dict(net=net, src=a, dst=b))
        rx, dr = cat_codes("reactor")[0], cat_codes("drive")[0]
        link("power_propulsion", rx, dr)
        for c in cat_codes("rcs"):
            link("power_propulsion", rx, c)
        sv = cat_codes("service_reactor")[0]
        consumers = [i["code"] for i in items if i["cat"] in ("computer", "radar", "comm", "transponder", "ecm", "sensor_mast", "life_support",
                                                             "medbay", "workshop", "weapon", "coolant_loop", "drone_bay", "hangar", "boat_bay",
                                                             "jump_core", "passenger_module", "cryo_tank", "docking_port", "tow_rig")]
        for c in consumers:
            link("power_service", sv, c)
        loop = cat_codes("coolant_loop")[0]
        for c in [dr, rx, sv] + [i["code"] for i in items if i["cat"] in ("computer", "radar", "ecm", "life_support", "weapon", "jump_core")]:
            link("coolant", c, loop)
        for c in cat_codes("radiator"):
            link("coolant", loop, c)
        for i in by_cat.get("tank", []):
            fl = prods[i["product"]]["fluid"]
            if fl == "fusion_pellets":
                link("fuel", i["code"], rx)
                link("fuel", i["code"], sv)
            elif fl == "jump_charge":
                for c in cat_codes("jump_core"):
                    link("fuel", i["code"], c)
            elif fl == "argon":
                for c in cat_codes("rcs"):
                    link("fuel", i["code"], c)
        mc = cat_codes("computer")[0]
        for i in items:
            if i["cat"] in ("drive", "reactor", "rcs", "jump_core", "radar", "comm", "weapon", "ecm", "service_reactor", "life_support", "coolant_loop"):
                link("data", mc, i["code"])
        for c in cat_codes("life_support"):
            for e2 in cat_codes("passenger_module") + cat_codes("medbay"):
                link("life", c, e2)

        # functions -> required items (each entry: alternatives that can stand in for one another)
        def units(cat):
            return [(i["code"], i["qty"]) for i in by_cat.get(cat, [])]

        def need(*cats):
            return [units(c) for c in cats if units(c)]
        functions = dict(
            thrust=need("drive", "reactor", "coolant_loop", "computer", "tank"),
            jump=need("jump_core", "computer", "service_reactor") if cls["jump"] else [],
            life=need("life_support", "service_reactor", "coolant_loop"),
            sensors=need("radar", "service_reactor", "computer"),
            comms=need("comm", "transponder"),
            attitude=need("rcs", "reactor") if "rcs" in by_cat else need("drive", "reactor"),
        )
        spof = sorted({alts[0][0] for f, req in functions.items() for alts in req if len(alts) == 1 and alts[0][1] == 1})
        redundancy = {f: (min(sum(q for _, q in alts) for alts in req) if req else 0) for f, req in functions.items()}
        # crewed passenger / security / military ships: thrust, life and comms survive any single failure
        # (the sealed jump core is a factory item by doctrine and is not counted here)
        core_codes = set(cat_codes("jump_core"))
        critical = {f for f in ("thrust", "life", "comms")}
        crit_spof = sorted({alts[0][0] for f, req in functions.items() if f in critical for alts in req
                            if len(alts) == 1 and alts[0][1] == 1 and alts[0][0] not in core_codes})
        strict = s["passengers"] > 0 or cls["family"] in ("military", "security")
        add("no_single_point_of_failure_in_critical_functions", not crit_spof, ", ".join(crit_spof) or "thrust / life / comms redundant",
            "FAIL" if strict else "WARN")

        out_ships.append(dict(
            id=s["id"], designation=root, name=s["name"], cls=s["cls"], base=s.get("base", False), variant_of=s.get("variant_of"),
            variant_reason=s.get("variant_reason"), manufacturer=s["mfr"], manufacturer_name=mfr[s["mfr"]]["name"],
            developer=s.get("developer"), polity=s["polity"], bloc=bloc, school=school, yard=s["yard"], purpose=s["purpose"],
            flight=dict(law=s["law"], layout=s["layout"], runtime_model=layout_key, runtime=s.get("runtime", "generic")),
            hull=dict(s["hull"], structure_t=r(structure, 1)), crew=s["crew"], passengers=s["passengers"], cold_sleep=cold,
            payload=dict(cargo_t=s["cargo_t"], c20=s["c20"], passenger_places=pax_seats),
            endurance_days=s["endurance_days"], service_life_years=round(s["service_life_years"] * sx.get("life_x", 1.0)),
            mass=dict(dry_t=dry, equipment_t=r(eq_mass, 1), structure_t=r(structure, 1), fluids_t=r(fluids, 3), consumables_t=r(consumables, 3),
                      service_t=r(service, 3), net_payload_t=r(s["cargo_t"] + 0.09 * s["passengers"], 2),
                      container_tare_t=r(container_tare, 2), c20_tare_t=C20_TARE_T, gross_t=r(gross, 3)),
            propulsion=dict(drive=drive["id"], drives=n_drives, aft=lay["aft"], fore=lay["fore"], thrust_mn=r(thrust, 2),
                            accel_g_loaded=r(a_full / G0, 3), accel_g_service=r(a_service / G0, 3), class_burn_g=target_g,
                            drive_emitter_diameter_m=r(emitter_d, 2), sustainable_thrust01=r(sustainable, 3)),
            power=dict(reactor=reactor["id"], generators=qty("reactor"), propulsion_mwe=prop_mwe, drive_input_mw=drive_input,
                       service_mwe=apu, hotel_mw=r(hotel, 1), rcs_mw=rcs_mw, reactor_pad_m2=pad_m2),
            thermal=dict(reactor_waste_mw=r(rx_waste, 1), drive_waste_mw=r(dr_waste, 1), service_heat_mw=r(service_heat, 1),
                         coolant_capacity_mw=loop_cap, radiator_area_m2=rad_area, radiator_temp_k=rad["temp_k"], radiator_eps=rad["eps"],
                         radiator_mw=r(rad_mw, 1), radiator_material=rad.get("material", "")),
            price=dict(parts_cr=round(parts), hull_cr=round(hull_cr), list_cr=round(list_price, -3),
                       groups_cr={g: round(v) for g, v in groups.items()}, class_target_cr={g: round(v) for g, v in target.items()}),
            installed=items, schematic=dict(nodes=nodes, edges=edges, functions={f: [[a for a, _ in alts] for alts in req] for f, req in functions.items()},
                                            single_points_of_failure=spof, redundancy=redundancy),
            checks=chk,
        ))
    return D, prods, mfr, out_ships


# ------------------------------------------------------------------------------------------- drawings
def svg_views(s, prods):
    L, W, H = s["hull"]["envelope_m"]
    form = s["hull"]["form"]
    pad, vw = 20, 300.0
    scale = vw / max(L, W, H)
    w_px, h_px = 3 * (vw + pad) + pad, vw + 2 * pad + 70
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w_px:.0f} {h_px:.0f}" font-family="Segoe UI, Arial, sans-serif" font-size="10">',
           f'<rect width="100%" height="100%" fill="#0e1620"/>',
           f'<text x="{pad}" y="16" fill="#f0b46a" font-size="14">{s["name"]} — {s["manufacturer_name"]}</text>']

    def hull_shape(cx, cy, a, b, view):
        # a: extent along screen x, b: along screen y (meters * scale)
        if form in ("wedge", "dart") and view == "top":
            return f'<polygon points="{cx},{cy - b / 2} {cx + a / 2},{cy + b / 2} {cx - a / 2},{cy + b / 2}" />'
        if form in ("cylinder", "lifter", "train") and view == "front":
            return f'<ellipse cx="{cx}" cy="{cy}" rx="{a / 2}" ry="{b / 2}" />'
        if form == "asteroid":
            pts = " ".join(f"{cx + a / 2 * math.cos(t) * (0.85 + 0.15 * math.sin(3 * t + 1))},{cy + b / 2 * math.sin(t) * (0.85 + 0.15 * math.cos(5 * t))}"
                           for t in [k * math.pi / 9 for k in range(18)])
            return f'<polygon points="{pts}" />'
        if form in ("cylinder", "lifter") and view in ("top", "side"):
            rr = min(a, b) / 2
            return f'<rect x="{cx - a / 2}" y="{cy - b / 2}" width="{a}" height="{b}" rx="{rr}" />'
        if form == "tug":
            return f'<rect x="{cx - a / 2}" y="{cy - b / 2}" width="{a}" height="{b}" fill-opacity="0.25" stroke-dasharray="4 2" />'
        return f'<rect x="{cx - a / 2}" y="{cy - b / 2}" width="{a}" height="{b}" rx="3" />'

    views = [("top", "вид сверху", W, L, (0, 2)), ("side", "вид сбоку", H, L, (1, 2)), ("front", "вид спереди", W, H, (0, 1))]
    for k, (view, label, ext_x, ext_y, axes) in enumerate(views):
        ox = pad + k * (vw + pad)
        cx, cy = ox + vw / 2, pad + 30 + vw / 2
        out.append(f'<rect x="{ox}" y="{pad + 30}" width="{vw}" height="{vw}" fill="none" stroke="#24364a"/>')
        out.append(f'<text x="{ox + 4}" y="{pad + 44}" fill="#8fb3d1">{label}</text>')
        if view in ("top", "side"):
            # fore up, aft down
            a, b = ext_x * scale, ext_y * scale
        else:
            a, b = ext_x * scale, ext_y * scale
        out.append(f'<g fill="#1f3348" stroke="#7fa6c9" stroke-width="1.2">{hull_shape(cx, cy, a, b, view)}</g>')
        rad_area = s["thermal"]["radiator_area_m2"]
        if view in ("top", "front") and rad_area:
            # radiator wings: two panels, span from area over the hull length band they occupy
            band = (b if view == "top" else a) * 0.35
            span_m = rad_area / 2.0 / max(L * 0.35, 1.0)
            span = min(span_m * scale, vw * 0.45 - a / 2)
            if view == "top" and span > 1:
                for sgn in (-1, 1):
                    x0 = cx + sgn * a / 2 if sgn > 0 else cx - a / 2 - span
                    out.append(f'<rect x="{x0:.1f}" y="{cy + b * 0.05:.1f}" width="{span:.1f}" height="{band:.1f}" fill="#7fb8e6" fill-opacity="0.25" '
                               f'stroke="#7fb8e6" stroke-width="0.8"><title>радиаторы {rad_area:.0f} м2</title></rect>')
            elif view == "front" and span > 1:
                for sgn in (-1, 1):
                    x0 = cx + a / 2 if sgn > 0 else cx - a / 2 - span
                    out.append(f'<line x1="{x0:.1f}" y1="{cy:.1f}" x2="{x0 + span:.1f}" y2="{cy:.1f}" stroke="#7fb8e6" stroke-width="2"/>')
        mods = [i for i in s["installed"] if i["cat"] in ("train_module", "passenger_module", "cryo_tank") and not i["extra"].get("alt_load")]
        if mods and view in ("top", "side"):
            n = sum(i["qty"] for i in mods)
            seg = b * 0.6 / max(n, 1)
            k = 0
            for i in mods:
                for _ in range(i["qty"]):
                    y0 = cy - b * 0.32 + k * seg
                    out.append(f'<rect x="{cx - a * 0.32:.1f}" y="{y0:.1f}" width="{a * 0.64:.1f}" height="{max(seg - 1.5, 1):.1f}" fill="{CAT_COLOR[i["cat"]]}" '
                               f'fill-opacity="0.35" stroke="{CAT_COLOR[i["cat"]]}" stroke-width="0.6"/>')
                    k += 1
        dims = {"top": f"{W:g} × {L:g} м", "side": f"{H:g} × {L:g} м", "front": f"{W:g} × {H:g} м"}[view]
        out.append(f'<text x="{ox + vw - 4}" y="{pad + vw + 26}" fill="#8fb3d1" text-anchor="end">{dims}</text>')
        # equipment glyphs at their normalized positions
        for it in s["installed"]:
            for pos in it.get("positions", []):
                px, py, pz = pos
                if view == "top":
                    gx, gy = cx + px * a / 2 * 0.85, cy + pz * b / 2 * 0.85
                elif view == "side":
                    gx, gy = cx + py * a / 2 * 0.85, cy + pz * b / 2 * 0.85
                else:
                    gx, gy = cx + px * a / 2 * 0.85, cy - py * b / 2 * 0.85
                col = CAT_COLOR.get(it["cat"], "#cccccc")
                rad = 4.5 if it["cat"] in ("drive", "reactor", "jump_core") else 3
                out.append(f'<circle cx="{gx:.1f}" cy="{gy:.1f}" r="{rad}" fill="{col}" fill-opacity="0.9"><title>{it["code"]} {prods[it["product"]]["name"]}</title></circle>')
    # legend
    ly = pad + vw + 48
    cats = []
    for it in s["installed"]:
        if it["cat"] not in cats:
            cats.append(it["cat"])
    for k, c in enumerate(cats[:14]):
        x = pad + (k % 7) * 130
        y = ly + (k // 7) * 14
        out.append(f'<circle cx="{x + 5}" cy="{y - 3}" r="4" fill="{CAT_COLOR.get(c, "#ccc")}"/><text x="{x + 13}" y="{y}" fill="#c8d6e5">{CAT_RU.get(c, c)}</text>')
    out.append("</svg>")
    return "\n".join(out)


def svg_schematic(s, prods):
    sc = s["schematic"]
    cols = {"A4": 0, "A2": 1, "A3": 2, "A1": 3, "A5": 3, "A6": 4, "A7": 5, "A8": 5, "A9": 4, "A10": 5, "A11": 4, "A12": 5}
    colx = [70, 230, 390, 550, 710, 870]
    rows = {}
    pos = {}
    for n in sc["nodes"]:
        c = cols.get(n["assembly"], 5)
        k = rows.get(c, 0)
        rows[c] = k + 1
        pos[n["code"]] = (colx[c], 70 + k * 46)
    h = 100 + max(rows.values()) * 46
    net_col = {"power_propulsion": "#e8762b", "power_service": "#f0d060", "coolant": "#3d8fd6", "fuel": "#b08ad6", "data": "#3dbf9a", "life": "#8cc56f"}
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1000 {h}" font-family="Segoe UI, Arial, sans-serif" font-size="10">',
           '<rect width="100%" height="100%" fill="#0e1620"/>',
           f'<text x="16" y="22" fill="#f0b46a" font-size="14">{s["name"]}: функциональная схема</text>']
    for k, (net, col) in enumerate(net_col.items()):
        out.append(f'<line x1="{560 + k * 72}" y1="18" x2="{580 + k * 72}" y2="18" stroke="{col}" stroke-width="2"/>'
                   f'<text x="{584 + k * 72}" y="22" fill="#c8d6e5">{net.replace("power_", "")}</text>')
    for e in sc["edges"]:
        if e["src"] in pos and e["dst"] in pos:
            (x1, y1), (x2, y2) = pos[e["src"]], pos[e["dst"]]
            out.append(f'<path d="M{x1 + 60},{y1} C{(x1 + x2) / 2 + 60},{y1} {(x1 + x2) / 2 - 60},{y2} {x2 - 60},{y2}" fill="none" '
                       f'stroke="{net_col[e["net"]]}" stroke-opacity="0.55" stroke-width="1.2"/>')
    spof = set(sc["single_points_of_failure"])
    for n in sc["nodes"]:
        x, y = pos[n["code"]]
        stroke = "#ff5b5b" if n["code"] in spof else "#7fa6c9"
        out.append(f'<rect x="{x - 60}" y="{y - 16}" width="120" height="32" rx="4" fill="#1f3348" stroke="{stroke}" stroke-width="{2 if n["code"] in spof else 1}"/>')
        label = prods[n["product"]]["model"] + (f" ×{n['qty']}" if n["qty"] > 1 else "")
        out.append(f'<text x="{x}" y="{y - 3}" fill="#e6eef6" text-anchor="middle">{label}</text>')
        out.append(f'<text x="{x}" y="{y + 10}" fill="#8fb3d1" text-anchor="middle" font-size="8">{n["code"]} · {n["repair_level"]}</text>')
    out.append(f'<text x="16" y="{h - 12}" fill="#ff8a8a">красная рамка — единичная точка отказа функции</text>')
    out.append("</svg>")
    return "\n".join(out)


# Runtime slot ids of the six fixed layouts (src/game/ship/plant/CanonicalActuatorLayouts.h), in canonical order.
SLOT_IDS = {
    ("Newtonian", "I"): ["main_center"],
    ("Newtonian", "Y"): ["main_top", "main_lower_left", "main_lower_right"],
    ("Newtonian", "X"): ["main_upper_left", "main_upper_right", "main_lower_left", "main_lower_right"],
    ("Assisted", "I"): ["aft_left", "aft_right", "fore_left", "fore_right"],
    ("Assisted", "Y"): ["aft_top", "aft_lower_left", "aft_lower_right", "fore_top", "fore_lower_left", "fore_lower_right"],
    ("Assisted", "X"): ["aft_upper_left", "aft_upper_right", "aft_lower_left", "aft_lower_right",
                        "fore_upper_left", "fore_upper_right", "fore_lower_left", "fore_lower_right"],
}
GROUP_IDS = {"I": ["drive_center"], "Y": ["drive_top", "drive_lower_left", "drive_lower_right"],
             "X": ["drive_upper_left", "drive_upper_right", "drive_lower_left", "drive_lower_right"]}
ASSISTED_GROUP_IDS = {"I": ["drive_left", "drive_right"], "Y": GROUP_IDS["Y"], "X": GROUP_IDS["X"]}
RCS_NODE_IDS = ["rcs_fore_upper_left", "rcs_fore_upper_right", "rcs_fore_lower_left", "rcs_fore_lower_right",
                "rcs_aft_upper_left", "rcs_aft_upper_right", "rcs_aft_lower_left", "rcs_aft_lower_right"]
# Equipment density (t/m3) and box aspect (w, h, l) of primitive blocks for products without an authored size.
BLOCK = {
    "drive": (1.6, (1, 1, 2.2)), "reactor": (2.2, (1, 1, 1.2)), "service_reactor": (2.2, (1, 1, 1)), "coolant_loop": (0.8, (2, 1, 1)),
    "jump_core": (2.5, (1, 1, 1)), "tank": (0.9, (1, 1, 1.6)), "rcs": (1.0, (1, 1, 1)), "computer": (0.6, (1, 0.6, 1)),
    "radar": (0.35, (1, 0.4, 1)), "comm": (0.4, (1, 0.5, 1)), "transponder": (0.5, (1, 1, 1)), "ecm": (0.4, (1, 0.5, 1.2)),
    "sensor_mast": (0.3, (0.6, 2, 0.6)), "life_support": (0.45, (1.4, 0.8, 1)), "escape_capsule": (0.25, (1, 1, 1.4)),
    "lifeboat": (0.15, (1, 0.7, 2.2)), "medbay": (0.2, (1.4, 0.7, 1)), "docking_port": (0.6, (1, 1, 0.5)),
    "tow_rig": (1.2, (1.6, 1.6, 0.6)), "workshop": (0.25, (1.4, 0.8, 1)), "landing_gear": (1.5, (1, 1.4, 1)),
    "weapon": (0.8, (1, 0.6, 1.6)), "armour": (7.8, (3, 2, 0.15)), "heat_shield": (2.0, (2, 0.1, 2)),
}


def _box(volume, aspect):
    k = (max(volume, 1e-3) / (aspect[0] * aspect[1] * aspect[2])) ** (1 / 3)
    return [aspect[0] * k, aspect[1] * k, aspect[2] * k]


def place(s, prods):
    """Normalized mount positions (x right, y up, z +aft), runtime slot ids and primitive blocks (metres) of every unit."""
    lay = s["flight"]["layout"]
    law = s["flight"]["law"]
    assisted = law == "Assisted"
    existing = s["flight"]["runtime"].startswith("existing:")
    L, W, H = s["hull"]["envelope_m"]
    half = (W / 2, H / 2, L / 2)
    for it in s["installed"]:
        n = it["qty"]
        c = it["cat"]
        p = prods[it["product"]]
        slots = None
        if c == "drive":
            pts = (SLOT_POS[lay]["assisted_aft"] + SLOT_POS[lay]["assisted_fore"]) if assisted else SLOT_POS[lay]["aft"]
            slots = SLOT_IDS[(law, lay)]
        elif c == "reactor":
            base = SLOT_POS[lay]["assisted_aft"] if assisted else SLOT_POS[lay]["aft"]
            pts = [(x * 0.7, y * 0.7, 0.55) for x, y, _ in base]
            if existing:   # Cobrett: the installed runtime generator positions (canonical skeleton)
                pts = [(0.0, 0.0, 0.65)] if not assisted else [(-0.7, 0.0, 0.0), (0.7, 0.0, 0.0)]
            slots = (ASSISTED_GROUP_IDS if assisted else GROUP_IDS)[lay]
        elif c == "rcs":
            r = (1.0, 1.0, 1.0) if existing else (0.9, 0.8, 0.9)   # Cobrett: the canonical nozzle bank corners
            pts = [(x * r[0], y * r[1], z * r[2]) for z in (-1, 1) for y in (1, -1) for x in (-1, 1)]
            slots = RCS_NODE_IDS
        elif c == "radiator":
            pts = [(-1.0, 0, 0.3), (1.0, 0, 0.3)]
        else:
            z = {"service_reactor": 0.3, "coolant_loop": 0.4, "jump_core": 0.1, "tank": 0.3, "computer": -0.75, "radar": -0.85, "comm": -0.8,
                 "transponder": -0.8, "ecm": -0.6, "sensor_mast": -0.5, "life_support": -0.6, "escape_capsule": -0.55, "lifeboat": -0.2,
                 "medbay": -0.5, "container_rack": 0.0, "passenger_module": -0.1, "train_module": 0.5, "cryo_tank": 0.0, "bulk_hold": 0.1,
                 "docking_port": -1.0, "boat_bay": 0.2, "drone_bay": 0.25, "hangar": 0.1, "tow_rig": -1.0, "workshop": -0.2, "heat_shield": 0.0,
                 "landing_gear": 0.6, "weapon": -0.3, "armour": -0.95}.get(c, 0.0)
            y = {"radar": 0.9, "sensor_mast": 1.0, "weapon": 0.8, "landing_gear": -1.0, "heat_shield": -0.9}.get(c, 0.0)
            k = min(n, 4)
            pts = [((j - (k - 1) / 2) * (0.5 if k > 1 else 0), y, z) for j in range(k)]
        it["positions"] = [list(map(lambda v: round(v, 3), q)) for q in pts]
        if slots is not None:
            it["slots"] = slots[: len(pts)]

        # ---- primitive blocks: one box per position; a bulk position carries its share of the units
        per = n / len(pts)
        if c == "radiator":
            side = p.get("area_m2", 10.0) * n / 2
            l = min(0.7 * L, max(2.0, side ** 0.5 * 1.5))
            size = [min(0.45 * W, side / l), 0.25, l]
        elif c == "container_rack":
            size = _box(38.3 * per, (2.44, 2.59, 6.06))
        elif c == "train_module":
            size = _box((p.get("payload_t", p.get("capacity_t", 4800)) / 0.7 + p["mass_t"] / 2.0) * per, (1, 1, 2.5))
        elif c == "passenger_module":
            size = _box(4.0 * p.get("seats", 50) * per, (1.4, 1, 2))
        elif c == "cryo_tank":
            size = _box((p.get("capacity_t", 100) / 0.8) * per, (1, 1, 1.8))
        elif c == "bulk_hold":
            size = _box((p.get("capacity_t", 100) / 0.7) * per, (1.4, 1, 2))
        elif c in ("hangar", "boat_bay", "drone_bay"):
            size = _box(250.0 * max(1, p.get("craft_slots", p.get("drones", 2))) * per, (1.6, 0.8, 2))
        elif c == "tank":
            size = _box((p.get("capacity_t", 0) / 0.9 + p["mass_t"] / 2.0) * per, BLOCK["tank"][1])
        else:
            rho, aspect = BLOCK.get(c, (1.0, (1, 1, 1)))
            size = _box(max(0.05, p["mass_t"] * per / rho), aspect)
        size = [min(v, 0.95 * 2 * h) for v, h in zip(size, half)]
        blocks = []
        for q in pts:
            ctr = [q[i] * half[i] for i in range(3)]
            if c == "drive":
                ctr[2] -= size[2] / 2 * (1 if q[2] > 0 else -1)   # the drive body sits inboard of its nozzle plane
            if c != "rcs":   # boxes stay inside the hull envelope; RCS nodes keep their mount point on the skin
                ctr = [max(-(half[i] - size[i] / 2), min(half[i] - size[i] / 2, ctr[i])) for i in range(3)]
            blocks.append(dict(c=[round(v, 3) for v in ctr], s=[round(v, 3) for v in size]))
        it["blocks"] = blocks


# ------------------------------------------------------------------------------------------- outputs
def report(D, prods, mfr, ships):
    L = ["# Корабли 3126 — проектные паспорта", "",
         "Статус: PROPOSAL. Генерирует `tools/colonization_lab/ship_designer.py` из `canon/ship_designs.json`; не править руками.", "",
         "| Модель | Класс | Производитель | Схема | Сухая, т | Груз | Тяга, МН | g груж./лёгк. | Реакторы, МВт | Ядро | Цена, млн Cr | Проверки |",
         "|---|---|---|---|---:|---|---:|---|---:|---|---:|---|"]
    for s in ships:
        bad = [c for c in s["checks"] if c["level"] == "FAIL"]
        warn = [c for c in s["checks"] if c["level"] == "WARN"]
        core = next((prods[i["product"]]["model"] for i in s["installed"] if i["cat"] == "jump_core"), "—")
        pay = f"{s['payload']['cargo_t']} т" + (f", {s['passengers']} пасс." if s["passengers"] else "")
        L.append(f"| {s['name']}{'' if s['base'] else ' (вариант)'} | {s['cls']} | {s['manufacturer_name']} | {s['flight']['runtime_model']} | "
                 f"{s['mass']['dry_t']} | {pay} | {s['propulsion']['thrust_mn']} | {s['propulsion']['accel_g_loaded']}/{s['propulsion']['accel_g_service']} | "
                 f"{s['power']['propulsion_mwe']} | {core} | {s['price']['list_cr'] / 1e6:.1f} | "
                 f"{'OK' if not bad else 'FAIL ' + str(len(bad))}{(' · WARN ' + str(len(warn))) if warn else ''} |")
    for s in ships:
        L += ["", f"## {s['name']} ({s['designation']})", "",
              f"- Класс: `{s['cls']}`{'' if s['base'] else ' — вариант ' + str(s['variant_of']) + ': ' + str(s['variant_reason'])}.",
              f"- Производитель: {s['manufacturer_name']} ({s['polity']}, блок {s['bloc']}, школа {s['school']}); верфь: {s['yard']}.",
              f"- Назначение: {s['purpose']}.",
              f"- Корпус: {s['hull']['form']}, {s['hull']['envelope_m'][0]} × {s['hull']['envelope_m'][1]} × {s['hull']['envelope_m'][2]} м; "
              f"{s['hull']['structure']}; {s['hull']['material']}; силовой набор {s['hull']['structure_t']} т.",
              f"- Схема управления: {s['flight']['law']} {s['flight']['layout']} (рантайм `{s['flight']['runtime_model']}`, {s['flight']['runtime']}).",
              f"- Экипаж {s['crew']}, пассажиры {s['passengers']}{' (в анабиозе ' + str(s['cold_sleep']) + ')' if s['cold_sleep'] else ''}; "
              f"груз {s['payload']['cargo_t']} т, C20 {s['payload']['c20']}; автономность {s['endurance_days']} сут; ресурс {s['service_life_years']} лет.",
              f"- Масса: сухая {s['mass']['dry_t']} т (оборудование {s['mass']['equipment_t']} т), в рабочем снаряжении {s['mass']['service_t']} т, полная {s['mass']['gross_t']} т (груз {s['mass']['net_payload_t']} т + тара C20 {s['mass']['container_tare_t']} т).",
              f"- Тяга {s['propulsion']['thrust_mn']} МН: {s['propulsion']['accel_g_loaded']} g гружёный (полная масса), {s['propulsion']['accel_g_service']} g в рабочем снаряжении; "
              f"длительно допустимая доля тяги {s['propulsion']['sustainable_thrust01']}.",
              f"- Энергия: {s['power']['generators']} × {prods[s['power']['reactor']]['model']} = {s['power']['propulsion_mwe']} МВт; "
              f"бытовая нагрузка {s['power']['hotel_mw']} МВт от {s['power']['service_mwe']} МВт; площадка реактора {s['power']['reactor_pad_m2']} м².",
              f"- Охлаждение: {s['thermal']['radiator_area_m2']} м² радиаторов при {s['thermal']['radiator_temp_k']} K = {s['thermal']['radiator_mw']} МВт; "
              f"контуры {s['thermal']['coolant_capacity_mw']} МВт; излучатель двигателя ⌀ {s['propulsion']['drive_emitter_diameter_m']} м.",
              f"- Цена: детали {s['price']['parts_cr'] / 1e6:.2f} млн, корпус {s['price']['hull_cr'] / 1e6:.2f} млн, прейскурант {s['price']['list_cr'] / 1e6:.2f} млн Cr.",
              f"- Единичные точки отказа: {', '.join(s['schematic']['single_points_of_failure']) or 'нет'}.", "",
              "| Позиция | Изделие | Кол. | Производитель | Ремонт |", "|---|---|---:|---|---|"]
        for n in s["schematic"]["nodes"]:
            p = prods[n["product"]]
            L.append(f"| {n['code']} | {p['name']} | {n['qty']} | {mfr.get(p['mfr'], {}).get('name', p['mfr'])} | "
                     f"{n['repair_level']}{', LRU' if n['line_replaceable'] else ''}{', ' + str(n['replace_hours']) + ' ч' if n['replace_hours'] else ''} |")
        L += ["", "Проверки:"] + [f"- {c['level']} `{c['check']}` — {c['detail']}" for c in s["checks"]]
    return "\n".join(L) + "\n"


def catalog_page(prods, ships, svgs):
    """One local page with every model: passport summary, drawing, functional schematic."""
    esc = lambda t: str(t).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    h = ['<!doctype html><html lang="ru"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">',
         '<title>Корабли 3126</title><style>',
         ':root{--bg:#0e1620;--fg:#e6eef6;--mut:#8fb3d1;--acc:#f0b46a;--line:#24364a}',
         'body{margin:0;background:var(--bg);color:var(--fg);font:14px/1.45 "Segoe UI",Arial,sans-serif}',
         'main{max-width:1100px;margin:0 auto;padding:16px}h1{color:var(--acc);font-weight:500}h2{color:var(--acc);font-weight:500;margin-top:40px}',
         'table{border-collapse:collapse;width:100%;margin:8px 0}td,th{border-bottom:1px solid var(--line);padding:4px 6px;text-align:left;vertical-align:top}',
         'th{color:var(--mut);font-weight:500}.svg svg{width:100%;height:auto;display:block;margin:8px 0}.mut{color:var(--mut)}',
         'nav a{color:var(--mut);margin-right:10px;text-decoration:none}nav a:hover{color:var(--acc)}',
         '</style></head><body><main><h1>Корабли 3126</h1>',
         '<p class="mut">PROPOSAL. Сгенерировано <code>ship_designer.py</code> из <code>canon/ship_designs.json</code>. Чертёж — упрощённая компоновка '
         '(точки — агрегаты по позициям установки); схема — сети питания, охлаждения, топлива, данных и жизнеобеспечения, красная рамка — единичная точка отказа.</p><nav>']
    h += [f'<a href="#{esc(s["id"])}">{esc(s["designation"])}</a>' for s in ships]
    h.append("</nav>")
    for s in ships:
        h.append(f'<h2 id="{esc(s["id"])}">{esc(s["name"])}{"" if s["base"] else " — вариант"}</h2>')
        h.append(f'<p>{esc(s["purpose"])}.' + (f' <span class="mut">Вариант: {esc(s["variant_reason"])}.</span>' if not s["base"] else "") + "</p>")
        rows = [("Производитель", f'{s["manufacturer_name"]} ({s["bloc"]}, школа {s["school"]}); верфь {s["yard"]}'),
                ("Корпус", f'{s["hull"]["form"]}, {" × ".join(str(x) for x in s["hull"]["envelope_m"])} м; {s["hull"]["structure"]}'),
                ("Схема управления", f'{s["flight"]["law"]} {s["flight"]["layout"]}'),
                ("Масса / нагрузка", f'сухая {s["mass"]["dry_t"]} т, груз {s["payload"]["cargo_t"]} т, C20 {s["payload"]["c20"]}, экипаж {s["crew"]}, пассажиры {s["passengers"]}'),
                ("Тяга", f'{s["propulsion"]["thrust_mn"]} МН — {s["propulsion"]["accel_g_loaded"]} g гружёный, {s["propulsion"]["accel_g_service"]} g в рабочем снаряжении'),
                ("Энергия / тепло", f'{s["power"]["propulsion_mwe"]} МВт двигательных, {s["power"]["service_mwe"]} МВт служебных; радиаторы {s["thermal"]["radiator_area_m2"]} м²'),
                ("Цена", f'{s["price"]["list_cr"] / 1e6:.2f} млн Cr'),
                ("Единичные точки отказа", ", ".join(s["schematic"]["single_points_of_failure"]) or "нет")]
        h.append("<table>" + "".join(f"<tr><th>{esc(a)}</th><td>{esc(b)}</td></tr>" for a, b in rows) + "</table>")
        h.append(f'<div class="svg">{svgs[s["id"]][0]}</div><div class="svg">{svgs[s["id"]][1]}</div>')
    h.append("</main></body></html>")
    return "\n".join(h) + "\n"


def runtime_copy(D, prods, ships):
    used = sorted({i["product"] for s in ships for i in s["installed"]})
    return dict(
        schema="elite.ship_catalog.v1", do_not_edit=True, generated_by="tools/colonization_lab/ship_designer.py",
        status="PROPOSAL", universe=dict(pad_cooling_mw_per_m2=D["rules"]["pad_cooling_mw_per_m2"]),
        products=[prods[p] for p in used],
        ships=[dict(id=s["id"], designation=s["designation"], name=s["name"], cls=s["cls"], manufacturer=s["manufacturer_name"],
                    runtime=s["flight"]["runtime"], physical_model=s["flight"]["runtime_model"], law=s["flight"]["law"],
                    envelope_m=s["hull"]["envelope_m"], form=s["hull"]["form"], dry_mass_t=s["mass"]["dry_t"],
                    crew=s["crew"], passengers=s["passengers"], cargo_t=s["payload"]["cargo_t"], c20=s["payload"]["c20"],
                    propulsion=s["propulsion"], power=s["power"], thermal=s["thermal"], endurance_days=s["endurance_days"],
                    installed=[dict(code=i["code"], product=i["product"], cat=i["cat"], qty=i["qty"], positions=i.get("positions", []),
                                    slots=i.get("slots", []), blocks=i.get("blocks", []))
                               for i in s["installed"]],
                    mass=s["mass"], price=s["price"], purpose=s["purpose"], manufacturer_id=s["manufacturer"],
                    service_life_years=s["service_life_years"], cold_sleep=s["cold_sleep"], schematic=s["schematic"]) for s in ships],
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    D, prods, mfr, ships = compute()
    for s in ships:
        place(s, prods)
    files = {
        os.path.join(OUT, "ship_designs_3126.json"): json.dumps(dict(status="PROPOSAL", ships=ships), ensure_ascii=False, indent=1) + "\n",
        os.path.join(OUT, "SHIP_DESIGNS_3126.md"): report(D, prods, mfr, ships),
        RUNTIME: json.dumps(runtime_copy(D, prods, ships), ensure_ascii=False, indent=1) + "\n",
    }
    svgs = {}
    for s in ships:
        svgs[s["id"]] = (svg_views(s, prods), svg_schematic(s, prods))
        files[os.path.join(OUT, "svg", s["id"] + ".svg")] = svgs[s["id"]][0]
        files[os.path.join(OUT, "svg", s["id"] + ".schematic.svg")] = svgs[s["id"]][1]
    files[os.path.join(OUT, "index.html")] = catalog_page(prods, ships, svgs)
    fails = [(s["id"], c) for s in ships for c in s["checks"] if c["level"] == "FAIL"]
    warns = [(s["id"], c) for s in ships for c in s["checks"] if c["level"] == "WARN"]
    if a.check:
        stale = [p for p, t in files.items() if not os.path.exists(p) or open(p, encoding="utf-8").read() != t]
        for p in stale:
            print("STALE:", p)
        for sid, c in fails:
            print(f"FAIL {sid} {c['check']}: {c['detail']}")
        ok = not stale and not fails
        print("ship_designer check", "OK" if ok else "FAILED")
        return 0 if ok else 1
    for p, t in files.items():
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(t)
    for sid, c in fails:
        print(f"FAIL {sid} {c['check']}: {c['detail']}")
    for sid, c in warns:
        print(f"WARN {sid} {c['check']}: {c['detail']}")
    print(f"{len(ships)} ships, {len(fails)} FAIL, {len(warns)} WARN")
    return 0 if not fails else 1


if __name__ == "__main__":
    sys.exit(main())
