"""Dump a hub's generated layout to JSON (a regression reference for the hub map refactor / the hub editor):
python tools/models/hub_map_dump.py <hub_id> <out.json>"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from units.hub_plane import plane_layout  # noqa: E402


def main():
    L = plane_layout(sys.argv[1])
    out = {k: L[k] for k in ("nodes", "spinning", "haze", "docks", "lanes", "connectors", "red_zones", "districts")}
    open(sys.argv[2], "w", encoding="utf-8").write(json.dumps(out, default=list, indent=0))
    print(len(L["nodes"]), "nodes")


if __name__ == "__main__":
    main()
