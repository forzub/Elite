"""The junction LEVEL RULE for a hub city's lane turns (author 2026-11-22: «когда менял высоту, чтобы перелететь через
здание, находился в зелёной зоне, красная зона шла сверху. Вот он через неё и пролетел»; «надо вводить правила
движения»).

The opposite lanes of a street are stacked over each other (x3.5.0: 320 m one way, 430 m the other); a turn connector
used to ease its height over its whole length - through the oncoming lane's tube at the junction. The rule:
  * a turn changes its level ONLY on a street, before the junction (on the lane it leaves) or after it (on the lane
    it joins) - whichever has no other lane's tube in the way; through the junction square it flies at one level;
  * the climb / descent is no steeper than CLIMB_DEG (20 deg) (its run starts / ends further along the street - a station is
    inserted there);
  * a turn that has no clear street (an oncoming lane between the levels on both) does not exist at that junction -
    the router takes another junction (a forbidden turn).
The connector's centre line keeps out of every other lane's tube by its radius + SHIP_MARGIN_M.

Operates on the elite.hub_lanes.v1 JSON (src/assets/data/navigation/hub_lanes/<hub>.json): hub_belt_world.py applies
it before writing; run directly it rewrites the existing files:
    python tools/models/units/hub_lane_levels.py [files...]   (default: all hub_lanes/*.json)
"""
import glob
import json
import math
import os
import sys

import numpy as np

TURN_R, TURN_RUN = 100.0, 150.0          # hub_lanes.py: the arc radius inside the junction, the minimum street run
CLIMB_DEG = 20.0          # 35: a 46 deg peak over ~300 m - the autopilot left a turn by 194 m at 45 m/s (2026-11-22)
SHIP_MARGIN_M = 10.0
TURN_SPEED_MPS = 30.0
STEP_M = 12.5


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def _h(v):
    n = math.hypot(v[0], v[2])
    return np.array([v[0] / n, 0.0, v[2] / n])


def _seg(a, b, step=STEP_M):
    n = max(1, int(math.ceil(np.linalg.norm(b - a) / step)))
    return [a + (b - a) * (i / n) for i in range(n + 1)]


def _ease(pts, y0, y1):
    """height eased from y0 to y1 over the horizontal length of pts"""
    cum = [0.0]
    for a, b in zip(pts, pts[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[2] - a[2]))
    total = max(cum[-1], 1e-9)
    return [np.array([p[0], y0 + (y1 - y0) * _smooth(c / total), p[2]]) for p, c in zip(pts, cum)]


class _Tubes:
    def __init__(self, lanes):
        a, b, r, owner = [], [], [], []
        for li, ln in enumerate(lanes):
            st = ln["stations_local_m"]
            for p, q in zip(st, st[1:]):
                a.append(p)
                b.append(q)
                r.append(ln.get("radius_m", 45.0))
                owner.append(li)
        self.a, self.b = np.array(a, float), np.array(b, float)
        self.r, self.owner = np.array(r, float), np.array(owner)

    def clear(self, pts, skip):
        P = np.array(pts, float)[:, None, :]                       # n x 1 x 3
        ab = self.b - self.a
        t = np.clip(np.einsum("nsk,sk->ns", P - self.a, ab) / np.maximum(np.einsum("sk,sk->s", ab, ab), 1e-9), 0, 1)
        d = np.linalg.norm(P - (self.a + ab * t[..., None]), axis=2)
        keep = ~np.isin(self.owner, list(skip))
        return not np.any((d < (self.r + SHIP_MARGIN_M)[None, :]) & keep[None, :])


def _insert(st, p):
    """a station at p on the lane's polyline (travel order); -> True, or False when p is not on it"""
    for q in st:
        if np.linalg.norm(np.array(q) - p) < 0.5:
            return True
    for k in range(len(st) - 1):
        a, b = np.array(st[k]), np.array(st[k + 1])
        ab = b - a
        t = np.dot(p - a, ab) / np.dot(ab, ab)
        if 0.0 < t < 1.0 and np.linalg.norm(a + ab * t - p) < 0.5:
            st.insert(k + 1, [round(float(c), 2) for c in p])
            return True
    return False


def apply(doc):
    """doc: an elite.hub_lanes.v1 dict, changed in place -> (kept as is, rebuilt, dropped)"""
    lanes = doc.get("lanes", [])
    by_id = {ln["id"]: i for i, ln in enumerate(lanes)}
    tubes = _Tubes(lanes)
    conns = doc.get("connectors", [])
    # every connector's end points, to re-resolve the station indices after insertions
    ends = []
    for cn in conns:
        A, B = lanes[by_id[cn["from_lane"]]], lanes[by_id[cn["to_lane"]]]
        ends.append((np.array(A["stations_local_m"][cn["from_station"]], float), np.array(B["stations_local_m"][cn["to_station"]], float)))
    out, out_ends = [], []
    kept = rebuilt = dropped = 0
    run = lambda dy: max(TURN_RUN, abs(dy) / math.tan(math.radians(CLIMB_DEG)))
    for cn, (p_from, p_to) in zip(conns, ends):
        if cn["id"].startswith("turn."):
            # a junction turn (a TURN_R arc) is flown slowly: TURN_SPEED_MPS ~ 9 m/s2 on the arc (at 36-47 m/s the
            # autopilot left the arc by 40 m, suite v3; the author: «повороты крутые, скорость надо сбрасывать почти до
            # нуля»)
            cn["max_speed_mps"] = min(cn.get("max_speed_mps", TURN_SPEED_MPS), TURN_SPEED_MPS)
        if not cn["id"].startswith("turn."):
            out.append(cn)
            out_ends.append((p_from, p_to))
            continue
        ia, ib = by_id[cn["from_lane"]], by_id[cn["to_lane"]]
        A, B = lanes[ia], lanes[ib]
        pts = [np.array(q, float) for q in cn["points_local_m"]]
        skip = {ia, ib}
        if tubes.clear(pts, skip):
            kept += 1
            out.append(cn)
            out_ends.append((p_from, p_to))
            continue
        sa, sb = A["stations_local_m"], B["stations_local_m"]
        fA = _h(np.array(sa[1]) - np.array(sa[0]))
        fB = _h(np.array(sb[1]) - np.array(sb[0]))
        yA, yB = float(sa[0][1]), float(sb[0][1])
        # the junction centre: lane A's line at the crossing with B's (on the plane)
        pa = np.array(sa[0], float)
        pb = np.array(sb[0], float)
        # solve pa + fA s = pb + fB u in xz
        M = np.array([[fA[0], -fB[0]], [fA[2], -fB[2]]])
        s, _u = np.linalg.solve(M, np.array([pb[0] - pa[0], pb[2] - pa[2]]))
        c = pa + fA * s
        c[1] = 0.0
        a0, b0 = c - fA * TURN_R, c + fB * TURN_R
        q = a0 + fB * TURN_R
        arc = [q - fB * (TURN_R * math.cos(math.radians(f))) + fA * (TURN_R * math.sin(math.radians(f))) for f in range(0, 91, 10)]
        dy = yB - yA
        options = []
        # 1) the level change on lane A's street before the junction
        start = c - fA * (TURN_R + run(dy))
        end = c + fB * (TURN_R + TURN_RUN)
        climb = _ease([np.array([p[0], 0.0, p[2]]) for p in _seg(start, a0)], yA, yB)
        rest = [np.array([p[0], yB, p[2]]) for p in arc[1:] + _seg(b0, end)[1:]]
        options.append((climb + rest, np.array([start[0], yA, start[2]]), np.array([end[0], yB, end[2]])))
        # 2) on lane B's street after the junction
        start = c - fA * (TURN_R + TURN_RUN)
        end = c + fB * (TURN_R + run(dy))
        flat = [np.array([p[0], yA, p[2]]) for p in _seg(start, a0)[:-1] + arc]
        climb = _ease([np.array([p[0], 0.0, p[2]]) for p in _seg(b0, end)], yA, yB)
        options.append((flat + climb[1:], np.array([start[0], yA, start[2]]), np.array([end[0], yB, end[2]])))
        chosen = None
        for opts, f_pt, t_pt in options:
            # room on both lanes: the new ends on their straight polylines, not into the overrun ends
            if np.dot(f_pt - np.array(sa[0]), fA) < 20.0 or np.dot(np.array(sb[-1]) - t_pt, fB) < 20.0:
                continue
            if tubes.clear(opts, skip):
                chosen = (opts, f_pt, t_pt)
                break
        if not chosen:
            dropped += 1
            continue
        opts, f_pt, t_pt = chosen
        if not (_insert(sa, f_pt) and _insert(sb, t_pt)):
            dropped += 1
            continue
        cn = dict(cn)
        cn["points_local_m"] = [[round(float(v), 1) for v in p] for p in opts]
        out.append(cn)
        out_ends.append((f_pt, t_pt))
        rebuilt += 1
    # resolve the station indices (insertions shifted them)
    for cn, (p_from, p_to) in zip(out, out_ends):
        for key, lane_id, p in (("from_station", cn["from_lane"], p_from), ("to_station", cn["to_lane"], p_to)):
            st = lanes[by_id[lane_id]]["stations_local_m"]
            k = min(range(len(st)), key=lambda i: np.linalg.norm(np.array(st[i]) - p))
            assert np.linalg.norm(np.array(st[k]) - p) < 0.6, (cn["id"], key)
            cn[key] = k
    doc["connectors"] = out
    return kept, rebuilt, dropped


def main(paths):
    if not paths:
        root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", ".."))
        paths = sorted(glob.glob(os.path.join(root, "src", "assets", "data", "navigation", "hub_lanes", "*.json")))
    total = [0, 0, 0]
    for p in paths:
        doc = json.load(open(p, encoding="utf-8"))
        if not any(cn["id"].startswith("turn.") for cn in doc.get("connectors", [])):
            continue
        k, r, d = apply(doc)
        total = [total[0] + k, total[1] + r, total[2] + d]
        open(p, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, indent=1) + "\n")
        print(f"{os.path.basename(p)}: turns kept {k}, rebuilt {r}, dropped {d}")
    print("total kept %d rebuilt %d dropped %d" % tuple(total))


if __name__ == "__main__":
    main(sys.argv[1:])
