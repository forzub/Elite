"""Terminal sequence report. usage: python terminal.py TAG"""
import csv, os, sys
tag = sys.argv[1]
base = os.path.join(os.environ.get("ELITE_DIAG_DIR", "D:/tmp"), f"elite_{tag}")
keys = set(); rows = []
for r in csv.DictReader(open(base + "_decision.tsv"), delimiter="\t"):
    if not r["tol_m"] or float(r["tol_m"]) <= 0: continue
    keys.add((r["t"], r["s_m"])); rows.append(r)
ev = [e for e in csv.DictReader(open(base + "_events.tsv"), delimiter="\t")
      if (e["t"], e["s_m"]) in keys and e["event"] in ("TERMINAL_STEP", "PROGRAM")]
term = [r for r in rows if r.get("term_step", "0") not in ("0", "")]
if not term:
    print(f"[{tag}] no terminal sequence"); sys.exit(0)
t0 = float(term[0]["t"])
for e in ev:
    if float(e["t"]) >= t0 - 0.05:
        print(f"  {e['t']:>8} {e['event']:<14} {e['detail']}")
done = [r for r in term if r["term_step"] == "4"]
w = max(float(r["omega"]) for r in term)
print(f"[{tag}] terminal from t={t0:.1f}: max omega={w:.3f} rad/s, max throttle={max(float(r['throttle']) for r in term):.3f}")
if done:
    td = float(done[0]["t"])
    after = [r for r in term if float(r["t"]) >= td]
    print(f"  DONE at t={td:.1f}: after DONE max throttle={max(float(r['throttle']) for r in after):.3f}"
          f" max |rcs|={max(abs(float(r[c])) for r in after for c in ('rcs_f','rcs_s','rcs_l')):.3f}"
          f" max |rot|={max(abs(float(r[c])) for r in after for c in ('pitch','yaw','roll')):.3f}"
          f" final dist={after[-1]['term_dist_m']} m speed={after[-1]['speed_mps']} att={after[-1]['term_att_deg']} roll={after[-1]['term_roll_deg']}")
step = 1.0
last = -9
print("  t  step dist speed throttle rcs_f rcs_s rcs_l att roll omega")
for r in term:
    if float(r["t"]) - last >= step:
        last = float(r["t"])
        print("  " + " ".join(r[c] for c in ("t","term_step","term_dist_m","speed_mps","throttle","rcs_f","rcs_s","rcs_l","term_att_deg","term_roll_deg","omega")))
