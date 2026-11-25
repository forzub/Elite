"""The RING hub (author 2026-11-22: «закольцованный хаб - это большое кольцо с одним этажом. все постройки на внутренней
стороне кольца. рост - вширь»; «там верх будет иметь свою ориентацию. там будет сложнее из-за вращения»).

Built as the FLAT STRIP + ONE BEND MAPPING: the ordinary planar generator lays a strip of blocks (one storey, no
underside), then every node / lane / connector / red zone is bent round the ring's axis. The 2D map / the hub plan stay
the strip (hub_plan.py reads L["flat"]); the plan carries "ring" {radius, length, x0} so the game bends plan points the
same way (src/game/navigation/HubRingBend.h).

The bend (city frame, the ring's axis = city +Z, the axis through the city origin):
    strip (x, y, z): x along the ring, y up from the deck, z across
    theta = 2 pi (x - x0) / L,   r = R - y
    frame = (r sin theta, -r cos theta, z);   "up" = (-sin theta, cos theta, 0) - towards the axis
At theta 0 the strip point (x0, y, z) sits at (0, y - R, z): the flat city moved down by R.
L = the strip's span + one local street (the seam street between the last and the first column), R = L / 2 pi.

The CITY spins (author 2026-11-25) by its attachment spin about its +Z (GameSceneSetup: the ring's period from the plan; the hub's
self rotation is not applied to attached modules): the city module is turned -90 deg about X (its
+Z = the hub's +Y) and the hub's self_rotation_period_seconds is the 1 g period 2 pi sqrt(R / g).
"""
import hashlib
import math
import random

from units.hub_plane import (CELL, MAP_SCHEMA, STREET_AVENUE, STREET_LOCAL, ZONE_MODULES, _pack, _rot_yaw, add_services,
                             layout_from_map)

RING_HUBS = {
    # hub id: (columns along the ring, rows across, the zones round the ring - repeated to fill)
    "earth_ring_hub": (8, 2, ["residential", "downtown", "residential", "repair", "residential", "storage",
                              "residential", "industry", "downtown", "residential", "logistics", "residential",
                              "rig", "residential", "storage", "residential"]),
}
G = 9.81

# THE AXIS (author 2026-11-25: «если там есть плавильня в невесомости - мы плавильню ставим в центр кольца, а всё, что
# требует гравитации - на ободе»): the zones whose work wants weightlessness are built along the spin axis, not on the
# rim - the rim (1 g) is for the people and everything that needs weight. Per hub: the zones on its axis.
AXIS_ZONES = {
    "earth_ring_hub": ["industry"],
}
AXIS_START_M = 900.0     # along the axis (city +Z = the hub's +Y), past the dock's service block (510 m): the dock's
                         # entry faces the other way, its approach stays clear
AXIS_STEP_M = 260.0


def ring_map(hub_id):
    """the strip's map: N x M blocks, all present, one storey, the avenue down the middle of the strip"""
    N, M, zones = RING_HUBS[hub_id]
    rng = random.Random(int(hashlib.sha1(hub_id.encode("utf-8")).hexdigest()[:12], 16))
    h = CELL / 2
    cells = []
    k = 0
    for i in range(N):
        for j in range(M):
            kd = zones[k % len(zones)]
            k += 1
            placed = _pack(rng, (-h + 10.0, -h + 10.0, h - 10.0, h - 10.0), ZONE_MODULES[kd], max_top=300.0)
            cells.append({"i": i, "j": j, "zone": kd,
                          "top": [{"module": f, "x": round(p[0], 3), "z": round(p[2], 3), "yaw": _rot_yaw(r, False)} for f, r, p in placed],
                          "under": []})
    av_x = [i for i in range(N - 1) if i % 3 == 1]
    av_z = list(range(M - 1))                                    # the avenue(s) along the ring
    return add_services({"schema": MAP_SCHEMA, "hub_id": hub_id, "title": hub_id, "generated_from": hub_id, "style": "",
                         "grid": {"cell_m": CELL, "street_local_m": STREET_LOCAL, "street_avenue_m": STREET_AVENUE,
                                  "anchor": {"i": 0, "j": 0, "x": None, "z": None}, "extent": [0, 0, N - 1, M - 1]},
                         "avenues": {"x": av_x, "z": av_z}, "cells": cells, "lanes_off": [], "red_zones": []})


def ring_geometry(flat):
    """(x0, L, R) of a flat strip layout: x0 = the strip's -X edge, L = its span + the seam street"""
    xs = flat["centres"]["x"]
    lo = min(xs.values()) - CELL / 2
    hi = max(xs.values()) + CELL / 2
    L = (hi - lo) + STREET_LOCAL
    x0 = lo - STREET_LOCAL / 2                                   # the seam street's middle
    return x0, L, L / (2.0 * math.pi)


def bend_point(p, x0, L, R):
    th = 2.0 * math.pi * (p[0] - x0) / L
    r = R - p[1]
    return (r * math.sin(th), -r * math.cos(th), p[2])


def bend_rot(theta, rot):
    """a node's rotation (euler deg, T * Rx * Ry * Rz) turned by theta about the ring's axis (+Z)"""
    import pads
    c, s = math.cos(theta), math.sin(theta)
    Rz = [[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]]
    M = pads.euler(rot)
    out = [[sum(Rz[i][k] * M[k][j] for k in range(3)) for j in range(3)] for i in range(3)]
    return tuple(round(v, 4) + 0.0 for v in pads.to_euler(out))


def _densify(points, step=40.0):
    out = [points[0]]
    for a, b in zip(points, points[1:]):
        d = math.dist(a, b)
        n = max(1, int(math.ceil(d / step)))
        for k in range(1, n + 1):
            out.append(tuple(a[q] + (b[q] - a[q]) * k / n for q in range(3)))
    return out


def ring_layout(hub_id):
    flat = layout_from_map(ring_map(hub_id))
    x0, L, R = ring_geometry(flat)
    th = lambda x: 2.0 * math.pi * (x - x0) / L
    nodes = []
    for nid, folder, pos, rot, role, parent in flat["nodes"]:
        nodes.append((nid, folder, tuple(round(v, 3) + 0.0 for v in bend_point(pos, x0, L, R)), bend_rot(th(pos[0]), rot), role, parent))
    lanes = []
    # the lanes along the ring are CLOSED circles (the strip's seam joined): the station spacing 40 m; the underside's
    # lanes are dropped (one storey: no underside)
    ring_lanes = []
    flat_lanes = []
    k = 0
    for ln in flat["lanes"]:
        if ".under." in ln["id"]:
            continue
        st = ln["stations"]
        along_x = abs(st[-1][0] - st[0][0]) > abs(st[-1][2] - st[0][2])
        if along_x and ln.get("kind", "green") == "green":
            forward = st[-1][0] > st[0][0]
            n = int(math.ceil(L / 40.0))
            xs = [x0 + L * q / n for q in range(n + 1)]
            if not forward:
                xs.reverse()
            y, z = st[0][1], st[0][2]
            ring_lanes.append(dict(ln, id=f"green.ring{k}.{'p' if forward else 'n'}",
                                   stations=[tuple(round(v, 2) for v in bend_point((x, y, z), x0, L, R)) for x in xs]))
            k += 1
        else:
            flat_lanes.append(ln)
    lanes += ring_lanes
    # the other lanes' stations: densified first (a straight chord between two far stations would cut through the ring)
    idx_maps = {}
    for ln in flat_lanes:
        st = [tuple(p) for p in ln["stations"]]
        dense, mapping = [st[0]], [0]
        for a, b in zip(st, st[1:]):
            seg = _densify([a, b])[1:]
            dense += seg
            mapping.append(len(dense) - 1)
        idx_maps[ln["id"]] = mapping
        lanes.append(dict(ln, stations=[tuple(round(v, 2) for v in bend_point(p, x0, L, R)) for p in dense]))
    connectors = []
    for cn in flat["connectors"]:
        if cn["from_lane"] not in idx_maps or cn["to_lane"] not in idx_maps:
            continue   # onto / off a lane that is now a ring circle (or dropped): left out for now
        pts = _densify([tuple(p) for p in cn["points"]], 15.0) if len(cn["points"]) > 1 else cn["points"]
        connectors.append(dict(cn, from_station=idx_maps.get(cn["from_lane"], [])[cn["from_station"]] if cn["from_lane"] in idx_maps else cn["from_station"],
                               to_station=idx_maps.get(cn["to_lane"], [])[cn["to_station"]] if cn["to_lane"] in idx_maps else cn["to_station"],
                               points=[tuple(round(v, 2) for v in bend_point(p, x0, L, R)) for p in pts]))
    red = []
    for rz in flat["red_zones"]:
        c = rz["center"]
        red.append(dict(rz, center=tuple(round(v, 1) for v in bend_point(c, x0, L, R)),
                        rotation_deg=bend_rot(th(c[0]), rz["rotation_deg"])))
    # the zero-g zones on the AXIS: their modules one after another along it (a spine - in weightlessness at the axis)
    z = AXIS_START_M
    k = 0
    for zone in AXIS_ZONES.get(hub_id, []):
        for folder, count in ZONE_MODULES.get(zone, []):
            for _ in range(count):
                nodes.append((f"axis_{zone}_{k}", folder, (0.0, 0.0, round(z, 3)), (0.0, 0.0, 0.0), "module", None))
                z += AXIS_STEP_M
                k += 1
    out = dict(flat)
    out.update({"nodes": nodes, "lanes": lanes, "connectors": connectors, "red_zones": red, "docks": [], "spinning": [],
                "monorail": None, "flat": flat, "haze": [],
                "ring": {"radius_m": round(R, 3), "length_m": round(L, 3), "x0_m": round(x0, 3), "axis": "z",
                         "period_s": round(2.0 * math.pi * math.sqrt(R / G), 3)}})
    return out
