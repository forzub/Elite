"""put layout-sketch renders of a ship into the reference workshop (key hull:<ship id>), replacing older ones"""
import json, os, sys, time
from PIL import Image
sid, prefix = sys.argv[1], sys.argv[2]
pairs = [a.split("=", 1) for a in sys.argv[3:]]          # png=caption
key = "hull:" + sid
d = "Notes/html/ship_reference/workshop/sketches/hull_" + sid
os.makedirs(d, exist_ok=True)
for f in os.listdir(d):
    os.remove(os.path.join(d, f))
p = "Notes/html/ship_reference/workshop/workshop.json"
raw = open(p, "rb").read(); w = json.loads(raw); ts = int(time.time() * 1000)
lst = w["sketches"].setdefault(key, []); lst.clear()
for png, cap in pairs:
    n = os.path.splitext(os.path.basename(png))[0]
    im = Image.open(png).convert("RGB"); im = im.resize((960, int(im.size[1] * 960 / im.size[0])), Image.LANCZOS)
    im.save(os.path.join(d, n + ".jpg"), quality=84, optimize=True)
    lst.append({"id": f"{prefix}-{n}", "file": n + ".jpg", "name": cap, "note": "tools/ships/layout_sketch.py", "added": ts, "updated": ts})
nl = b"\r\n" if b"\r\n" in raw else b"\n"
o = json.dumps(w, ensure_ascii=False, indent=1).encode("utf-8").replace(b"\n", nl)
if raw.endswith(nl): o += nl
open(p, "wb").write(o); print(key, len(lst))
