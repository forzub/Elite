"""Hub coverage of the inhabited world (author 2026-11-18: every hub is built «by id» - repeatable and recognizable).

  python tools/models/hub_coverage.py            -> docs/HUB_COVERAGE.md
  python tools/models/hub_coverage.py --list     -> the ids hub_colonies.py should build (one per line, see BUILDABLE)

Every inhabited site of the world catalog (src/assets/data/galaxy_details/world_sites/sites_3126.json: "sites" +
"hidden_sites", population > 0) and what stands for it in the game:
  city      an authored orbital hub (src/assets/data/initial_world_state.json) with its frozen city layout
            (src/assets/data/navigation/hub_cities/<hub>.json) - the generated city of hub_colonies.py / the Sol cities
  hub       an authored hub without a city layout (a command station only)
  station   no authored hub; the server spawns a plain command station for it (GameServer::materializeWorldSiteHubs:
            systems other than the start system, one per body - the body's most populated site, an orbital one first)
  settle    a surface site: the procedural surface settlement (src/world/celestial/settlements/SurfaceSettlementLayout.h:
            entrance / domes / antenna / pads, or a rock passage on a small body)
  mine      a mining hub: the mine models' track (not built here)
  none      nothing in the game yet
"""
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SITES = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json")
WORLD = os.path.join(ROOT, "src", "assets", "data", "initial_world_state.json")
CITIES = os.path.join(ROOT, "src", "assets", "data", "navigation", "hub_cities")
OUT = os.path.join(ROOT, "docs", "HUB_COVERAGE.md")
START_SYSTEM = 0
# what hub_colonies.py builds: an orbital placement (a parent_orbit hub), not a mine (their own track), not a ship
# graveyard (a wreck field, not a city), not an outpost assembly (outposts.py: military outpost / lab / refuel station)
NOT_A_CITY = {"mining_hub": "mine track", "ship_graveyard": "a wreck field, not a city"}


def placement_of(s):
    pl = s.get("placement") or {}
    for k in ("surface", "orbit", "belt", "lagrange_point", "corridor", "heliocentric"):
        if k in pl:
            return k
    return "?"


def load():
    d = json.load(open(SITES, encoding="utf-8"))
    world = json.load(open(WORLD, encoding="utf-8"))
    hubs = {h["id"]: h for h in world["orbital_hubs"]}
    layouts = {f[:-5] for f in os.listdir(CITIES) if f.endswith(".json")} if os.path.isdir(CITIES) else set()
    sites = [dict(s, hidden=False) for s in d["sites"]] + [dict(s, hidden=True) for s in d.get("hidden_sites", [])]
    return d, sites, hubs, layouts


def hub_of(s, hubs):
    rb = s.get("runtime_binding") or ""
    if rb.startswith("initial_world_state.orbital_hubs["):
        return rb[len("initial_world_state.orbital_hubs["):-1]
    return s["id"] if s["id"] in hubs else None


def buildable(s, hubs):
    """-> None when hub_colonies.py can build this site's hub, else the reason it does not"""
    if not (s.get("population") or 0) > 0:
        return "uninhabited"
    if s.get("hidden"):
        return "hidden site (pirate / smuggler assembly)"
    if s["type"] in NOT_A_CITY:
        return NOT_A_CITY[s["type"]]
    if s.get("assembly"):
        return f"outpost assembly '{s['assembly']}'"
    if placement_of(s) != "orbit":
        return f"placement '{placement_of(s)}'"
    if hub_of(s, hubs):
        return "built"
    return None


def status(s, hubs, layouts, generated):
    h = hub_of(s, hubs)
    if h:
        return ("city", h) if h in layouts else ("hub", h)
    if s["type"] == "mining_hub":
        return ("mine", "")
    if s["id"] in generated:
        return ("station", "")
    if placement_of(s) == "surface" and not s.get("hidden"):
        return ("settle", "")
    return ("none", "")


def generated_stations(sites, hubs):
    """the server's plain stations: per body of a non-start system without an authored hub, its most populated site
    (an orbital one first) - the catalog side of GameServer::materializeWorldSiteHubs (the body's own validity - a
    real body with a radius and mu, not a belt reservoir - is approximated by the parent id: not *_belt / *.belt)"""
    authored_bodies = {(h["system_id"], h["parent_body_id"]) for h in hubs.values()}
    best = {}
    for s in sites:
        if s.get("hidden") or s["system_id"] == START_SYSTEM:
            continue
        body = s.get("parent_body_id") or ""
        if body.endswith("belt") or (s["system_id"], body) in authored_bodies:
            continue
        orbit = placement_of(s) == "orbit"
        cur = best.get(body)
        cur_orbit = cur is not None and placement_of(cur) == "orbit"
        if cur is None or (orbit and not cur_orbit) or (orbit == cur_orbit and (s.get("population") or 0) > (cur.get("population") or 0)):
            best[body] = s
    return {s["id"] for s in best.values()}


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    d, sites, hubs, layouts = load()
    inhabited = [s for s in sites if (s.get("population") or 0) > 0]
    if "--list" in sys.argv:
        for s in inhabited:
            if buildable(s, hubs) is None:
                print(s["id"])
        return
    names = {}
    for sy in d.get("systems", []):
        if isinstance(sy, dict):
            names[sy.get("system_id", sy.get("id"))] = sy.get("name")
    for s in sites:                       # the systems the catalog header does not name: from the body ids
        parts = (s.get("parent_body_id") or "").split(".")
        if len(parts) > 1 and not names.get(s["system_id"]):
            names[s["system_id"]] = parts[1].replace("_", " ")
    gen = generated_stations(inhabited, hubs)
    prof = json.load(open(os.path.join(HERE, "hub_profiles.json"), encoding="utf-8"))["hubs"]
    rows = []
    for s in inhabited:
        st, h = status(s, hubs, layouts, gen)
        rows.append((s, st, h))
    by = collections.Counter(st for _, st, _ in rows)
    by_type = collections.defaultdict(collections.Counter)
    for s, st, _ in rows:
        by_type[(s["type"] + (" (hidden)" if s.get("hidden") else ""), placement_of(s))][st] += 1
    cols = ["city", "hub", "station", "settle", "mine", "none"]
    out = ["# Hub coverage of the inhabited world",
           "",
           "Generated by `python tools/models/hub_coverage.py` from the world sites catalog "
           "(`src/assets/data/galaxy_details/world_sites/sites_3126.json`, `sites` + `hidden_sites`, population > 0), "
           "the authored hubs (`src/assets/data/initial_world_state.json`) and the frozen city layouts "
           "(`src/assets/data/navigation/hub_cities/`). Do not edit by hand - rerun the tool.",
           "",
           "Status: **city** = authored hub with its generated city (frozen layout); **hub** = authored hub without a city; "
           "**station** = no authored hub, the server spawns a plain command station for the site's body "
           "(`GameServer::materializeWorldSiteHubs`, non-start systems, one per body); **settle** = surface site, the "
           "procedural surface settlement (`SurfaceSettlementLayout.h`); **mine** = mining hub (the mine models' track); "
           "**none** = nothing in the game.",
           "",
           f"Inhabited sites: {len(inhabited)} ({sum(1 for s in inhabited if s.get('hidden'))} hidden). " +
           ", ".join(f"{c} {by[c]}" for c in cols) + ".",
           "",
           "## By type and placement",
           "",
           "| type | placement | " + " | ".join(cols) + " |",
           "|---|---|" + "---|" * len(cols)]
    for (t, pl), c in sorted(by_type.items()):
        out.append(f"| {t} | {pl} | " + " | ".join(str(c[k] or "") for k in cols) + " |")
    out += ["", "## By system", "", "| system | sites | " + " | ".join(cols) + " |", "|---|---|" + "---|" * len(cols)]
    by_sys = collections.defaultdict(collections.Counter)
    for s, st, _ in rows:
        by_sys[s["system_id"]][st] += 1
    for sid in sorted(by_sys):
        c = by_sys[sid]
        out.append(f"| {sid} {names.get(sid) or ''} | {sum(c.values())} | " + " | ".join(str(c[k] or "") for k in cols) + " |")
    out += ["", "## Not built as a city, and why", ""]
    why = collections.Counter(buildable(s, hubs) for s in inhabited)
    for k, v in why.most_common():
        if k not in (None, "built"):
            out.append(f"- {v}: {k}")
    # the surface settlements: what exists and what is missing
    surf = [s for s in inhabited if placement_of(s) == "surface" and not s.get("hidden")]
    by_kind = collections.Counter(s["type"] for s in surf)
    unreachable = [s for s in surf if not s["id"].startswith("site.")]
    big = sorted(surf, key=lambda s: -(s.get("population") or 0))[:8]
    out += ["", "## Surface settlements", "",
            f"{len(surf)} public surface sites (" + ", ".join(f"{k} {v}" for k, v in by_kind.most_common()) + "), "
            f"{sum(1 for s in inhabited if placement_of(s) == 'surface' and s.get('hidden'))} hidden ones (pirate / smuggler).",
            "",
            "What exists: `src/world/celestial/settlements/SurfaceSettlementLayout.h` lays out EVERY surface site from its id "
            "(server terrain flattening, landing autopilot pads, renderer): an entrance into the ground, 2-3 small domes, an "
            "antenna mast and 1-3 pads (a rock passage + pad on a body under 400 km). The same small layout for a 600-person "
            "lab and for a 1.2-billion agglomeration, and the same on airless and atmospheric bodies.",
            "",
            "Missing:",
            f"- a city of the hub kit on the ground (the author's decision 2026-11-18: «surface settlements = the same hubs on a "
            f"planet»): the planar generator's map (blocks / streets / zones / landmarks) placed on the terrain - the frozen "
            f"layout format already fits (the module y = 0 plane -> the local tangent plane at the site), what is missing is "
            f"the terrain side (flattening per block, the footprint union) and the size rule (the biggest: " +
            ", ".join(f"{(s.get('name') or s['id'])} {s.get('population') or 0:,}" for s in big[:5]) + ");",
            f"- {len(unreachable)} surface sites have ids not starting with `site.` (Earth agglomerations `city.000.*`, "
            f"plants `plant.*`, ...): `surfaceSettlementBody()` parses `site.%d` only, so the landing autopilot / DEV start "
            f"cannot address them by id (fix: take the system id from the site's catalog entry instead of the id prefix);",
            "- the recognizable terrain classes the author asked for around settlements (canyon / rift, plain) - "
            "FeatureClass has MountainRange, ImpactCrater, Volcano, Lake, IceCap only;",
            "- an atmospheric-body style (Earth / Mars cities are drawn as airless-Moon entrances).",
            ]
    out += ["", "## Every inhabited site", "",
            "Profile = the city's generation profile (tools/models/hub_profiles.json): blocks N x M, shape, national style.",
            "", "| system | site id | name | type | placement | population | status | profile |", "|---|---|---|---|---|---|---|---|"]
    for s, st, h in sorted(rows, key=lambda r: (r[0]["system_id"], r[0]["id"])):
        p = prof.get(h) if h else None
        ps = f"{p['size'][0]}x{p['size'][1]} {p.get('shape', '')} {p.get('style', '')}".strip() if p else ""
        nm = (s.get("name") or "").replace("|", "/")
        out.append(f"| {s['system_id']} | `{s['id']}` | {nm} | {s['type']}{' (hidden)' if s.get('hidden') else ''} | "
                   f"{placement_of(s)} | {s.get('population') or 0} | {st}{(' `' + h + '`') if h and h != s['id'] else ''} | {ps} |")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    open(OUT, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print(f"{OUT}: {len(inhabited)} inhabited sites; " + ", ".join(f"{c} {by[c]}" for c in cols))


if __name__ == "__main__":
    main()
