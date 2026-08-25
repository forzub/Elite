#!/usr/bin/env python3
from pathlib import Path
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "src/game/scene/GameSceneSetup.cpp"


def fail(message: str) -> None:
    print(f"[FAIL] Hub guidance test geometry: {message}", file=sys.stderr)
    raise SystemExit(1)


text = SCENE.read_text(encoding="utf-8", errors="replace")

cube_pos = text.find('"guidance_dock_cube_a"')
cylinder_pos = text.find('"guidance_dock_cylinder_b"')
specs_end = text.find("    };", cylinder_pos)
if min(cube_pos, cylinder_pos, specs_end) < 0 or not (cube_pos < cylinder_pos < specs_end):
    fail("guidance cube/cylinder specs are missing or reordered unexpectedly")

cube_block = text[cube_pos:cylinder_pos]
cylinder_block = text[cylinder_pos:specs_end]


def last_vec3(block: str, label: str) -> tuple[float, float, float]:
    matches = re.findall(r"glm::dvec3\(([^)]*)\)", block)
    if not matches:
        fail(f"{label} has no glm::dvec3 fields")
    parts = [part.strip() for part in matches[-1].split(",")]
    if len(parts) == 1:
        value = float(parts[0])
        return (value, value, value)
    if len(parts) != 3:
        fail(f"{label} angular velocity is not a 3-vector")
    try:
        return tuple(float(part) for part in parts)
    except ValueError as exc:
        fail(f"{label} angular velocity is not numeric: {exc}")


cube_spin = last_vec3(cube_block, "guidance box")
cylinder_spin = last_vec3(cylinder_block, "guidance cylinder")

if abs(cube_spin[0]) > 1.0e-12 or abs(cube_spin[1]) > 1.0e-12:
    fail("guidance box must rotate only around local Z")
if not (0.0 < abs(cube_spin[2]) <= 2.0):
    fail("guidance box must retain a slow non-zero local-Z rotation (<= 2 deg/s)")
if any(abs(component) > 1.0e-12 for component in cylinder_spin):
    fail("guidance cylinder must remain completely non-rotating")


# Dock apertures are deliberately horizontal: width is the long X/right side,
# height is the short Y/up side. The visible apron remains on local -Y so the
# ship's belly/down direction is unambiguous without a HUD marker.
anchors_path = ROOT / "src/assets/data/navigation/hub_semantic_anchors.json"
anchors = json.loads(anchors_path.read_text(encoding="utf-8"))
for module_id in ("guidance_dock_cube_a", "guidance_dock_cylinder_b"):
    module = next((m for m in anchors["modules"] if m["module_id"] == module_id), None)
    if not module:
        fail(f"missing semantic module {module_id}")
    docks = [a for a in module["anchors"] if a.get("kind") == "docking_port"]
    if not docks:
        fail(f"{module_id} has no docking ports")
    for dock in docks:
        width, height = dock["extent_m"][:2]
        if not width > height:
            fail(f"{module_id}/{dock['id']} aperture must be wider than tall")

for mesh_name, half_width, half_height in (
    ("guidance_dock_cube.obj", 95.0, 55.0),
    ("guidance_dock_cylinder.obj", 100.0, 60.0),
):
    mesh = (ROOT / "src/assets/models/hub/guidance_test" / mesh_name).read_text(
        encoding="utf-8", errors="replace"
    )
    if f"{half_width:.6f}" not in mesh or f"{half_height:.6f}" not in mesh:
        fail(f"{mesh_name} does not expose the horizontal aperture dimensions")

print("[PASS] Hub guidance box rotates slowly, cylinder stays static, apertures are horizontal")
