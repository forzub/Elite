"""Intermediate LODs by pruning small details (author 2026-11-23: «пару промежуточных LOD, в которых убрано 50 % и 75 %
мелких деталей» - the hub city popped from a bare box to a lit house).

A detail = a connected piece of one Part's faces (pieces built by the primitives share no vertices). "Small" = its
bounding diagonal under SMALL_FRACTION of the whole asset's diagonal. prune(parts, keep_count) keeps the `keep_count`
LARGEST small pieces (the smallest go first) and every big one; glowing materials are never pruned (a lamp is small but
it is what the eye finds - the far lights carry them further, src/model_asset/ModelAssetLights.h).
"""
import math

import eqlib

SMALL_FRACTION = 0.12
GLOW = ("window", "beacon", "flood", "flare", "nav", "facade")


def _glowing(mat):
    return any(k in mat for k in GLOW)


def _pieces(part):
    """[(diag, [(mat, face_index)])] - the connected pieces of a part (union-find over shared vertices)"""
    parent = list(range(len(part.v)))

    def find(a):
        while parent[a] != a:
            parent[a] = parent[parent[a]]
            a = parent[a]
        return a

    for mat, faces in part.faces.items():
        for f in faces:
            r = find(f[0])
            for i in f[1:]:
                q = find(i)
                if q != r:
                    parent[q] = r
    groups = {}
    for mat, faces in part.faces.items():
        for fi, f in enumerate(faces):
            groups.setdefault(find(f[0]), []).append((mat, fi))
    out = []
    for root, fl in groups.items():
        lo = [1e30] * 3
        hi = [-1e30] * 3
        glow = False
        for mat, fi in fl:
            glow = glow or _glowing(mat)
            for vi in part.faces[mat][fi]:
                p = part.v[vi]
                for k in range(3):
                    lo[k] = min(lo[k], p[k])
                    hi[k] = max(hi[k], p[k])
        out.append((math.dist(lo, hi), glow, fl))
    return out


def asset_diag(parts):
    lo = [1e30] * 3
    hi = [-1e30] * 3
    for part in parts.values():
        for p in part.v:
            for k in range(3):
                lo[k] = min(lo[k], p[k])
                hi[k] = max(hi[k], p[k])
    return math.dist(lo, hi) if hi[0] >= lo[0] else 0.0


def small_count(parts, diag):
    lim = SMALL_FRACTION * diag
    return sum(1 for part in parts.values() for d, glow, _ in _pieces(part) if d < lim and not glow)


def prune(parts, keep_count, diag):
    """keep the keep_count largest small (non-glowing) pieces across all parts; returns (new parts, removed)"""
    lim = SMALL_FRACTION * diag
    small = []
    per_part = {}
    for name, part in parts.items():
        ps = _pieces(part)
        per_part[name] = ps
        for idx, (d, glow, _) in enumerate(ps):
            if d < lim and not glow:
                small.append((d, name, idx))
    small.sort(reverse=True)                     # largest first: those are kept
    drop = {(n, i) for _, n, i in small[max(0, int(keep_count)):]}
    if not drop:
        return parts, 0
    out = {}
    for name, part in parts.items():
        keep = {}
        for idx, (d, glow, fl) in enumerate(per_part[name]):
            if (name, idx) in drop:
                continue
            for mat, fi in fl:
                keep.setdefault(mat, []).append(part.faces[mat][fi])
        np_ = eqlib.Part(part.name)
        remap = {}
        for mat, faces in keep.items():
            for f in faces:
                nf = []
                for vi in f:
                    if vi not in remap:
                        remap[vi] = len(np_.v)
                        np_.v.append(part.v[vi])
                        np_.n.append(part.n[vi])
                    nf.append(remap[vi])
                np_.faces.setdefault(mat, []).append(tuple(nf))
        out[name] = np_
    return out, len(drop)
