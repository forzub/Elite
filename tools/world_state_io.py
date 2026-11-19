"""The ONE writer of src/assets/data/initial_world_state.json (architecture audit 2026-11-19).

Several tools write orbital hubs into this file (world_sites.py / sol_network: Sol's hubs; hub_colonies.py: colony
hubs; hub_belt_world.py: belt settlements and the city attachment of colony hubs; hub_district_world.py: the district
prototype). Before, each did its own read-modify-write, and world_sites.py replaced EVERY system-0 hub with its own
list - Sol hubs added by the other tools were silently lost.

Rules:
- every hub entry carries "world_state_owner": the tool that wrote it last;
- update(fn): read -> fn(doc) -> write, under a lock file, written atomically (<file>.tmp + os.replace) in one format;
- upsert_hub(doc, hub, owner): add or replace one hub by id, keeping its place in the list;
- replace_owned(doc, owner, hubs, legacy_ids=()): a generator's whole set - removes the hubs it owns (and untagged ones
  with the same ids or in legacy_ids), then puts its hubs first, in its order; everything else is kept.
"""
import json
import os
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WORLD = os.path.join(ROOT, "src", "assets", "data", "initial_world_state.json")
OWNER_KEY = "world_state_owner"


def load(path=WORLD):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def dumps(doc):
    return json.dumps(doc, ensure_ascii=False, indent=2) + "\n"


def save(doc, path=WORLD):
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        f.write(dumps(doc))
        f.flush()
        os.fsync(f.fileno())
    os.replace(tmp, path)


class _Lock:
    def __init__(self, path, timeout_s=120.0):
        self.path, self.timeout_s, self.fd = path + ".lock", timeout_s, None

    def __enter__(self):
        start = time.time()
        while True:
            try:
                self.fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                os.write(self.fd, str(os.getpid()).encode())
                return self
            except FileExistsError:
                if time.time() - start > self.timeout_s:
                    raise SystemExit(f"{self.path}: locked for {self.timeout_s:.0f} s (a crashed writer? delete the lock)")
                time.sleep(0.05)

    def __exit__(self, *exc):
        os.close(self.fd)
        os.remove(self.path)


def update(fn, path=WORLD):
    """read -> fn(doc) -> atomic write, under the lock; returns fn's result"""
    with _Lock(path):
        doc = load(path)
        result = fn(doc)
        save(doc, path)
        return result


def upsert_hub(doc, hub, owner):
    hub = dict(hub)
    hub[OWNER_KEY] = owner
    hubs = doc.setdefault("orbital_hubs", [])
    for k, h in enumerate(hubs):
        if h.get("id") == hub["id"]:
            hubs[k] = hub
            return
    hubs.append(hub)


def _keep_attached_modules(new_hub, old_hub):
    """a generator's hub keeps the modules another tool ATTACHED to it (marked "generated", e.g. the city of
    hub_belt_world.py --attach): the generator does not produce them and must not erase them"""
    old_modules = (old_hub or {}).get("dock_modules") or []
    new_modules = list(new_hub.get("dock_modules") or [])
    have = {m.get("id") for m in new_modules}
    attached = [m for m in old_modules if "generated" in m and m.get("id") not in have]
    if attached:
        new_hub = dict(new_hub, dock_modules=new_modules + attached)
    return new_hub


def replace_owned(doc, owner, hubs, legacy_ids=()):
    old = {h.get("id"): h for h in doc.get("orbital_hubs", [])}
    hubs = [_keep_attached_modules(h, old.get(h["id"])) for h in hubs]
    new_ids = {h["id"] for h in hubs}
    legacy = set(legacy_ids)
    keep = [h for h in doc.get("orbital_hubs", [])
            if not (h.get(OWNER_KEY) == owner or
                    (OWNER_KEY not in h and (h.get("id") in new_ids or h.get("id") in legacy)) or
                    h.get("id") in new_ids)]
    doc["orbital_hubs"] = [dict(h, **{OWNER_KEY: owner}) for h in hubs] + keep
