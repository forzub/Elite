"""Fit the one free number per unit of canon/population_history.json: `intensity`.

The author's 3126 scale is an AUTHOR_TARGET. The dated epochs (habitat boom, terraforming,
high-fertility era, immigration programmes) are PROPOSAL history; `intensity` scales how far each
epoch moves its lever (1 = the narrative values). This tool searches the intensities so that the
causal model (demography.py) lands every polity on its target centre, and Earth + Sol on the
Earth and Sol centres together. It never multiplies a population.

    python calibrate_population.py            # fit and print (no write)
    python calibrate_population.py --write    # fit and store the intensities
    python calibrate_population.py --check    # one run: every target inside its range?
"""
import argparse
import json
import math
import os
import sys

import demography as demo
import endowment as endow
import lab

HERE = os.path.dirname(os.path.abspath(__file__))
PH_PATH = os.path.join(HERE, "canon", "population_history.json")
EARTH = ("earth_north", "earth_south")


def setup(seed):
    E = endow.load_endowment()
    W = lab.load("world.json")
    W["population_history"] = lab.load("population_history.json")
    missions = {m["id"]: m for m in lab.load("missions.json")["missions"]}
    voyages = {mid: lab.Voyage(m, E, seed).run() for mid, m in sorted(missions.items())}
    cmap = {c["id"]: c for c in W["colonies"]}
    base_cult = {c["id"]: lab.colony_profile(c, voyages, E, missions, pop=c["pop_3126_authored"])["culture"] for c in W["colonies"]}
    earth_cult = lab.blend({"earth_late": 1.0})
    return dict(E=E, W=W, missions=missions, voyages=voyages,
                culture_of=lambda uid: base_cult.get(uid, earth_cult),
                regime_of=lambda uid: cmap[uid]["regime_3126"] if uid in cmap else "sol_authority")


def simulate(S, seed):
    W, E = S["W"], S["E"]
    demo.SCEN.clear()
    demo.SCEN.update(demo.SCENARIOS["baseline"])
    units, _, _ = demo.simulate(W, S["missions"], S["voyages"], E, S["culture_of"], S["regime_of"],
                                lambda y: lab.beacon_graph(W, E, y), lab.shortest, seed)
    return units


def groups(ph):
    """{group: (unit ids, target centre, (lo, hi))}"""
    wt = ph["world_targets"]
    g = {"earth": (list(EARTH), wt["earth"]["centre"] + wt["sol_outside_earth"]["centre"],
                   (wt["earth"]["range"][0] + wt["sol_outside_earth"]["range"][0], wt["earth"]["range"][1] + wt["sol_outside_earth"]["range"][1]))}
    for uid in sorted(ph["units"]):
        t = ph["units"][uid].get("target")
        if t:
            g[uid] = ([uid], (t[0] + t[1]) / 2.0, (t[0], t[1]))
    return g


def measure(units, g):
    return {k: sum(units[u].P() for u in ids) for k, (ids, _, _) in g.items()}


def fit(seed, iters=24, tol=0.03, log=print):
    S = setup(seed)
    ph = S["W"]["population_history"]
    g = groups(ph)
    lam = {k: ph["units"][ids[0]].get("intensity", 1.0) for k, (ids, _, _) in g.items()}
    hist = {k: [] for k in g}
    for it in range(iters):
        for k, (ids, _, _) in g.items():
            for u in ids:
                ph["units"][u]["intensity"] = lam[k]
        P = measure(simulate(S, seed), g)
        worst = max(abs(math.log(P[k] / g[k][1])) for k in g)
        log(f"iter {it:2d} worst |log err| {worst:.3f}  off: " + " ".join(f"{k}={lam[k]:.3f}:{P[k] / g[k][1]:.2f}" for k in sorted(g) if abs(math.log(P[k] / g[k][1])) >= tol))
        if worst < tol:
            break
        # damped Newton step in log space: the response is steep and monotone (log P vs log
        # intensity has a slope of ~2..10), and the units interact through migration, so the slope
        # comes from the last two points only when they are far enough apart
        for k in g:
            y = math.log(P[k] / g[k][1])
            hist[k].append((math.log(max(lam[k], 1e-3)), y))
            if abs(y) < tol * 0.5:
                continue
            s = 5.0
            if len(hist[k]) >= 2:
                (x0, y0), (x1, y1) = hist[k][-2], hist[k][-1]
                if abs(x1 - x0) > 0.01 and (y1 - y0) / (x1 - x0) > 0:
                    s = min(25.0, max(1.0, (y1 - y0) / (x1 - x0)))
            step = max(-0.3, min(0.3, -y / s))
            lam[k] = round(min(6.0, math.exp(math.log(max(lam[k], 1e-3)) + step)), 4)
    return lam, P, g


def check(seed):
    S = setup(seed)
    ph = S["W"]["population_history"]
    g = groups(ph)
    P = measure(simulate(S, seed), g)
    bad = [f"{k}: {P[k] / 1e9:.3f} bn not in {r[0] / 1e9:.2f}-{r[1] / 1e9:.2f}" for k, (_, _, r) in sorted(g.items()) if not (r[0] <= P[k] <= r[1])]
    return bad, P


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=3126)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    if a.check:
        bad, _ = check(a.seed)
        if bad:
            print("population calibration check FAILED:\n  " + "\n  ".join(bad))
            sys.exit(1)
        print("population calibration check OK")
        return
    lam, P, g = fit(a.seed)
    if a.write:
        ph = json.load(open(PH_PATH, encoding="utf-8"))
        for k, (ids, _, _) in g.items():
            for u in ids:
                ph["units"][u]["intensity"] = lam[k]
        with open(PH_PATH, "w", encoding="utf-8", newline="\n") as f:
            json.dump(ph, f, ensure_ascii=False, indent=2)
            f.write("\n")
        print("written", PH_PATH)


if __name__ == "__main__":
    main()
