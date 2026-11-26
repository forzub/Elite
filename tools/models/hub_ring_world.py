"""Put the RING hub into the world (author 2026-11-22: «закольцованный хаб ... в пятиминутной доступности от плоского
хаба, что бы тестировать механику»): builds it with hub_belt_world.py (city layout, lanes, plan, schedule) and then
moves its record from the generator's Ceres slot onto the Earth hub's orbit 40 km ahead, spinning about the hub's Y at
the period of its rim gravity (units/hub_ring.py: R and RING_RIM_G), the city module turned so the ring's axis is the spin axis.

usage: python tools/models/hub_ring_world.py [--hub earth_ring_hub] [--ahead-km 40]
"""
import argparse
import copy
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.dirname(HERE))
import world_state_io  # noqa: E402

ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
WORLD = os.path.join(ROOT, "src", "assets", "data", "initial_world_state.json")
EARTH_R_KM = 6371.0


def add_axial_dock(hub, hub_id):
    """THE RING'S DOCK (author 2026-11-24: «нет ... куда пристыковаться ... мы его делали обычным хабом»): a HexDock on the
    spin axis in the ring's centre - the one place of a spinning station that does not move sideways - its axis along the
    spin axis (local +Z -> the hub's +Y, the city's turn), the service block behind it on the axis (as the flat city's
    cty_dock / cty_dock_base). It turns with the hub (the docking tunnel matches the roll)."""
    hub["dock_modules"] = [m for m in hub["dock_modules"]
                           if m["id"] not in (f"{hub_id}.cty_dock", f"{hub_id}.cty_dock_base")]
    from units.hub_ring import DOCK_SERVICE_BLOCK_AXIAL_M
    # turns_with_ring: the game gives these the ring's spin (GameSceneSetup) - by the data, not by their ids
    hub["dock_modules"].append({"id": f"{hub_id}.cty_dock", "type": "hub_hex_dock", "name": "Док",
                                "offset_m": {"x": 0.0, "y": 0.0, "z": 0.0}, "rotation_deg": {"x": -90.0, "y": 0.0, "z": 0.0},
                                "generated": "hub_city", "angular_velocity_deg_s": {"x": 0.0, "y": 0.0, "z": 0.0},
                                "turns_with_ring": True})
    hub["dock_modules"].append({"id": f"{hub_id}.cty_dock_base", "type": "dock_service_block", "name": "Док: служебный блок",
                                "offset_m": {"x": 0.0, "y": DOCK_SERVICE_BLOCK_AXIAL_M, "z": 0.0},
                                "rotation_deg": {"x": -90.0, "y": 0.0, "z": 0.0},
                                "generated": "hub_city", "angular_velocity_deg_s": {"x": 0.0, "y": 0.0, "z": 0.0},
                                "turns_with_ring": True})
    tp = hub.setdefault("traffic_profile", {})
    tp["dock_sectors"] = max(1, int(tp.get("dock_sectors", 0)))


def register_axial_dock_anchors(hub_id):
    """The axial dock's NAVIGATION ANCHORS and port state (2026-11-26: the ring's dock had none - a docking request
    failed "dock state unavailable"; it is the Newtonian ships' dock of the ring, city pads are closed to them): copied
    from the reference hex dock entry (hub_hex_dock_a), as hub_belt_world.py does for a planar city's dock - the docking
    target is (module id, anchor id). Idempotent: the module's old entries are replaced in place."""
    nav = os.path.join(ROOT, "src", "assets", "data", "navigation")
    a_path = os.path.join(nav, "hub_semantic_anchors.json")
    p_path = os.path.join(nav, "hub_docking_runtime_test.json")
    anchors = json.loads(open(a_path, encoding="utf-8").read())
    ports = json.loads(open(p_path, encoding="utf-8").read())
    mid = f"{hub_id}.cty_dock"
    at_a = next((k for k, m in enumerate(anchors["modules"]) if m["module_id"] == mid), len(anchors["modules"]))
    at_p = next((k for k, q in enumerate(ports["ports"]) if q["module_id"] == mid), len(ports["ports"]))
    anchors["modules"] = [m for m in anchors["modules"] if m["module_id"] != mid]
    ports["ports"] = [q for q in ports["ports"] if q["module_id"] != mid]
    m = copy.deepcopy(next(m for m in anchors["modules"] if m["module_id"] == "hub_hex_dock_a"))
    m["module_id"] = mid
    new_ports = [{"module_id": mid, "anchor_id": an["id"], "operational": "online", "occupancy": "free", "access": "allowed"}
                 for an in m["anchors"] if an["kind"] == "docking_port"]
    anchors["modules"][at_a:at_a] = [m]
    ports["ports"][at_p:at_p] = new_ports
    open(a_path, "w", encoding="utf-8", newline="\n").write(json.dumps(anchors, ensure_ascii=False, indent=2) + "\n")
    open(p_path, "w", encoding="utf-8", newline="\n").write(json.dumps(ports, ensure_ascii=False, indent=2) + "\n")
    print(f"{mid}: {len(m['anchors'])} anchors, {len(new_ports)} port(s)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--hub", default="earth_ring_hub")
    ap.add_argument("--ahead-km", type=float, default=40.0)
    ap.add_argument("--name", default="Кольцо")
    a = ap.parse_args()
    subprocess.check_call([sys.executable, os.path.join(HERE, "hub_city_layout.py"), a.hub], stdin=subprocess.DEVNULL)
    subprocess.check_call([sys.executable, os.path.join(HERE, "hub_belt_world.py"), "--hub", a.hub, "--name", a.name],
                          stdin=subprocess.DEVNULL)
    from units.hub_ring import ring_layout
    ring = ring_layout(a.hub)["ring"]

    def fix(doc):
        hubs = doc["orbital_hubs"]
        earth = next(h for h in hubs if h["id"] == "earth_orbital_hub")
        hub = next(h for h in hubs if h["id"] == a.hub)
        hub["owner"] = earth["owner"]
        hub["operator"] = f"Служба кольца «{a.name}» ({earth['owner']})"
        hub["role"] = f"вращающееся кольцо ({ring['rim_g']:g} g, R {ring['radius_m']:.0f} м), один этаж на внутренней стороне; испытательный хаб у Высокого"
        hub["system_id"] = earth["system_id"]
        hub["parent_body_id"] = earth["parent_body_id"]
        hub["model_family"] = "hub_ring"
        hub["traffic_profile"] = {"profile": "residential", "arrivals_day": 0, "dock_sectors": 0}
        hub["motion"] = copy.deepcopy(earth["motion"])
        r_km = EARTH_R_KM + float(earth["motion"]["altitude_km"])
        hub["motion"]["initial_phase_deg"] = round(float(earth["motion"]["initial_phase_deg"]) +
                                                   math.degrees(a.ahead_km / r_km), 7)
        hub["motion"]["self_rotation_period_seconds"] = ring["period_s"]
        for m in hub["dock_modules"]:
            if m["id"] == f"{a.hub}.city":
                m["offset_m"] = {"x": 0.0, "y": 0.0, "z": 0.0}
                m["rotation_deg"] = {"x": -90.0, "y": 0.0, "z": 0.0}   # the ring's axis (city +Z) = the hub's spin axis (+Y)
                m["turns_with_ring"] = True
        add_axial_dock(hub, a.hub)
        hub["note"] = "generated by tools/models/hub_ring_world.py (units/hub_ring.py); a test hub, not in the canon"
        return doc

    world_state_io.update(fix, WORLD)
    register_axial_dock_anchors(a.hub)
    print(f"{a.hub}: on the Earth hub's orbit {a.ahead_km} km ahead, R {ring['radius_m']} m, spin {ring['period_s']} s")


if __name__ == "__main__":
    main()
