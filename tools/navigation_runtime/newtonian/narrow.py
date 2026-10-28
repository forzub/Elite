"""Narrow-zone report (Step 3). usage: python narrow.py TAG [CASE_FILE]

Reads $ELITE_DIAG_DIR/elite_TAG_decision.tsv (rows of the route pilot) and,
when present, the zone up vector from $ELITE_DIAG_DIR/elite_TAG_case.txt.
"""
import csv, math, os, sys

tag = sys.argv[1]
base = os.path.join(os.environ.get("ELITE_DIAG_DIR", "D:/tmp"), f"elite_{tag}")
rows = [r for r in csv.DictReader(open(base + "_decision.tsv"), delimiter="\t")
        if r.get("tol_m") and float(r["tol_m"]) > 0 and r.get("narrow") is not None]
inside = [r for r in rows if r["narrow"] == "1"]
if not inside:
    print(f"[{tag}] no narrow-zone rows"); sys.exit(0)

zone_up = None
try:
    case_path = sys.argv[2] if len(sys.argv) > 2 else base + "_case.txt"
    words = open(case_path).read().split()
    i = words.index("narrow")
    if int(words[i + 1]) > 0:
        vals = [float.fromhex(w) for w in words[i + 2:i + 2 + 9]]
        zone_up = vals[5:8]
except (OSError, ValueError, IndexError):
    pass

def roll_error(r):
    if zone_up is None:
        return float("nan")
    up = [float(r["up_x"]), float(r["up_y"]), float(r["up_z"])]
    c = abs(sum(a * b for a, b in zip(up, zone_up)))
    return math.degrees(math.acos(max(-1.0, min(1.0, c))))

t0, t1 = float(inside[0]["t"]), float(inside[-1]["t"])
print(f"[{tag}] narrow zone t={t0:.1f}..{t1:.1f}s s={float(inside[0]['s_m']):.0f}..{float(inside[-1]['s_m']):.0f} m "
      f"tube tolerance={min(float(r['tol_m']) for r in inside):.1f} m (corridor) rows={len(inside)}")
print(f"  max throttle={max(float(r['throttle']) for r in inside):.3f}")
print(f"  max cross-track={max(float(r['xtrack_m']) for r in inside):.2f} m  max lateral v={max(float(r['lat_v_mps']) for r in inside):.2f} m/s")
print(f"  max attitude error vs zone axis={max(float(r['plan_hull_err_deg']) for r in inside):.2f} deg  "
      f"max roll error={max(roll_error(r) for r in inside):.2f} deg")
print(f"  max angular rate={max(float(r['omega']) for r in inside):.3f} rad/s  "
      f"speed {min(float(r['speed_mps']) for r in inside):.1f}..{max(float(r['speed_mps']) for r in inside):.1f} m/s")
print(f"  safety override rows={sum(1 for r in inside if r['override'] == '1')}")
