"""Colony hubs with generated cities (author 2026-11-18: «если ты хочешь делать хабы других колоний сборкой по ай-ди,
то почему их все не делать сборкой через ай-ди? главное, чтобы вид хаба был повторяем и узнаваем»).

  python tools/models/hub_colonies.py <world site id> [...]

For each site of the world catalog (src/assets/data/galaxy_details/world_sites/sites_3126.json, an orbital site):
  1. its generation profile in tools/models/hub_profiles.json: the size from the population, shape / under-city / port
     share from the id, the national style of its polity (units/hub_styles.polity_style), landmarks on;
  2. its world entry in src/assets/data/initial_world_state.json (an orbital hub of its system with a command station;
     the server spawns the authored hubs of every system at start - GameServer::materializeWorldSiteHubs - and the
     body gets no generated station then);
  3. the city attached 6 km under the station (tools/models/hub_belt_world.py --attach): spinning bodies, docks, lanes,
     the Hub-map plan, the daily schedule, the haze;
  4. its frozen city layout (tools/models/hub_city_layout.py) - the game assembles the city in memory from it.
Re-running keeps an existing profile and world entry (a hub stays what it was); --refresh rebuilds the profile;
--no-layout skips step 4 (tools/models/hub_colonies_batch.py runs the layouts in parallel).
"""
import hashlib
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
SITES = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json")
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))   # tools/: world_state_io
import world_state_io  # noqa: E402 - the one writer of initial_world_state.json
WORLD = os.path.join(ROOT, "src", "assets", "data", "initial_world_state.json")
PROFILES = os.path.join(HERE, "hub_profiles.json")
CITY_OFFSET = [0, -6000, 0]
SPIRE_ONE_IN = 5                # one hub in five (by its id) is a spire cluster
PEOPLE_PER_BLOCK = 22000.0      # the Sol cities: ~1.2-1.6 M people on ~56-99 blocks (hub_plan.population_estimate)


def _h(site_id, salt):
    return int(hashlib.sha1(f"{salt}:{site_id}".encode("utf-8")).hexdigest()[:8], 16)


def profile_of(site):
    from units.hub_styles import polity_style
    sid = site["id"]
    blocks = max(12, min(110, round((site.get("population") or 0) / PEOPLE_PER_BLOCK)))
    shape = ("blob", "disc", "long")[_h(sid, "shape") % 3]
    if _h(sid, "spire") % SPIRE_ONE_IN == 0:
        shape = "spire"          # a spire cluster (units/hub_spire.py): its own draw - the hubs before it keep theirs
    if shape == "long":
        m = max(3, round((blocks / 2.0) ** 0.5))
        n = max(m + 2, round(blocks / m))
    else:
        n = max(4, round(blocks ** 0.5 * 1.1))
        m = max(3, round(blocks / n * 1.15))
    return {"size": [n, m], "shape": shape, "under": _h(sid, "under") % 3 != 0,
            "port": round(0.3 + 0.6 * (_h(sid, "port") % 100) / 100.0, 2),
            "style": polity_style(site.get("polity") or ""), "landmarks": True, "attach_offset_m": CITY_OFFSET,
            "site": sid}


def world_entry(site):
    sid = site["id"]
    orbit = (site.get("placement") or {}).get("orbit") or {}
    station = sid + ".station"
    return {
        "id": sid, "name": site.get("name") or sid, "owner": site.get("polity_name") or site.get("owner") or "",
        "operator": site.get("operator") or "", "role": (site.get("purpose") or site.get("function") or site.get("type") or ""),
        "system_id": site["system_id"], "parent_body_id": site["parent_body_id"], "map_object_module_id": station,
        "model_family": "command_station",
        "traffic_profile": {"profile": "colony", "arrivals_day": site.get("arrivals_day") or 0,
                            "dock_sectors": site.get("dock_sectors") or 0},
        "motion": {"type": "parent_orbit", "altitude_km": orbit.get("altitude_km") or 800, "orbital_period_seconds": 0,
                   "self_rotation_period_seconds": 180, "inclination_deg": orbit.get("inclination_deg") or 0.0,
                   "longitude_of_ascending_node_deg": _h(sid, "lan") % 360, "argument_of_periapsis_deg": 0,
                   "initial_phase_deg": _h(sid, "phase") % 360, "epoch_seconds": 0, "orbital_period_mode": "kepler"},
        "modules": [{"id": station, "type": "command_station", "name": site.get("name") or sid,
                     "offset_m": {"x": 0, "y": 0, "z": 0}, "state": {"exists": True},
                     "local_rotation_deg": {"pitch": 45, "yaw": 0, "roll": 0}, "map_visible": True}],
        "dock_modules": [],
        "note": "a colony hub with a generated city: tools/models/hub_colonies.py (world site " + sid + ")"}


def run(cmd):
    r = subprocess.run([sys.executable] + cmd, cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(r.stdout[-1500:], r.stderr[-3000:])
        raise SystemExit(f"failed: {' '.join(cmd)}")
    return r.stdout


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    args = sys.argv[1:]
    refresh = "--refresh" in args
    ids = [a for a in args if not a.startswith("--")]
    sites = {s["id"]: s for s in json.load(open(SITES, encoding="utf-8"))["sites"]}
    for sid in ids:
        site = sites.get(sid)
        if not site:
            raise SystemExit(f"{sid}: no such world site")
        if "orbit" not in (site.get("placement") or {}):
            raise SystemExit(f"{sid}: not an orbital site (surface settlements are a later stage)")
        if site.get("system_id") is None or site.get("runtime_binding"):
            raise SystemExit(f"{sid}: bound to an authored hub already (the Sol cities have their own profiles)")
        prof = json.load(open(PROFILES, encoding="utf-8"))
        if refresh or sid not in prof["hubs"]:
            prof["hubs"][sid] = profile_of(site)
            with open(PROFILES + ".tmp", "w", encoding="utf-8", newline="\n") as f:   # atomic: parallel layouts read it
                f.write("{\n")
                f.write(f' "schema": {json.dumps(prof["schema"])},\n "note": {json.dumps(prof["note"], ensure_ascii=False)},\n "hubs": {{\n')
                f.write(",\n".join(f"  {json.dumps(k)}: {json.dumps(v, ensure_ascii=False)}" for k, v in prof["hubs"].items()))
                f.write("\n }\n}\n")
            os.replace(PROFILES + ".tmp", PROFILES)
        def add_if_missing(doc, sid=sid, site=site):
            if not any(h["id"] == sid for h in doc["orbital_hubs"]):
                world_state_io.upsert_hub(doc, world_entry(site), "hub_colonies")
        world_state_io.update(add_if_missing, WORLD)
        out = run([os.path.join(HERE, "hub_belt_world.py"), "--hub", sid, "--attach", "--city-offset", ",".join(map(str, CITY_OFFSET))])
        print(out.strip().splitlines()[-1] if out.strip() else sid)
        if "--no-layout" not in args:                 # a batch (hub_colonies_batch.py) runs the layouts aside
            print(run([os.path.join(HERE, "hub_city_layout.py"), sid]).strip())
        p = prof["hubs"][sid]
        print(f"  {sid}: «{site.get('name')}» system {site['system_id']} pop {site.get('population')} -> {p['size']} {p['shape']} "
              f"style '{p['style']}' under {p['under']} port {p['port']}")


if __name__ == "__main__":
    main()
