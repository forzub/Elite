"""Newtonian decision-trace analysis (Step 2 acceptance criteria).

usage: python decision.py TAG
inputs: $ELITE_DIAG_DIR/elite_TAG_decision.tsv, $ELITE_DIAG_DIR/elite_TAG_events.tsv
"""
import csv, os, sys
from collections import Counter

tag = sys.argv[1]
base = os.path.join(os.environ.get("ELITE_DIAG_DIR", "D:/tmp"), f"elite_{tag}")
PH = {0: "COAST", 1: "FLIP", 2: "ALIGN", 3: "BURN", 4: "HOLD"}

rows = []
with open(base + "_decision.tsv") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        try:
            d = {k: float(v) for k, v in r.items()}
        except (ValueError, TypeError):
            continue
        if d["tol_m"] > 0.0:
            rows.append(d)
# Every event tick also writes a decision row with the same (t, s); events of
# controller instances without a route corridor (tol 0) are dropped.
route_keys = set()
with open(base + "_decision.tsv") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        if r["tol_m"] and float(r["tol_m"]) > 0.0:
            route_keys.add((r["t"], r["s_m"]))
events = []
with open(base + "_events.tsv") as f:
    for r in csv.DictReader(f, delimiter="\t"):
        if (r["t"], r["s_m"]) not in route_keys:
            continue
        r["t"] = float(r["t"]); r["s_m"] = float(r["s_m"])
        events.append(r)
if not rows:
    print("no route rows"); sys.exit(0)

tol = max(r["tol_m"] for r in rows)
t0, t1 = rows[0]["t"], rows[-1]["t"]
xt = max(rows, key=lambda r: r["xtrack_m"])
lv = max(rows, key=lambda r: r["lat_v_mps"])
print(f"[{tag}] route rows={len(rows)} t={t0:.1f}..{t1:.1f}s s_max={max(r['s_m'] for r in rows):.0f} m tol={tol:.0f} m")
print("SAFETY")
viol = [e for e in events if e["event"] == "CORRIDOR_VIOLATION"]
print(f"  corridor violations: {len(viol)}  max xtrack={xt['xtrack_m']:.1f} m at t={xt['t']:.1f} s={xt['s_m']:.0f}")
bad = [r for r in rows if r["throttle"] > 0.5 and r["thrust_err_deg"] > 15.0]
print(f"  strong main (>50%) at >15deg achievable thrust error: {len(bad)} rows")
print("ROUTE EXECUTION")
print(f"  max lateral velocity={lv['lat_v_mps']:.1f} m/s at t={lv['t']:.1f}")
ev = Counter(e["event"] for e in events)
print("  events: " + ", ".join(f"{k}={v}" for k, v in sorted(ev.items())))
print("CONTROL QUALITY")
big = [e for e in events if e["event"] == "TARGET_SWITCH"]
rev = [e for e in events if e["event"] == "COMMAND_REVERSAL"]
print(f"  hull-target jumps >25deg/tick: {len(big)} | thrust reversals >90deg/tick or rotation sign flips: {len(rev)}")
PROG = {0:"COAST",1:"ORIENT",2:"FLIP",3:"BURN",4:"BRAKE",5:"CURVE",6:"SETTLE",-1:"NONE"}
if "prog_phase" in rows[0]:
    pp = Counter(int(r["prog_phase"]) for r in rows)
    print("  program phase share: " + " ".join(f"{PROG.get(k,k)}={100*v/len(rows):.0f}%" for k, v in sorted(pp.items())))
    ov = sum(1 for r in rows if r["override"] == 1.0)
    print(f"  safety override rows: {ov}  plan-hull error max={max(r['plan_hull_err_deg'] for r in rows):.1f} deg")
ph = Counter(int(r["phase"]) for r in rows)
print("  phase share: " + " ".join(f"{PH.get(k,k)}={100*v/len(rows):.0f}%" for k, v in sorted(ph.items())))

# Conflict episodes: cluster events within 2 s, rank by number of
# switches/reversals/gate churn.
interesting = {"SAFETY_ON", "TARGET_SWITCH", "COMMAND_REVERSAL", "URGENT_ON", "URGENT_OFF",
               "BURN_GATE_OPEN", "BURN_GATE_CLOSE", "LATCH_SET", "LATCH_CLEAR",
               "CORRIDOR_VIOLATION", "ENTER_CURVE", "LEAVE_CURVE"}
clusters = []
for e in events:
    if e["event"] not in interesting:
        continue
    if clusters and e["t"] - clusters[-1][-1]["t"] <= 2.0:
        clusters[-1].append(e)
    else:
        clusters.append([e])
def score(c):
    return sum(3 if e["event"] in ("COMMAND_REVERSAL", "TARGET_SWITCH", "CORRIDOR_VIOLATION") else 1 for e in c)
print("TOP CONFLICT EPISODES")
for c in sorted(clusters, key=score, reverse=True)[:8]:
    k = Counter(e["event"] for e in c)
    print(f"  t={c[0]['t']:.1f}-{c[-1]['t']:.1f}s s={c[0]['s_m']:.0f}-{c[-1]['s_m']:.0f} m score={score(c)}: " +
          ", ".join(f"{a}x{b}" for a, b in k.most_common()))
progs = [e for e in events if e["event"] == "PROGRAM"]
if progs:
    import re
    defs = Counter(re.search(r"deficit_deg=([0-9.]+) at=([0-9]+)", e["detail"]).groups() for e in progs
                   if re.search(r"deficit_deg=", e["detail"]))
    print("PROGRAM thrust deficits (deg at s, count): " +
          ", ".join(f"{d}@{s}x{c}" for (d, s), c in defs.most_common(4)))
