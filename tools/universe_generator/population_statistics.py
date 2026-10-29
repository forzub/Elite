"""Population sanity statistics for a bulk run of the universe generator.

usage: python population_statistics.py PREFIX
       (PREFIX_systems.tsv / PREFIX_planets.tsv from `EliteUniverseGenerator bulk`)

The goal is to catch astrophysical absurdity, not to fit one table. Each line
prints the generated value next to an observational reference band:
- small-planet occurrence: Kepler (Fressin 2013, Petigura 2018; Dressing &
  Charbonneau 2015 for M dwarfs);
- giant occurrence and its metallicity dependence: Cumming 2008, Fischer &
  Valenti 2005, Johnson 2010 (M dwarfs ~3 %);
- hot Jupiters ~0.5-1 % (Wright 2012);
- eccentricities: Kepler multis ~0.02-0.05, RV giants median ~0.2-0.25;
- stellar multiplicity: Raghavan 2010, Duchene & Kraus 2013.
"""
import csv
import math
import statistics
import sys
from collections import Counter


def load(path):
    return list(csv.DictReader(open(path, encoding="utf-8"), delimiter="\t"))


def pct(x, n):
    return 100.0 * x / max(n, 1)


def band(name, value, lo, hi, unit=""):
    flag = "ok " if lo <= value <= hi else "OUT"
    print(f"  [{flag}] {name:52} {value:9.3g}{unit}   reference {lo:g}..{hi:g}{unit}")
    return lo <= value <= hi


def main(prefix):
    systems = load(prefix + "_systems.tsv")
    planets = load(prefix + "_planets.tsv")
    n = len(systems)
    by_index = {}
    for p in planets:
        by_index.setdefault(p["index"], []).append(p)
    out_of_band = 0

    def check(*a, **k):
        nonlocal out_of_band
        if not band(*a, **k):
            out_of_band += 1

    print(f"systems: {n}, planets: {len(planets)}")
    print("stellar")
    mult = sum(1 for s in systems if int(s["stars"]) > 1)
    check("multiple-star fraction %", pct(mult, n), 25, 45)
    pops = Counter(s["population"] for s in systems)
    print("   populations:", dict(pops))

    def host_class(s):
        m = float(s["primary_mass"])
        return "M" if m < 0.6 else "FGK" if m < 1.5 else "A+"

    groups = {"M": [], "FGK": [], "A+": []}
    for s in systems:
        if int(s["stars"]) == 1:
            groups[host_class(s)].append(s)

    print("occurrence (single stars)")
    for cls in ("FGK", "M"):
        g = groups[cls]
        if not g:
            continue
        small_short = giants = hot = 0
        for s in g:
            ps = by_index.get(s["index"], [])
            small_short += sum(1 for p in ps if 1.0 <= float(p["mass_earth"]) < 20 and float(p["period_days"]) < 200)
            giants += any(float(p["mass_earth"]) > 50 and float(p["a_au"]) < 10 for p in ps)
            hot += any(float(p["mass_earth"]) > 100 and float(p["a_au"]) < 0.1 for p in ps)
        if cls == "FGK":
            check("FGK small planets (1-20 Me, P<200 d) per star", small_short / len(g), 0.5, 2.0)
            check("FGK giant (>50 Me, a<10 AU) host fraction %", pct(giants, len(g)), 7, 22)
            check("FGK hot Jupiter (>100 Me, a<0.1 AU) %", pct(hot, len(g)), 0.2, 2.0)
        else:
            check("M small planets (1-20 Me, P<200 d) per star", small_short / len(g), 1.0, 3.0)
            check("M giant host fraction %", pct(giants, len(g)), 1, 8)

    # Giant frequency vs metallicity (FGK).
    rich = [s for s in groups["FGK"] if float(s["feh"]) > 0.15]
    poor = [s for s in groups["FGK"] if float(s["feh"]) < -0.15]
    if rich and poor:
        def giant_frac(ss):
            return sum(1 for s in ss if any(float(p["mass_earth"]) > 50 for p in by_index.get(s["index"], []))) / len(ss)
        ratio = giant_frac(rich) / max(giant_frac(poor), 1e-3)
        check("giant fraction ratio [Fe/H]>+0.15 / <-0.15", ratio, 2.0, 12.0)

    print("orbits")
    small = [p for p in planets if float(p["mass_earth"]) < 20]
    giants = [p for p in planets if float(p["mass_earth"]) > 50]
    if small:
        check("small planets median e", statistics.median(float(p["e"]) for p in small), 0.01, 0.08)
        pers = sorted(float(p["period_days"]) for p in small if float(p["period_days"]) < 400)
        if pers:
            check("small planets median period (P<400 d) d", statistics.median(pers), 8, 60)
    if giants:
        check("giants median e", statistics.median(float(p["e"]) for p in giants), 0.05, 0.3)
        check("giants median mass Me", statistics.median(float(p["mass_earth"]) for p in giants), 100, 900)

    print("bodies")
    classes = Counter(p["class"] for p in planets)
    print("   classes:", dict(classes.most_common()))
    for p in planets:
        m, r = float(p["mass_earth"]), float(p["radius_earth"])
        rho = 5.51 * m / r ** 3
        if not (0.05 < rho < 30):
            print("   absurd density", p)
            out_of_band += 1
            break
    gas = [p for p in planets if p["class"] in ("gas_giant", "super_jupiter")]
    if gas:
        moons = [int(p["moons"]) for p in gas]
        check("gas giants mean moons", statistics.mean(moons), 1.5, 8)
        check("gas giants with main rings %", pct(sum(int(p["ring"]) for p in gas), len(gas)), 10, 60)
    rocky = [p for p in planets if p["class"] in ("rocky", "iron_rich")]
    if rocky:
        check("rocky planets with a moon %", pct(sum(1 for p in rocky if int(p["moons"]) > 0), len(rocky)), 2, 35)
        locked = sum(1 for p in rocky if p["locked"] != "none")
        print(f"   rocky tidally locked: {pct(locked, len(rocky)):.1f} %")
        hydro = Counter(p["hydrosphere"] for p in rocky)
        print("   rocky hydrospheres:", dict(hydro.most_common()))
        temperate = [p for p in rocky if 250 < float(p["tsurf_k"]) < 330]
        print(f"   rocky with 250-330 K surface: {len(temperate)} ({pct(len(temperate), len(rocky)):.1f} %)")

    print("reservoirs")
    ab = [float(s["asteroid_belt_mass"]) for s in systems if float(s["asteroid_belt_mass"]) > 0]
    ob = [float(s["outer_belt_mass"]) for s in systems if float(s["outer_belt_mass"]) > 0]
    if ab:
        check("asteroid belt median mass Me", statistics.median(ab), 1e-4, 1e-2)
        print(f"   systems with asteroid belt: {pct(len(ab), n):.1f} %")
    if ob:
        check("outer belt median mass Me", statistics.median(ob), 1e-3, 1.0)
        print(f"   systems with outer belt / debris disk: {pct(len(ob), n):.1f} %")
    print(f"   systems with comet cloud: {pct(sum(1 for s in systems if s['comet_cloud'] == '1'), n):.1f} %")

    print("stability")
    fails = [s for s in systems if s["stability"] != "PASS"]
    check("systems failing a stability gate %", pct(len(fails), n), 0, 0)
    for s in fails[:5]:
        print("   ", s["index"], s["failed"][:200])
    modes = Counter(m.split(":")[-1] for s in systems for m in s["architecture"].split("; "))
    print("   architecture modes:", dict(modes.most_common()))
    print(f"out-of-band checks: {out_of_band}")
    return out_of_band


if __name__ == "__main__":
    sys.exit(1 if main(sys.argv[1]) else 0)
