"""One-line summary of a replay/flight decision trace for parameter sweeps.

usage: python sweep_summary.py TAG
prints: violations max_xtrack max_latv gate_closes hull_turn_rad done_s zone_xtrack
"""
import csv, os, sys

tag = sys.argv[1]
base = os.path.join(os.environ.get("ELITE_DIAG_DIR", "D:/tmp"), f"elite_{tag}")
keys = set(); rows = []
for r in csv.DictReader(open(base + "_decision.tsv"), delimiter="\t"):
    if not r.get("tol_m") or float(r["tol_m"]) <= 0:
        continue
    keys.add((r["t"], r["s_m"])); rows.append(r)
events = [e for e in csv.DictReader(open(base + "_events.tsv"), delimiter="\t")
          if (e["t"], e["s_m"]) in keys]
violations = sum(1 for e in events if e["event"] == "CORRIDOR_VIOLATION")
closes = sum(1 for e in events if e["event"] == "BURN_GATE_CLOSE")
xt = max(float(r["xtrack_m"]) for r in rows)
lv = max(float(r["lat_v_mps"]) for r in rows)
turn = 0.0
prev = None
for r in rows:
    t = float(r["t"])
    if prev is not None and 0.0 < t - prev[0] < 0.25:
        turn += 0.5 * (float(r["omega"]) + prev[1]) * (t - prev[0])
    prev = (t, float(r["omega"]))
done = next((float(e["t"]) for e in events
             if e["event"] == "TERMINAL_STEP" and e["detail"].startswith("DONE")), float("nan"))
zone = [float(r["xtrack_m"]) for r in rows if r.get("narrow") == "1"]
zone_xt = max(zone) if zone else float("nan")
print(f"{tag}\tviol={violations}\txt={xt:.1f}\tlatv={lv:.1f}\tgate_close={closes}"
      f"\tturn={turn:.1f}\tdone={done:.1f}\tzone_xt={zone_xt:.2f}")
