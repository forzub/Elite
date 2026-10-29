"""Source contracts read a class as ONE unit even when its implementation is
split into several translation units by responsibility.

Importing this module makes Path.read_text() of a split base file return the
base file followed by its sibling units (and the private *Internal.h), so a
contract such as "SpaceState publishes X" keeps checking the whole class
instead of breaking when code moves between its units. Every other path reads
unchanged.
"""
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]

# base file -> sibling units (glob, relative to the repository root)
SPLIT_UNITS = {
    "src/game/SpaceState.cpp": ["src/game/SpaceState*.cpp", "src/game/SpaceStateInternal.h"],
    "src/game/simulation/GameSimulation.cpp": [
        "src/game/simulation/GameSimulation*.cpp",
        "src/game/simulation/GameSimulationInternal.h",
    ],
    "src/game/system_map/SystemMapRenderer.cpp": [
        "src/game/system_map/SystemMapRenderer*.cpp",
        "src/game/system_map/SystemMapRendererInternal.h",
    ],
    "src/game/ship/core/ShipCore.cpp": [
        "src/game/ship/core/ShipCore*.cpp",
        "src/game/ship/core/ShipCoreInternal.h",
    ],
    "src/game/navigation/autopilot/DedicatedClientRouteAutopilot.h": [
        "src/game/navigation/autopilot/RouteExecutionState.h",
        "src/game/navigation/autopilot/RouteExecutionCore.h",
        "src/game/navigation/autopilot/RouteExecutionCore.cpp",
    ],
}

_BASES = {(_ROOT / base).resolve(): base for base in SPLIT_UNITS}
_original_read_text = Path.read_text


def _units(base):
    files = [_ROOT / base]
    for pattern in SPLIT_UNITS[base]:
        for path in sorted(_ROOT.glob(pattern)):
            if path not in files:
                files.append(path)
    return files


def _read_text(self, *args, **kwargs):
    base = _BASES.get(Path(self).resolve())
    if base is None:
        return _original_read_text(self, *args, **kwargs)
    return "\n".join(_original_read_text(p, *args, **kwargs) for p in _units(base))


Path.read_text = _read_text
