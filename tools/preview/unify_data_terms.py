"""One English form per proper name across every data_terms table (the parallel translators chose differently).

    python tools/preview/unify_data_terms.py

Canonical choices: the form the game data / ids already use where one exists (frontier.worlds_ass, the data's own
"Blooming Harmony" en entry), else the majority."""
import glob
import json
import os
import re

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TABLES = os.path.join(ROOT, "src", "assets", "localization", "data")
CANON = [
    (r"Ass[- ]End of the World", "World's Ass"),
    (r"\bWarpstanis\b", "Varpstanis"),
    (r"\bDiscord Belt\b", "Belt of Discord"),
    (r"\bBlossoming Harmony\b", "Blooming Harmony"),
    (r"\bLal-Kot\b", "Lal Kot"),
    (r"\bHeavenly Stronghold\b", "Celestial Stronghold"),
    (r"\bSky Fortress\b", "Celestial Stronghold"),
    (r"\bNovo-Thebaid\b", "New Thebaid"),
    (r"\bNovo-Fivaida\b", "New Thebaid"),
    # hub_modules_b vs the map's own UI strings (map_messages.json) and the other tables
    (r"\bPacific\b", "Tikhiy"),
    (r"\bNew Shanghai\b", "Xin Shanghai"),
    (r"\bStrife Belt\b", "Belt of Discord"),
    (r"\bFuel, repair, holding\b", "Refuelling, repair, holding"),
    (r"\bmedbay\b", "medical bay"),
    (r"\bquarantine block\b", "quarantine ward"),
    (r"\bbreaking yard\b", "salvage yard"),
    (r"\bslipway\b", "shipyard slip"),
    (r"\bground spaceport\b", "surface spaceport"),
    (r"\bresearch hub\b", "science hub"),
    (r"\bOFU\b", "Orbital Freight Union"),
    (r"\bKern Prospectorate\b", "Kern Prospectorate"),
]
changed = 0
for p in sorted(glob.glob(os.path.join(TABLES, "*.json"))):
    j = json.load(open(p, encoding="utf-8"))
    n = 0
    for k, v in j.get("terms", {}).items():
        en = v.get("en", "")
        new = en
        for pat, rep in CANON:
            new = re.sub(pat, rep, new)
        if new != en:
            v["en"] = new
            n += 1
    if n:
        json.dump(j, open(p, "w", encoding="utf-8", newline="\n"), ensure_ascii=False, indent=1)
    print(f"{os.path.basename(p)}: {n} entries unified")
    changed += n
print("total", changed)
