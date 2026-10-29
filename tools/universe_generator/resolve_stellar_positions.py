"""Resolve the real ICRS position of every catalog star system.

usage: python resolve_stellar_positions.py CATALOG_ROOT HYG_CSV ARCHIVE_CSV OUT_CSV

Sources (both pinned, see tools/universe_generator/reference/README.md):
- HYG v4.1 (astronexus) - Hipparcos / Yale / Gliese merge, J2000 RA/Dec,
  distance in parsecs;
- NASA Exoplanet Archive pscomppars - for planet hosts absent from HYG
  (ultracool dwarfs).

Every system is resolved through an explicit catalogue identifier below, never
through a fuzzy name match. The legacy catalog distance is kept as an identity
check only: a resolved star whose distance differs by more than 15 % is
reported as a mismatch instead of being accepted.
"""
import csv
import glob
import json
import math
import os
import sys

LY_PER_PC = 3.261563777

# system id -> (source, key, value). The primary (A) component defines the
# system position; companions are placed by the stellar hierarchy.
IDENTIFIERS = {
    1: ("hyg", "gl", "Gl 559A"),        # alpha Centauri A
    2: ("hyg", "gl", "Gl 699"),         # Barnard's Star
    3: ("hyg", "gl", "Gl 406"),         # Wolf 359
    4: ("hyg", "gl", "Gl 411"),         # Lalande 21185
    5: ("hyg", "gl", "Gl 244A"),        # Sirius A
    6: ("hyg", "gl", "Gl 273"),         # Luyten's Star
    7: ("hyg", "gl", "Gl 191"),         # Kapteyn's Star
    8: ("hyg", "gl", "Gl 71"),          # tau Ceti
    9: ("hyg", "gl", "Gl 144"),         # epsilon Eridani
    10: ("hyg", "gl", "Gl 581"),
    11: ("hyg", "gl", "Gl 667A"),
    12: ("hyg", "gl", "Gl 832"),
    13: ("hyg", "gl", "Gl 876"),
    14: ("hyg", "hd", "20794"),
    15: ("hyg", "hd", "40307"),
    16: ("hyg", "hd", "69830"),
    17: ("archive", "hostname", "TRAPPIST-1"),
    19: ("hyg", "hd", "102194"),
    20: ("hyg", "hd", "93083"),
    21: ("archive", "hostname", "GJ 1214"),
    22: ("hyg", "hip", "99240"),        # delta Pavonis
    23: ("hyg", "gl", "Gl 892"),        # = HD 219134
    24: ("hyg", "gl", "Gl 436"),
    25: ("archive", "hostname", "GJ 357"),
    26: ("archive", "hostname", "Teegarden's Star"),
    27: ("hyg", "gl", "Gl 447"),        # Ross 128
    28: ("hyg", "gl", "Gl 54.1"),       # YZ Ceti
    29: ("hyg", "gl", "Gl 229"),
    30: ("hyg", "gl", "Gl 845"),        # epsilon Indi A
    31: ("hyg", "gl", "Gl 820A"),       # 61 Cygni A
    32: ("hyg", "proper", "Aldebaran"),
    33: ("hyg", "proper", "Hamal"),
    34: ("hyg", "hip", "19893"),        # gamma Doradus
    35: ("hyg", "hip", "104887"),       # tau Cygni
    36: ("hyg", "hd", "1835"),          # 9 Ceti
    37: ("hyg", "hd", "114783"),
    38: ("hyg", "hd", "78154"),         # sigma2 Ursae Majoris
    39: ("hyg", "hd", "114613"),
    40: ("hyg", "hd", "24496"),
    41: ("hyg", "hd", "104067"),
    42: ("hyg", "hd", "187013"),        # 17 Cygni
    43: ("hyg", "hd", "144284"),        # theta Draconis
    44: ("hyg", "hd", "33564"),
    45: ("hyg", "hd", "99491"),         # 83 Leonis A
    46: ("hyg", "hd", "1237"),          # GJ 3021
    47: ("hyg", "hd", "154345"),
    48: ("hyg", "hd", "102365"),
    49: ("hyg", "gl", "Gl 176"),
    50: ("archive", "hostname", "GJ 3293"),
    51: ("hyg", "gl", "Gl 433"),
    52: ("hyg", "gl", "Gl 280A"),       # Procyon A
    53: ("hyg", "gl", "Gl 674"),
    54: ("hyg", "hd", "85512"),
    55: ("archive", "hostname", "GJ 163"),
    56: ("archive", "hostname", "GJ 422"),
    57: ("hyg", "hd", "10360"),         # p Eridani B
    58: ("hyg", "gl", "Gl 514"),
    59: ("hyg", "gl", "Gl 625"),
    60: ("hyg", "gl", "Gl 551"),        # Proxima Centauri
    100062: ("archive", "hostname", "Kepler-62"),
}


def icrs_ly(ra_deg, dec_deg, dist_ly):
    ra, dec = math.radians(ra_deg), math.radians(dec_deg)
    return (dist_ly * math.cos(dec) * math.cos(ra),
            dist_ly * math.cos(dec) * math.sin(ra),
            dist_ly * math.sin(dec))


def norm(s):
    return " ".join(s.split()).lower()


def load_hyg(path):
    index = {"gl": {}, "hd": {}, "hip": {}, "proper": {}}
    for r in csv.DictReader(open(path, encoding="utf-8")):
        for k in index:
            if r[k]:
                index[k].setdefault(norm(r[k]), r)
    return index


def load_archive(path):
    hosts = {}
    for r in csv.DictReader(open(path, encoding="utf-8")):
        hosts.setdefault(norm(r["hostname"]), r)
    return hosts


def resolve(root, hyg_path, archive_path):
    hyg = load_hyg(hyg_path)
    archive = load_archive(archive_path)
    out = []
    for sub in ("systems_details", "distant_systems_details"):
        for path in sorted(glob.glob(os.path.join(root, sub, "*.json"))):
            doc = json.load(open(path, encoding="utf-8"))
            sid = doc["id"]
            p = doc["position_ly"]
            old = (p["x"], p["y"], p["z"])
            old_d = math.sqrt(sum(c * c for c in old))
            if sid == 0:
                out.append(dict(id=0, name=doc["name"], source="definition", key="Sol",
                                ra=0.0, dec=0.0, dist_ly=0.0, x=0.0, y=0.0, z=0.0,
                                old_x=0.0, old_y=0.0, old_z=0.0, shift_ly=0.0,
                                dist_ratio=1.0, spect="G2V", status="ok"))
                continue
            source, field, value = IDENTIFIERS[sid]
            row = (hyg[field].get(norm(value)) if source == "hyg"
                   else archive.get(norm(value)))
            if row is None:
                out.append(dict(id=sid, name=doc["name"], source=source,
                                key=f"{field}={value}", status="unresolved"))
                continue
            if source == "hyg":
                ra = float(row["ra"]) * 15.0
                dec = float(row["dec"])
                dist = float(row["dist"]) * LY_PER_PC
                spect = row["spect"]
            else:
                ra = float(row["ra"])
                dec = float(row["dec"])
                dist = float(row["sy_dist"]) * LY_PER_PC
                spect = row["st_spectype"]
            pos = icrs_ly(ra, dec, dist)
            ratio = dist / old_d
            out.append(dict(
                id=sid, name=doc["name"], source=source, key=f"{field}={value}",
                ra=round(ra, 6), dec=round(dec, 6), dist_ly=round(dist, 4),
                x=round(pos[0], 4), y=round(pos[1], 4), z=round(pos[2], 4),
                old_x=old[0], old_y=old[1], old_z=old[2],
                shift_ly=round(math.dist(pos, old), 3), dist_ratio=round(ratio, 3),
                spect=spect,
                status="ok" if abs(ratio - 1.0) <= 0.15 else "distance_mismatch"))
    return out


if __name__ == "__main__":
    rows = resolve(sys.argv[1], sys.argv[2], sys.argv[3])
    cols = ["id", "name", "source", "key", "ra", "dec", "dist_ly", "x", "y", "z",
            "old_x", "old_y", "old_z", "shift_ly", "dist_ratio", "spect", "status"]
    with open(sys.argv[4], "w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        for r in rows:
            w.writerow({c: r.get(c, "") for c in cols})
    for r in rows:
        print(f"{r['id']:>6} {r['name'][:24]:24} {r['status']:18} {r.get('key',''):22} "
              f"d={r.get('dist_ly','')} ratio={r.get('dist_ratio','')} shift={r.get('shift_ly','')} {r.get('spect','')}")
