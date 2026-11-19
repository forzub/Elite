"""Gate: the fuel stations (docs/FUEL_STATIONS.md) - data up to date with its generator, and the refuel service wired.

1. tools/colonization_lab/fuel_stations.py --check: src/assets/data/fuel/fuel_stations.json and docs/FUEL_STATIONS.md
   are what the generator writes from the current inputs; its consistency checks pass.
2. The data: every runtime hub a pilot can dock at today sells fuel (except the ruin / prototypes); every illegal place
   is flagged, has a risk and a markup; the placement pass's 6 refuelling stations and 17 hidden sites are there.
3. The wiring (source audit): the server command, the service file in both simulation builds, the wire fields, the
   client key, the DEV verbs, the economy stub - so the service cannot silently fall out of a target.
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
fails = []


def need(cond, what):
    if not cond:
        fails.append(what)


def read(*p):
    with open(os.path.join(ROOT, *p), encoding="utf-8") as f:
        return f.read()


r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "colonization_lab", "fuel_stations.py"), "--check"],
                   cwd=ROOT, capture_output=True, text=True, stdin=subprocess.DEVNULL, timeout=300)
need(r.returncode == 0, "fuel_stations.py --check: " + (r.stdout + r.stderr).strip()[-400:])

data = json.loads(read("src", "assets", "data", "fuel", "fuel_stations.json"))
st = {s["id"]: s for s in data["stations"]}
iws = json.loads(read("src", "assets", "data", "initial_world_state.json"))
for h in iws["orbital_hubs"]:
    if h["id"] in ("plane_ruin", "proto_district", "proto_mega"):
        need(h["id"] not in st, f"{h['id']} should not sell fuel")
        continue
    s = st.get(h["id"])
    need(s is not None and not s["illegal"] and s["stock_t"]["jump_charge"] > 0, f"runtime hub {h['id']} sells no fuel")
need(sum(1 for s in st.values() if s["kind"] == "refuel_point") >= 6, "the 6 refuelling stations")
hidden = json.loads(read("src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json"))["hidden_sites"]
for h in hidden:
    need(st.get(h["id"], {}).get("illegal") is True, f"hidden site {h['id']} not an illegal fuel station")
for s in st.values():
    if s["illegal"]:
        need(s.get("risk") is not None and s["price_factor"] > 1.0 and s["world_object"] is False, f"illegal {s['id']}: risk / markup / not spawned")

cmake = read("CMakeLists.txt")
need(cmake.count("src/game/simulation/GameSimulationRefuel.cpp") >= 2, "GameSimulationRefuel.cpp in the game AND the server")
need(cmake.count("src/game/fuel/FuelStationCatalog.cpp") >= 2, "FuelStationCatalog.cpp in the game AND the server")
server = read("src", "game", "server", "GameServer.cpp")
need("case ClientShipCommand::RequestRefuel:" in server, "server handles RequestRefuel")
need(re.search(r"ClientShipCommand::RequestRefuel\)\)", read("src", "game", "network", "WireProtocol.h")) is not None,
     "WireProtocol accepts the RequestRefuel command type")
schema = read("src", "game", "network", "WireDataSchema.h")
for f in ("refuelState", "refuelRefusal", "refuelSerial", "refuelDeliveredT", "refuelRateTpm"):
    need(f"v.{f}," in schema, f"wire field {f}")
need("updateRefuels(dt);" in read("src", "game", "simulation", "GameSimulationFreeFlight.cpp"), "refuel jobs ticked in the pass")
need("requestRefuel(it->first, service.hubId)" in read("src", "game", "simulation", "GameSimulationRoute.cpp"),
     "a route stop's refuel goes through the refuel service")
need("GLFW_KEY_U" in read("src", "game", "SpaceState.cpp"), "Ctrl+U on the client")
dev = read("src", "game", "dev", "DevScriptRunner.cpp")
need('cmd == "refuel"' in dev and 'condition == "refueled"' in dev, "DEV verbs refuel / waitfor:refueled")
refuel = read("src", "game", "simulation", "GameSimulationRefuel.cpp")
need("game::economy::service::fuelPriceCrPerTonne" in refuel and "game::economy::service::chargeAccount" in refuel,
     "the refuel service prices and charges through the economy service (EconomyService.h)")
need("sellsFuel(hubId)" in read("src", "game", "SpaceStateRoute.cpp"), "the route planner inserts refuel stops only at fuel stations")

if fails:
    print("FAIL")
    for f in fails:
        print(" -", f)
    sys.exit(1)
print(f"OK: {len(st)} fuel stations ({data['summary']['illegal']} illegal), service wired")
