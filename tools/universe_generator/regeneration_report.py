"""Comparative report legacy catalog -> regenerated catalog.

usage: python regeneration_report.py GALAXY_DETAILS_ROOT OUT.md

Reads the legacy documents (systems_details/, distant_systems_details/) and the
regenerated per-system directories (systems/, distant_systems/: system.json,
stars/, observed/, generated/), one section per system: identity, stars,
coordinates before / after, stellar parameters, legacy vs generated content,
stability, seeds, migration warnings.
"""
import glob
import json
import math
import os
import sys
from collections import Counter


def load(path):
    return json.load(open(path, encoding="utf-8"))


def legacy_counts(doc):
    planets = moons = rings = belts = 0
    for s in doc["details"]["stars"]:
        for p in s.get("planets", []):
            planets += 1
            moons += len(p.get("moons", []))
            rings += 1 if (p.get("rings") or p.get("ring_system")) else 0
        belts += len(s.get("asteroid_belts", []))
    belts += len(doc["details"].get("asteroid_belts", []))
    return planets, moons, rings, belts


def main(root, out):
    legacy = {}
    for sub in ("systems_details", "distant_systems_details"):
        for p in glob.glob(os.path.join(root, sub, "*.json")):
            d = load(p)
            legacy[d["id"]] = d
    lines = ["# Universe regeneration report", "",
             "Legacy catalog (`systems_details/`) vs regenerated catalog (`systems/`, `distant_systems/`).",
             "Planets, moons, rings and belts are regenerated; stars, IDs and catalog flags are preserved.", ""]
    rows = []
    totals = Counter()
    for sub in ("systems", "distant_systems"):
        for d in sorted(glob.glob(os.path.join(root, sub, "*", "system.json"))):
            sdir = os.path.dirname(d)
            sysdoc = load(d)
            gen = load(os.path.join(sdir, "generated", "system.json"))
            bodies = [load(p) for p in sorted(glob.glob(os.path.join(sdir, "generated", "bodies", "*.json")))]
            res = [load(p) for p in sorted(glob.glob(os.path.join(sdir, "generated", "reservoirs", "*.json")))]
            sid = sysdoc["id"]
            leg = legacy.get(sid)
            lp = leg["position_ly"] if leg else {"x": 0, "y": 0, "z": 0}
            np_ = sysdoc["position_ly"]
            shift = math.dist((lp["x"], lp["y"], lp["z"]), (np_["x"], np_["y"], np_["z"]))
            planets = [b for b in bodies if b["kind"] == "planet"]
            moons = [b for b in bodies if b["kind"] == "moon"]
            dwarfs = [b for b in bodies if b["kind"] == "dwarf_planet"]
            ringed = [b for b in planets if any(r["visibility_class"] != "faint" for r in b["rings"])]
            anchors = [b for b in planets if b["origin"] != "generated"]
            lpl, lmo, lri, lbe = legacy_counts(leg) if leg else (0, 0, 0, 0)
            totals.update(legacy_planets=lpl, legacy_moons=lmo, planets=len(planets), moons=len(moons),
                          anchors=len(anchors), dwarfs=len(dwarfs), reservoirs=len(res), ringed=len(ringed))
            legacy_stars = [s["name"] for s in leg["details"]["stars"]] if leg else []
            stars = gen["stars"]
            retained = all(any(s["name"] == ls for s in stars) for ls in legacy_stars)
            added = [s["name"] for s in stars if s["name"] not in legacy_stars]
            failed = [c for c in gen["stability"] if not c["passed"]]
            rows.append((sid, sysdoc["name"], shift, len(planets), lpl))
            lines.append(f"## {sid} {sysdoc['name']}")
            lines.append("")
            lines.append(f"- directory: `{sub}/{os.path.basename(sdir)}`; scope {sysdoc['catalog_scope']}, "
                         f"atlas_visible {sysdoc['atlas_visible']}, route_target {sysdoc['route_target']}"
                         + (f", quest_role {sysdoc['quest_role']}" if sysdoc.get("quest_role") else ""))
            lines.append(f"- position before: ({lp['x']:.3f}, {lp['y']:.3f}, {lp['z']:.3f}) ly; after: "
                         f"({np_['x']:.3f}, {np_['y']:.3f}, {np_['z']:.3f}) ly; shift {shift:.2f} ly; "
                         f"source: {sysdoc.get('position_source', '')}")
            lines.append(f"- stars retained: {'yes' if retained else 'NO'} ({', '.join(legacy_stars)})"
                         + (f"; added real component(s): {', '.join(added)}" if added else ""))
            for s in stars:
                lines.append(f"  - {s['name']}: {s['spectral_type']} {s['stage']}, M={s['mass_solar']:.3g} "
                             f"R={s['radius_solar']:.3g} L={s['luminosity_solar']:.3g} Teff={s['teff_k']:.0f} K "
                             f"[Fe/H]={s['metallicity_feh']:+.2f} age={s['age_gyr']:.3g} Gyr "
                             f"(mass: {s['provenance'].get('mass_solar')}, age: {s['provenance'].get('age_gyr')})")
            for n in gen["barycenters"]:
                o = n.get("relative_orbit")
                if o:
                    lines.append(f"  - binary {n['id']}: a={o['semi_major_axis_au']:.4g} AU e={o['eccentricity']:.3f} "
                                 f"P={o['period_days'] / 365.25:.4g} yr (a: {n['provenance'].get('semi_major_axis_au')})")
            lines.append(f"- legacy content: {lpl} planets, {lmo} moons, {lri} ringed, {lbe} belts")
            lines.append(f"- regenerated: {len(planets)} planets ({len(anchors)} observational anchors), "
                         f"{len(moons)} moons, {len(ringed)} ringed planets, {len(dwarfs)} dwarf planets, "
                         f"{len(res)} reservoirs; architecture `{gen['architecture_mode']}`")
            for b in planets:
                ph, th, at = b["physical"], b["thermal"], b["atmosphere"]
                lines.append(f"  - {b['designation']} [{b['origin']}] {ph['body_class']}: a={b['orbit']['semi_major_axis_au']:.4g} AU "
                             f"e={b['orbit']['eccentricity']:.3f} M={ph['mass_earth']:.3g} Me R={ph['radius_km']:.0f} km "
                             f"T={th['mean_surface_temperature_k']:.0f} K P={at['surface_pressure_pa'] / 1e5:.3g} bar "
                             f"{b['geophysics']['hydrosphere']}, moons {sum(1 for m in moons if m['parent_id'] == b['id'])}")
            for r in res:
                lines.append(f"  - reservoir {r['name']} ({r['kind']}): {r['inner_au']:.3g}-{r['outer_au']:.3g} AU, "
                             f"{r['total_mass_earth']:.3g} Me")
            lines.append(f"- stability: {'PASS' if not failed else 'FAIL ' + '; '.join(c['check'] for c in failed)}"
                         f" ({len(gen['stability'])} checks)")
            lines.append(f"- seeds: universe {gen['generation']['universe_seed']}, system {gen['generation']['system_seed']}; "
                         f"versions {gen['generation']['versions']}")
            warn = list(gen["warnings"])
            if shift > 0.5:
                warn.append(f"star moved {shift:.1f} ly to its real ICRS position")
            if lpl and not anchors:
                warn.append(f"all {lpl} legacy planets replaced; no confirmed planet in the archive snapshot")
            if sysdoc.get("reserved_designations"):
                warn.append("reserved catalogue designations: " + ", ".join(sysdoc["reserved_designations"]))
            if warn:
                lines.append("- migration warnings:")
                lines += [f"  - {w}" for w in warn]
            lines.append("")
    head = ["## Summary", "",
            f"- systems: {len(rows)}; legacy planets {totals['legacy_planets']} / moons {totals['legacy_moons']}",
            f"- regenerated planets {totals['planets']} (observational anchors {totals['anchors']}), moons "
            f"{totals['moons']}, ringed planets {totals['ringed']}, dwarf planets {totals['dwarfs']}, "
            f"reservoirs {totals['reservoirs']}",
            f"- systems moved > 0.5 ly: {sum(1 for r in rows if r[2] > 0.5)}", ""]
    lines[5:5] = head
    open(out, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    print("\n".join(head))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
