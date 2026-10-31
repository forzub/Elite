"""Natural endowment of the physical catalog, reduced to the few indices the
colonization/economy lab needs.

Reads src/assets/data/galaxy_details (the authoritative physical catalog) and
reuses the scoring of tools/universe_generator/resource_audit.py, so the lab
never invents resources a system does not have.

Indices (all 0..1, percentile rank over the 60 local systems, Sol included):
  water       accessible water ice
  volatiles   giants + comet clouds + water (hydrogen, nitrogen, carbon gases)
  he3         helium-3 reachable in giant atmospheres (aneutronic drive fuel)
  metals      structural metals (Fe, Al, Mg, Ti) weighted by extraction cost
  strategic   platinum group, refractory metals, actinides
  agri        best agriculture potential of a sealed/pressure-suit habitat
  settle      best settlement score
"""
import glob
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CATALOG = os.path.join(ROOT, "src", "assets", "data", "galaxy_details")
sys.path.insert(0, os.path.join(ROOT, "tools", "universe_generator"))
import resource_audit as ra  # noqa: E402

GIANTS = {"gas_giant": 1.0, "super_jupiter": 1.3, "ice_giant": 1.2, "sub_neptune": 0.4}


def _load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def raw_system(d):
    sysdoc = _load(os.path.join(d, "system.json"))
    pos = sysdoc["position_ly"]
    docs = [_load(p) for p in sorted(glob.glob(os.path.join(d, "generated", "bodies", "*.json")))]
    resv = [_load(p) for p in sorted(glob.glob(os.path.join(d, "generated", "reservoirs", "*.json")))]
    alld = [x for x in docs + resv if x.get("resources")]
    giants = [b for b in docs if b["physical"]["body_class"] in GIANTS]
    giant_w = sum(GIANTS[b["physical"]["body_class"]] for b in giants)
    comet = sum(r["total_mass_earth"] for r in resv if r["kind"] in ("comet_cloud", "kuiper_belt"))
    belt = sum(r["total_mass_earth"] for r in resv if r["kind"] in ("asteroid_belt", "debris_disk", "trojans"))
    he3 = 0.0
    for b in giants:
        dep = b["resources"]["natural_resources"]["deposits"].get("helium3")
        if dep:
            he3 += b["physical"]["mass_kg"] * dep["accessible_mass_fraction"]
    agri = max([x["resources"]["habitability"]["agriculture_potential"] for x in docs
                if x.get("resources") and x["resources"]["habitability"]["settlement_class"] != "orbital_only"] or [0.0])
    settle = max([ra.settlement(x) for x in alld] or [-1.0])
    return dict(
        id=sysdoc["id"], name=sysdoc["name"],
        pos=(pos["x"], pos["y"], pos["z"]),
        dist=math.sqrt(pos["x"] ** 2 + pos["y"] ** 2 + pos["z"] ** 2),
        giants=[b["designation"] for b in giants], giant_w=giant_w,
        comet_me=comet, belt_me=belt,
        water_s=max([ra.score(x, ("water_ice",)) for x in alld] or [0]),
        metals_s=max([ra.score(x, ra.STRUCTURAL) for x in alld] or [0]),
        strategic_s=max([ra.score(x, ra.STRATEGIC) for x in alld] or [0]),
        he3_kg=he3, agri=agri, settle=settle,
    )


def _rank(values):
    order = sorted(values)
    n = len(order)
    return [(sum(1 for o in order if o < v) + 0.5 * sum(1 for o in order if o == v)) / n for v in values]


def load_endowment(catalog=CATALOG):
    dirs = [d for d in sorted(glob.glob(os.path.join(catalog, "systems", "*")))
            if os.path.isfile(os.path.join(d, "system.json"))]
    raws = [raw_system(d) for d in dirs]
    keys = {
        "water": [r["water_s"] for r in raws],
        "volatiles": [r["giant_w"] * 2.0 + math.log10(1.0 + 100.0 * r["comet_me"]) + 0.3 * r["water_s"] for r in raws],
        "he3": [math.log10(1.0 + r["he3_kg"] / 1e15) for r in raws],
        "metals": [r["metals_s"] + 0.5 * math.log10(1.0 + 1e4 * r["belt_me"]) for r in raws],
        "strategic": [r["strategic_s"] for r in raws],
        "agri": [r["agri"] for r in raws],
        "settle": [r["settle"] for r in raws],
    }
    ranks = {k: _rank(v) for k, v in keys.items()}
    out = {}
    for i, r in enumerate(raws):
        r = dict(r)
        r["idx"] = {k: round(ranks[k][i], 3) for k in keys}
        if r["he3_kg"] <= 0.0:
            r["idx"]["he3"] = 0.0  # no giant, no helium-3 industry, whatever the rank
        out[r["id"]] = r
    return out


def distance(a, b):
    return math.sqrt(sum((a["pos"][i] - b["pos"][i]) ** 2 for i in range(3)))


if __name__ == "__main__":
    e = load_endowment()
    for sid in sorted(e):
        r = e[sid]
        print(f"{sid:3d} {r['name'][:26]:26s} {r['dist']:6.2f} ly giants={len(r['giants'])} "
              + " ".join(f"{k}={v:.2f}" for k, v in r["idx"].items()))
