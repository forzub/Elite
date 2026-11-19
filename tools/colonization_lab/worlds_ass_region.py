"""The World's Ass region 3126 (offline, deterministic, stdlib only).

Called by world_sites.py right after outposts.py. Input: canon/worlds_ass_region.json (PROPOSAL: factions, sites,
the lost generation ship, ambush / patrol zones). It adds, never changes:

  - in-system manned places (pirate lairs, a rogue-military anchorage, a military post, labs) to the outposts layer
    (visible_sites / hidden_sites and the labs / military / pirates lists), in the shape outposts.site_record makes,
    so the runtime export, the law model, economy_live and fuel_stations read them unchanged;
  - deep-space places (a pirate lair at the field rim, a classified lab on the search line) as region records with
    an ICRS position (hidden / classified ones are not exported to the Galaxy Map);
  - wrecks (science / military ships, one wreck field) with year, cause, what is left, and placement;
  - the lost generation ship «Парамита» (NIRVANA-1's detached seed module): AUTHOR_TRUTH position computed from the
    braking physics (direction = Kepler-62 in ICRS, distance = v_rel x time since separation); the world only has
    search hints.

Ids: pir.* / mil.* / lab.* / wreck.* / gen.* (unique with the outposts ids; checked).
"""
import json
import math
import os

import outposts as op

HERE = os.path.dirname(os.path.abspath(__file__))
CANON = os.path.join(HERE, "canon", "worlds_ass_region.json")
TAG = "PROPOSAL"
AU_LY = op.AU_KM / op.LY_KM
KIND_TO_TYPE = {"pirate": "pirate_enclave", "rogue_military": "pirate_enclave", "military_outpost": "military_hub", "lab": "science_hub"}
KIND_TO_OUTPOST = {"pirate": "pirate_settlement", "rogue_military": "pirate_settlement", "military_outpost": "military_outpost", "lab": "lab"}
CLASS_RU = {"science": "научный", "military": "военный", "mixed": "смешанный"}
KNOWN_RU = {"public": "известен", "partial": "частично", "rumour": "слухи", "hidden": "скрыт", "classified": "засекречен", "unknown_position": "место неизвестно"}


def load():
    with open(CANON, encoding="utf-8") as f:
        return json.load(f)


def unit(v):
    n = op.vlen(v)
    return [x / n for x in v]


def radec_unit(ra_deg, dec_deg):
    ra, dec = math.radians(ra_deg), math.radians(dec_deg)
    return [math.cos(dec) * math.cos(ra), math.cos(dec) * math.sin(ra), math.sin(dec)]


def perp(u):
    """A fixed unit vector perpendicular to u (deterministic: the one in the plane of u and the ICRS pole)."""
    z = [0.0, 0.0, 1.0]
    c = [u[1] * z[2] - u[2] * z[1], u[2] * z[0] - u[0] * z[2], u[0] * z[1] - u[1] * z[0]]
    return unit(c)


# ---------------------------------------------------------------- the lost generation ship
def paramita(I, X, G):
    """AUTHOR_TRUTH position of the detached seed module: the fold keeps the velocity direction (Kepler-62, ICRS);
    the module left 0.07 year before the end of braking and burned forward."""
    ph = G["physics"]
    lot = X.dso[G["parent_site"]]
    axis = radec_unit(*ph["kepler62_radec_deg"])
    years = I.WS["epoch_year"] - ph["separation_year"]
    dist_km = ph["v_rel_km_s"] * years * op.SECONDS_Y
    dist_ly = dist_km / op.LY_KM
    pos = [op.r3(x) for x in op.vadd(lot["position_ly"], op.vmul(axis, dist_ly))]
    pos_full = op.vadd(lot["position_ly"], op.vmul(axis, dist_ly))
    vel = [round(a + ph["v_rel_km_s"] * b, 3) for a, b in zip(lot.get("velocity_km_s", [0.0, 0.0, 0.0]), axis)]
    # the braking check: 0.08 c stopped in (to - from) years
    dec = ph["braking"]["from_km_s"] * 1e3 / ((ph["braking"]["to_year"] - ph["braking"]["from_year"]) * op.SECONDS_Y)
    v_sep = dec * (ph["braking"]["to_year"] - ph["separation_year"]) * op.SECONDS_Y / 1e3
    d19 = op.vlen(op.vsub(pos_full, I.pos[19]))
    d41 = op.vlen(op.vsub(pos_full, I.pos[41]))
    return dict(axis_icrs=[round(x, 6) for x in axis], distance_from_lotus_ly=round(dist_ly, 4), distance_from_lotus_au=round(dist_km / op.AU_KM),
                drift_au_per_year=round(ph["v_rel_km_s"] * op.SECONDS_Y / op.AU_KM, 1), position_ly=pos, velocity_km_s=vel,
                braking_decel_m_s2=round(dec, 4), v_at_separation_check_km_s=round(v_sep, 1), distance_to_19_ly=round(d19, 2), distance_to_41_ly=round(d41, 2),
                cone_half_angle_deg=3.0, search_range_ly=[0.1, 1.2], frame="ICRS_heliocentric", epoch_year=I.WS["epoch_year"]), axis, pos_full


def deep_place(I, X, s, axis, gen_pos):
    """ICRS position (ly) and velocity (km/s) of a deep-space region record."""
    lot = X.dso["dso.lotus"]
    if "near" in s:
        ref = X.dso[s["near"]]
        p = op.vadd(ref["position_ly"], op.vmul(s["offset_au"], AU_LY))
        return p, list(ref.get("velocity_km_s", [0.0, 0.0, 0.0])), dict(near=s["near"], offset_au=s["offset_au"])
    if "line_ly" in s:
        if s["line_ly"] == "computed":
            return gen_pos, None, dict(on_axis="kepler62")
        p = op.vadd(lot["position_ly"], op.vmul(axis, s["line_ly"]))
        if s.get("lateral_au"):
            p = op.vadd(p, op.vmul(perp(axis), s["lateral_au"] * AU_LY))
        return p, list(lot.get("velocity_km_s", [0.0, 0.0, 0.0])), dict(on_axis="kepler62", from_lotus_ly=s["line_ly"], lateral_au=s.get("lateral_au", 0.0))
    a, b = s["between"]
    pa = I.pos[a] if isinstance(a, int) else X.dso[a]["position_ly"]
    pb = I.pos[b] if isinstance(b, int) else X.dso[b]["position_ly"]
    u = unit(op.vsub(pb, pa))
    p = op.vadd(pa, op.vmul(op.vsub(pb, pa), s["fraction"]))
    p = op.vadd(p, op.vmul(perp(u), s.get("lateral_ly", 0.0)))
    return p, [0.0, 0.0, 0.0], dict(between=s["between"], fraction=s["fraction"], lateral_ly=s.get("lateral_ly", 0.0))


def in_system_place(I, X, s):
    if s.get("corridor"):
        place, star = op.corridor_place(X, s["system"], s["corridor"])
        return star, place
    if s.get("surface"):
        return s["body"], dict(surface=X.surface_latlon(I, s["body"], s["id"]))
    return s["body"], dict(orbit=dict(s["orbit"]))


def staff_of(I, s):
    if s.get("forces"):
        crew = sum(n * I.SE["ship_classes"][k]["crew"] for k, n in s["forces"].items())
        return s.get("base_staff", 0) + crew
    return s["staff"]


def source_of(X, sid, system, polity, hubs):
    if sid and sid in hubs:
        h = hubs[sid]
        return dict(id=h["id"], name=h["name"], system=h.get("system")), 0.0
    if sid and sid in X.dso:
        return dict(id=sid, name=X.dso[sid]["name"], system=None), 0.0
    return X.nearest_market(system, polity)


# ---------------------------------------------------------------- compute
def compute(I, out, g, base_site, surface_latlon, O):
    C = load()
    X = op.Ctx(I, out, g, base_site, surface_latlon)
    fac = {f["id"]: f for f in C["factions"]}
    hubs = {s["id"]: s for s in X.S}
    G = C["generation_ship"]
    gen, axis, gen_pos = paramita(I, X, G)
    manned, deep, wrecks = [], [], []
    for s in C["sites"]:
        k = s["kind"]
        f = fac.get(s.get("faction"), {})
        if k in ("wreck", "wreck_field", "generation_ship"):
            continue
        staff = staff_of(I, s)
        polity = s.get("polity")
        operator = s.get("operator") or f.get("name")
        if s.get("deep"):
            p, vel, how = deep_place(I, X, s, axis, gen_pos)
            src, ly = source_of(X, s.get("supply_via"), None, polity, hubs)
            extra = {"science_kit": X.C["labs"]["science_kit_t_day"]} if k == "lab" else None
            sup = op.supply(X, staff, False, src, extra, ly or 0.0)
            if k == "pirate":
                sup["note"] = "покупают через скупщиков и подставные компании; часть — краденое"
            deep.append(dict(id=s["id"], name=s["name"], kind=KIND_TO_OUTPOST[k], region_kind=k, faction=s.get("faction"), faction_name=f.get("name"),
                             polity=polity, operator=operator, staff=staff, ships=s.get("ships"), legal=s["legal"], known_to_player=s["known_to_player"],
                             form=s.get("form"), interest=s.get("interest"), position_ly=[op.r3(x) for x in p], velocity_km_s=vel, placement=how,
                             frame="ICRS_heliocentric", reason=s["reason"], lore=s.get("lore"), supply=sup,
                             assembly="lab_free_floating" if k == "lab" else "pirate_settlement", status=TAG))
            continue
        body, place = in_system_place(I, X, s)
        r = dict(id=s["id"], name=s["name"], kind=KIND_TO_OUTPOST[k], system=s["system"], body=body, placement=place, polity=polity, operator=operator,
                 staff=staff, legal=s["legal"], known_to_player=s["known_to_player"], reason=s["reason"],
                 on_ice=bool(s.get("surface")) and I.bodies.get(body, {}).get("kind") == "dwarf_planet")
        if k == "lab":
            r.update(form=s["form"], form_ru=op.FORM_RU[s["form"]], interest=s.get("interest"), assembly="lab_" + s["form"])
        elif k == "military_outpost":
            r.update(forces=s["forces"], role="гидрографическая партия (поиск «второго корпуса»)", assembly="military_outpost")
        else:
            r.update(ships=s.get("ships") or sum((s.get("forces") or {}).values()), size="малое", assembly="pirate_settlement",
                     role="непризнанный отряд флота (каперы)" if k == "rogue_military" else None)
            if s.get("forces"):
                r["forces"] = s["forces"]
        src, ly = source_of(X, s.get("supply_via"), s["system"], polity, hubs)
        extra = {}
        if k == "lab":
            extra["science_kit"] = X.C["labs"]["science_kit_t_day"]
        if s.get("forces"):
            extra["weapons"] = 0.02 * sum(s["forces"].values())
            extra["jump_charge"] = 0.05 * sum(s["forces"].values())
        r["supply"] = op.supply(X, staff, r["on_ice"], src, extra, ly or 0.0)
        if k in ("pirate", "rogue_military"):
            r["supply"]["note"] = "покупают через скупщиков и подставные компании; часть — краденое"
        r["faction"], r["region_kind"], r["lore"] = s.get("faction"), k, s.get("lore")
        manned.append(r)
    for s in C["sites"]:
        if s["kind"] not in ("wreck", "wreck_field"):
            continue
        rec = dict(id=s["id"], name=s["name"], kind=s["kind"], wreck_class=s["class"], wreck_class_ru=CLASS_RU[s["class"]], known_to_player=s["known_to_player"],
                   cause=s["cause"], left=s["left"], salvage=s.get("salvage"), reason=s["reason"], status=TAG)
        for key in ("ship_class", "hull", "owner", "died_year", "crew", "survivors", "wrecks", "died_years"):
            if s.get(key) is not None:
                rec[key] = s[key]
        if s.get("deep"):
            p, vel, how = deep_place(I, X, s, axis, gen_pos)
            rec.update(system=None, position_ly=[op.r3(x) for x in p], velocity_km_s=vel, placement=how, frame="ICRS_heliocentric")
        else:
            body, place = in_system_place(I, X, s)
            rec.update(system=s["system"], system_name=op.sysname(I, s["system"]), body=body, placement=place)
        wrecks.append(rec)
    gsite = next(s for s in C["sites"] if s["kind"] == "generation_ship")
    gen_rec = dict(id=gsite["id"], name=gsite["name"], kind="generation_ship", parent=G["parent"], parent_site=G["parent_site"],
                   known_to_player=gsite["known_to_player"], reason=gsite["reason"], what=G["what"], hull=G["hull"], world_names=G["world_names"],
                   history=G["history"], physics=G["physics"], truth=gen, search_hints=G["search_hints"], value=G["value"], crew_note=G["crew_note"],
                   knowledge=dict(status=G["status"], visibility=G["visibility"], basis=G["basis"]), status=TAG)
    # into the outposts layer (in-system manned places only: they have a body and a system)
    vis, hid = [], []
    for r in manned:
        typ = KIND_TO_TYPE[r["region_kind"]]
        rec = op.site_record(X, r, typ)
        rec["faction"], rec["region"] = r["faction"], "frontier.worlds_ass"
        if r["region_kind"] in ("pirate", "rogue_military"):
            rec["map_level"] = "none"
            hid.append(rec)
        else:
            vis.append(rec)
        {"lab": O["labs"], "military_outpost": O["military"]}.get(r["kind"], O["pirates"]).append(r)
    O["visible_sites"] += vis
    O["hidden_sites"] += hid
    # summary
    by_kind = {}
    for r in manned + deep:
        by_kind[r["region_kind"]] = by_kind.get(r["region_kind"], 0) + 1
    for w in wrecks:
        by_kind[w["kind"]] = by_kind.get(w["kind"], 0) + 1
    by_kind["generation_ship"] = 1
    wc = {}
    for w in wrecks:
        if w["kind"] == "wreck":
            wc[w["wreck_class"]] = wc.get(w["wreck_class"], 0) + 1
        else:
            for c, n in w["wrecks"].items():
                wc[c] = wc.get(c, 0) + n
    return dict(schema="elite.worlds_ass_region_3126.v1", status=TAG, generated_by="tools/colonization_lab/worlds_ass_region.py",
                region=C["region"], factions=C["factions"], manned_in_system=manned, deep_space=deep, wrecks=wrecks, generation_ship=gen_rec,
                zones=C["zones"], heat=C["heat"], supersedes_proposal=C["supersedes_proposal"],
                summary=dict(by_kind=dict(sorted(by_kind.items())), hulls_by_class=dict(sorted(wc.items())), visible_added=len(vis), hidden_added=len(hid),
                             deep_space=len(deep), staff=sum(r["staff"] for r in manned + deep)))


def runtime_rows(R):
    """Runtime copy for sites_3126.json: wrecks as future derelict objects (no spawner yet) - known ones only;
    hidden / classified wrecks and the generation ship's true position stay in out/ (AUTHOR_TRUTH)."""
    rows = []
    for w in R["wrecks"]:
        if w["known_to_player"] in ("hidden", "classified"):
            continue
        row = {k: w[k] for k in ("id", "name", "kind", "wreck_class", "ship_class", "hull", "owner", "died_year", "known_to_player", "cause", "left", "salvage", "wrecks")
               if w.get(k) is not None}
        if w.get("system") is not None:
            row.update(system_id=w["system"], parent_body_id=w["body"], placement=w["placement"])
        else:
            row.update(position_ly=w["position_ly"], velocity_km_s=w["velocity_km_s"], frame=w["frame"])
        rows.append(row)
    return rows


def checks(I, R, O):
    res = []

    def add(name, ok, detail):
        res.append(dict(check="worlds_ass." + name, ok=bool(ok), detail=detail))
    ids = [r["id"] for r in R["manned_in_system"] + R["deep_space"] + R["wrecks"]] + [R["generation_ship"]["id"]]
    other = [s["id"] for s in O["visible_sites"] + O["hidden_sites"]] + [d["id"] for d in O["deep_labs"]] + [b["id"] for b in O["beacons"]["in_system"] + O["beacons"]["route"]]
    dup = sorted({x for x in ids if ids.count(x) > 1 or other.count(x) > (1 if x in [r["id"] for r in R["manned_in_system"]] else 0)})
    add("ids_unique", not dup, ",".join(dup) or f"{len(ids)} ids")
    bad = [w["id"] for w in R["wrecks"] + R["manned_in_system"] if w.get("system") is not None and w["body"] not in I.bodies and w["body"] not in I.reservoirs and str(w["body"]).count(".") != 1]
    add("bodies_exist", not bad, ",".join(bad) or "ok")
    nocause = [w["id"] for w in R["wrecks"] if not (w.get("cause") and w.get("left") and (w.get("died_year") or w.get("died_years")))]
    add("wrecks_have_year_cause_remains", not nocause, ",".join(nocause) or f"{len(R['wrecks'])} обломков")
    G = R["generation_ship"]["truth"]
    add("lost_ship_outside_field_and_in_region", G["distance_from_lotus_au"] > 1.0 and max(G["distance_to_19_ly"], G["distance_to_41_ly"]) < 32.0,
        f"{G['distance_from_lotus_ly']} св. лет от «Лотоса» ({G['distance_from_lotus_au']} а.е.); до HD 102195 {G['distance_to_19_ly']}, до HD 104067 {G['distance_to_41_ly']} св. лет")
    ph = R["generation_ship"]["physics"]
    add("braking_physics_consistent", abs(G["v_at_separation_check_km_s"] - ph["v_at_separation_km_s"]) < 0.05 * ph["v_at_separation_km_s"]
        and abs(ph["v_at_separation_km_s"] + ph["own_burn_km_s"] - ph["v_rel_km_s"]) < 1e-6,
        f"торможение {G['braking_decel_m_s2']} м/с², при отделении {G['v_at_separation_check_km_s']} км/с, v_rel {ph['v_rel_km_s']} км/с")
    nohint = len(R["generation_ship"]["search_hints"]) < 5 or not any(h["known"] == "public" for h in R["generation_ship"]["search_hints"])
    add("lost_ship_has_public_search_hints", not nohint, f"{len(R['generation_ship']['search_hints'])} подсказок")
    k = R["summary"]["by_kind"]
    hot = k.get("pirate", 0) + k.get("rogue_military", 0) + k.get("military_outpost", 0) >= 4 and k.get("lab", 0) >= 3 and k.get("wreck", 0) >= 6
    wc = R["summary"]["hulls_by_class"]
    add("hot_region_mix", hot and wc.get("science", 0) + wc.get("military", 0) > 2 * wc.get("civil", 0), json.dumps(k, ensure_ascii=False) + " " + json.dumps(wc, ensure_ascii=False))
    nosup = [r["id"] for r in R["manned_in_system"] + R["deep_space"] if not (r.get("supply") or {}).get("source")]
    add("every_manned_place_has_supply_source", not nosup, ",".join(nosup) or "ok")
    fac = {f["id"] for f in R["factions"]}
    unk = sorted({r.get("faction") for r in R["manned_in_system"] + R["deep_space"] if r.get("faction") not in fac})
    add("factions_known", not unk, ",".join(map(str, unk)) or f"{len(fac)} фракций")
    return res
