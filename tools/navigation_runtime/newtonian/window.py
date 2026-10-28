"""usage: python window.py TAG T0 T1 [step]  - decision rows + events in a window"""
import csv, os, sys
tag, t0, t1 = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
step = float(sys.argv[4]) if len(sys.argv) > 4 else 0.0
base = os.path.join(os.environ.get("ELITE_DIAG_DIR", "D:/tmp"), f"elite_{tag}")
keys = set(); rows = []
for r in csv.DictReader(open(base + "_decision.tsv"), delimiter="\t"):
    if not r["tol_m"] or float(r["tol_m"]) <= 0: continue
    keys.add((r["t"], r["s_m"]))
    if t0 <= float(r["t"]) <= t1: rows.append(r)
ev = [e for e in csv.DictReader(open(base + "_events.tsv"), delimiter="\t")
      if (e["t"], e["s_m"]) in keys and t0 <= float(e["t"]) <= t1]
cols = ["t", "s_m", "radius_m", "profile_mps", "speed_mps", "xtrack_m", "pred2_m", "lat_v_mps",
        "long_err_mps", "ff_along", "ff_lat", "demand_acc"]
first = rows[0] if rows else {}
for c in (["prog_phase", "phase_idx", "main_wanted", "override", "plan_hull_err_deg"] if "prog_phase" in first
          else ["preview_k", "preview_reason", "preview_start_s", "urgent", "anticipate", "curve_ant", "latch"]):
    cols.append(c)
cols += ["phase", "gate", "gate_reason", "tube", "emergency", "braking", "hull_err_deg",
         "thrust_err_deg", "throttle", "omega", "target_delta_deg", "thrust_delta_deg"]
print("\t".join(c[:9] for c in cols))
last = -1e9
for r in rows:
    if float(r["t"]) - last + 1e-9 < step: continue
    last = float(r["t"])
    print("\t".join(r[c] for c in cols))
print("--- events")
for e in ev:
    print(e["t"], e["s_m"], e["event"], e["detail"])
