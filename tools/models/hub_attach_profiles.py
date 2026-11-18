"""Attach the generated city of every hub with a profile (tools/models/hub_profiles.json) to its existing world hub:
python tools/models/hub_attach_profiles.py [hub_id ...]  (runs hub_belt_world.py --hub <id> --attach --city-offset ...;
the hub assembly sources must exist: build_equipment_models.py --only hub_<id>; package hub_<id> in the mesh editor)"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))


def main():
    prof = json.load(open(os.path.join(HERE, "hub_profiles.json"), encoding="utf-8"))["hubs"]
    hubs = sys.argv[1:] or sorted(prof)
    for h in hubs:
        off = prof[h].get("attach_offset_m", [0, 0, 0])
        r = subprocess.run([sys.executable, os.path.join(HERE, "hub_belt_world.py"), "--hub", h, "--attach", "--city-offset",
                            ",".join(str(v) for v in off)], cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        print(f"== {h} (rc {r.returncode})")
        print((r.stdout or "")[-600:], (r.stderr or "")[-1200:])


if __name__ == "__main__":
    main()
