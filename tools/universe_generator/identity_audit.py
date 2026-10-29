"""Identity audit of every catalog star system against real catalogues.

usage: python identity_audit.py LEGACY_ROOT HYG_FULL_CSV OUT_MD

LEGACY_ROOT   the legacy galaxy_details tree (systems_details/,
              distant_systems_details/) extracted from git history - the
              authored identity: name, aliases, approximate distance,
              spectral type, planetary lore
HYG_FULL_CSV  the full HYG v4.1 (sha256 pinned in reference/README.md)

A resolved identity is accepted only when several independent clues agree,
never on one matching catalogue number:
- designation / aliases contain the resolved identifier;
- legacy distance within 15 % of the real one;
- legacy spectral class (letter, luminosity class) compatible;
- legacy planetary lore compatible with the archive planets of the host.

For every system it also lists alternative candidates (catalogue numbers
adjacent to the one in the name, archive hosts near the legacy distance whose
name shares a token) so a typo such as HD 102194 -> HD 102195 is visible.
"""
import csv
import glob
import json
import math
import os
import re
import sys

import resolve_stellar_positions as rsp
import build_stellar_inputs as bsi

LY_PER_PC = rsp.LY_PER_PC


def spectral_key(sp):
    sp = (sp or "").strip()
    m = re.match(r"(sd|d|D)?([OBAFGKMLTYW])", sp)
    letter = m.group(2) if m else "?"
    if sp.startswith("D") and not sp.startswith("dM"):
        return "D", "WD"
    lum = "V"
    if re.search(r"III|II(?!I)|Ib|Ia", sp):
        lum = "III"
    elif re.search(r"IV(?![-/]V)", sp):
        lum = "IV"
    return letter, lum


ORDER = "OBAFGKMLTY"


def spectral_compatible(a, b):
    la, ca = spectral_key(a)
    lb, cb = spectral_key(b)
    if "?" in (la, lb):
        return True, "unknown"
    if la == "D" or lb == "D":
        return la == lb, f"{la}/{lb}"
    sub = abs(ORDER.index(la) - ORDER.index(lb))
    giant_mismatch = (ca == "III") != (cb == "III")
    ok = sub <= 1 and not giant_mismatch
    return ok, f"{la}{ca} vs {lb}{cb}"


GREEK = dict(alp="alpha", bet="beta", gam="gamma", del_="delta", eps="epsilon", zet="zeta",
             eta="eta", the="theta", iot="iota", kap="kappa", lam="lambda", mu="mu", nu="nu",
             xi="xi", omi="omicron", pi="pi", rho="rho", sig="sigma", tau="tau", ups="upsilon",
             phi="phi", chi="chi", psi="psi", ome="omega")


def name_key(n):
    n = n.lower().replace("²", "2").replace("¹", "1").replace("'", "")
    n = re.sub(r"(?<![a-z])(gliese|gj)\s*", "gl ", n)
    return " ".join(n.split())


def designation_match(row, names):
    """True when an authored name names this HYG star (proper, Bayer, Flamsteed, Gl, HD, HIP)."""
    keys = [name_key(n) for n in names]
    words = [k.split() for k in keys]
    con = row["con"].lower()

    def has(pred):
        return any(pred(k, w) for k, w in zip(keys, words))

    if row["proper"] and has(lambda k, w: name_key(row["proper"]) in k):
        return True
    for col, label in (("hd", "hd"), ("hip", "hip")):
        if row[col] and has(lambda k, w, v=row[col], l=label: re.search(rf"(?<![a-z]){l}\s*{v}(?!\d)", k)):
            return True
    if row["gl"]:
        g = name_key(row["gl"]).replace("gl ", "")
        g0 = re.sub(r"[a-z]+$", "", g)
        if has(lambda k, w: re.search(rf"(?<![a-z])gl\s*{re.escape(g0)}(?![\d.])", k)):
            return True
    if row["bayer"] and con:
        base, _, idx = row["bayer"].lower().partition("-")
        greek = GREEK.get(base, GREEK.get(base + "_", base))
        if has(lambda k, w: any(x.startswith(greek) for x in w) and any(x.startswith(con[:3]) for x in w[1:])):
            return True
    if row["flam"] and con:
        if has(lambda k, w: len(w) > 1 and w[0] == row["flam"] and w[1].startswith(con[:3])):
            return True
    return False


# Residual disagreements reviewed by hand: the identity is right, the failing
# clue is a defect of the legacy value or of the catalogue column.
REVIEWED = {
    34: "gamma Dor is the F1V prototype of its variable class (Gray et al. 2006); "
        "the HYG F4III is an old classification - identity correct",
    48: "HD 102365 (G3/G5V + M companion, 'binary' in lore) is unique; the legacy 42 ly was invented",
    50: "GJ 3293 b, c, d, e match the four archive planets by name; the legacy 51 ly was invented",
    57: "HD 10360 = p Eridani B (K0V) with p Eri A in the system; no candidate near the "
        "legacy 113 ly carries the name - the legacy distance and planets were invented",
}


def load_hyg_full(path):
    rows = list(csv.DictReader(open(path, encoding="utf-8")))
    by = {"hd": {}, "hip": {}, "gl": {}, "proper": {}}
    for r in rows:
        for k in by:
            if r[k]:
                by[k].setdefault(rsp.norm(r[k]), r)
    return rows, by


def legacy_systems(root):
    for sub in ("systems_details", "distant_systems_details"):
        for path in sorted(glob.glob(os.path.join(root, sub, "*.json"))):
            yield rsp.apply_corrections(json.load(open(path, encoding="utf-8")))


def legacy_planets(doc):
    out = []
    for s in doc["details"]["stars"]:
        for p in s.get("planets", []):
            out.append(dict(name=p["name"], period=p.get("orbital_period_days"),
                            a=p.get("distance_au"), mass=p.get("mass_earth"),
                            status=p.get("discovery_status", "")))
    return out


def tokens(doc):
    names = [doc["name"]] + doc["details"].get("alternative_names", [])
    for s in doc["details"]["stars"]:
        names += [s.get("name", "")] + s.get("alternative_names", [])
    return names


def main():
    legacy_root, hyg_path, out_path = sys.argv[1:4]
    _, hyg = load_hyg_full(hyg_path)
    hosts = bsi.archive_hosts()
    lines = ["# Identity audit of the local catalog star systems", "",
             "Each system: legacy authored identity vs the resolved real star. "
             "Verdict `ok` needs designation + distance + spectral class + planet lore to agree.", ""]
    summary = []
    for doc in legacy_systems(legacy_root):
        sid = doc["id"]
        if sid == 0:
            continue
        p = doc["position_ly"]
        legacy_d = math.sqrt(p["x"] ** 2 + p["y"] ** 2 + p["z"] ** 2)
        legacy_sp = doc["details"]["stars"][0].get("type", "").split(" ")[0]
        names = tokens(doc)
        source, field, value = rsp.IDENTIFIERS[sid]
        if source == "hyg":
            row = hyg[field].get(rsp.norm(value))
            real_d = float(row["dist"]) * LY_PER_PC
            real_sp = row["spect"]
            pos = rsp.icrs_ly(float(row["ra"]) * 15.0, float(row["dec"]), real_d)
            ids = f"HIP {row['hip']} HD {row['hd']} Gl {row['gl']} {row['proper']} {row['bf']}".strip()
        else:
            r = hosts[value][0]
            real_d = float(r["sy_dist"]) * LY_PER_PC
            real_sp = r["st_spectype"]
            pos = rsp.icrs_ly(float(r["ra"]), float(r["dec"]), real_d)
            ids = f"{value} {r['hd_name']} {r['hip_name']}"
        archive = bsi.host_for_position(hosts, pos)
        arch_planets = [(pl["pl_name"], bsi.fnum(pl, "pl_orbper"), bsi.fnum(pl, "pl_bmasse"))
                        for h in archive for pl in hosts[h]]
        lp = legacy_planets(doc)

        checks = []
        ratio = real_d / legacy_d if legacy_d else 1.0
        checks.append(("distance", abs(ratio - 1) <= 0.15, f"legacy {legacy_d:.1f} ly, real {real_d:.1f} ly (x{ratio:.2f})"))
        ok, note = spectral_compatible(legacy_sp, real_sp)
        checks.append(("spectral", ok, f"legacy {legacy_sp}, real {real_sp} ({note})"))
        num = re.sub(r"\D", "", value)
        if source == "hyg":
            named = designation_match(row, names)
        else:
            named = any(name_key(value) in name_key(n) for n in names)
        checks.append(("designation", named, f"resolved {field}={value}; names {names[:4]}"))
        # Planet lore: a legacy hot planet (P < 10 d, mass > 10 Me) needs a real one.
        hot_legacy = [x for x in lp if x["period"] and x["period"] < 10 and (x["mass"] or 0) > 10]
        hot_real = [x for x in arch_planets if x[1] and x[1] < 10 and (x[2] or 0) > 10]
        lore_ok = not hot_legacy or bool(hot_real)
        checks.append(("planet lore", lore_ok,
                       f"legacy hot massive {[x['name'] for x in hot_legacy]}, archive {[(n, round(per or 0, 2)) for n, per, _ in arch_planets]}"))

        # Alternative candidates: adjacent catalogue numbers, archive hosts sharing a name token.
        alts = []
        if field in ("hd", "hip") and num:
            for k in range(-3, 4):
                if k == 0:
                    continue
                cand = hyg[field].get(str(int(num) + k))
                if cand:
                    cd = float(cand["dist"]) * LY_PER_PC
                    if abs(cd / legacy_d - 1) < 0.25:
                        alts.append(f"HYG {field.upper()} {int(num) + k}: {cand['spect']} {cd:.1f} ly")
        for h, rows in hosts.items():
            hn = re.sub(r"\D", "", h)
            if num and hn and abs(int(hn) - int(num)) <= 3 and hn != num and rows[0]["sy_dist"]:
                hd_ = float(rows[0]["sy_dist"]) * LY_PER_PC
                if abs(hd_ / legacy_d - 1) < 0.25 and h.split()[0] in value.upper() + "HD GJ":
                    alts.append(f"archive {h}: {rows[0]['st_spectype']} {hd_:.1f} ly planets "
                                f"{[(x['pl_name'], round(float(x['pl_orbper'] or 0), 2)) for x in rows]}")
        # Every catalogue number used as an alias must name a star of this system.
        bad = []
        for n in names:
            for kind, num_ in re.findall(r"(?i)\b(HD|HIP|GJ|Gl|Gliese)\s*(\d+(?:\.\d+)?)", n):
                col = {"hd": "hd", "hip": "hip"}.get(kind.lower(), "gl")
                cand = hyg[col].get(num_ if col != "gl" else f"gl {num_}")
                if cand is None and col == "gl":
                    cand = hyg["gl"].get(f"gl {num_}a")
                if cand is None:
                    continue
                cpos = rsp.icrs_ly(float(cand["ra"]) * 15.0, float(cand["dec"]), float(cand["dist"]) * LY_PER_PC)
                # parallax catalogues differ by a few per cent (HYG vs Gaia)
                if math.dist(cpos, pos) > max(1.0, 0.06 * real_d):
                    bad.append(n)
        checks.append(("aliases", not bad, f"aliases naming another star: {sorted(set(bad))}" if bad else "consistent"))
        verdict = "ok" if all(c[1] for c in checks) else "SUSPECT"
        if verdict == "SUSPECT" and sid in REVIEWED:
            verdict = "accepted (reviewed)"
            checks.append(("review", True, REVIEWED[sid]))
        if doc.get("identity_correction"):
            checks.append(("correction", True, doc["identity_correction"]["reason"]))
        if alts and verdict == "ok":
            verdict = "ok (alternatives listed)"
        summary.append((sid, doc["name"], verdict, [c[0] for c in checks if not c[1]]))
        lines.append(f"## {sid} {doc['name']} - {verdict}")
        lines.append(f"- resolved: {ids}")
        for name, good, note in checks:
            lines.append(f"- {'PASS' if good else 'FAIL'} {name}: {note}")
        lines.append(f"- legacy planets: {[(x['name'], x['period'], x['mass']) for x in lp]}")
        for a in alts:
            lines.append(f"- candidate: {a}")
        lines.append("")
    head = ["| id | system | verdict | failed checks |", "|---|---|---|---|"]
    head += [f"| {s} | {n} | {v} | {', '.join(f)} |" for s, n, v, f in summary]
    lines[4:4] = head + [""]
    open(out_path, "w", encoding="utf-8").write("\n".join(lines) + "\n")
    for s, n, v, f in summary:
        if v != "ok":
            print(s, n, v, f)


if __name__ == "__main__":
    main()
