#!/usr/bin/env python3
"""Damaged variants of the equipment models (MESH_EDITOR_SPEC 18.10): the intact assembly is torn, not cut.

Input:  <assets_source>/models/equipment/<asset>/intact/assembly/LOD0..3/<part>.obj + .mtl, hit_volumes.json
Output: <asset>/damaged/<variant>/assembly/LOD0..3/<part>.obj + .mtl, hit_volumes.json, damage.json (the plan: seed,
        wounds) and the variants in the passport <asset>/model.json (entries with "generated": true).
Variants (author 2026-11-15: "ровно то количество, чтобы при встрече с обломками не было впечатления, что они все
сделаны по одному шаблону ... можно разворотить, чтобы края торчали наружу"):
  hole   "Пробоина"     - one exit wound: a jagged hole whose petals are peeled outward, soot and torn bright rim;
                          two or three small entry holes elsewhere (petals pushed in);
  torn   "Разворочен"   - a big blast wound, long tears between the petals, the petals bent far out; small parts near
                          it blown off or knocked askew; sometimes a second wound;
  broken "Сорван кусок" - the module broken across: everything past a jagged break is gone, the edge zone crumpled
                          and flared outward; a small wound elsewhere.
Each module gets its own seeded places, sizes, petal counts and tear angles, so no two modules (and no two variants)
share a pattern; the runtime rotation / mirroring of debris multiplies that further.
Method: the mesh is refined (conformal edge splits) only around the damage, then triangles are removed by a noisy
radius (jagged hole), the rest of the zone is displaced by a position-only field (petals, crumple) - duplicated vertices
of hard edges move together, no cracks open - and the zone gets the damage materials. LODs share the plan; the
coarser LODs refine less.
Only folders whose damage.json says they are generated are rewritten; the author's own variants are never touched.

usage: python tools/models/build_damage_variants.py [--only <asset-substring>] [--jobs N]
"""
import argparse
import json
import math
import os
import random
import shutil
import sys
import zlib
from multiprocessing import Pool

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import eqlib  # noqa: E402
import source_layout  # noqa: E402

VARIANTS = [("hole", "Пробоина"), ("torn", "Разворочен"), ("broken", "Сорван кусок")]
# a drive also gets its nozzle end (+Z, where the pad sits) torn up / torn off (author 2026-11-15: «сейчас у тебя
# двигатель разворочен больше теми частями, которые ближе к реактору. надо хотя бы один с развороченным соплом, а
# лучше - два»)
DRIVE_VARIANTS = [("nozzle_torn", "Разворочено сопло"), ("nozzle_off", "Сорвано сопло")]


def is_drive(asset_dir):
    """a drive: its pad mount (mounts.json, MESH_EDITOR_SPEC 18.11) turns the pad's +Y onto the unit's +Z"""
    mp = os.path.join(source_layout.assembly_dir(asset_dir), "mounts.json")
    if not os.path.exists(mp):
        return False
    return any(m.get("rotation_deg") == [90.0, 0.0, 0.0] for m in json.load(open(mp, encoding="utf-8")).get("mounts", []))


def variants_for(asset_dir):
    return VARIANTS + (DRIVE_VARIANTS if is_drive(asset_dir) else [])
DMG_MATERIALS = {
    # torn bright metal at the rim, scorch next to it, soot fading out
    "eq_dmg_torn": ((0.44, 0.43, 0.41), (0.55, 0.55, 0.55), 140),
    "eq_dmg_scorch": ((0.055, 0.048, 0.042), (0.05, 0.05, 0.05), 10),
    "eq_dmg_soot": ((0.17, 0.16, 0.15), (0.08, 0.08, 0.08), 15),
    "eq_dmg_interior": ((0.13, 0.13, 0.14), (0.10, 0.10, 0.10), 20),   # the inside behind a wound (innards)
}
eqlib.register_materials(DMG_MATERIALS)
KEEP_MATS = {"eq_hole"}            # dark discs that close openings keep their colour
REFINE_DIV = {0: 4.0, 1: 2.5, 2: 1.4, 3: 0.8}   # target edge = wound radius / this, per LOD
REFINE_EXTRA = {0: 1500, 1: 700, 2: 250, 3: 60}   # triangles a part may gain by the refinement, per LOD
GENERATOR = "tools/models/build_damage_variants.py"


# ---------------------------------------------------------------- small vector helpers
def sub(a, b): return (a[0] - b[0], a[1] - b[1], a[2] - b[2])
def add(a, b): return (a[0] + b[0], a[1] + b[1], a[2] + b[2])
def mul(a, k): return (a[0] * k, a[1] * k, a[2] * k)
def dot(a, b): return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
def cross(a, b): return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])
def length(a): return math.sqrt(dot(a, a))
def unit(a):
    l = length(a)
    return (a[0] / l, a[1] / l, a[2] / l) if l > 1e-12 else (0.0, 1.0, 0.0)


def basis(d):
    t = (1.0, 0.0, 0.0) if abs(d[0]) < 0.8 else (0.0, 0.0, 1.0)
    e1 = unit(cross(d, t))
    return e1, cross(d, e1)


def rotate(v, axis, ang):
    """Rodrigues"""
    c, s = math.cos(ang), math.sin(ang)
    return add(add(mul(v, c), mul(cross(axis, v), s)), mul(axis, dot(axis, v) * (1 - c)))


# ---------------------------------------------------------------- noise (deterministic, position only)
def _hash(ix, iy, iz, seed):
    n = (ix * 73856093) ^ (iy * 19349663) ^ (iz * 83492791) ^ (seed * 2654435761)
    n &= 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 0xFFFF) / 32767.5 - 1.0


def noise3(p, scale, seed):
    x, y, z = p[0] / scale, p[1] / scale, p[2] / scale
    ix, iy, iz = math.floor(x), math.floor(y), math.floor(z)
    fx, fy, fz = x - ix, y - iy, z - iz
    sx, sy, sz = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy), fz * fz * (3 - 2 * fz)
    out = 0.0
    for dx in (0, 1):
        wx = sx if dx else 1 - sx
        for dy in (0, 1):
            wy = sy if dy else 1 - sy
            for dz in (0, 1):
                wz = sz if dz else 1 - sz
                out += wx * wy * wz * _hash(ix + dx, iy + dy, iz + dz, seed)
    return out


class Ring:
    """periodic noise over an angle: sum of harmonics with seeded phases, about -1..1"""
    def __init__(self, rng, lo=2, hi=9):
        self.h = [(k, rng.uniform(0, 2 * math.pi), rng.uniform(0.3, 1.0) / k ** 0.6) for k in range(lo, hi)]
        self.s = sum(a for _, _, a in self.h) * 0.7

    def __call__(self, th):
        return sum(a * math.sin(k * th + ph) for k, ph, a in self.h) / self.s


# ---------------------------------------------------------------- OBJ in / out
def read_obj(path):
    vs, ns, corners, faces = [], [], {}, {}
    P, N = [], []
    mat, name, mtllib = None, os.path.splitext(os.path.basename(path))[0], None
    with open(path, encoding="utf-8") as f:
        for line in f:
            t = line.split()
            if not t:
                continue
            if t[0] == "v":
                vs.append((float(t[1]), float(t[2]), float(t[3])))
            elif t[0] == "vn":
                ns.append((float(t[1]), float(t[2]), float(t[3])))
            elif t[0] == "usemtl":
                mat = t[1]
            elif t[0] == "mtllib":
                mtllib = t[1]
            elif t[0] == "o":
                name = t[1]
            elif t[0] == "f":
                idx = []
                for c in t[1:]:
                    q = c.split("/")
                    vi = int(q[0]) - 1 if int(q[0]) > 0 else len(vs) + int(q[0])
                    ni = (int(q[2]) - 1 if int(q[2]) > 0 else len(ns) + int(q[2])) if len(q) > 2 and q[2] else -1
                    key = (vi, ni)
                    if key not in corners:
                        corners[key] = len(P)
                        P.append(vs[vi])
                        N.append(ns[ni] if ni >= 0 else (0.0, 0.0, 0.0))
                    idx.append(corners[key])
                fl = faces.setdefault(mat or "eq_metal_graphite", [])
                for k in range(1, len(idx) - 1):
                    fl.append((idx[0], idx[k], idx[k + 1]))
    return {"name": name, "mtllib": mtllib, "P": P, "N": N, "faces": faces}


def obj_text(mesh, mtl_name, variant):
    used = sorted({i for fl in mesh["faces"].values() for f in fl for i in f})
    remap = {o: k + 1 for k, o in enumerate(used)}
    out = [f"# {mesh['name']} - damaged variant '{variant}', generated by {GENERATOR} (do not edit)",
           f"mtllib {mtl_name}", f"o {mesh['name']}"]
    out += [f"v {mesh['P'][i][0]:.5f} {mesh['P'][i][1]:.5f} {mesh['P'][i][2]:.5f}" for i in used]
    out += [f"vn {mesh['N'][i][0]:.4f} {mesh['N'][i][1]:.4f} {mesh['N'][i][2]:.4f}" for i in used]
    for mat in sorted(mesh["faces"]):
        if not mesh["faces"][mat]:
            continue
        out.append(f"usemtl {mat}")
        out += ["f " + " ".join(f"{remap[i]}//{remap[i]}" for i in f) for f in mesh["faces"][mat]]
    return "\n".join(out) + "\n"


def read_mtl(path):
    blocks, cur = {}, None
    if os.path.exists(path):
        for line in open(path, encoding="utf-8"):
            t = line.split()
            if t and t[0] == "newmtl":
                cur = t[1]
                blocks[cur] = [line.rstrip("\n")]
            elif cur and line.strip():
                blocks[cur].append(line.rstrip("\n"))
    return blocks


def mtl_text(mats, intact_blocks):
    out = [f"# damaged variant - generated by {GENERATOR}; material names are shared asset-level ids"]
    for m in sorted(mats):
        if m in DMG_MATERIALS:
            kd, ks, ns = DMG_MATERIALS[m]
            out += [f"newmtl {m}", "Ka 0 0 0", f"Kd {kd[0]:.3f} {kd[1]:.3f} {kd[2]:.3f}", f"Ks {ks[0]:.3f} {ks[1]:.3f} {ks[2]:.3f}",
                    f"Ns {ns}", "d 1", "illum 2"]
        elif m in intact_blocks:
            out += intact_blocks[m]
        elif m in eqlib.MATERIALS:
            kd, ks, ns = eqlib.MATERIALS[m]
            out += [f"newmtl {m}", "Ka 0 0 0", f"Kd {kd[0]:.3f} {kd[1]:.3f} {kd[2]:.3f}", f"Ks {ks[0]:.3f} {ks[1]:.3f} {ks[2]:.3f}",
                    f"Ns {ns}", "d 1", "illum 2"]
        out.append("")
    return "\n".join(out)


# ---------------------------------------------------------------- the damage plan
class Wound:
    """a hole with petals: centre c on the surface, outward normal d; rc hole radius, ro deformation radius;
    sign +1 = petals peeled outward (exit / blast), -1 = pushed in (entry)"""
    def __init__(self, rng, c, d, rc, ro, sign, amp, tears):
        self.c, self.d, self.rc, self.ro, self.sign, self.amp = c, unit(d), rc, ro, sign, amp
        self.e1, self.e2 = basis(self.d)
        self.jag = Ring(rng)
        self.petal = Ring(rng, 3, 7)
        self.tears = [(rng.uniform(-math.pi, math.pi), rng.uniform(0.35, 1.0) * (ro - rc), rng.uniform(0.05, 0.11)) for _ in range(tears)]
        self.seed = rng.randrange(1 << 30)

    def local(self, p):
        q = sub(p, self.c)
        u, v, h = dot(q, self.e1), dot(q, self.e2), dot(q, self.d)
        return u, v, h, math.atan2(v, u)

    def dist(self, p):
        u, v, h, th = self.local(p)
        return math.sqrt(u * u + v * v + (h / 1.6) ** 2), th, u, v, h

    def r_hole(self, th):
        return self.rc * (1.0 + 0.32 * self.jag(th))

    def reach(self):
        return self.ro * 1.3

    def field(self, p):
        """signed distance-like value, < 0 inside the hole (the jagged rim plus the tears between the petals)"""
        r, th, *_ = self.dist(p)
        rh = self.r_hole(th)
        f = r - rh
        for ta, tl, tw in self.tears:                       # tears radiate from the hole between the petals
            da = abs((th - ta + math.pi) % (2 * math.pi) - math.pi)
            if r < rh + tl * 1.2:
                f = min(f, da * max(r, 1e-6) - tw * self.rc * (1.0 - (r - rh) / (tl + 1e-9)))
        return f

    def removed(self, p):
        return self.field(p) < 0

    def displace(self, p):
        r, th, u, v, h = self.dist(p)
        rh = self.r_hole(th)
        if r >= self.ro or r <= 0:
            return (0.0, 0.0, 0.0)
        t = 1.0 - max(0.0, r - rh) / max(self.ro - rh, 1e-6)
        t = t * t
        a = 0.55 + 0.45 * self.petal(th)                     # petals: sectors bent more or less
        rr = math.hypot(u, v) or 1e-6
        radial = add(mul(self.e1, u / rr), mul(self.e2, v / rr))
        out = add(mul(self.d, self.sign * self.amp * a * t * self.rc), mul(radial, self.amp * 0.55 * a * t * self.rc))
        crumple = 0.07 * self.rc * t
        return add(out, (crumple * noise3(p, self.rc * 0.5, self.seed), crumple * noise3(p, self.rc * 0.5, self.seed + 1),
                         crumple * noise3(p, self.rc * 0.5, self.seed + 2)))

    def material(self, p, mat):
        r, th, *_ = self.dist(p)
        rh = self.r_hole(th)
        n = 0.25 * noise3(p, self.rc * 0.6, self.seed + 7)
        if r < rh * (1.18 + n * 0.3):
            return "eq_dmg_torn"
        if r < rh + (self.ro - rh) * (0.3 + 0.6 * n):
            return "eq_dmg_scorch"
        if r < self.ro * (1.0 + 0.6 * n):
            return "eq_dmg_soot"
        return None

    def target(self, p, lod):
        r, *_ = self.dist(p)
        return self.rc / REFINE_DIV[lod] * (1.0 + 1.5 * max(0.0, r - self.rc) / self.ro) if r < self.reach() else None

    def closest(self, a, b):
        ab = sub(b, a)
        t = dot(sub(self.c, a), ab) / max(dot(ab, ab), 1e-18)
        return add(a, mul(ab, min(max(t, 0.0), 1.0)))

    def plan(self):
        return {"type": "wound", "center": [round(x, 4) for x in self.c], "normal": [round(x, 4) for x in self.d],
                "hole_radius": round(self.rc, 4), "zone_radius": round(self.ro, 4), "petals": "out" if self.sign > 0 else "in"}


class Break:
    """the module broken across: a jagged plane (normal n points to the part that is gone); a band before it crumpled
    and flared outward from the module axis"""
    def __init__(self, rng, c, n, amp, band, centre):
        self.c, self.n, self.amp, self.band, self.centre = c, unit(n), amp, band, centre
        self.e1, self.e2 = basis(self.n)
        self.seed = rng.randrange(1 << 30)
        self.rc = band
        self.inner = None                 # (u, v centre, half extents) of the cross-section at the break

    def s(self, p):
        q = sub(p, self.c)
        u, v = dot(q, self.e1), dot(q, self.e2)
        j = noise3((u, v, 0.0), self.band * 1.3, self.seed) + 0.25 * noise3((u, v, 0.0), self.band * 0.5, self.seed + 3)
        return dot(q, self.n) + self.amp * j

    def field(self, p):
        return -self.s(p)

    def removed(self, p):
        return self.s(p) > 0

    def displace(self, p):
        s = self.s(p)
        if s > 0 or s < -self.band:
            return (0.0, 0.0, 0.0)
        t = 1.0 + s / self.band
        t = t * t
        q = sub(p, self.centre)
        radial = unit(sub(q, mul(self.n, dot(q, self.n))))
        flare = mul(radial, 0.55 * self.band * t * (0.6 + 0.4 * noise3(p, self.band * 0.8, self.seed + 5)))
        lean = mul(self.n, 0.25 * self.band * t * noise3(p, self.band, self.seed + 6))
        cr = 0.08 * self.band * t
        return add(add(flare, lean), (cr * noise3(p, self.band * 0.35, self.seed + 1), cr * noise3(p, self.band * 0.35, self.seed + 2),
                                      cr * noise3(p, self.band * 0.35, self.seed + 4)))

    def material(self, p, mat):
        s = self.s(p)
        n = 0.3 * noise3(p, self.band * 0.6, self.seed + 9)
        if s > -self.band * (0.12 + 0.1 * n):
            return "eq_dmg_torn"
        if s > -self.band * (0.55 + n):
            return "eq_dmg_scorch"
        if s > -self.band * (1.3 + n):
            return "eq_dmg_soot"
        return None

    def target(self, p, lod):
        return self.band / REFINE_DIV[lod] * 1.4 if -self.band * 1.4 < self.s(p) < self.band * 0.5 + self.amp else None

    def closest(self, a, b):
        sa, sb = self.s(a), self.s(b)
        if sa * sb < 0:
            return add(a, mul(sub(b, a), sa / (sa - sb)))
        return a if abs(sa) < abs(sb) else b

    def plan(self):
        return {"type": "break", "point": [round(x, 4) for x in self.c], "normal": [round(x, 4) for x in self.n],
                "jag": round(self.amp, 4), "band": round(self.band, 4)}


# ---------------------------------------------------------------- per-module analysis
def bbox(points):
    lo = [min(p[k] for p in points) for k in range(3)]
    hi = [max(p[k] for p in points) for k in range(3)]
    return lo, hi


def part_info(lods):
    """name -> (centre, diagonal) from the most detailed LOD that has the part"""
    info = {}
    for lod in sorted(lods):
        for name, m in lods[lod].items():
            if name in info or not m["P"]:
                continue
            lo, hi = bbox(m["P"])
            info[name] = (tuple((lo[k] + hi[k]) / 2 for k in range(3)), length(sub(hi, lo)))
    return info


def surface_samples(meshes, info, D):
    """area-weighted triangles of the big parts (outward face normal) - where a wound may sit"""
    tris = []
    for name, m in meshes.items():
        if info[name][1] < 0.22 * D:
            continue
        for mat, fl in m["faces"].items():
            if mat in KEEP_MATS:
                continue
            for a, b, c in fl:
                n = cross(sub(m["P"][b], m["P"][a]), sub(m["P"][c], m["P"][a]))
                ar = length(n) * 0.5
                if ar > 1e-6:
                    tris.append((ar, m["P"][a], m["P"][b], m["P"][c], unit(n)))
    return tris


def ray_hits(o, d, occ):
    """does the ray o + t d (t > 0) hit any of the occluder triangles (Moller-Trumbore)"""
    for a, e1, e2 in occ:
        pv = cross(d, e2)
        det = dot(e1, pv)
        if abs(det) < 1e-12:
            continue
        inv = 1.0 / det
        tv = sub(o, a)
        u = dot(tv, pv) * inv
        if u < 0 or u > 1:
            continue
        qv = cross(tv, e1)
        v = dot(d, qv) * inv
        if v < 0 or u + v > 1:
            continue
        if dot(e2, qv) * inv > 1e-4:
            return True
    return False


def occluders(meshes):
    out = []
    for m in meshes.values():
        for fl in m["faces"].values():
            for a, b, c in fl:
                out.append((m["P"][a], sub(m["P"][b], m["P"][a]), sub(m["P"][c], m["P"][a])))
    return out


def pick_surface(rng, tris, centre, far_from=(), min_gap=0.0, occ=None):
    """an area-weighted point of the outer surface: facing away from the centre and seen from outside (nothing in
    front of it along its normal), away from the given points"""
    total = sum(t[0] for t in tris)
    for attempt in range(60):
        x = rng.uniform(0, total)
        for ar, a, b, c, n in tris:
            x -= ar
            if x <= 0:
                break
        r1, r2 = rng.random(), rng.random()
        if r1 + r2 > 1:
            r1, r2 = 1 - r1, 1 - r2
        p = add(a, add(mul(sub(b, a), r1), mul(sub(c, a), r2)))
        if dot(n, sub(p, centre)) < 0:          # an inner face (looking at the module centre): not a place to hit
            continue
        if occ and attempt < 50 and ray_hits(add(p, mul(n, 1e-3)), n, occ):
            continue                             # covered by another part: a wound there would not be seen
        if all(length(sub(p, q)) > min_gap for q in far_from):
            return p, n
    return p, n


def make_plan(kind, rng, lods, info):
    allp = [p for m in lods[min(lods)].values() for p in m["P"]]
    lo, hi = bbox(allp)
    centre = tuple((lo[k] + hi[k]) / 2 for k in range(3))
    size = [hi[k] - lo[k] for k in range(3)]
    D = length(size)
    ref = lods[2] if 2 in lods else lods[max(lods)]
    tris = surface_samples(ref, info, D) or surface_samples(lods[min(lods)], info, 0.0)
    occ = occluders(ref)
    feats, removed_parts, moved_parts = [], set(), {}
    if kind == "nozzle_torn":
        # the nozzle end: the aft third along +Z (the pad's side) - a big wound there, often a second one beside it
        aft = [t for t in tris if t[1][2] > hi[2] - 0.33 * size[2]]
        p, n = pick_surface(rng, aft or tris, centre, occ=occ)
        rc = max(0.12, rng.uniform(0.11, 0.15) * D)
        w = Wound(rng, p, n, rc, rc * rng.uniform(2.0, 2.4), +1, rng.uniform(1.0, 1.3), rng.randint(5, 8))
        feats.append(w)
        if rng.random() < 0.6:
            q, m = pick_surface(rng, aft or tris, centre, [p], 0.3 * D, occ)
            r2 = max(0.06, rng.uniform(0.05, 0.07) * D)
            feats.append(Wound(rng, q, m, r2, r2 * 2.1, +1, rng.uniform(0.8, 1.1), rng.randint(3, 5)))
        for name in sorted(info):
            c, diag = info[name]
            if diag < 0.2 * D and length(sub(c, p)) < w.ro:
                if rng.random() < 0.6:
                    removed_parts.add(name)
                else:
                    axis = unit((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
                    moved_parts[name] = (c, axis, math.radians(rng.uniform(15, 40)), mul(w.d, rng.uniform(0.3, 0.8) * rc))
    elif kind == "hole":
        p, n = pick_surface(rng, tris, centre, occ=occ)
        rc = max(0.05, rng.uniform(0.035, 0.055) * D)
        feats.append(Wound(rng, p, n, rc, rc * rng.uniform(1.9, 2.3), +1, rng.uniform(0.6, 0.9), rng.randint(2, 4)))
        for _ in range(rng.randint(2, 3)):
            q, m = pick_surface(rng, tris, centre, [p], 0.3 * D, occ)
            r2 = max(0.025, rng.uniform(0.012, 0.02) * D)
            feats.append(Wound(rng, q, m, r2, r2 * rng.uniform(1.8, 2.2), -1, rng.uniform(0.5, 0.8), rng.randint(0, 2)))
    elif kind == "torn":
        p, n = pick_surface(rng, tris, centre, occ=occ)
        rc = max(0.10, rng.uniform(0.09, 0.13) * D)
        w = Wound(rng, p, n, rc, rc * rng.uniform(2.0, 2.4), +1, rng.uniform(0.9, 1.2), rng.randint(4, 7))
        feats.append(w)
        if rng.random() < 0.5:
            q, m = pick_surface(rng, tris, centre, [p], 0.45 * D, occ)
            r2 = max(0.05, rng.uniform(0.04, 0.06) * D)
            feats.append(Wound(rng, q, m, r2, r2 * 2.1, +1, rng.uniform(0.7, 1.0), rng.randint(2, 4)))
        for name in sorted(info):                    # small parts near the blast: blown off or knocked askew
            c, diag = info[name]
            if diag < 0.2 * D and length(sub(c, p)) < w.ro:
                if rng.random() < 0.5:
                    removed_parts.add(name)
                else:
                    axis = unit((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-1, 1)))
                    moved_parts[name] = (c, axis, math.radians(rng.uniform(12, 38)), mul(w.d, rng.uniform(0.25, 0.7) * rc))
    else:   # broken / nozzle_off
        order = sorted(range(3), key=lambda k: -size[k])
        ax = order[0] if rng.random() < 0.7 else order[1]
        side = 1 if rng.random() < 0.5 else -1
        if kind == "nozzle_off":                        # the nozzle end (+Z) torn off
            ax, side = 2, 1
        n = [0.0, 0.0, 0.0]
        n[ax] = float(side)
        tilt = math.radians(rng.uniform(10, 28))
        n = unit(rotate(tuple(n), unit(cross(tuple(n), (0.3, 0.9, 0.2) if ax != 1 else (0.9, 0.2, 0.3))), tilt))
        frac = rng.uniform(0.58, 0.74)                  # the kept share along the axis
        if kind == "nozzle_off":
            frac = rng.uniform(0.80, 0.88)
        c = list(centre)
        c[ax] = (lo[ax] if side > 0 else hi[ax]) + side * frac * size[ax]
        band = max(0.08, rng.uniform(0.07, 0.10) * D)
        br = Break(rng, tuple(c), n, band * rng.uniform(0.35, 0.55), band, centre)
        # the real cross-section at the break (not the bounding box: a thin leg under a wide pad): the spread of the
        # kept geometry just before the break, 15..85 % along e1 / e2
        near = [p for m in ref.values() for p in m["P"] if -1.5 * band < br.s(p) < 0]
        if len(near) >= 8:
            us = sorted(dot(sub(q, br.c), br.e1) for q in near)
            vs = sorted(dot(sub(q, br.c), br.e2) for q in near)
            pc = lambda a, f: a[min(len(a) - 1, int(f * len(a)))]
            br.inner = ((pc(us, 0.15) + pc(us, 0.85)) / 2, (pc(vs, 0.15) + pc(vs, 0.85)) / 2,
                        (pc(us, 0.85) - pc(us, 0.15)) / 2 * 0.8, (pc(vs, 0.85) - pc(vs, 0.15)) / 2 * 0.8)
        feats.append(br)
        for name in sorted(info):
            pc, diag = info[name]
            if br.s(pc) > diag * 0.5 + br.amp:       # wholly past the break
                removed_parts.add(name)
        kept = [t for t in tris if br.s(t[1]) < -band]
        if kept:
            q, m = pick_surface(rng, kept, centre, occ=occ)
            r2 = max(0.04, rng.uniform(0.03, 0.05) * D)
            feats.append(Wound(rng, q, m, r2, r2 * 2.1, +1, rng.uniform(0.6, 0.9), rng.randint(1, 3)))
    return {"features": feats, "removed": removed_parts, "moved": moved_parts, "size": D, "lo": lo, "hi": hi,
            "centre": centre, "seed": rng.randrange(1 << 30)}


# ---------------------------------------------------------------- applying the plan to one mesh
def inside_tri(p, a, b, c, reach):
    """p projects inside triangle abc and lies within reach of its plane"""
    n = cross(sub(b, a), sub(c, a))
    nn = dot(n, n)
    if nn < 1e-18:
        return False
    if abs(dot(sub(p, a), n)) / math.sqrt(nn) > reach:
        return False
    for u, v in ((a, b), (b, c), (c, a)):
        if dot(cross(sub(v, u), sub(p, u)), n) < 0:
            return False
    return True


def refine(m, feats, lod, budget):
    P, N, faces = m["P"], m["N"], m["faces"]

    def target(p):
        t = None
        for f in feats:
            x = f.target(p, lod)
            if x is not None and (t is None or x < t):
                t = x
        return t

    for _ in range(7):
        ntri = sum(len(fl) for fl in faces.values())
        if ntri > budget:
            break
        split = {}
        for fl in faces.values():
            for tri in fl:
                for k in range(3):
                    a, b = tri[k], tri[(k + 1) % 3]
                    key = (a, b) if a < b else (b, a)
                    if key in split:
                        continue
                    pa, pb = P[a], P[b]
                    t = None
                    for f in feats:                      # at the segment's point nearest to the damage
                        x = f.target(f.closest(pa, pb), lod)
                        if x is not None and (t is None or x < t):
                            t = x
                    split[key] = t is not None and length(sub(pa, pb)) > t
        # a big triangle with a wound inside it and no edge near enough: split its longest edge
        wounds = [f for f in feats if isinstance(f, Wound)]
        for fl in faces.values():
            for tri in fl:
                keys3 = [((tri[k], tri[(k + 1) % 3]) if tri[k] < tri[(k + 1) % 3] else (tri[(k + 1) % 3], tri[k])) for k in range(3)]
                if any(split.get(k) for k in keys3):
                    continue
                a, b, c = P[tri[0]], P[tri[1]], P[tri[2]]
                for w in wounds:
                    if inside_tri(w.c, a, b, c, w.reach()) and max(length(sub(P[k[0]], P[k[1]])) for k in keys3) > w.target(w.c, lod):
                        split[max(keys3, key=lambda k: length(sub(P[k[0]], P[k[1]])))] = True
                        break
        keys = [k for k, v in split.items() if v]
        if not keys:
            break
        mids = {}
        for a, b in keys:
            mids[(a, b)] = len(P)
            P.append(mul(add(P[a], P[b]), 0.5))
            N.append(unit(add(N[a], N[b])))

        def md(a, b):
            return mids.get((a, b) if a < b else (b, a))

        for mat, fl in faces.items():
            out = []
            for tri in fl:
                v = list(tri)
                ms = [md(v[0], v[1]), md(v[1], v[2]), md(v[2], v[0])]
                cnt = sum(x is not None for x in ms)
                if cnt == 0:
                    out.append(tri)
                    continue
                if cnt == 3:
                    a, b, c = v
                    mab, mbc, mca = ms
                    out += [(a, mab, mca), (mab, b, mbc), (mca, mbc, c), (mab, mbc, mca)]
                    continue
                # rotate so that the pattern starts at edge 0
                for r in range(3):
                    vv = v[r:] + v[:r]
                    mm = ms[r:] + ms[:r]
                    if cnt == 1 and mm[0] is not None:
                        a, b, c = vv
                        out += [(a, mm[0], c), (mm[0], b, c)]
                        break
                    if cnt == 2 and mm[0] is not None and mm[1] is not None:
                        a, b, c = vv
                        out += [(mm[0], b, mm[1]), (a, mm[0], mm[1]), (a, mm[1], c)]
                        break
            faces[mat] = out


def apply_plan(m, plan, lod):
    feats = plan["features"]
    name = m["name"]
    m = {"name": name, "mtllib": m["mtllib"], "P": list(m["P"]), "N": list(m["N"]),
         "faces": {k: list(v) for k, v in m["faces"].items()}}
    if name in plan["moved"]:                        # knocked askew: a rigid move, no tearing of its own
        c, axis, ang, shift = plan["moved"][name]
        m["P"] = [add(add(rotate(sub(p, c), axis, ang), c), shift) for p in m["P"]]
        m["N"] = [rotate(n, axis, ang) for n in m["N"]]
        return m
    reach = [f for f in feats if part_touched(m, f)] if m["P"] else []
    if not reach:
        return m
    base = sum(len(fl) for fl in m["faces"].values())
    refine(m, reach, lod, int(base * 1.25) + REFINE_EXTRA[lod])
    P0 = m["P"]
    clip(m, reach)
    P0 = m["P"]
    # materials of the zone (by the triangle centre, before displacement)
    recol = {}
    for mat, fl in m["faces"].items():
        for tri in fl:
            if mat in KEEP_MATS:
                recol.setdefault(mat, []).append(tri)
                continue
            a, b, c = P0[tri[0]], P0[tri[1]], P0[tri[2]]
            ce = ((a[0] + b[0] + c[0]) / 3, (a[1] + b[1] + c[1]) / 3, (a[2] + b[2] + c[2]) / 3)
            new = None
            for f in reach:
                x = f.material(ce, mat)
                if x and (new is None or ["eq_dmg_soot", "eq_dmg_scorch", "eq_dmg_torn"].index(x) >
                                         ["eq_dmg_soot", "eq_dmg_scorch", "eq_dmg_torn"].index(new)):
                    new = x
            recol.setdefault(new or mat, []).append(tri)
    m["faces"] = recol
    # displacement (position only: the copies of a hard-edge vertex move together)
    used = {i for fl in m["faces"].values() for t in fl for i in t}
    P = list(P0)
    moved = set()
    for i in used:
        d = (0.0, 0.0, 0.0)
        for f in reach:
            d = add(d, f.displace(P0[i]))
        if dot(d, d) > 1e-12:
            P[i] = add(P0[i], d)
            moved.add(i)
    m["P"] = P
    if moved:                                         # normals of the moved vertices from their faces
        acc = {i: (0.0, 0.0, 0.0) for i in moved}
        for fl in m["faces"].values():
            for a, b, c in fl:
                if a in acc or b in acc or c in acc:
                    n = cross(sub(P[b], P[a]), sub(P[c], P[a]))
                    for i in (a, b, c):
                        if i in acc:
                            acc[i] = add(acc[i], n)
        for i, n in acc.items():
            if dot(n, n) > 1e-18:
                m["N"][i] = unit(n)
    return m


def clip(m, feats):
    """cut the triangles along the zero line of the damage field (< 0 = gone): the rim follows the field, not the
    triangle edges; a cut point is shared by both sides of an edge (hard-edge copies get the same position)"""
    P, N = m["P"], m["N"]
    F = {}

    def fv(i):
        if i not in F:
            F[i] = min(f.field(P[i]) for f in feats)
        return F[i]

    cut = {}

    def cp(a, b):
        key = (a, b) if a < b else (b, a)
        if key not in cut:
            fa, fb = fv(key[0]), fv(key[1])
            t = fa / (fa - fb)
            t = min(max(t, 0.0), 1.0)
            pa, pb = P[key[0]], P[key[1]]
            P.append(add(pa, mul(sub(pb, pa), t)))
            N.append(unit(add(mul(N[key[0]], 1 - t), mul(N[key[1]], t))))
            cut[key] = len(P) - 1
        return cut[key]

    for mat in list(m["faces"]):
        out = []
        for tri in m["faces"][mat]:
            ins = [fv(i) < 0 for i in tri]
            k = sum(ins)
            if k == 0:
                out.append(tri)
            elif k == 3:
                continue
            else:
                for r in range(3):
                    a, b, c = tri[r:] + tri[:r]
                    ia, ib, ic = ins[r:] + ins[:r]
                    if k == 1 and ia:                   # a is gone: quad b c cac cab
                        x, y = cp(a, b), cp(c, a)
                        out += [(x, b, c), (x, c, y)]
                        break
                    if k == 2 and not ia:               # only a stays
                        out.append((a, cp(a, b), cp(c, a)))
                        break
        m["faces"][mat] = out


def part_touched(m, f):
    lo, hi = bbox(m["P"])
    c = tuple((lo[k] + hi[k]) / 2 for k in range(3))
    r = length(sub(hi, lo)) / 2
    if isinstance(f, Wound):
        return length(sub(c, f.c)) < f.reach() + r
    return abs(f.s(c)) < r + f.band * 1.5 + f.amp


# ---------------------------------------------------------------- innards behind the wounds
def to_world(o, e1, e2, e3, x, y, z):
    return (o[0] + e1[0] * x + e2[0] * y + e3[0] * z, o[1] + e1[1] * x + e2[1] * y + e3[1] * z,
            o[2] + e1[2] * x + e2[2] * y + e3[2] * z)


def frame(e1, e2, e3):
    """rotation whose columns are e1, e2, e3 (local x, y, z -> world)"""
    return [[e1[0], e2[0], e3[0]], [e1[1], e2[1], e3[1]], [e1[2], e2[2], e3[2]]]


def ray_extent(c, d, lo, hi):
    """distance from c along d to the bounding box wall"""
    t = 1e9
    for k in range(3):
        if d[k] > 1e-9:
            t = min(t, (hi[k] - c[k]) / d[k])
        elif d[k] < -1e-9:
            t = min(t, (lo[k] - c[k]) / d[k])
    return max(t, 0.0)


CABLE_MATS = ("eq_insulator", "eq_copper", "eq_status_amber", "eq_dmg_interior")


def cable(part, rng, pts, r, lod):
    if lod >= 2:
        return
    path = eqlib.bezier(pts[0], pts[1], pts[2], pts[3], 8 if lod == 0 else 5)
    eqlib.tube(part, rng.choice(CABLE_MATS), path, r, 6 if lod == 0 else 4)


def wound_innards(part, w, rng, lod, lo, hi):
    """what shows through a hole: a dark back wall, frame members (one broken, its ends bent out), a pipe with a torn
    end sticking out, cables hanging out of the hole, a couple of boxes on the back wall"""
    rc = w.rc
    e1, e2, d = w.e1, w.e2, w.d
    depth = min(rc * 1.3, 0.8 * ray_extent(w.c, mul(d, -1.0), lo, hi))
    if depth < 0.15 * rc:
        return
    R = frame(e1, e2, d)
    P = lambda x, y, z: to_world(w.c, e1, e2, d, x, y, z)
    th = max(0.01, 0.03 * rc)
    eqlib.box(part, "eq_dmg_interior", P(0, 0, -depth), (1.9 * rc, 1.9 * rc, th), R)
    if lod >= 3:
        return
    for k in range(rng.randint(2, 3)):                       # frame members across the hole
        along_x = rng.random() < 0.5
        off = rng.uniform(-0.6, 0.6) * rc
        z = -depth * rng.uniform(0.35, 0.75)
        s = 0.07 * rc
        if k == 0:                                           # broken: two stubs, the torn ends bent outward
            gap = rng.uniform(0.15, 0.35) * rc
            for sgn in (-1, 1):
                a0, a1 = sgn * gap, sgn * 1.0 * rc
                mid = (a0 + a1) / 2
                ax = (1, 0, 0) if along_x else (0, 1, 0)
                bend = eqlib.rot_y(-sgn * 0.45) if along_x else eqlib.rot_x(sgn * 0.45)
                Rl = eqlib.mat_mul(R, bend)
                c = P(mid if along_x else off, off if along_x else mid, z + 0.15 * rc)
                size = (abs(a1 - a0), s, s) if along_x else (s, abs(a1 - a0), s)
                eqlib.box(part, "eq_metal_band", c, size, Rl)
        else:
            c = P(0 if along_x else off, off if along_x else 0, z)
            eqlib.box(part, "eq_metal_band", c, (2.0 * rc, s, s) if along_x else (s, 2.0 * rc, s), R)
    if lod >= 2:
        return
    seg = 8 if lod == 0 else 6
    a = rng.uniform(0, math.pi)                              # a pipe across, torn: one end sticks out of the hole
    u = (math.cos(a), math.sin(a))
    z = -depth * 0.55
    pr = max(0.008, 0.045 * rc)
    eqlib.cyl(part, "eq_steel", P(-u[0] * 1.0 * rc, -u[1] * 1.0 * rc, z), P(-u[0] * 0.1 * rc, -u[1] * 0.1 * rc, z), pr, seg)
    eqlib.cyl(part, "eq_steel", P(u[0] * 1.0 * rc, u[1] * 1.0 * rc, z), P(u[0] * 0.25 * rc, u[1] * 0.25 * rc, z), pr, seg)
    eqlib.cyl(part, "eq_steel", P(u[0] * 0.25 * rc, u[1] * 0.25 * rc, z), P(u[0] * 0.1 * rc, u[1] * 0.1 * rc, z + 0.9 * rc + depth * 0.3), pr * 0.9, seg)
    for _ in range(rng.randint(2, 4)):                       # cables out of the hole, drooping
        b = rng.uniform(0, 2 * math.pi)
        r0 = rng.uniform(0.2, 0.7) * rc
        p0 = P(r0 * math.cos(b), r0 * math.sin(b), -depth * 0.8)
        out = rng.uniform(0.3, 0.8) * rc
        b2 = b + rng.uniform(-0.8, 0.8)
        p3 = P(rc * 1.1 * math.cos(b2), rc * 1.1 * math.sin(b2), out * 0.4)
        cable(part, rng, [p0, P(r0 * math.cos(b) * 0.6, r0 * math.sin(b) * 0.6, out), P(rc * 0.8 * math.cos(b2), rc * 0.8 * math.sin(b2), out * 1.1), p3],
              max(0.004, 0.02 * rc), lod)
    for _ in range(rng.randint(1, 3)):                       # boxes on the back wall
        x, y = rng.uniform(-0.6, 0.6) * rc, rng.uniform(-0.6, 0.6) * rc
        h = rng.uniform(0.12, 0.3) * rc
        eqlib.box(part, rng.choice(("eq_metal_panel", "eq_metal_graphite", "eq_dmg_scorch")), P(x, y, -depth + h / 2),
                  (rng.uniform(0.2, 0.45) * rc, rng.uniform(0.2, 0.45) * rc, h), R)


def break_innards(part, br, rng, lod, lo, hi):
    """the broken end: an inner bulkhead a little behind the break, longerons and pipes sticking out of it with bent
    ends, cables hanging out"""
    n, e1, e2 = br.n, br.e1, br.e2
    R = frame(e1, e2, n)
    # the cross-section half extents along e1 / e2 (inside the box, with a margin)
    if br.inner is None:
        return
    uc, vc, hx, hy = br.inner
    if hx < 0.05 or hy < 0.05:
        return
    b = br.band
    P = lambda x, y, z: to_world(br.c, e1, e2, n, x + uc, y + vc, z)
    eqlib.box(part, "eq_dmg_interior", P(0, 0, -0.9 * b - br.amp), (2 * hx, 2 * hy, max(0.01, 0.03 * b)), R)
    if lod >= 3:
        return
    s = max(0.02, 0.12 * b)
    for k in range(rng.randint(3, 5)):                       # longerons out of the break
        a = 2 * math.pi * k / 4 + rng.uniform(-0.3, 0.3)
        x, y = hx * 0.85 * math.cos(a), hy * 0.85 * math.sin(a)
        L = rng.uniform(0.6, 1.4) * b
        z1 = rng.uniform(-0.2, 0.5) * b
        eqlib.box(part, "eq_metal_band", P(x, y, z1 - L / 2 - br.amp * 0.5), (s, s, L + br.amp), R)
        if lod < 2 and rng.random() < 0.6:                   # its torn tip bent off
            tip = eqlib.mat_mul(R, eqlib.rot_x(rng.uniform(-0.9, 0.9)))
            eqlib.box(part, "eq_metal_band", P(x, y, z1 + 0.2 * b), (s, s, 0.45 * b), tip)
    if lod >= 2:
        return
    seg = 8 if lod == 0 else 6
    for _ in range(rng.randint(1, 3)):                       # pipes, torn ends
        x, y = rng.uniform(-0.6, 0.6) * hx, rng.uniform(-0.6, 0.6) * hy
        pr = max(0.008, 0.07 * b)
        z1 = rng.uniform(0.0, 0.6) * b
        eqlib.cyl(part, "eq_steel", P(x, y, -1.5 * b), P(x, y, z1), pr, seg)
        eqlib.cyl(part, "eq_steel", P(x, y, z1), P(x + rng.uniform(-0.4, 0.4) * b, y + rng.uniform(-0.4, 0.4) * b, z1 + 0.35 * b), pr, seg)
    for _ in range(rng.randint(3, 5)):                       # cables hanging out
        x, y = rng.uniform(-0.7, 0.7) * hx, rng.uniform(-0.7, 0.7) * hy
        dx, dy = rng.uniform(-1, 1) * b, rng.uniform(-1, 1) * b
        cable(part, rng, [P(x, y, -1.2 * b), P(x, y, 0.2 * b), P(x + dx * 0.6, y + dy * 0.6, 0.8 * b), P(x + dx, y + dy, rng.uniform(0.3, 1.0) * b)],
              max(0.004, 0.035 * b), lod)


def innards(plan, lod):
    """one extra part per LOD, the same for every LOD (seeded from the plan), coarser LODs drop the small things"""
    part = eqlib.Part("dmg_innards")
    rng = random.Random(plan["seed"])
    for f in plan["features"]:
        sub_rng = random.Random(rng.randrange(1 << 30))
        if isinstance(f, Break):
            break_innards(part, f, sub_rng, lod, plan["lo"], plan["hi"])
        elif f.sign > 0:
            wound_innards(part, f, sub_rng, lod, plan["lo"], plan["hi"])
    return part


# ---------------------------------------------------------------- one module
def load_intact(asset_dir):
    lods = {}
    for lod in range(4):
        d = source_layout.lod_dir(asset_dir, lod)
        if not os.path.isdir(d):
            continue
        lods[lod] = {}
        for f in sorted(os.listdir(d)):
            if f.endswith(".obj"):
                m = read_obj(os.path.join(d, f))
                m["mtl"] = read_mtl(os.path.join(d, (m["mtllib"] or os.path.splitext(f)[0] + ".mtl")))
                lods[lod][os.path.splitext(f)[0]] = m
    return lods


def is_generated(vdir):
    p = os.path.join(vdir, "assembly", "damage.json")
    if not os.path.exists(p):
        return not os.path.exists(vdir)
    try:
        return json.load(open(p, encoding="utf-8")).get("generator") == GENERATOR
    except ValueError:
        return False


def process(asset_dir):
    asset = os.path.basename(asset_dir)
    lods = load_intact(asset_dir)
    if not lods:
        return asset, "no intact assembly", []
    info = part_info(lods)
    hits_path = os.path.join(source_layout.assembly_dir(asset_dir), "hit_volumes.json")
    hits = json.load(open(hits_path, encoding="utf-8")) if os.path.exists(hits_path) else None
    report = []
    for vid, title in variants_for(asset_dir):
        vdir = source_layout.variant_dir(asset_dir, vid)
        if not is_generated(vdir):
            report.append(f"{vid}: the author's own, skipped")
            continue
        seed = zlib.crc32(f"{asset}/{vid}".encode())
        rng = random.Random(seed)
        plan = make_plan(vid, rng, lods, info)
        adir = source_layout.assembly_dir(asset_dir, vid)
        if os.path.exists(adir):
            shutil.rmtree(adir)
        tri_counts = {}
        for lod, parts in lods.items():
            d = source_layout.lod_dir(asset_dir, lod, vid)
            os.makedirs(d, exist_ok=True)
            total = 0
            for name, m in parts.items():
                if name in plan["removed"]:
                    continue
                out = apply_plan(m, plan, lod)
                n = sum(len(fl) for fl in out["faces"].values())
                if not n:
                    continue
                total += n
                open(os.path.join(d, name + ".obj"), "w", encoding="utf-8", newline="\n").write(obj_text(out, name + ".mtl", vid))
                mats = {k for k, v in out["faces"].items() if v}
                open(os.path.join(d, name + ".mtl"), "w", encoding="utf-8", newline="\n").write(mtl_text(mats, m["mtl"]))
            inn = innards(plan, lod)
            if inn.tris():
                open(os.path.join(d, "dmg_innards.obj"), "w", encoding="utf-8", newline="\n").write(eqlib.obj_text(inn, "dmg_innards.mtl"))
                open(os.path.join(d, "dmg_innards.mtl"), "w", encoding="utf-8", newline="\n").write(eqlib.mtl_text(inn.faces.keys()))
                total += inn.tris()
            tri_counts[f"LOD{lod}"] = total
        if hits:
            h = dict(hits)
            vols = [v for v in hits.get("volumes", []) if v.get("part") not in plan["removed"]]
            brk = [f for f in plan["features"] if isinstance(f, Break)]
            if brk:
                vols = [v for v in vols if "position" not in v or brk[0].s(tuple(v["position"])) < 0]
            h["volumes"] = vols
            h["generator"] = f"{GENERATOR} (do not edit) - the intact volumes without the parts that are gone"
            open(os.path.join(adir, "hit_volumes.json"), "w", encoding="utf-8", newline="\n").write(
                json.dumps(h, ensure_ascii=False, indent=1) + "\n")
        doc = {"generator": GENERATOR, "variant": vid, "title": title, "seed": seed,
               "features": [f.plan() for f in plan["features"]], "removed_parts": sorted(plan["removed"]),
               "moved_parts": sorted(plan["moved"]), "triangles": tri_counts}
        open(os.path.join(adir, "damage.json"), "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=1) + "\n")
        report.append(f"{vid}: " + " ".join(f"{k} {v}" for k, v in tri_counts.items()) +
                      (f", gone {len(plan['removed'])}" if plan["removed"] else "") + (f", askew {len(plan['moved'])}" if plan["moved"] else ""))
    update_passport(asset_dir, asset)
    return asset, None, report


def update_passport(asset_dir, asset):
    p = os.path.join(asset_dir, "model.json")
    doc = json.load(open(p, encoding="utf-8")) if os.path.exists(p) else None
    if doc is None:
        source_layout.passport(asset_dir, asset)
        doc = json.load(open(p, encoding="utf-8"))
    vs = doc.setdefault("variants", [])
    if not any(v.get("id") == "intact" for v in vs):
        vs.insert(0, {"id": "intact", "title": "Целая", "inGame": "shell"})
    for vid, title in variants_for(asset_dir):
        e = next((v for v in vs if v.get("id") == vid), None)
        if e is None:
            vs.append({"id": vid, "title": title, "inGame": "shell", "generated": True})
        elif e.get("generated"):
            e["title"] = title
    open(p, "w", encoding="utf-8", newline="\n").write(json.dumps(doc, ensure_ascii=False, indent=1) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", help="folder substrings, comma-separated")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    a = ap.parse_args()
    only = [x for x in (a.only or "").split(",") if x]

    def module(d):
        """a module of the equipment (not a set "drive + pad": kind assembly, its parts have their own variants)"""
        m = os.path.join(eqlib.OUT, d)
        if not os.path.isdir(source_layout.assembly_dir(m)):
            return False
        return source_layout.passport(m, d, write=False).get("kind", "module") == "module"

    assets = sorted(os.path.join(eqlib.OUT, d) for d in os.listdir(eqlib.OUT)
                    if os.path.isdir(os.path.join(eqlib.OUT, d)) and (not only or any(x in d for x in only)) and module(d))
    bad = 0
    with Pool(min(a.jobs, len(assets) or 1)) as pool:
        for asset, err, report in pool.imap_unordered(process, assets):
            if err:
                bad += 1
                print(f"{asset}: {err}", flush=True)
            else:
                print(f"{asset}: " + "; ".join(report), flush=True)
    print(f"done: {len(assets) - bad} of {len(assets)} modules")


if __name__ == "__main__":
    main()
