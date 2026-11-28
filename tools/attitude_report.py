"""B4 measurement: the own ship's drawn attitude vs the authoritative one, from [AttitudeTrace] (ELITE_DEV_ATTITUDE_TRACE=1).

    python tools/attitude_report.py <log> [<log> ...]

Per log, over the 2 s windows where the ship actually turns (authoritative mean > 1 deg/s): the drawn reversals / s
(sign changes of the frame-to-frame turn-speed change), the drawn spread vs the authoritative spread, and the max."""
import re
import sys

rx_draw = re.compile(r"\[AttitudeTrace\] (\d+) frames: turn ([\d.e+-]+) deg/s mean, spread ([\d.e+-]+), max ([\d.e+-]+), reversals (\d+) \(([\d.e+-]+) /s\)")
rx_auth = re.compile(r"\[AttitudeTrace\]\s+authoritative: (\d+) snapshots: turn ([\d.e+-]+) deg/s mean, spread ([\d.e+-]+), max ([\d.e+-]+), reversals (\d+)")
for path in sys.argv[1:]:
    rows = []
    draw = None
    for line in open(path, encoding="utf-8", errors="replace"):
        m = rx_draw.search(line)
        if m:
            draw = [float(x) for x in m.groups()]
            continue
        a = rx_auth.search(line)
        if a and draw:
            rows.append((draw, [float(x) for x in a.groups()]))
            draw = None
    turning = [(d, a) for d, a in rows if a[1] > 1.0]
    if not turning:
        print(f"{path}: no turning windows ({len(rows)} windows)")
        continue
    n = len(turning)
    rev = sum(d[5] for d, a in turning) / n
    dspread = sum(d[2] for d, a in turning) / n
    aspread = sum(a[2] for d, a in turning) / n
    dmax = max(d[3] for d, a in turning)
    amax = max(a[3] for d, a in turning)
    dmean = sum(d[1] for d, a in turning) / n
    amean = sum(a[1] for d, a in turning) / n
    print(f"{path}: {n} turning windows | drawn mean {dmean:.2f} deg/s (auth {amean:.2f}) | drawn spread {dspread:.2f} "
          f"(auth {aspread:.2f}) | drawn max {dmax:.1f} (auth {amax:.1f}) | drawn reversals {rev:.1f} /s")
