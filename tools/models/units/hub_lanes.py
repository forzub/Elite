"""The traffic network of a generated hub (split out of hub_plane.py, audit 2026-11-18): GREEN lanes down the street
gaps, pre-written CONNECTORS (crossing turns, the plaza ring's merges / diverges), BLUE dock approaches. Pure geometry
in the city frame; hub_belt_world.py writes it to src/assets/data/navigation/hub_lanes/<hub>.json (elite.hub_lanes.v1),
read by game::navigation::traffic::HubLaneNetwork. The future hub editor edits the same structures.
"""
import math

from eqlib import apply  # noqa: F401 - the unit kernel's rotation
from units.hub_belt import _add, _mul, _sub


# ---------------------------------------------------------------- green traffic lanes (author 2026-11-17: «предлагаю
# использовать таки зеленые, синие и красные зоны в пределах хаба ... зеленые тунели - зоны через весь город - карта
# быстрых маршрутов сформированная заранее ... участки для докинга грузовиков - делаем красной зоной ... можно какую-то
# часть вести в центр - там сделать типа площади и из центра уже разлетаться»): every straight street line of 3+
# blocks carries air lanes down the middle of its gap - two one-way lanes side by side (right-hand traffic, 50 m off
# the gap centre, radius 45 m: a T-5000 is 50 x 40), the lanes along Z at 150 m above the street plane, those along X
# at 290 m (no lane crosses another at its level); the same set mirrored under the plane. The four lines round the
# central block make the square round the canopy - the "plaza" every crossing lane passes.
# REVISED the same day (3 traffic classes, free legs only in open sky): the two directions of a street are stacked in
# its centre (Z streets: +Z at 100 m, -Z at 210 m; X streets: +X at 320 m, -X at 430 m above the plane, mirrored
# under it) - no lane crosses another at its level, and the clearance to the blocks is the whole half-gap.
# Local streets: Assisted <= 55 m, r 35 m, 100 m/s; avenues: Assisted <= 100 m, r 55 m, 150 m/s.
LANE_LEVELS = {("z", 1): 100.0, ("z", -1): 210.0, ("x", 1): 320.0, ("x", -1): 430.0}
LANE_CLASS = {False: {"radius": 35.0, "speed": 100.0, "max_ship_m": 55.0},
              True: {"radius": 55.0, "speed": 150.0, "max_ship_m": 100.0}}
LANE_MIN_BLOCKS = 3
LANE_OVERRUN = 120.0           # past the end crossings


def _plane_lanes(runs, line_x, line_z, av_x, av_z):
    """-> [{id, a, b, stations (city frame, in travel order), radius, speed, max_ship_m, level}]"""
    by_line = {}
    for ax, line, at in runs:
        by_line.setdefault((ax, int(line - 0.5)), []).append(at)
    lanes = []
    for (ax, li), ats in sorted(by_line.items()):
        ats = sorted(ats)
        seg = [ats[0]]
        groups = []
        for v in ats[1:]:
            if v == seg[-1] + 1:
                seg.append(v)
            else:
                groups.append(seg)
                seg = [v]
        groups.append(seg)
        avenue = av_x(li) if ax == "z" else av_z(li)
        cls = LANE_CLASS[avenue]
        c_line = line_x(li) if ax == "z" else line_z(li)
        node_at = line_z if ax == "z" else line_x
        for gi, gr in enumerate(groups):
            if len(gr) < LANE_MIN_BLOCKS:
                continue
            along = [node_at(gr[0] - 1) - LANE_OVERRUN] + [node_at(j - 1) for j in gr] + [node_at(gr[-1]), node_at(gr[-1]) + LANE_OVERRUN]
            for level, ys in (("top", 1.0), ("under", -1.0)):
                for sgn in (1, -1):
                    y = ys * LANE_LEVELS[(ax, sgn)]
                    st = [(c_line, y, v) if ax == "z" else (v, y, c_line) for v in along]
                    if sgn < 0:
                        st = st[::-1]
                    lanes.append({"id": f"green.{ax}{li + 0.5:g}.{gi}.{level}.{'p' if sgn > 0 else 'n'}", "a": st[0], "b": st[-1],
                                  "stations": [tuple(round(c, 2) for c in q) for q in st], "radius": cls["radius"],
                                  "speed": cls["speed"], "max_ship_m": cls["max_ship_m"], "level": level, "avenue": avenue})
    return lanes


# ---------------------------------------------------------------- pre-written lane CONNECTORS (author 2026-11-17: «в
# зеленой и синей зонах маршрут должен быть уже прописан заранее ... можно какую-то часть вести в центр - там сделать
# типа площади и из центра уже разлетаться по нужным направлениям»). A ship flies only pre-written geometry inside the
# city; free legs exist only in open sky beyond the rim (the lanes' first / last stations are the open portals).
#  - TURNS at every crossing: from a lane to each perpendicular lane of the same side (top / under) - a straight climb
#    run along the first street, a quarter arc inside the crossing square (radius TURN_R: 100 m keeps 170 m off the
#    block corners), a run along the second street; the height changes smoothly over the whole connector (the levels
#    differ by up to 330 m: the ship crosses the other levels of the junction - a junction is where levels cross).
#  - the PLAZA: a one-way ring under the canopy disc (top side; mirrored under the plane) round the central block,
#    PLAZA_Y above the plane, between the canopy's trumpet and the four avenues round the centre. At its four axis
#    points the avenue lane running the ring's way merges in (an S-curve over PLAZA_MERGE m) and diverges out again.
#    The ring's stations run 0..630 deg (one and three quarter laps) so that from any merge every diverge is ahead.
# Each connector joins station from_station of from_lane to station to_station of to_lane (the stations are inserted
# into the lanes here); its points include both stations.
TURN_R, TURN_RUN = 100.0, 150.0
PLAZA_Y, PLAZA_MERGE, PLAZA_SPEED = 265.0, 450.0, 80.0


def _norm_h(v):
    n = math.hypot(v[0], v[2])
    return (v[0] / n, 0.0, v[2] / n)


def _dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def _dist(a, b):
    return math.sqrt(sum((x - y) ** 2 for x, y in zip(a, b)))


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def _lane_dir(ln):
    return _norm_h(tuple(b - a for a, b in zip(ln["stations"][0], ln["stations"][1])))


def _insert_station(ln, p):
    """put point p (on the lane's straight polyline) into its stations in travel order"""
    st = ln["stations"]
    p = tuple(round(c, 2) for c in p)
    for q in st:
        if _dist(q, p) < 0.5:
            return
    for k in range(len(st) - 1):
        a, b = st[k], st[k + 1]
        ab = tuple(y - x for x, y in zip(a, b))
        t = _dot(tuple(y - x for x, y in zip(a, p)), ab) / _dot(ab, ab)
        if 0.0 < t < 1.0 and _dist(tuple(x + c * t for x, c in zip(a, ab)), p) < 0.5:
            st.insert(k + 1, p)
            return
    raise ValueError(f"station {p} is not on lane {ln['id']}")


def _vprofile(horiz, y0, y1):
    """horizontal polyline -> 3D with the height eased from y0 to y1 over its length"""
    cum = [0.0]
    for a, b in zip(horiz, horiz[1:]):
        cum.append(cum[-1] + math.hypot(b[0] - a[0], b[2] - a[2]))
    return [(p[0], y0 + (y1 - y0) * _smooth(c / cum[-1]), p[2]) for p, c in zip(horiz, cum)]


def _seg_pts(a, b, step=25.0):
    n = max(1, int(math.ceil(_dist(a, b) / step)))
    return [tuple(x + (y - x) * i / n for x, y in zip(a, b)) for i in range(n + 1)]


def _hermite(p0, t0, p1, t1, n=24):
    out = []
    for i in range(n + 1):
        s = i / n
        h00, h10, h01, h11 = 2 * s ** 3 - 3 * s ** 2 + 1, s ** 3 - 2 * s ** 2 + s, -2 * s ** 3 + 3 * s ** 2, s ** 3 - s ** 2
        out.append(tuple(h00 * a + h10 * b + h01 * c + h11 * d for a, b, c, d in zip(p0, t0, p1, t1)))
    return out


def _plane_connectors(lanes, centre_xz, lines_x, lines_z):
    """-> (plaza lanes, connectors); inserts the connectors' stations into the lanes"""
    conns = []
    D = TURN_R + TURN_RUN
    # ---- turns at the crossings
    by_xz = {}
    for ln in lanes:
        for q in ln["stations"][1:-1]:                      # the crossing stations (not the overrun ends)
            by_xz.setdefault((round(q[0], 1), round(q[2], 1), ln["level"]), []).append(ln)
    for (x, z, level), here in sorted(by_xz.items(), key=lambda kv: kv[0]):
        for A in here:
            fA = _lane_dir(A)
            yA = A["stations"][0][1]
            for B in here:
                fB = _lane_dir(B)
                if B is A or abs(_dot(fA, fB)) > 0.1:
                    continue
                yB = B["stations"][0][1]
                c = (x, 0.0, z)
                # room for the run on both lanes (not into an overrun end)
                if _dot(_sub(c, A["stations"][0]), fA) < D + 20.0 or _dot(_sub(B["stations"][-1], c), fB) < D + 20.0:
                    continue
                off, a0 = _add(c, _mul(fA, -D)), _add(c, _mul(fA, -TURN_R))
                q = _add(a0, _mul(fB, TURN_R))
                arc = [_add(q, _add(_mul(fB, -TURN_R * math.cos(math.radians(f))), _mul(fA, TURN_R * math.sin(math.radians(f)))))
                       for f in range(0, 91, 10)]
                b0, on = _add(c, _mul(fB, TURN_R)), _add(c, _mul(fB, D))
                horiz = _seg_pts(off, a0)[:-1] + arc + _seg_pts(b0, on)[1:]
                pts = _vprofile(horiz, yA, yB)
                p_off, p_on = (off[0], yA, off[2]), (on[0], yB, on[2])
                _insert_station(A, p_off)
                _insert_station(B, p_on)
                conns.append({"id": f"turn.{A['id']}>{B['id']}", "from_lane": A["id"], "from_point": p_off,
                              "to_lane": B["id"], "to_point": p_on, "points": pts,
                              "radius": min(A["radius"], B["radius"]), "speed": min(A["speed"], B["speed"]),
                              "max_ship_m": min(A["max_ship_m"], B["max_ship_m"])})
    # ---- the plaza ring(s)
    cx, cz = centre_xz
    reach = min(abs(v - cx) for v in lines_x)
    reach = min([reach] + [abs(v - cz) for v in lines_z])
    rp = reach - 150.0
    plaza = []
    cls = LANE_CLASS[True]
    for level, ys in (("top", 1.0), ("under", -1.0)):
        y = ys * PLAZA_Y
        st = [(round(cx + rp * math.cos(math.radians(t)), 2), y, round(cz + rp * math.sin(math.radians(t)), 2))
              for t in range(0, 631, 10)]
        ring = {"id": f"green.plaza.{level}", "a": st[0], "b": st[-1], "stations": st, "radius": cls["radius"],
                "speed": PLAZA_SPEED, "max_ship_m": cls["max_ship_m"], "level": level, "avenue": True, "plaza": True}
        plaza.append(ring)
        for th in (0, 90, 180, 270):
            P = st[th // 10]
            t = (-math.sin(math.radians(th)), 0.0, math.cos(math.radians(th)))
            u = (math.cos(math.radians(th)), 0.0, math.sin(math.radians(th)))
            best = None
            for ln in lanes:
                if ln["level"] != level or _dot(_lane_dir(ln), t) < 0.99:
                    continue
                d = _dot(_sub(ln["stations"][0], P), u)
                if not 50.0 < d < 400.0:
                    continue
                s0, s1 = _dot(_sub(ln["stations"][0], P), t), _dot(_sub(ln["stations"][-1], P), t)
                if s0 > -PLAZA_MERGE - 20.0 or s1 < PLAZA_MERGE + 20.0:
                    continue
                if best is None or d < best[0]:
                    best = (d, ln)
            if not best:
                continue
            d, ln = best
            yl = ln["stations"][0][1]
            side = _add(P, _mul(u, d))
            p_in = (side[0] - t[0] * PLAZA_MERGE, yl, side[2] - t[2] * PLAZA_MERGE)
            p_out = (side[0] + t[0] * PLAZA_MERGE, yl, side[2] + t[2] * PLAZA_MERGE)
            tan = _mul(t, PLAZA_MERGE)
            _insert_station(ln, p_in)
            _insert_station(ln, p_out)
            conns.append({"id": f"plaza.in.{ln['id']}@{th}", "from_lane": ln["id"], "from_point": p_in, "to_lane": ring["id"],
                          "to_point": P, "points": _hermite(p_in, tan, P, tan), "radius": cls["radius"], "speed": PLAZA_SPEED,
                          "max_ship_m": ln["max_ship_m"]})
            for lap in range(th // 10, len(st), 36):
                if lap == 0:
                    continue                                  # the ring's very first station: nothing ridden yet
                Q = st[lap]
                conns.append({"id": f"plaza.out@{lap * 10}.{ln['id']}", "from_lane": ring["id"], "from_point": Q,
                              "to_lane": ln["id"], "to_point": p_out, "points": _hermite(Q, tan, p_out, tan),
                              "radius": cls["radius"], "speed": PLAZA_SPEED, "max_ship_m": ln["max_ship_m"]})
    # resolve the stations
    by_id = {ln["id"]: ln for ln in lanes + plaza}

    def index(lid, p):
        st = by_id[lid]["stations"]
        k = min(range(len(st)), key=lambda i: _dist(st[i], p))
        assert _dist(st[k], p) < 0.5, (lid, p)
        return k
    for cn in conns:
        cn["from_station"] = index(cn["from_lane"], cn["from_point"])
        cn["to_station"] = index(cn["to_lane"], cn["to_point"])
        cn["points"] = [tuple(round(v, 1) for v in q) for q in cn["points"]]
    for ln in lanes + plaza:
        ln["a"], ln["b"] = ln["stations"][0], ln["stations"][-1]
    return plaza, conns


# ---------------------------------------------------------------- BLUE dock approaches (author 2026-11-17: «в зеленой
# и синей зонах маршрут должен быть уже прописан заранее ... а после выхода из нее или в новую зону или уже в док»).
# A dock on the rim faces out; the lanes leave the rim outward too - a ship leaving a lane near the dock would have to
# turn round inside a kilometre (live run 8: the final leg infeasible). So from the end of every lane that ends within
# DOCK_APPROACH_REACH of the dock a pre-written BLUE approach runs on: the shortest path with BLUE_TURN_R turns (Dubins
# CSC in the plane, the height eased) onto the dock's axis, DOCK_AXIS_OUT in front of its slot, heading into it. The
# ship leaves the approach there and flies the last straight along the axis to the dock's own holds / axis join.
# DOCK_AXIS_OUT (live run 14): the final leg along the axis to the docking goal (~500 m off the slot) needs its 150 m
# start straight + ~1 km turn budget + 900 m goal straight; 1.7 km left 1.1 km and the squeezed curve looped back
BLUE_TURN_R, BLUE_SPEED, DOCK_AXIS_OUT, DOCK_APPROACH_REACH = 400.0, 80.0, 3300.0, 3000.0
BLUE_AXIS_RUN = 300.0


def _dubins(p0, a0, p1, a1, r):
    """shortest CSC path in the x-z plane from p0 heading a0 (rad) to p1 heading a1 -> list of (x, z) points"""
    def ctr(p, a, left):
        return (p[0] - r * math.sin(a), p[1] + r * math.cos(a)) if left else (p[0] + r * math.sin(a), p[1] - r * math.cos(a))

    def arc(c, frm, to, left):
        s0, s1 = math.atan2(frm[1] - c[1], frm[0] - c[0]), math.atan2(to[1] - c[1], to[0] - c[0])
        d = (s1 - s0) % (2 * math.pi) if left else (s0 - s1) % (2 * math.pi)
        n = max(1, int(math.ceil(d / math.radians(10.0))))
        sg = 1.0 if left else -1.0
        return [(c[0] + r * math.cos(s0 + sg * d * i / n), c[1] + r * math.sin(s0 + sg * d * i / n)) for i in range(n + 1)], d * r

    best = None
    for w0, w1 in ((True, True), (False, False), (True, False), (False, True)):
        c0, c1 = ctr(p0, a0, w0), ctr(p1, a1, w1)
        v = (c1[0] - c0[0], c1[1] - c0[1])
        D = math.hypot(*v)
        if D < 1.0e-6:
            continue
        if w0 == w1:
            n0 = (v[1] / D, -v[0] / D) if w0 else (-v[1] / D, v[0] / D)
            n1 = n0
        else:
            if D < 2 * r:
                continue
            phi, k = math.atan2(v[1], v[0]), math.acos(2 * r / D)
            n0 = None
            for ang in (phi + k, phi - k):
                cand = (math.cos(ang), math.sin(ang))
                u = (-cand[1], cand[0]) if w0 else (cand[1], -cand[0])
                if u[0] * v[0] + u[1] * v[1] > 0:
                    n0 = cand
            if n0 is None:
                continue
            n1 = (-n0[0], -n0[1])
        t0 = (c0[0] + r * n0[0], c0[1] + r * n0[1])
        t1 = (c1[0] + r * n1[0], c1[1] + r * n1[1])
        arc0, l0 = arc(c0, p0, t0, w0)
        arc1, l1 = arc(c1, t1, p1, w1)
        ls = math.hypot(t1[0] - t0[0], t1[1] - t0[1])
        mid = [(x, z) for x, _, z in _seg_pts((t0[0], 0.0, t0[1]), (t1[0], 0.0, t1[1]))]
        total = l0 + ls + l1
        if best is None or total < best[0]:
            best = (total, arc0 + mid[1:-1] + arc1)
    return best[1] if best else None


def _plane_dock_approaches(lanes, docks):
    """-> the blue approach lanes (each starting at a green lane's last station)"""
    out = []
    for dk in docks:
        if dk.get("type") != "hub_hex_dock":
            continue
        import pads
        R = pads.euler(dk["rotation_deg"])
        o = _norm_h(apply(R, (0.0, 0.0, -1.0)))                 # the slot's outward direction
        A = _add(dk["position"], _mul(o, DOCK_AXIS_OUT))
        A = (A[0], dk["position"][1], A[2])
        a1 = math.atan2(-o[2], -o[0])                              # arriving into the slot
        for ln in lanes:
            if ln.get("plaza") or ln.get("kind") == "blue":
                continue
            E = ln["stations"][-1]
            if math.hypot(E[0] - dk["position"][0], E[2] - dk["position"][2]) > DOCK_APPROACH_REACH:
                continue
            f = _lane_dir(ln)
            # the curve ends BLUE_AXIS_RUN before A; the last stretch is an exact straight on the axis (the thinned
            # arc stations alone left the final segment 5 deg off it)
            A0 = _add(A, _mul(o, BLUE_AXIS_RUN))
            flat = _dubins((E[0], E[2]), math.atan2(f[2], f[0]), (A0[0], A0[2]), a1, BLUE_TURN_R)
            if not flat:
                continue
            pts = _vprofile([(x, 0.0, z) for x, z in flat], E[1], A[1])
            pts[0], pts[-1] = E, (A0[0], A[1], A0[2])
            pts.append(A)
            # thin the stations to ~every 50 m (the arcs are sampled at 10 deg = 70 m, the straights at 25 m)
            st = [pts[0]]
            for q in pts[1:-2]:
                if _dist(q, st[-1]) >= 50.0:
                    st.append(q)
            st += pts[-2:]
            out.append({"id": f"blue.{dk['id']}.from.{ln['id']}", "kind": "blue", "a": st[0], "b": st[-1],
                        "stations": [tuple(round(c, 2) for c in q) for q in st], "radius": ln["radius"], "speed": BLUE_SPEED,
                        "max_ship_m": ln["max_ship_m"], "level": ln["level"], "avenue": ln["avenue"], "from_lane": ln["id"]})
    return out
