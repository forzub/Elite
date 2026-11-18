"""Add the power station blocks and the police / rescue service blocks to a SAVED hub map (hub_plane.add_services:
the maps generated from now on get them anyway): python tools/models/hub_map_add_services.py <hub_id>"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import units.hub_plane as hp  # noqa: E402


def main():
    hub = sys.argv[1]
    m = hp.load_map(hub)
    if not m:
        print(f"no saved map for {hub}")
        sys.exit(1)
    before = {(c["i"], c["j"]): c["zone"] for c in m["cells"]}
    hp.add_services(m)
    changed = [(k, before[k], c["zone"]) for c in m["cells"] for k in [(c["i"], c["j"])] if before[k] != c["zone"]]
    path = hp.map_path(hub)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(m, f, ensure_ascii=False, indent=1)
        f.write("\n")
    for k, a, b in changed:
        print(f"block {k}: {a} -> {b}")
    print(f"{hub}: {len(changed)} blocks changed -> {path}")


if __name__ == "__main__":
    main()
