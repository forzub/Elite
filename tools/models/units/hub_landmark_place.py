"""Place a hub's landmarks and its block-layout signature on its map (the rules: units/hub_landmarks.py docstring,
"HOW THE GENERATOR SHOULD CONSUME IT"). add_landmarks(map, hub_id, style) runs after hub_plane.add_services on a
generated map whose profile asks for it ("landmarks": true in tools/models/hub_profiles.json): the cities frozen before
(hub_cities/<hub>.json) are not reshuffled. Pure in the hub id; its own rng (the map's draws stay as they were).

A landmark takes a whole cell (its "top" becomes the landmark card alone, zone kept, "landmark" set); a bridge stands
on the gap between two cells (its card in the first cell, offset onto the gap centre) and clears a strip of both cells
next to the gap. A place with no cell left drops its landmark (generic modules are never moved to make room).
"""
import hashlib
import math
import random

CELL = 600.0
STREET_LOCAL, STREET_AVENUE = 300.0, 500.0
BUSY = ("dock", "services", "power", "canopy")


def _height_of():
    from units.hub_belt import _module_obb
    obb = _module_obb()
    return lambda module: obb[module][1][1] if module in obb else 999.0


def add_landmarks(mp, hub_id, style=""):
    from units.hub_landmarks import landmarks_for
    if any(c.get("landmark") for c in mp["cells"]):
        return mp                                            # idempotent
    pick = landmarks_for(hub_id, style or "")
    rng = random.Random(int(hashlib.sha1(("lm:" + hub_id).encode("utf-8")).hexdigest()[:12], 16))
    height = _height_of()
    cells = {(c["i"], c["j"]): c for c in mp["cells"]}
    canopy = next((k for k, c in cells.items() if c["zone"] == "canopy"), None)
    if canopy is None:
        return mp
    av_x, av_z = set(mp["avenues"]["x"]), set(mp["avenues"]["z"])
    cheb = lambda k: max(abs(k[0] - canopy[0]), abs(k[1] - canopy[1]))
    used = set()
    free = lambda k: k in cells and k not in used and cells[k]["zone"] not in BUSY

    def lower(k, max_top):
        c = cells[k]
        c["top"] = [m for m in c["top"] if height(m["module"]) <= max_top]

    def put(k, module, yaw=0, x=0.0, z=0.0, alone=True, zone=None):
        c = cells[k]
        card = {"module": module, "x": round(x, 3), "z": round(z, 3), "yaw": yaw}
        c["top"] = [card] if alone else c["top"] + [card]
        c.pop("piers", None)
        c["landmark"] = module
        if zone:
            c["zone"] = zone
        used.add(k)

    def gap(axis, a):
        """the street gap width after column / row a (axis 'x': between i=a and a+1)"""
        return STREET_AVENUE if a in (av_x if axis == "x" else av_z) else STREET_LOCAL

    def clear_strip(k, side, strip):
        """drop the cards of cell k within `strip` (+ a module's half size) of its side (unit vector)"""
        c = cells[k]
        h = CELL / 2
        keep = []
        for m in c["top"]:
            along = m["x"] * side[0] + m["z"] * side[1]
            if along < h - strip - 70.0:
                keep.append(m)
        c["top"] = keep

    def face_centre_yaw(k):
        """yaw so local +z faces the canopy"""
        dx, dz = canopy[0] - k[0], canopy[1] - k[1]
        if abs(dz) >= abs(dx):
            return 0 if dz > 0 else 180
        return 90 if dx > 0 else 270

    sig = pick["signature"]
    placed = []
    name = sig["name"]
    if name == "wide_square":
        for k in sorted((k for k in cells if free(k) and cells[k]["zone"] == "downtown"), key=lambda k: (cheb(k), k)):
            for o in ((1, 0), (0, 1), (-1, 0), (0, -1)):
                n = (k[0] + o[0], k[1] + o[1])
                axis, a = ("x", min(k[0], n[0])) if o[0] else ("z", min(k[1], n[1]))
                if not free(n) or cells[n]["zone"] not in ("downtown", "residential") or gap(axis, a) != STREET_LOCAL:
                    continue
                g = CELL / 2 + STREET_LOCAL / 2
                # the monument corner (+x +z of the square) away from the canopy
                yaw = {(1, 1): 0, (1, -1): 90, (-1, -1): 180, (-1, 1): 270}[(1 if k[0] >= canopy[0] else -1, 1 if k[1] >= canopy[1] else -1)]
                put(k, "hubl_square", yaw, zone="square")
                cells[k]["under"] = []
                put(n, "hubl_square_open", 0, zone="square")
                cells[n]["under"] = []
                cells[k]["top"].append({"module": "hubl_plaza_deck", "x": round(o[0] * g, 3), "z": round(o[1] * g, 3),
                                        "yaw": 90 if o[0] else 0})
                placed.append("hubl_square")
                break
            if placed:
                break
    elif name == "ring_boulevard":
        for k in cells:
            if cheb(k) == 1 and free(k):
                lower(k, 120.0)
    elif name == "diagonal_avenue":
        d = tuple(sig.get("dir", (1, 1)))
        diag = []
        k = (canopy[0] + d[0], canopy[1] + d[1])
        while k in cells:
            diag.append(k)
            k = (k[0] + d[0], k[1] + d[1])
        for k in diag:
            if free(k):
                lower(k, 40.0)
        rim_end = next((k for k in reversed(diag) if free(k)), None)
        if rim_end and "hubl_gate" in [l["module"] for l in pick["landmarks"]]:
            put(rim_end, "hubl_gate", face_centre_yaw(rim_end))
            placed.append("hubl_gate")
    elif name == "central_void":
        d = tuple(sig.get("dir", (1, 0)))
        k = (canopy[0] + d[0], canopy[1] + d[1])
        if free(k):
            cells[k]["top"], cells[k]["under"] = [], []
            cells[k]["zone"] = "void"
            cells[k]["landmark"] = "void"
            cells[k].pop("piers", None)
            used.add(k)
    elif name == "canal":
        # the avenue gap j|j+1 nearest the canopy (a street along X): both banks low, the arch on the pair nearest the centre
        js = sorted(av_z, key=lambda j: abs(j + 0.5 - canopy[1]))
        if js:
            j = js[0]
            bank = [k for k in cells if k[1] in (j, j + 1)]
            for k in bank:
                if free(k):
                    lower(k, 150.0)
            pairs = sorted((k for k in cells if k[1] == j and free(k) and free((k[0], j + 1))), key=lambda k: (abs(k[0] - canopy[0]), k))
            if pairs and "hubl_bridge_arch" in [l["module"] for l in pick["landmarks"]]:
                a = pairs[0]
                b = (a[0], j + 1)
                clear_strip(a, (0, 1), 80.0)
                clear_strip(b, (0, -1), 80.0)
                g = CELL / 2 + STREET_AVENUE / 2
                cells[a]["top"].append({"module": "hubl_bridge_arch", "x": 0.0, "z": g, "yaw": 0})
                cells[a]["landmark"] = "hubl_bridge_arch"
                used.update({a, b})
                placed.append("hubl_bridge_arch")

    # the landmarks by place
    for lm in pick["landmarks"]:
        m, place = lm["module"], lm["place"]
        if m in placed:
            continue
        if lm["kind"] == "bridge":
            arch = m == "hubl_bridge_arch"
            cand = []
            for k in sorted(cells, key=lambda k: (cheb(k), k)):
                n = (k[0], k[1] + 1)
                if not (free(k) and free(n)):
                    continue
                g = gap("z", k[1])
                if (arch and g != STREET_AVENUE) or (not arch and g != STREET_LOCAL):
                    continue
                cand.append((k, n, g))
            if not cand:
                continue
            k, n, g = cand[0]
            clear_strip(k, (0, 1), 80.0 if arch else 60.0)
            clear_strip(n, (0, -1), 80.0 if arch else 60.0)
            cells[k]["top"].append({"module": m, "x": 0.0, "z": CELL / 2 + g / 2, "yaw": 0})
            cells[k]["landmark"] = m
            used.update({k, n})
            placed.append(m)
            continue
        if place == "plaza":
            pool = [k for k in cells if free(k) and cells[k]["zone"] in ("downtown", "residential") and 1 <= cheb(k) <= 2]
        elif place == "downtown":
            pool = [k for k in cells if free(k) and cells[k]["zone"] == "downtown"]
        elif place == "residential":
            pool = [k for k in cells if free(k) and cells[k]["zone"] in ("residential", "hotel")]
        elif place == "axis":
            pool = [k for k in cells if free(k) and (k[0] == canopy[0] or k[1] == canopy[1]) and 2 <= cheb(k) <= 3]
        else:
            pool = [k for k in cells if free(k) and cells[k]["zone"] in ("downtown", "residential")]
        if not pool:
            pool = [k for k in cells if free(k) and cells[k]["zone"] in ("downtown", "residential", "industry", "storage")]
        if not pool:
            continue
        pool.sort(key=lambda k: (cheb(k), k))
        k = pool[rng.randrange(min(3, len(pool)))]
        put(k, m, face_centre_yaw(k) if place == "axis" else 0)
        placed.append(m)
    mp["landmarks"] = {"signature": name, "title": sig.get("title"), "modules": placed}
    return mp
