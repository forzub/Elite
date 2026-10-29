"""Rebuild world/star_systems localization files for the physical catalog.

usage: python build_localization.py GALAXY_DETAILS_ROOT LOCALIZATION_STAR_SYSTEMS_DIR

One file per system keeps its file name, system names and hubs. Body keys are
"<system_id>:<body_id>" for every star, planet, dwarf planet, moon and map
belt of the regenerated catalog:
- an existing translation of the same id is kept (stars, all Sol bodies);
- a procedural designation "<prefix> <suffix>" is translated by replacing the
  prefix with the localized name of its star / planet (no guessing by
  similar names);
- reservoir names use a translated kind phrase; Sol's renamed belt keys move
  by kind (asteroid belt = legacy belt_0, Kuiper belt = legacy belt_1).
Keys of bodies that no longer exist are dropped.
"""
import glob
import json
import os
import sys

LOCALES = ("en", "ru", "zh-Hans", "es", "ja")
RESERVOIR_WORDS = {
    "asteroid_belt": {"en": "{p} asteroid belt", "ru": "Пояс астероидов {p}", "zh-Hans": "{p}小行星带",
                      "es": "Cinturón de asteroides de {p}", "ja": "{p}小惑星帯"},
    "kuiper_belt": {"en": "{p} outer belt", "ru": "Внешний пояс {p}", "zh-Hans": "{p}外带",
                    "es": "Cinturón exterior de {p}", "ja": "{p}外縁帯"},
    "debris_disk": {"en": "{p} debris disk", "ru": "Диск обломков {p}", "zh-Hans": "{p}碎屑盘",
                    "es": "Disco de escombros de {p}", "ja": "{p}デブリ円盤"},
}
SOL_BELT_LEGACY = {"system_0.Sol.asteroid_belt": "0:system_0.barycenter.belt_0",
                   "system_0.Sol.kuiper_belt": "0:system_0.barycenter.belt_1"}


def load(p):
    return json.load(open(p, encoding="utf-8"))


def main(root, loc_dir):
    files = {}
    for p in glob.glob(os.path.join(loc_dir, "*.json")):
        d = load(p)
        files[str(d["system_id"])] = (p, d)
    written = 0
    for sub in ("systems", "distant_systems"):
        for sysdir in sorted(glob.glob(os.path.join(root, sub, "*", ""))):
            sysdoc = load(os.path.join(sysdir, "system.json"))
            gen = load(os.path.join(sysdir, "generated", "system.json"))
            sid = str(sysdoc["id"])
            path, old = files.get(sid, (None, None))
            if old is None:
                raise SystemExit(f"no localization file for system {sid}")
            old_bodies = old.get("bodies", {})
            bodies = {}
            # Localized names by English name, for prefix substitution.
            names_by_english = {}

            def keep(key, english):
                tr = dict(old_bodies.get(key, {}))
                tr.setdefault("en", english)
                bodies[key] = tr
                names_by_english[english] = tr
                return tr

            def derived(key, english):
                tr = {"en": english}
                for prefix in sorted(names_by_english, key=len, reverse=True):
                    if english == prefix or english.startswith(prefix + " "):
                        suffix = english[len(prefix):]
                        for loc, name in names_by_english[prefix].items():
                            tr[loc] = name + suffix
                        tr["en"] = english
                        break
                if key in old_bodies:
                    tr.update(old_bodies[key])
                bodies[key] = tr
                names_by_english[english] = tr

            for s in gen["stars"]:
                keep(f"{sid}:{s['id']}", s["name"])
            # Circumbinary prefixes "<base> (AB)": the base is the first star name.
            for s in gen["stars"]:
                base = s["name"][:-2] if len(s["name"]) > 2 and s["name"][-2] == " " else None
                if base and base not in names_by_english:
                    tr = names_by_english[s["name"]]
                    names_by_english[base] = {loc: (n[:-2] if len(n) > 2 and n[-2] == " " else n)
                                              for loc, n in tr.items()}
            body_docs = [load(p) for p in glob.glob(os.path.join(sysdir, "generated", "bodies", "*.json"))]
            for b in sorted(body_docs, key=lambda b: (b["kind"] == "moon", b["id"])):
                derived(f"{sid}:{b['id']}", b["designation"])
            for r in sorted(glob.glob(os.path.join(sysdir, "generated", "reservoirs", "*.json"))):
                rd = load(r)
                if rd["kind"] not in RESERVOIR_WORDS:
                    continue
                key = f"{sid}:{rd['id']}"
                legacy_key = SOL_BELT_LEGACY.get(rd["id"])
                if legacy_key and legacy_key in old_bodies:
                    tr = dict(old_bodies[legacy_key])
                    tr.setdefault("ru", rd["name"])
                    bodies[key] = tr
                    continue
                prefix_en = rd["name"]
                for word in (" asteroid belt", " outer belt", " debris disk"):
                    if prefix_en.endswith(word):
                        prefix_en = prefix_en[: -len(word)]
                tr = {}
                pref = names_by_english.get(prefix_en, {"en": prefix_en})
                for loc in LOCALES:
                    if loc in pref or loc == "en":
                        tr[loc] = RESERVOIR_WORDS[rd["kind"]][loc].format(p=pref.get(loc, prefix_en))
                bodies[key] = tr
            new = dict(old)
            new["bodies"] = dict(sorted(bodies.items()))
            with open(path, "w", encoding="utf-8", newline="\n") as f:
                json.dump(new, f, ensure_ascii=False, indent=2)
                f.write("\n")
            written += 1
    print(f"localization files rebuilt: {written}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
