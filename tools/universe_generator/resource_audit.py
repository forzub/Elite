"""Resource audit of the materialised catalog (natural endowment, no prices).

usage: python resource_audit.py GALAXY_DETAILS_ROOT OUT_MD

Per system: accessible water, accessible fuel, structural metals (Fe, Al,
Mg, Ti), high-value / strategic materials (platinum group, refractory
metals, actinides, helium-3), the major industrial bodies / belts, the best
settlement target, the lowest extraction-cost target and the major hazards.
Then a comparative-advantage table: which system leads each category - the
universe should show clear differences without one system holding the best
of everything.

Reachable mass: for a body the accessible fraction of its upper 10 km shell
(crust, regolith, ice, atmosphere for gases); for a small-body population
its total mass. Scores are log10 of reachable mass weighted by ore grade and
divided by the relative extraction cost from the generated
industrial_suitability layer.
"""
import glob
import json
import math
import os
import sys

STRUCTURAL = ("iron", "aluminium", "magnesium", "titanium")
STRATEGIC = ("platinum_group", "refractory_metals", "actinides", "helium3")
EARTH_KG = 5.9722e24


def load(p):
    return json.load(open(p, encoding="utf-8"))


def reachable_kg(doc, dep, family):
    if doc.get("schema", "").endswith("reservoir.v1"):
        return doc["total_mass_earth"] * EARTH_KG * dep["accessible_mass_fraction"]
    phys = doc["physical"]
    r = phys["radius_km"] * 1000.0
    shell = 4.0 * math.pi * r * r * 1.0e4 * phys.get("density_kg_m3", 3000.0)
    base = phys["mass_kg"] if dep["depth_class"] == "atmosphere" else min(phys["mass_kg"], shell)
    return base * dep["accessible_mass_fraction"]


def score(doc, families):
    res = doc.get("resources")
    if not res:
        return 0.0
    deps = res["natural_resources"]["deposits"]
    cost = res["industrial_suitability"]["relative_extraction_cost"]
    s = 0.0
    for f in families:
        if f in deps and f in cost:
            kg = reachable_kg(doc, deps[f], f)
            if kg > 0:
                s += max(0.0, math.log10(kg) - 9.0) * (0.5 + deps[f]["ore_grade"]) / cost[f]
    return s


def settlement(doc):
    res = doc.get("resources")
    if not res or doc.get("kind") not in ("planet", "dwarf_planet", "moon"):
        return -1.0
    h = res["habitability"]
    if h["settlement_class"] == "orbital_only":
        return -1.0
    return (0.35 * (1.0 - h["life_support_cost"]) + 0.25 * h["water_accessibility"] + 0.2 * h["gravity_comfort"] +
            0.1 * (1.0 - h["radiation_protection_requirement"]) + 0.1 * h["agriculture_potential"])


def name(doc):
    return doc.get("designation") or doc.get("name") or doc["id"]


def main(root, out):
    systems = []
    for sub in ("systems", "distant_systems"):
        for d in sorted(glob.glob(os.path.join(root, sub, "*"))):
            if not os.path.isfile(os.path.join(d, "system.json")):
                continue
            sysdoc = load(os.path.join(d, "system.json"))
            gen = load(os.path.join(d, "generated", "system.json"))
            docs = [load(p) for p in glob.glob(os.path.join(d, "generated", "bodies", "*.json"))]
            docs += [load(p) for p in glob.glob(os.path.join(d, "generated", "reservoirs", "*.json"))]
            docs = [x for x in docs if x.get("resources")]
            if not docs:
                continue
            rec = dict(id=sysdoc["id"], name=sysdoc["name"],
                       stars=", ".join(s.get("spectral_type", "") for s in gen["stars"]))
            for cat, fams in (("water", ("water_ice",)), ("structural", STRUCTURAL), ("strategic", STRATEGIC)):
                best = max(docs, key=lambda x: score(x, fams))
                rec[cat] = (score(best, fams), name(best))
            fuels = [(x["resources"]["fuel_potential"]["best_net_index"], x["resources"]["fuel_potential"]["best_source"],
                      name(x)) for x in docs]
            rec["fuel"] = max(fuels)
            industrial = sorted(docs, key=lambda x: -(score(x, STRUCTURAL) + score(x, STRATEGIC) + score(x, ("water_ice",))))
            rec["industrial"] = [name(x) for x in industrial[:3]]
            st = max(docs, key=settlement)
            rec["settlement"] = (settlement(st), name(st), st["resources"]["habitability"]["settlement_class"])
            costs = []
            for x in docs:
                c = x["resources"]["industrial_suitability"]["relative_extraction_cost"]
                vals = [c[f] for f in STRUCTURAL if f in c]
                if vals:
                    costs.append((sum(vals) / len(vals), name(x)))
            rec["cheapest"] = min(costs) if costs else (0.0, "-")
            hazards = set()
            for s in gen["stars"]:
                if s.get("teff_k", 6000) < 3900:
                    hazards.add("flare-active M dwarf host")
                if s.get("stage") in ("giant", "red_giant"):
                    hazards.add("giant host (luminous, evolving)")
                if s.get("stage") == "white_dwarf":
                    hazards.add("white dwarf companion")
            for x in docs:
                i = x["resources"]["industrial_suitability"]
                if i.get("tidal_volcanic_hazard", 0) > 0.5:
                    hazards.add(f"tidal / volcanic hazard ({name(x)})")
                if i.get("surface_radiation_index", 0) > 0.9 and x.get("kind") != "planet":
                    pass
            if len(gen["stars"]) > 1:
                hazards.add("multiple-star illumination / orbits")
            rec["hazards"] = sorted(hazards)
            systems.append(rec)

    L = ["# Resource audit (natural endowment and accessibility; no prices)", "",
         "| id | system | stars | water (best) | fuel (best) | structural metals | strategic | settlement | "
         "cheapest structural target | major industrial bodies | hazards |", "|" + "---|" * 11]
    for r in systems:
        L.append(f"| {r['id']} | {r['name']} | {r['stars']} | {r['water'][0]:.1f} {r['water'][1]} | "
                 f"{r['fuel'][0]:.2f} {r['fuel'][1]} @ {r['fuel'][2]} | {r['structural'][0]:.1f} {r['structural'][1]} | "
                 f"{r['strategic'][0]:.1f} {r['strategic'][1]} | {r['settlement'][0]:.2f} {r['settlement'][1]} "
                 f"({r['settlement'][2]}) | {r['cheapest'][0]:.1f} {r['cheapest'][1]} | {', '.join(r['industrial'])} | "
                 f"{'; '.join(r['hazards'])} |")

    # Comparative advantage: rank per category, leaders, and whether one
    # system dominates.
    cats = {"water": lambda r: r["water"][0], "fuel": lambda r: r["fuel"][0],
            "structural": lambda r: r["structural"][0], "strategic": lambda r: r["strategic"][0],
            "settlement": lambda r: r["settlement"][0], "low_cost": lambda r: -r["cheapest"][0]}
    L += ["", "## Comparative advantage", ""]
    top_counts = {}
    for cat, key in cats.items():
        ranked = sorted(systems, key=key, reverse=True)
        top = ranked[:5]
        L.append(f"- {cat}: " + "; ".join(f"{r['name']} ({key(r):.2f})" for r in top))
        for r in ranked[:10]:
            top_counts.setdefault(r["name"], []).append(cat)
        vals = [abs(key(r)) for r in systems]
        mean = sum(vals) / len(vals)
        sd = math.sqrt(sum((v - mean) ** 2 for v in vals) / len(vals))
        L.append(f"  spread: mean {mean:.2f}, sd {sd:.2f}, max/median {max(vals) / max(sorted(vals)[len(vals) // 2], 1e-9):.2f}")
    L += ["", "Systems in the top 10 of several categories:", ""]
    for n, c in sorted(top_counts.items(), key=lambda kv: -len(kv[1])):
        if len(c) >= 3:
            L.append(f"- {n}: {', '.join(c)}")
    L.append("")
    for label, items in (("all systems", top_counts.items()),
                         ("procedural systems (Sol excluded: authored, fully observed)",
                          [kv for kv in top_counts.items() if not kv[0].startswith("Sol")])):
        worst = max(items, key=lambda kv: len(kv[1]))
        L.append(f"Most categories in the top 10, {label}: {worst[0]} with {len(worst[1])} of {len(cats)}.")
    open(out, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("\n".join(L[-30:]))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
