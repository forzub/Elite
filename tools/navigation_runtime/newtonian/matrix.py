"""Behaviour criteria of one offline matrix run (production route autopilot +
production ShipCore step), or the summary table of a whole matrix.

usage: python matrix.py run TAG GEOM     -> one TSV result line on stdout
       python matrix.py table RESULTS    -> PASS/FAIL table of a results file

inputs of `run`: $ELITE_DIAG_DIR/elite_TAG_{decision,events,state}.tsv written
by EliteNewtonianReplay (ELITE_NEWTONIAN_POLICY_TRACE / _EVENT_TRACE,
ELITE_REPLAY_STATE_TRACE) and the scenario .geom file of
EliteNewtonianScenarios.

Hard criteria (FAIL): corridor violations, hull outside a KeepInside volume
(real hull box corners, exact clearance), hull into an obstacle sphere, no
terminal DONE, non-finite state, main engine inside a narrow zone, main in the
terminal sequence, hull-target jumps not explained by a phase/program/terminal
change, a main-engine micro-brake (< 3 m/s, not the terminal stop). Recorded metrics:
program recompiles, worst program thrust deficit, max angular rate inside
narrow zones, DONE time, max cross-track, re-holds after DONE.
"""
import csv, math, os, re, sys

DIAG = os.environ.get("ELITE_DIAG_DIR", "D:/tmp")
PROG = {0: "COAST", 1: "ORIENT", 2: "FLIP", 3: "BURN", 4: "BRAKE", 5: "CURVE", 6: "SETTLE"}
COLUMNS = ["tag", "plan", "viol", "keep_m", "obst_m", "done_s", "finite", "main_narrow",
           "term_main", "jumps", "microbrake", "recompiles", "deficit_deg", "narrow_omega",
           "xtrack_m", "reholds"]


def read_geom(path):
    g = {"volumes": [], "spheres": [], "plan": "missing", "hull": None, "narrow": []}
    for line in open(path):
        w = line.split()
        if not w:
            continue
        if w[0] == "hull":
            g["hull"] = tuple(float(x) for x in w[1:4]); g["radius"] = float(w[5])
        elif w[0] == "volume":
            v = [float(x) for x in w[8:]]
            g["volumes"].append({"rect": w[1] == "rect", "clearance": float(w[3]),
                                 "narrow": w[5] == "0", "a": v[0:3], "b": v[3:6],
                                 "up": v[6:9], "radius": v[9], "hw": v[10], "hh": v[11]})
        elif w[0] == "sphere":
            g["spheres"].append(([float(x) for x in w[1:4]], float(w[4])))
        elif w[0] == "plan":
            g["plan"] = "ok" if w[1] == "ok" else "rejected"
        elif w[0] == "narrow":
            g["narrow"].append((float(w[1]), float(w[2])))
    return g


def sub(a, b): return [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def norm(a):
    n = math.sqrt(dot(a, a)); return [a[0] / n, a[1] / n, a[2] / n]
def cross(a, b):
    return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]


def run(tag, geom_path):
    g = read_geom(geom_path)
    out = dict.fromkeys(COLUMNS, "")
    out["tag"] = tag; out["plan"] = g["plan"]
    if g["plan"] != "ok":
        return out
    base = os.path.join(DIAG, f"elite_{tag}")
    hw, hh, hl = g["hull"]
    for v in g["volumes"]:
        v["axis"] = norm(sub(v["b"], v["a"])); v["len"] = math.dist(v["a"], v["b"])
        v["u"] = norm(v["up"]); v["r"] = cross(v["axis"], v["u"])

    finite = True; keep = math.inf; obst = math.inf; main_narrow = 0; narrow_omega = 0.0
    with open(base + "_state.tsv") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            vals = {k: float(x) for k, x in r.items()}
            if not all(math.isfinite(x) for x in vals.values()):
                finite = False; continue
            p = [vals["px"], vals["py"], vals["pz"]]
            fw = norm([vals["fx"], vals["fy"], vals["fz"]]); up = norm([vals["ux"], vals["uy"], vals["uz"]])
            rt = cross(fw, up)
            omega = math.sqrt(vals["wp"] ** 2 + vals["wy"] ** 2 + vals["wr"] ** 2)
            for v in g["volumes"]:
                s = dot(sub(p, v["a"]), v["axis"])
                if not (0.0 <= s <= v["len"]):
                    continue
                # In the transit, not elsewhere on the route crossing the
                # volume's axial slab: the COM within the outer bound plus
                # the hull's bounding radius.
                d0 = sub(p, v["a"])
                outer = (v["radius"] if not v["rect"] else math.hypot(v["hw"], v["hh"])) + g["radius"]
                if math.hypot(dot(d0, v["r"]), dot(d0, v["u"])) > outer:
                    continue
                if v["narrow"]:
                    narrow_omega = max(narrow_omega, omega)
                    if vals["main"] > 0.01:
                        main_narrow += 1
                for i in (-1, 1):
                    for j in (-1, 1):
                        for k in (-1, 1):
                            c = [p[n] + i * hl * fw[n] + j * hh * up[n] + k * hw * rt[n] for n in range(3)]
                            d = sub(c, v["a"])
                            if not (0.0 <= dot(d, v["axis"]) <= v["len"]):
                                continue
                            x, y = dot(d, v["r"]), dot(d, v["u"])
                            if v["rect"]:
                                m = min(v["hw"] - v["clearance"] - abs(x), v["hh"] - v["clearance"] - abs(y))
                            else:
                                m = v["radius"] - v["clearance"] - math.hypot(x, y)
                            keep = min(keep, m)
            for c, radius in g["spheres"]:
                obst = min(obst, math.dist(p, c) - radius - g["radius"])

    rows = []; keys = set()
    with open(base + "_decision.tsv") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            try:
                d = {k: float(x) for k, x in r.items()}
            except (TypeError, ValueError):
                continue
            if d["tol_m"] > 0.0:
                rows.append(d); keys.add((r["t"], r["s_m"]))
    events = []
    with open(base + "_events.tsv") as f:
        for e in csv.DictReader(f, delimiter="\t"):
            if (e["t"], e["s_m"]) in keys:
                e["t"] = float(e["t"]); events.append(e)

    viol = sum(1 for e in events if e["event"] == "CORRIDOR_VIOLATION")
    done = [e["t"] for e in events if e["event"] == "TERMINAL_STEP" and e["detail"].startswith("DONE")]
    reholds = sum(1 for e in events if e["event"] == "TERMINAL_STEP" and done and
                  e["t"] > done[0] and e["detail"].startswith("POSITION_HOLD"))
    term_main = sum(1 for r in rows if r["term_step"] > 0 and r["throttle"] > 0.01)
    explain = [e["t"] for e in events if e["event"] in ("MANOEUVRE_PHASE", "PROGRAM", "TERMINAL_STEP",
                                                        "PHASE_CHANGE", "SAFETY_ON", "SAFETY_OFF")]
    jumps = sum(1 for e in events if e["event"] == "TARGET_SWITCH" and
                not any(abs(e["t"] - t) <= 0.1 for t in explain))
    cruise = max(r["profile_mps"] for r in rows) if rows else 0.0
    # A micro-brake: a main-engine BRAKE episode (throttle > 5 %) shedding
    # less than 3 m/s that is not the terminal stop. A phase label without
    # main, or a small trim flown on RCS, is no manoeuvre.
    microbrake = 0
    episode = None
    for r in rows + [None]:
        braking = r is not None and int(r["prog_phase"]) == 4 and r["term_step"] == 0
        if braking:
            if episode is None:
                episode = [r["speed_mps"], r["speed_mps"], 0.0]
            episode[1] = min(episode[1], r["speed_mps"])
            episode[2] = max(episode[2], r["throttle"])
        elif episode is not None:
            if (episode[0] - episode[1] < 3.0 and episode[2] > 0.05 and r is not None
                    and r["term_step"] == 0 and episode[1] > 5.0):
                microbrake += 1
            episode = None
    recompiles = sum(1 for e in events if e["event"] == "PROGRAM")
    deficits = [float(m.group(1)) for e in events if e["event"] == "PROGRAM"
                for m in [re.search(r"deficit_deg=([0-9.]+)", e["detail"])] if m]
    out.update(viol=viol, keep_m=f"{keep:.2f}" if keep < math.inf else "-",
               obst_m=f"{obst:.1f}" if obst < math.inf else "-",
               done_s=f"{done[0]:.1f}" if done else "none", finite=int(finite),
               main_narrow=main_narrow, term_main=term_main, jumps=jumps, microbrake=microbrake,
               recompiles=recompiles, deficit_deg=f"{max(deficits):.1f}" if deficits else "0",
               narrow_omega=f"{narrow_omega:.3f}" if any(v["narrow"] for v in g["volumes"]) else "-",
               xtrack_m=f"{max(r['xtrack_m'] for r in rows):.1f}" if rows else "-", reholds=reholds)
    return out


def failures(r):
    bad = []
    if r["plan"] != "ok":
        return ["plan-" + r["plan"]]
    if int(r["viol"]): bad.append("corridor")
    if r["keep_m"] not in ("", "-") and float(r["keep_m"]) < 0.0: bad.append("keep-inside")
    if r["obst_m"] not in ("", "-") and float(r["obst_m"]) < 0.0: bad.append("obstacle")
    if r["done_s"] == "none": bad.append("no-DONE")
    if r["finite"] != "1": bad.append("non-finite")
    if int(r["main_narrow"]): bad.append("main-in-narrow")
    if int(r["term_main"]): bad.append("terminal-main")
    if int(r["jumps"]): bad.append("target-jumps")
    if int(r["microbrake"]): bad.append("micro-brake")
    return bad


if __name__ == "__main__":
    if sys.argv[1] == "run":
        r = run(sys.argv[2], sys.argv[3])
        print("\t".join(str(r[c]) for c in COLUMNS))
    else:
        rows = list(csv.DictReader(open(sys.argv[2]), delimiter="\t", fieldnames=COLUMNS))
        npass = 0
        print("\t".join(COLUMNS[:1] + ["verdict"] + COLUMNS[1:]))
        for r in sorted(rows, key=lambda r: r["tag"]):
            bad = failures(r)
            npass += not bad
            print("\t".join([r["tag"], "PASS" if not bad else "FAIL:" + ",".join(bad)] +
                            [str(r[c]) for c in COLUMNS[1:]]))
        print(f"# {npass}/{len(rows)} PASS")
