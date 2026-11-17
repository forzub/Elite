#!/usr/bin/env python3
"""World-state contract for hub dock modules and hub lane networks (audit 2026-11-18).

src/assets/data/initial_world_state.json is written by several generators (tools/colonization_lab/sol_network.py,
world_sites.py, lore_sites.py, naming.py; tools/models/hub_belt_world.py, hub_district_world.py). One of them dropped
Высокий's physical dock (429488bf2) and nothing noticed until a live docking run failed. This gate checks the facts the
runtime depends on:
  - orbital_hubs[].dock_modules, where present, is the runtime LIST of modules (never the capacity model's count);
  - Высокий (earth_orbital_hub) keeps its hex dock hub_hex_dock_a + service block (the DEV docking target, the
    authored BLUE traffic graph hub_traffic_route_graph.json);
  - every hub lane network (src/assets/data/navigation/hub_lanes/*.json) names an existing hub and a frame module that
    hub really has.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def fail(message: str) -> None:
    print(f"World-state hub dock check failed: {message}", file=sys.stderr)
    raise SystemExit(1)


world = json.loads((ROOT / "src/assets/data/initial_world_state.json").read_text(encoding="utf-8"))
hubs = {h["id"]: h for h in world.get("orbital_hubs", [])}

for hub_id, hub in hubs.items():
    modules = hub.get("dock_modules")
    if modules is None:
        continue
    if not isinstance(modules, list) or not all(isinstance(m, dict) and m.get("id") and m.get("type") for m in modules):
        fail(f"orbital_hubs[{hub_id}].dock_modules must be a list of {{id, type, ...}} modules, got {type(modules).__name__}")

vys = hubs.get("earth_orbital_hub")
if vys is None:
    fail("earth_orbital_hub (Высокий) is missing")
vys_modules = {m["id"]: m["type"] for m in vys.get("dock_modules") or []}
for module_id, module_type in (("hub_hex_dock_a", "hub_hex_dock"), ("hub_hex_dock_base", "dock_service_block")):
    if vys_modules.get(module_id) != module_type:
        fail(f"earth_orbital_hub lost its dock module {module_id} ({module_type}); "
             "sol_network.py runtime_hubs must carry EARTH_ORBITAL_HUB_DOCK_MODULES")

for path in sorted((ROOT / "src/assets/data/navigation/hub_lanes").glob("*.json")):
    doc = json.loads(path.read_text(encoding="utf-8"))
    hub_id = doc.get("hub_id")
    if hub_id not in hubs:
        fail(f"{path.name}: hub_id {hub_id!r} is not an orbital hub of the world state")
    ids = {m["id"] for m in hubs[hub_id].get("dock_modules") or []}
    if doc.get("frame_module_id") not in ids:
        fail(f"{path.name}: frame_module_id {doc.get('frame_module_id')!r} is not a module of hub {hub_id}")

print("World-state hub dock check passed")
