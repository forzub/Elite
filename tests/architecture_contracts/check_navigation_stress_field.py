#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[2]
scene = (root / "src/game/scene/GameSceneSetup.cpp").read_text(encoding="utf-8")
obstacles = (root / "src/world/navigation/NavigationObstacleGeometry.cpp").read_text(encoding="utf-8")
perf_header = (root / "src/world/navigation/NavigationPerfLog.h").read_text(encoding="utf-8")
hub_basis = (root / "src/game/navigation/HubFrameBasis.h").read_text(encoding="utf-8")
hub_backend = (root / "src/game/system_map/HubMapBackend.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit("NAVIGATION STRESS FIELD CONTRACT: FAIL\n" + message)


# Visual Hub local is X=normal, Y=radial, Z=-prograde while navigation local is
# X=prograde, Y=radial, Z=normal. Keep the transform pinned because layout and
# navigation must describe the same physical field.
require(
    # Formatting-insensitive: the subtraction now spans two source lines.
    '-progradeAxis*localVector.z' in "".join(hub_basis.split()),
    "Hub visual/navigation basis sign contract changed",
)

required_scene_tokens = [
    '"hub_hex_dock_a"',
    '"guidance_dock_cylinder_b"',
    # The two-shell comment is re-wrapped; pin the shell labels instead.
    'Inner shell: radius ~= 3 km',
    'Outer shell: radius ~= 5 km',
    # Representative points on both shells. These intentionally do not form the
    # old player->dock barrier.
    'glm::dvec3(2770.0, 900.0, 1150.0)',
    'glm::dvec3(-2770.0, 1100.0, -1150.0)',
    'glm::dvec3(4900.0, -1700.0, 975.0)',
    'glm::dvec3(-2780.0, 1000.0, -4160.0)',
]
for index in range(1, 9):
    required_scene_tokens.append(f'"nav_stress_cube_{index:02d}"')
    required_scene_tokens.append(f'"nav_stress_cylinder_{index:02d}"')

missing_scene = [token for token in required_scene_tokens if token not in scene]
require(not missing_scene, "missing scene tokens: " + ", ".join(missing_scene))

old_barrier_tokens = [
    'glm::dvec3(575.0, 2090.0, 8000.0)',
    'glm::dvec3(1260.0, 1595.0, 5600.0)',
    'glm::dvec3(1925.0, 1120.0, 3300.0)',
    'glm::dvec3(2440.0, 750.0, 1500.0)',
]
require(
    not any(token in scene for token in old_barrier_tokens),
    "old four-band player->dock barrier is still present",
)

required_obstacle_tokens = [
    'obstacleBroadphaseRadiusMeters(',
    'segmentOutsideObstacleBroadphase(',
    'if (segmentOutsideObstacleBroadphase(',
]
missing_obstacle = [token for token in required_obstacle_tokens if token not in obstacles]
require(not missing_obstacle, "missing broadphase tokens: " + ", ".join(missing_obstacle))

# The global perf logger was removed (22503389): the header only composes an
# explicit path and must stay free of I/O, clocks and working-directory lookups.
required_perf_tokens = [
    'navigationPerfLogPath(',
    'const std::filesystem::path& diagnosticsRoot',
    '"navigation_perf.log"',
]
for forbidden in ('std::ofstream', 'std::chrono', 'current_path(', 'std::cout', 'std::cerr'):
    require(forbidden not in perf_header, 'navigation perf header regained side effects: ' + forbidden)
missing_perf = [token for token in required_perf_tokens if token not in perf_header]
require(not missing_perf, "missing perf-log path tokens: " + ", ".join(missing_perf))

# Hub Map labels are one generic hover policy over the shared object overlay.
# Persistent per-module and per-ship names are forbidden.
required_hover_tokens = [
    'presentation.frame.objectOverlay.items',
    'glfwGetCursorPos(',
    'const MapObjectOverlayItem* hovered',
    'glm::vec4(0.78f, 0.91f, 0.98f, 0.72f)',
]
missing_hover = [token for token in required_hover_tokens if token not in hub_backend]
require(not missing_hover, "missing hover-label tokens: " + ", ".join(missing_hover))
require(
    'text.textDrawPx(\n                    mod.name' not in hub_backend,
    "persistent Hub module labels returned",
)

print("NAVIGATION STRESS FIELD CONTRACT: PASS")
print(" - Hub visual/navigation basis sign is pinned")
print(" - 2 authored docking targets retained")
print(" - 16 deterministic obstacles are distributed on two staggered Hub shells")
print(" - old route-crossing barrier layout is forbidden")
print(" - conservative segment/obstacle broadphase retained")
print(" - Hub Map names are hover-only over the shared object overlay")
print(" - navigation perf log path is composed explicitly, without logger side effects")
