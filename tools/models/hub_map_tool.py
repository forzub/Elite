"""The hub map tool behind the mesh editor's hub mode (tools/mesh_editor/MeshEditorHub.cpp; the map schema:
tools/models/units/hub_plane.py "the hub MAP"). Every command writes one JSON document to --out.

  catalog                      the cards: every hub module with its footprint (bounds) and title; the zones
  list                         the hubs: the saved maps (tools/models/hub_maps) and the generator's planar hubs
  generate --hub H [--from S]  the generated start map of hub id S (default H), as hub H; anchor frozen; NOT saved
  load --hub H                 the saved map of hub H (null when there is none)
  derive --map F               what the map builds into, for the editor's overlay: blocks, streets, lanes,
                               connectors, red zones, docks, piers, crossings
  save --map F                 check the map and write tools/models/hub_maps/<hub_id>.json
  build --hub H                the source models of hub_<H> (build_equipment_models.py --only hub_<H>) and the hub in
                               the world (hub_belt_world.py: world entry, lanes, haze); the editor then packages hub_<H>
  serve [--port-file F] [--parent-pid P]   the same commands over HTTP (POST /<command>) for the editor's page
usage: python tools/models/hub_map_tool.py <command> [...] --out <file.json>
"""
import argparse
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))


def _write(path, doc):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, ensure_ascii=False, indent=1)
        f.write("\n")
    os.replace(tmp, path)


def _titles():
    import importlib
    out = {}
    for name in ("hub_district", "hub_mega", "hub_belt", "hub_vista", "hub_plane", "hub_damage"):
        mod = importlib.import_module(f"units.{name}")
        for k, v in getattr(mod, "PASSPORTS", {}).items():
            out[k] = v[0]
        for k, v in getattr(mod, "KIT", {}).items():
            if isinstance(v, tuple) and len(v) >= 3 and isinstance(v[2], str):
                out.setdefault(k, v[2])
    return out


def cmd_catalog(a):
    import units.hub_plane as hp
    from units.hub_belt import _module_obb
    titles = _titles()
    zone_of = {}
    for z, lst in list(hp.ZONE_MODULES.items()) + [("under:" + k, v) for k, v in hp.UNDER_MODULES.items()]:
        for f, _w in lst:
            zone_of.setdefault(f, []).append(z)
    mods = []
    for k, (lo, hi, _mat) in sorted(_module_obb().items()):
        if k in ("hubb_girder", "hubb_road", "hubb_tube", "hubb_monorail", "hubk_truss", "hubp_crossing", "hubb_junction"):
            continue                                    # the city's own infrastructure: derived, not a card
        mods.append({"id": k, "title": titles.get(k, k), "lo": [round(v, 1) for v in lo], "hi": [round(v, 1) for v in hi],
                     "zones": zone_of.get(k, []), "damaged": "_dmg_" in k})
    zones = [{"id": z, "title": hp.ZONE_TITLE.get(z, z), "special": z in hp.SPECIAL_ZONES,
              "modules": [f for f, _ in hp.ZONE_MODULES.get(z, [])], "under": hp.UNDER_OF.get(z)}
             for z in ["downtown", "residential", "hotel", "hab_rot", "storage", "industry", "port", "logistics", "refuel",
                       "repair", "rig", "canopy", "dock", "power", "services"]]
    piers = sorted({f for lst in hp.ZONE_PIERS.values() for f, _ in lst})
    return ({"type": "hubCatalog", "modules": mods, "zones": zones, "piers": piers,
                   "cell_m": hp.CELL, "street_local_m": hp.STREET_LOCAL, "street_avenue_m": hp.STREET_AVENUE,
                   "layers": [{"id": k, "y": v} for k, v in hp.LAYERS.items()]})


def cmd_list(a):
    import units.hub_plane as hp
    items = []
    saved = set()
    if os.path.isdir(hp.MAP_DIR):
        for f in sorted(os.listdir(hp.MAP_DIR)):
            if f.endswith(".json"):
                m = json.load(open(os.path.join(hp.MAP_DIR, f), encoding="utf-8"))
                saved.add(m["hub_id"])
                items.append({"id": m["hub_id"], "title": m.get("title", m["hub_id"]), "saved": True,
                              "cells": len(m["cells"]), "file": os.path.join(hp.MAP_DIR, f).replace("\\", "/")})
    for h in hp.PLANE_HUBS:
        if h not in saved and h not in hp.RUIN_OF:
            items.append({"id": h, "title": h, "saved": False, "cells": None, "file": None})
    return ({"type": "hubList", "items": items, "ruins": hp.RUIN_OF})


def cmd_generate(a):
    import units.hub_plane as hp
    m = hp._generate_map(getattr(a, "from") or a.hub)
    L = hp.layout_from_map(m)
    m["hub_id"] = a.hub
    m["title"] = a.title or a.hub
    m["grid"]["anchor"] = L["anchor"]                  # frozen: growing the outline later moves nothing
    return ({"type": "hubMap", "map": m})


def _load(path):
    return json.load(open(path, encoding="utf-8"))


def _map(a):
    """the map of a command: inline (the editor's request) or a file"""
    return getattr(a, "map_doc", None) or _load(a.map)


def _check(m):
    import units.hub_plane as hp
    from units.hub_belt import _module_obb
    obb = _module_obb()
    errs = []
    if m.get("schema") != hp.MAP_SCHEMA:
        errs.append(f"schema != {hp.MAP_SCHEMA}")
    if not m.get("hub_id") or not all(ch.isalnum() or ch == "_" for ch in m["hub_id"]):
        errs.append("hub_id: латиница, цифры, _")
    seen = set()
    for c in m.get("cells", []):
        k = (c["i"], c["j"])
        if k in seen:
            errs.append(f"квартал {k} дважды")
        seen.add(k)
        if c["zone"] not in hp.ZONE_TITLE:
            errs.append(f"квартал {k}: зона {c['zone']}?")
        for layer in hp.LAYERS:
            for card in c.get(layer, []):
                if card["module"] not in obb:
                    errs.append(f"квартал {k}: модуль {card['module']}?")
                if int(card.get("yaw", 0)) % 360 not in hp.YAWS:
                    errs.append(f"квартал {k}: поворот {card.get('yaw')}")
        if c["zone"] == "dock" and tuple(c.get("dock_side") or ()) not in hp._open_sides(k, {(q["i"], q["j"]) for q in m["cells"]}):
            errs.append(f"квартал {k}: док не смотрит в открытый край")
    if not m.get("cells"):
        errs.append("нет ни одного квартала")
    return errs


def cmd_derive(a):
    import units.hub_plane as hp
    import pads
    from units.hub_belt import _module_obb
    m = _map(a)
    errs = _check(m)
    if errs and not m.get("cells"):
        return ({"type": "hubDerived", "errors": errs})
        return
    L = hp.layout_from_map(m)
    obb = _module_obb()
    piers, crossings = [], []
    for nid, folder, pos, rot, role, parent in L["nodes"]:
        if role == "dock":                              # the piers of the rim blocks (derived from the cells' piers)
            lo, hi, _ = obb[folder]
            R = pads.euler(rot)
            cs = [_rot(R, (x, 0.0, z)) for x, z in ((lo[0], lo[2]), (hi[0], lo[2]), (hi[0], hi[2]), (lo[0], hi[2]))]
            piers.append({"module": folder, "corners": [[round(p[0] + pos[0], 1), round(p[2] + pos[2], 1)] for p in cs]})
    flat = lambda q: [round(q[0], 1), round(q[1], 1), round(q[2], 1)]
    lanes = [{"id": ln["id"], "kind": ln.get("kind", "green"), "level": ln["level"], "avenue": ln.get("avenue", False),
              "plaza": ln.get("plaza", False), "stations": [flat(q) for q in ln["stations"]], "speed": ln["speed"],
              "max_ship_m": ln["max_ship_m"]} for ln in L["lanes"]]
    conns = [{"id": cn["id"], "points": [flat(q) for q in cn["points"]]} for cn in L["connectors"]]
    reds = []
    for rz in L["red_zones"]:
        R = pads.euler(rz["rotation_deg"])
        hx, _hy, hz = rz["half_extents"]
        c = rz["center"]
        corners = []
        for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            p = _rot(R, (sx * hx, 0.0, sz * hz))
            corners.append([round(c[0] + p[0], 1), round(c[2] + p[2], 1)])
        reds.append({"id": rz["id"], "y": c[1], "corners": corners, "map": rz["id"].startswith("red.map.")})
    flat_ln = lambda ln: {"id": ln["id"], "kind": ln.get("kind", "green"), "level": ln["level"], "avenue": ln.get("avenue", False),
                          "plaza": ln.get("plaza", False), "stations": [flat(q) for q in ln["stations"]], "speed": ln["speed"],
                          "max_ship_m": ln["max_ship_m"]}
    every = _all_lanes(m)                               # the switched-off lanes too: the editor draws them dashed
    all_lane_ids = sorted({ln["id"] for ln in every})
    return ({"type": "hubDerived", "errors": errs, "anchor": L["anchor"],
                   "centres": {ax: {str(k): v for k, v in d.items()} for ax, d in L["centres"].items()},
                   "lines": {ax: {str(k): v for k, v in d.items()} for ax, d in L["lines"].items()},
                   "lanes": lanes, "connectors": conns, "red_zones": reds, "piers": piers, "crossings": crossings,
                   "docks": [{"id": d["id"], "type": d["type"], "x": d["position"][0], "z": d["position"][2],
                              "yaw": d["rotation_deg"][1]} for d in L["docks"]],
                   "spinning": [{"id": s["id"], "module": s["module"], "x": s["position"][0], "z": s["position"][2]} for s in L["spinning"]],
                   "lanes_all": all_lane_ids, "lanes_geo": [flat_ln(ln) for ln in every], "nodes": len(L["nodes"]), "streets": L["links"]})


def _rot(R, v):
    return tuple(sum(R[i][k] * v[k] for k in range(3)) for i in range(3))


def _all_lanes(m):
    """every lane the map could have (lanes_off ignored): the ids the editor can switch back on"""
    import units.hub_plane as hp
    m2 = dict(m)
    m2["lanes_off"] = []
    return hp.layout_from_map(m2)["lanes"]


def cmd_load(a):
    import units.hub_plane as hp
    m = hp.load_map(a.hub)
    return ({"type": "hubMap", "map": m, "saved": m is not None})


def cmd_save(a):
    import units.hub_plane as hp
    m = _map(a)
    errs = _check(m)
    if errs:
        return ({"type": "hubSaved", "ok": False, "errors": errs})
        return
    os.makedirs(hp.MAP_DIR, exist_ok=True)
    path = hp.map_path(m["hub_id"])
    m["cells"] = sorted(m["cells"], key=lambda c: (c["i"], c["j"]))
    _write(path, m)
    return ({"type": "hubSaved", "ok": True, "file": path.replace("\\", "/"), "errors": []})


def cmd_build(a):
    import units.hub_plane as hp
    m = hp.load_map(a.hub)
    if not m:
        return ({"type": "hubBuilt", "ok": False, "log": f"нет сохранённой карты {a.hub}"})
        return
    log = []
    py = sys.executable
    steps = [[py, os.path.join(HERE, "build_equipment_models.py"), "--only", f"hub_{a.hub}"],
             [py, os.path.join(HERE, "hub_belt_world.py"), "--hub", a.hub] +
             (["--name", m["title"]] if m.get("title") and m["title"] != a.hub else [])]
    ok = True
    for st in steps:
        r = subprocess.run(st, cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", errors="replace")
        log.append("$ " + " ".join(os.path.basename(x) if i < 2 else x for i, x in enumerate(st)))
        log.append((r.stdout or "")[-1500:] + (r.stderr or "")[-1500:])
        if r.returncode != 0:
            ok = False
            break
    return ({"type": "hubBuilt", "ok": ok, "hub": a.hub, "assembly": f"hub_{a.hub}", "log": "\n".join(log)})


def serve(a):
    """a local HTTP server for the editor's page: POST /<command> {hub, from, title, map} -> the command's JSON. The
    modules import once (~6 s); a derive then takes ~0.3 s. Exits with the editor (--parent-pid)."""
    import threading
    import types
    from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
    import units.hub_plane  # noqa: F401 - the slow import, before the port is announced

    class H(BaseHTTPRequestHandler):
        def _send(self, code, doc):
            body = json.dumps(doc, ensure_ascii=False).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_OPTIONS(self):
            self._send(200, {})

        def do_POST(self):
            cmd = self.path.strip("/")
            req = json.loads(self.rfile.read(int(self.headers.get("Content-Length", "0") or 0)) or b"{}")
            fn = globals().get("cmd_" + cmd)
            if cmd not in COMMANDS or not fn:
                self._send(404, {"type": "hubError", "error": f"no command {cmd}"})
                return
            ns = types.SimpleNamespace(hub=req.get("hub"), title=req.get("title"), map=None, map_doc=req.get("map"), out=None)
            setattr(ns, "from", req.get("from"))
            try:
                self._send(200, fn(ns))
            except Exception as e:
                import traceback
                self._send(200, {"type": "hubError", "command": cmd, "error": f"{type(e).__name__}: {e}",
                                 "trace": traceback.format_exc()[-3000:]})

        def log_message(self, *args):
            pass

    srv = ThreadingHTTPServer(("127.0.0.1", a.port or 0), H)
    port = srv.server_address[1]
    if a.port_file:
        _write(a.port_file, {"port": port, "pid": os.getpid()})
    print(f"[hub_map_tool] serving on 127.0.0.1:{port}", flush=True)
    if a.parent_pid:
        def watch():
            import ctypes
            import time
            k32 = ctypes.windll.kernel32
            h = k32.OpenProcess(0x00100000, False, a.parent_pid)          # SYNCHRONIZE
            if not h:
                os._exit(0)
            k32.WaitForSingleObject(h, 0xFFFFFFFF)                       # the editor exited
            time.sleep(0.2)
            os._exit(0)
        threading.Thread(target=watch, daemon=True).start()
    srv.serve_forever()


COMMANDS = ["catalog", "list", "generate", "load", "derive", "save", "build"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("command", choices=COMMANDS + ["serve"])
    ap.add_argument("--hub")
    ap.add_argument("--from", dest="from")
    ap.add_argument("--title")
    ap.add_argument("--map")
    ap.add_argument("--out")
    ap.add_argument("--port", type=int, default=0)
    ap.add_argument("--port-file")
    ap.add_argument("--parent-pid", type=int, default=0)
    a = ap.parse_args()
    if a.command == "serve":
        serve(a)
        return
    try:
        doc = globals()["cmd_" + a.command](a)
    except Exception as e:  # the editor shows the reason
        import traceback
        doc = {"type": "hubError", "command": a.command, "error": f"{type(e).__name__}: {e}", "trace": traceback.format_exc()[-3000:]}
    _write(a.out, doc)
    sys.exit(1 if doc.get("type") == "hubError" else 0)


if __name__ == "__main__":
    main()
