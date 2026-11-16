"""Pads as their own modules (MESH_EDITOR_SPEC 18.11).

Author 2026-11-15: «пятаки. по-идее это расходная часть и двигателя и реактора. т.е. по логике - они должны быть
отдельно. и когда будем собирать корабль - двигатель скорее всего будет под корпусом. а пятаки торчать наружу …
тогда у нас возникают две сборки - двигатель + пятак и реактор + пятак».

The generators keep building a drive / reactor with its pad (the pad's size is the passport's); build_equipment_models
then splits every unit that has a pad part into three models of the source:

  <unit>/        the drive / reactor without the pad; intact/assembly/mounts.json says where its pad(s) sit
  <unit>_pad/    the pad alone in its own frame: the mating face (the side towards the unit) at y = 0, centred on the
                 Y axis, the radiating side outward +Y - the hull-skin convention (it sticks out through the hull)
  <unit>_set/    kind "assembly": the unit + its pad(s) by reference (assembly.json nodes with ref); its LOD meshes are
                 the two models placed, for viewing - the package takes the referenced modules' own meshes

A drive's pad (engine_hot_emitter_annulus) is at its +Z end; a reactor's (reactor_cooling_pad) on top (+Y); the ER-5
has a second plate under the bay (-Y): one pad module, two mounts. Part ids stay the same inside the pad module
(engine_hot_emitter_annulus is the runtime thermal-emitter mesh id).
"""
import json
import math

from eqlib import Part, apply, norm, rot_x

PADS = {"engine_hot_emitter_annulus": "drive", "reactor_cooling_pad": "reactor"}
TITLES = {"drive": "Пятак-излучатель", "reactor": "Пятак охлаждения"}
GEN = "tools/models/build_equipment_models.py + pads.py (do not edit)"


def _mat(A, B):
    return [[sum(A[i][k] * B[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def _t(A):
    return [[A[j][i] for j in range(3)] for i in range(3)]


def euler(deg):
    """rotation_deg as used by hit volumes / assembly nodes: T * Rx * Ry * Rz"""
    rx, ry, rz = (math.radians(a) for a in deg)
    from eqlib import rot_y, rot_z
    return _mat(_mat(rot_x(rx), rot_y(ry)), rot_z(rz))


def to_euler(R):
    sy = max(-1.0, min(1.0, R[0][2]))
    ry = math.asin(sy)
    if abs(sy) < 0.9999:
        rx = math.atan2(-R[1][2], R[2][2])
        rz = math.atan2(-R[0][1], R[0][0])
    else:
        rx = math.atan2(R[2][1], R[1][1])
        rz = 0.0
    return [round(math.degrees(a), 4) + 0.0 for a in (rx, ry, rz)]


def pad_part(parts):
    """(part id, kind) of the unit's pad, or (None, None)"""
    for pid, kind in PADS.items():
        if pid in parts and parts[pid].tris():
            return pid, kind
    return None, None


def mounts(kind, part):
    """the pad mounts of a unit from its LOD0 pad part: pad frame -> unit frame (position, rotation_deg)"""
    axis = 2 if kind == "drive" else 1
    pos = [v[axis] for v in part.v if v[axis] > 0]
    m0 = round(min(pos), 4) + 0.0
    t = [0.0, 0.0, 0.0]
    t[axis] = m0
    out = [{"id": "pad", "position": t, "rotation_deg": [90.0, 0.0, 0.0] if kind == "drive" else [0.0, 0.0, 0.0]}]
    if any(v[axis] < -0.5 * m0 for v in part.v):
        b = [0.0, 0.0, 0.0]
        b[axis] = -m0
        out.append({"id": "pad_2", "position": b, "rotation_deg": [180.0, 0.0, 0.0]})
    for m in out:
        m["axis"] = axis
    return out


def _side(m, p):
    """does a point of the unit frame belong to this mount (its side of the unit)?"""
    a = m["axis"]
    return p[a] * m["position"][a] > 0


def to_pad(part, m):
    """the pad part in the pad frame: the faces on the mount's side, mapped by the inverse mount transform"""
    R = _t(euler(m["rotation_deg"]))
    t = m["position"]
    out = Part(part.name)
    remap = {}
    for mat, fl in part.faces.items():
        for f in fl:
            if not all(_side(m, part.v[i]) for i in f):
                continue
            idx = []
            for i in f:
                if i not in remap:
                    remap[i] = len(out.v)
                    q = apply(R, tuple(a - b for a, b in zip(part.v[i], t)))
                    out.v.append(tuple(round(x, 6) + 0.0 for x in q))
                    out.n.append(norm(apply(R, part.n[i])))
                idx.append(remap[i])
            out.faces.setdefault(mat, []).append(tuple(idx))
    return out


def vol_to_pad(v, m):
    R = _t(euler(m["rotation_deg"]))
    q = dict(v)
    q["position"] = list(apply(R, tuple(a - b for a, b in zip(v["position"], m["position"]))))
    q["rotation_deg"] = to_euler(_mat(R, euler(v.get("rotation_deg", [0, 0, 0]))))
    return q


def split_hits(vols, pid, ms):
    """(unit volumes, pad volumes in the pad frame - from the first mount)"""
    unit = [v for v in vols if v["part"] != pid]
    pad = [vol_to_pad(v, ms[0]) for v in vols if v["part"] == pid and _side(ms[0], v["position"])]
    return unit, pad


def merged(name, parts):
    """one part from several (the set's view mesh of a module)"""
    out = Part(name)
    for p in parts:
        base = len(out.v)
        out.v += p.v
        out.n += p.n
        for mat, fl in p.faces.items():
            out.faces.setdefault(mat, []).extend(tuple(i + base for i in f) for f in fl)
    return out


def mounts_text(asset, ms, pad_id):
    doc = {"schema": "elite.mounts.v1", "asset": asset, "generator": GEN,
           "frame": "the unit's OBJ frame; a mount places the pad module's frame (mating face at y = 0, outward +Y): "
                    "p_unit = T * Rx * Ry * Rz * p_pad",
           "mounts": [{"id": m["id"], "kind": "pad", "module": pad_id, "position": m["position"],
                       "rotation_deg": m["rotation_deg"]} for m in ms]}
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"


def pad_mounts_text(pad_id, unit):
    doc = {"schema": "elite.mounts.v1", "asset": pad_id, "generator": GEN,
           "frame": "the pad's own frame: the mating face at y = 0, the radiating side outward +Y",
           "mounts": [{"id": "base", "kind": "pad_base", "fits": unit, "position": [0.0, 0.0, 0.0],
                       "rotation_deg": [0.0, 0.0, 0.0], "normal": [0.0, -1.0, 0.0]}]}
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"


def set_doc(unit, ms, unit_hits, pad_hits):
    """assembly.json of <unit>_set: the unit and its pad(s) by reference (MESH_EDITOR_SPEC 16.3)"""
    pad_id = unit + "_pad"
    nodes = [{"id": unit, "part": unit, "parent": None, "role": "equipment", "ref": "equipment_ship/" + unit,
              "position": [0.0, 0.0, 0.0], "rotation_deg": [0.0, 0.0, 0.0]}]
    hv, mts, conns = [], [], []
    for h in unit_hits:
        q = dict(h)
        q["node"] = unit
        q["source"] = "hit_volumes.json"
        hv.append(q)
    for k, m in enumerate(ms):
        nid = m["id"]
        nodes.append({"id": nid, "part": pad_id, "parent": unit, "role": "pad", "ref": "equipment_ship/" + pad_id,
                      "position": m["position"], "rotation_deg": m["rotation_deg"]})
        R = euler(m["rotation_deg"])
        for h in pad_hits:
            q = dict(h)
            q["id"] = f"{nid}.{h['id']}"
            q["node"] = nid
            q["position"] = [round(a + b, 4) + 0.0 for a, b in zip(apply(R, h["position"]), m["position"])]
            q["rotation_deg"] = to_euler(_mat(R, euler(h.get("rotation_deg", [0, 0, 0]))))
            q["source"] = "hit_volumes.json"
            hv.append(q)
        mts.append({"id": f"mount_{nid}", "a": unit, "b": nid, "kind": "pad_mount", "load_bearing": False,
                    "position": m["position"], "rotation_deg": m["rotation_deg"], "half_size": [0.1, 0.05, 0.1],
                    "source": "generator"})
        conns.append({"id": f"heat_{nid}", "from": unit, "to": nid, "bus": "heat", "source": "generator"})
    doc = {"schema": "elite.assembly.v1", "model": f"equipment_ship/{unit}_set", "generator": GEN,
           "seededFrom": ["generator"], "parts": {}, "nodes": nodes, "materials": {}, "hit_volumes": hv, "zones": [],
           "mounts": mts, "seams": [], "connections": conns,
           "damage": {"states": [], "detachable": [m["id"] for m in ms]}}
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"


def ref_set_doc(set_id, nodes, hits_by_node):
    """assembly.json of a set of whole models by reference (std.drone.toolbox carry sets, 2026-11-16): nodes =
    [(node id, model folder, position, rotation_deg, role, parent)], hits_by_node = {node id: [hit dicts in the model's
    frame]} - placed into the set frame; mounts link each child to its parent"""
    out_nodes, hv, mts = [], [], []
    for nid, folder, pos, rot, role, parent in nodes:
        out_nodes.append({"id": nid, "part": folder, "parent": parent, "role": role, "ref": "equipment_ship/" + folder,
                          "position": [round(x, 4) + 0.0 for x in pos], "rotation_deg": [round(x, 4) + 0.0 for x in rot]})
        R = euler(rot)
        counts = {}
        for h in hits_by_node.get(nid, []):
            counts[h["part"]] = counts.get(h["part"], 0) + 1
            q = {k: h[k] for k in ("part", "shape", "half_size", "radius", "half_height", "radius_top", "hex_top") if k in h}
            q["id"] = f'{nid}.{h["part"]}.{counts[h["part"]]}'
            q["node"] = nid
            q["position"] = [round(a + b, 4) + 0.0 for a, b in zip(apply(R, h["position"]), pos)]
            q["rotation_deg"] = to_euler(_mat(R, euler(h.get("rotation_deg", [0, 0, 0]))))
            q["source"] = "hit_volumes.json"
            hv.append(q)
        if parent:
            mts.append({"id": f"mount_{nid}", "a": parent, "b": nid, "kind": role + "_mount", "load_bearing": False,
                        "position": [round(x, 4) + 0.0 for x in pos], "rotation_deg": [round(x, 4) + 0.0 for x in rot],
                        "half_size": [0.15, 0.02, 0.12], "source": "generator"})
    doc = {"schema": "elite.assembly.v1", "model": f"equipment_ship/{set_id}", "generator": GEN,
           "seededFrom": ["generator"], "parts": {}, "nodes": out_nodes, "materials": {}, "hit_volumes": hv, "zones": [],
           "mounts": mts, "seams": [], "connections": [],
           "damage": {"states": [], "detachable": [n[0] for n in nodes if n[5]]}}
    return json.dumps(doc, ensure_ascii=False, indent=1) + "\n"
