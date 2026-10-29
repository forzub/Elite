"""Planet-count and detectability report of the materialised catalog.

usage: python detectability_report.py GALAXY_DETAILS_ROOT OUT_MD

For every host star: number of bodies above 0.05 / 0.1 / 0.3 / 1 / 5 / 10
Earth masses, giants, total planet mass, the largest radial-velocity
semi-amplitude K of a procedural planet against the survey limit of the
system, and every procedural planet that a survey should already have found.

K = 28.4329 m/s (m sin i / M_J) (M_star + m)^(-2/3) (P / yr)^(-1/3) / sqrt(1 - e^2)
(Cumming et al. 1999). The sky inclination is shared by a host's planets
and is not stored; the generator samples cos i in [0, 0.7], so sin i lies in
[0.714, 1]: "detectable" = K sin i > limit for every allowed sin i and the
period inside the RV baseline; "marginal" = only for the most favourable sin i.
Transit surveys: radius above the limit inside the transit baseline.

The question this answers: does the generator create planets that should
have been found long ago (then fix generation / conditioning), or are the
extra bodies below the detection thresholds (then keep them).
"""
import glob
import json
import math
import os
import sys

MJ_EARTH = 317.828
MSUN_EARTH = 332946.0
SIN_MIN = math.sqrt(1.0 - 0.7 ** 2)
THRESHOLDS = (0.05, 0.1, 0.3, 1.0, 5.0, 10.0)
# High-completeness neighbours: long-baseline sub-m/s to ~1 m/s RV programmes.
HIGH_COMPLETENESS = {1: "alpha Cen A/B", 8: "tau Ceti", 9: "eps Eridani", 30: "eps Indi", 31: "61 Cygni",
                     14: "HD 20794", 22: "delta Pavonis", 5: "Sirius", 52: "Procyon", 60: "Proxima",
                     2: "Barnard's Star", 4: "Lalande 21185", 6: "Luyten's Star", 7: "Kapteyn's Star"}


def k_ms(m_earth, period_days, m_star_solar, e):
    m_j = m_earth / MJ_EARTH
    total = m_star_solar + m_earth / MSUN_EARTH
    return 28.4329 * m_j * total ** (-2.0 / 3.0) * (period_days / 365.25) ** (-1.0 / 3.0) / math.sqrt(
        max(1.0 - e * e, 1e-6))


def load(path):
    return json.load(open(path, encoding="utf-8"))


def main(root, out):
    rows = []
    for sub in ("systems", "distant_systems"):
        for d in sorted(glob.glob(os.path.join(root, sub, "*"))):
            if not os.path.isfile(os.path.join(d, "system.json")):
                continue
            sysdoc = load(os.path.join(d, "system.json"))
            gen = load(os.path.join(d, "generated", "system.json"))
            sv = sysdoc.get("survey_completeness", {})
            limit = sv.get("rv_detection_limit_ms", {}).get("value")
            base = sv.get("rv_baseline_days", {}).get("value", 0.0)
            tr = sv.get("transit_radius_limit_earth", {}).get("value")
            trb = sv.get("transit_baseline_days", {}).get("value", 0.0)
            stars = {s["key"]: s for s in gen["stars"]}
            bodies = [load(p) for p in glob.glob(os.path.join(d, "generated", "bodies", "*.json"))]
            hosts_sv = sv.get("hosts", {})
            sys_limit, sys_base = limit, base
            for key, star in stars.items():
                # Per-star survey override (literature limit, or 0 = not surveyed).
                limit, base = sys_limit, sys_base
                if key in hosts_sv:
                    limit = hosts_sv[key]["rv_detection_limit_ms"]["value"] or None
                    base = hosts_sv[key]["rv_baseline_days"]["value"]
                mine = [b for b in bodies if b.get("host_star") == key and b["kind"] in ("planet", "dwarf_planet")]
                if not mine and sysdoc["id"] != 0:
                    rows.append(dict(sid=sysdoc["id"], system=sysdoc["name"], host=star["name"],
                                     sp=star.get("spectral_type", ""), mass=star["mass_solar"], n={},
                                     giants=0, total=0.0, kmax=0.0, kmax_name="", limit=limit, base=base,
                                     detect=[], marginal=[], anchors=0, generated=0, sol=False))
                    continue
                n = {t: sum(1 for b in mine if b["physical"]["mass_earth"] >= t) for t in THRESHOLDS}
                giants = sum(1 for b in mine if b["physical"]["mass_earth"] >= 50.0)
                total = sum(b["physical"]["mass_earth"] for b in mine)
                kmax, kname, detect, marginal = 0.0, "", [], []
                for b in mine:
                    if b.get("origin") != "generated":
                        continue
                    m = b["physical"]["mass_earth"]
                    o = b["orbit"]
                    P = o["period_days"]
                    k = k_ms(m, P, star["mass_solar"], o.get("eccentricity", 0.0))
                    if k > kmax:
                        kmax, kname = k, b["designation"]
                    rad = b["physical"]["radius_km"] / 6371.0
                    tag = f"{b['designation']} ({m:.2f} Me, P {P:.1f} d, K {k:.2f} m/s, R {rad:.2f} Re)"
                    if limit and P < base:
                        if k * SIN_MIN > limit:
                            detect.append(tag)
                        elif k > limit:
                            marginal.append(tag)
                    if tr and P < trb and rad > tr:
                        detect.append(tag + " [transit]")
                rows.append(dict(sid=sysdoc["id"], system=sysdoc["name"], host=star["name"],
                                 sp=star.get("spectral_type", ""), mass=star["mass_solar"], n=n, giants=giants,
                                 total=total, kmax=kmax, kmax_name=kname, limit=limit, base=base,
                                 detect=detect, marginal=marginal,
                                 anchors=sum(1 for b in mine if b.get("origin") != "generated"),
                                 generated=sum(1 for b in mine if b.get("origin") == "generated"),
                                 sol=sysdoc["id"] == 0))

    L = ["# Planet count and detectability of the generated catalog", "",
         "Bodies = planets + dwarf planets around each host. K in m/s for sin i = 1; the sky",
         f"inclination of a host is not stored (cos i in [0, 0.7], sin i >= {SIN_MIN:.3f}).", ""]
    hdr = ("| id | host | SpT | M* | N>=0.05 | >=0.1 | >=0.3 | >=1 | >=5 | >=10 | giants | M_tot Me | "
           "obs | gen | max K (planet) | RV limit / baseline | should be detected | marginal |")
    L += [hdr, "|" + "---|" * 18]
    for r in rows:
        n = r["n"]
        L.append(f"| {r['sid']} | {r['host']} | {r['sp']} | {r['mass']:.2f} | "
                 + " | ".join(str(n.get(t, 0)) for t in THRESHOLDS)
                 + f" | {r['giants']} | {r['total']:.1f} | {r['anchors']} | {r['generated']} | "
                 f"{r['kmax']:.2f} ({r['kmax_name']}) | "
                 f"{r['limit'] if r['limit'] else '-'} / {r['base']:.0f} d | {len(r['detect'])} | {len(r['marginal'])} |")
    L += ["", "## Procedural planets a survey should already have found", ""]
    any_detect = False
    for r in rows:
        if r["detect"]:
            any_detect = True
            L.append(f"- {r['sid']} {r['host']}: " + "; ".join(r["detect"]))
    if not any_detect:
        L.append("- none")
    L += ["", "## Marginal (detectable only for the most favourable inclination)", ""]
    for r in rows:
        if r["marginal"]:
            L.append(f"- {r['sid']} {r['host']}: " + "; ".join(r["marginal"]))
    L += ["", "## High-completeness neighbours", ""]
    for r in rows:
        if r["sid"] in HIGH_COMPLETENESS:
            n = r["n"]
            L.append(f"- {r['sid']} {r['host']} ({r['sp']}): N>=0.05 {n.get(0.05, 0)}, N>=1 {n.get(1.0, 0)}, "
                     f"giants {r['giants']}, max K {r['kmax']:.2f} m/s vs limit {r['limit']} m/s, "
                     f"detectable {len(r['detect'])}, marginal {len(r['marginal'])}")
    # Population summary by spectral class (generated hosts, Sol excluded).
    L += ["", "## Summary by spectral class (Sol excluded)", ""]
    groups = {}
    for r in rows:
        if r["sol"] or not r["sp"]:
            continue
        cls = r["sp"][0] if r["sp"][0] in "OBAFGKM" else "other"
        if "III" in r["sp"] or "IV" in r["sp"]:
            cls += " (evolved)"
        groups.setdefault(cls, []).append(r)
    for cls, rs in sorted(groups.items()):
        med = sorted(x["n"].get(0.05, 0) for x in rs)[len(rs) // 2]
        below = sum(len(x["detect"]) for x in rs)
        kmed = sorted(x["kmax"] for x in rs)[len(rs) // 2]
        L.append(f"- {cls}: hosts {len(rs)}, median N>=0.05 {med}, median N>=1 "
                 f"{sorted(x['n'].get(1.0, 0) for x in rs)[len(rs) // 2]}, median max K {kmed:.2f} m/s, "
                 f"should-be-detected planets {below}")
    open(out, "w", encoding="utf-8").write("\n".join(L) + "\n")
    print("\n".join(L[-12:]))
    det = sum(len(r["detect"]) for r in rows)
    mar = sum(len(r["marginal"]) for r in rows)
    print(f"hosts {len(rows)}, should-be-detected {det}, marginal {mar}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
