#!/usr/bin/env python3
"""Build the ship compartment assets end to end (step 3): layout sketch -> SOURCE (ship_asset_source.py) ->
EliteShipAssetAssemble -> production package src/assets/compiled/models/<id>/<id>.elmodel (git-ignored like every
compiled package; the game copies it on build; opt-in in game with ELITE_SHIP_MODEL_ASSETS=1) + the WORKING copy in
the Model Asset Editor workspace build/tools/model_asset_editor/workspaces/<id>/working/<id>.elmodel, so the editor
opens it with every field filled (catalog group "ship_assets").
usage: python tools/ships/build_ship_assets.py [cobrett-i]   (needs build/EliteShipAssetAssemble.exe)
"""
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import ship_asset_source as S  # noqa: E402

ROOT = S.ROOT
EXE = os.path.join(ROOT, "build", "EliteShipAssetAssemble.exe")


def main():
    for name in (sys.argv[1:] or list(S.LS.SHIPS)):
        S.build(name)
        src = os.path.join(S.OUT, name, "source")
        cid = json.load(open(os.path.join(src, "ship_asset.json"), encoding="utf-8"))["catalog_id"]
        prod = os.path.join(ROOT, "src", "assets", "compiled", "models", cid, cid + ".elmodel")
        work = os.path.join(ROOT, "build", "tools", "model_asset_editor", "workspaces", cid, "working", cid + ".elmodel")
        os.makedirs(os.path.dirname(prod), exist_ok=True)
        r = subprocess.run([EXE, src, prod, work], capture_output=True, text=True)
        print((r.stdout + r.stderr).strip())
        if r.returncode:
            sys.exit(r.returncode)


if __name__ == "__main__":
    main()
