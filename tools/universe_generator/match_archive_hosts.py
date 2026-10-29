"""Match catalog star systems to NASA Exoplanet Archive hosts by sky position.

usage: python match_archive_hosts.py CATALOG_ROOT ARCHIVE_CSV

CATALOG_ROOT is the legacy galaxy_details directory (systems_details,
distant_systems_details). position_ly is ICRS Cartesian light-years from Sol:
x = d cos(dec) cos(ra), y = d cos(dec) sin(ra), z = d sin(dec).

A host matches a system when its direction lies within 0.5 deg and its
distance within 8 %. Names are never used for the match; they are printed so
a human can audit component assignment inside multiple systems.
"""
import csv
import glob
import json
import math
import os
import sys

LY_PER_PC = 3.261563777


def load_systems(root):
    out = []
    for sub in ("systems_details", "distant_systems_details"):
        for path in sorted(glob.glob(os.path.join(root, sub, "*.json"))):
            doc = json.load(open(path, encoding="utf-8"))
            p = doc["position_ly"]
            out.append((doc["id"], doc["name"], (p["x"], p["y"], p["z"]),
                        [s["name"] for s in doc["details"]["stars"]]))
    return out


def unit(v):
    n = math.sqrt(sum(c * c for c in v))
    return tuple(c / n for c in v), n


def host_vector(row):
    ra = math.radians(float(row["ra"]))
    dec = math.radians(float(row["dec"]))
    d = float(row["sy_dist"]) * LY_PER_PC
    return (d * math.cos(dec) * math.cos(ra), d * math.cos(dec) * math.sin(ra),
            d * math.sin(dec))


def match(root, archive):
    rows = list(csv.DictReader(open(archive, encoding="utf-8")))
    hosts = {}
    for r in rows:
        if r["ra"] and r["dec"] and r["sy_dist"]:
            hosts.setdefault(r["hostname"], r)
    result = {}
    for sid, name, pos, stars in load_systems(root):
        if not any(pos):
            continue  # Sol: the origin, observed directly, no archive host
        u, d = unit(pos)
        found = []
        for h, r in hosts.items():
            hu, hd = unit(host_vector(r))
            ang = math.degrees(math.acos(max(-1.0, min(1.0, sum(a * b for a, b in zip(u, hu))))))
            if ang < 0.5 and abs(hd - d) / d < 0.08:
                found.append((h, ang, hd))
        result[sid] = (name, stars, found)
    return result, rows


if __name__ == "__main__":
    res, rows = match(sys.argv[1], sys.argv[2])
    for sid, (name, stars, found) in sorted(res.items()):
        planets = {h: [r["pl_name"] for r in rows if r["hostname"] == h] for h, _, _ in found}
        print(f"{sid:>6} {name:28} stars={stars}")
        for h, ang, hd in found:
            print(f"         host {h!r} ang={ang:.3f} d={hd:.2f} ly planets={planets[h]}")
