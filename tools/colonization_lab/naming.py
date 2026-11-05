"""Naming layer 3126: the single reader of canon/naming.json.

Identity is a machine id (unit id, system:<StarAtlas id>, star:<HYG id>, body:..., site:<settlement id or
runtime site id>, mission:<code>); a name is a property of it. Every other place that shows a name is a
MIRROR written by `--write`:

  canon/world.json            colonies[].name / .polity, colony asset names
  canon/political_order.json  actors[].ru (+ government / short where present)
  canon/settlements.json      capital and named site names
  src/assets/data/galaxy/actors.json   legacy display fields (name, selfName, altNames, nameOrigin)
  src/assets/data/naming/names_3126.json  runtime resolver table (id -> native / ru / en / ...)

plus the accessibility model of Fortuna (Fomalhaut), computed from the real HYG position, the 3126
beacon network (world.json), the jump calibration (out/logistics.json) and the jump-error model
(world_economy.JUMP_ERROR).

Usage:  python naming.py --write    mirrors + out/world_3126/naming_3126.json
        python naming.py --check    recompute, compare mirrors and outputs, run the naming tests (no writes)
        python naming.py --lookup TEXT
"""

import argparse
import glob
import json
import math
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CANON = os.path.join(HERE, "canon")
OUT = os.path.join(HERE, "out", "world_3126", "naming_3126.json")
RUNTIME_NAMES = os.path.join(ROOT, "src", "assets", "data", "naming", "names_3126.json")
LEGACY_ACTORS = os.path.join(ROOT, "src", "assets", "data", "galaxy", "actors.json")
GALAXY_DETAILS = os.path.join(ROOT, "src", "assets", "data", "galaxy_details")
HYG = os.path.join(ROOT, "src", "assets", "data", "galaxy", "real_star_catalog.json")
LEGACY_EXTRA = {9: ("gj832_gaia_zone", "polity"), 14: ("gj832_gaia_zone", "colony")}   # legacy actors without a 3126 actor
LY_PER_PC = 3.261564
FOMALHAUT_ID = 61
LEXIE_BODY = "system_61.Fomalhaut.Lexie"
LEXIE_ENTITY = "body:" + LEXIE_BODY
YEAR = 3126


def _load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def _dump(d, indent=1, sort=False):
    return json.dumps(d, ensure_ascii=False, indent=indent, sort_keys=sort) + "\n"


def _cyr(s):
    return bool(re.search("[Ѐ-ӿ]", s or ""))


class Names:
    def __init__(self):
        self.N = _load(os.path.join(CANON, "naming.json"))
        self.E = self.N["entities"]
        self.units = [k for k, v in self.E.items() if v.get("type") == "unit"]

    # ---- parts
    def part(self, uid, which):
        return (self.E.get(uid) or {}).get(which)

    def colony_ru(self, uid):
        c = self.part(uid, "colony")
        return c["exonyms"]["ru"] if c else None

    def polity_ru(self, uid):
        p = self.part(uid, "polity")
        return p["exonyms"]["ru"] if p else None

    def polity_label_ru(self, uid):
        p = self.part(uid, "polity")
        return (p.get("label_ru") or p["exonyms"]["ru"]) if p else None

    def site_entity(self, sid):
        return self.E.get("site:" + sid)

    def site_name(self, sid, default, lang="ru"):
        e = self.site_entity(sid)
        return e["exonyms"].get(lang, default) if e else default

    # ---- generic resolver
    def block_of(self, eid):
        """(entity id) or 'colony:<uid>' / 'polity:<uid>' -> the names block."""
        if ":" in eid and eid.split(":", 1)[0] in ("colony", "polity"):
            kind, uid = eid.split(":", 1)
            return self.part(uid, kind)
        e = self.E.get(eid)
        if not e:
            return None
        if e.get("type") == "unit":
            return e.get("polity") or e.get("colony")
        return e.get("names") or e

    @staticmethod
    def native_of(b):
        for k in ("endonym", "international", "colony_era", "local"):
            if b.get(k):
                return b[k]
        cat = b.get("catalog_ids", {}).get("proper")
        return {"text": cat, "lang": "en"} if cat else None

    def resolve(self, eid, lang="ru"):
        b = self.block_of(eid)
        if b is None:
            return None
        if lang == "native":
            n = self.native_of(b)
            return n["text"] if n else None
        ex = b.get("exonyms", {})
        if lang in ex:
            return ex[lang]
        n = self.native_of(b)
        return n["text"] if n and n.get("lang") == lang else None

    def all_texts(self, b):
        """Every current name text in a block (endonym, formal, exonyms, aliases, translit)."""
        out = []
        for k in ("endonym", "formal", "short", "ceremonial", "liturgical", "international", "colony_era", "local", "traditional", "catalog"):
            v = b.get(k)
            if isinstance(v, dict):
                out += [v.get("text"), v.get("translit"), v.get("simplified")]
                out += [x for kk, x in v.items() if kk in ("ru", "en")]
        out += list(b.get("exonyms", {}).values()) + list(b.get("formal_exonyms", {}).values())
        out += [b.get("zh_translit")] + [a["text"] for a in b.get("aliases", [])]
        out += list(b.get("catalog_ids", {}).values())
        return [x for x in out if x]

    def lookup(self, text):
        t = text.strip().lower()
        hits = []
        for eid, e in self.E.items():
            blocks = [(eid, self.block_of(eid))]
            if e.get("type") == "unit":
                blocks = [(f"{k}:{eid}", e[k]) for k in ("colony", "polity") if e.get(k)]
            for bid, b in blocks:
                if b and any(t == x.lower() for x in self.all_texts(b)):
                    hits.append(bid)
        return sorted(set(hits))

    # ---- runtime table
    def runtime_table(self):
        rows = {}
        for eid, e in sorted(self.E.items()):
            blocks = [(eid, self.block_of(eid))]
            if e.get("type") == "unit":
                blocks = [(f"{k}:{eid}", e[k]) for k in ("colony", "polity") if e.get(k)]
            for bid, b in blocks:
                n = self.native_of(b) or {}
                r = dict(native=n.get("text"), native_lang=n.get("lang"), ru=self.resolve(bid, "ru"), en=self.resolve(bid, "en"))
                if n.get("translit"):
                    r["translit"] = n["translit"]
                if n.get("pron"):
                    r["pron"] = n["pron"]
                for lg, tx in sorted(b.get("exonyms", {}).items()):
                    if lg not in ("ru", "en"):
                        r[lg] = tx
                if bid.startswith("polity:"):
                    r["label_ru"] = self.polity_label_ru(eid)
                slang = [a["text"] for a in b.get("aliases", []) if a.get("kind") in ("slang", "hostile")]
                if slang:
                    r["slang"] = slang
                rows[bid] = r
        return dict(schema="elite.naming.runtime.v1", generated_by="tools/colonization_lab/naming.py (from canon/naming.json)", do_not_edit=True,
                    note="id -> имена. Отображение в игре: stable ID -> этот резолвер -> строка на языке интерфейса. colony:<unit> — столица / колония, polity:<unit> — полития.",
                    names=rows)


# ---------------------------------------------------------------- mirrors
def mirrors(NM):
    """Return {path: new_text} for every mirror file."""
    out = {}
    W = _load(os.path.join(CANON, "world.json"))
    for c in W["colonies"]:
        if c["id"] in NM.E:
            c["name"] = NM.colony_ru(c["id"])
            c["polity"] = NM.polity_ru(c["id"])
            for a in c.get("assets", []):
                a["name"] = NM.site_name(f"{c['id']}.{a['id']}", a["name"])
    out[os.path.join(CANON, "world.json")] = _dump(W)

    P = _load(os.path.join(CANON, "political_order.json"))
    for aid, a in P["actors"].items():
        if aid in NM.E:
            a["ru"] = NM.polity_label_ru(aid)
            pol = NM.part(aid, "polity")
            if "government" in a:
                a["government"] = pol.get("formal_exonyms", {}).get("ru", a["government"])
            if "short" in a:
                a["short"] = pol.get("short", {}).get("ru", a["short"])
    out[os.path.join(CANON, "political_order.json")] = _dump(P)

    S = _load(os.path.join(CANON, "settlements.json"))
    for uid, rows in S["polities"].items():
        for s in rows:
            if NM.site_entity(s["id"]):
                s["name"] = NM.site_name(s["id"], s["name"])
            elif s.get("role") == "capital" and uid in NM.E and NM.part(uid, "colony"):
                s["name"] = NM.colony_ru(uid)
    out[os.path.join(CANON, "settlements.json")] = _dump(S)

    PH = _load(os.path.join(CANON, "population_history.json"))
    for uid, v in PH["units"].items():
        if "author_name" in v and NM.colony_ru(uid):
            v["author_name"] = NM.colony_ru(uid)
    out[os.path.join(CANON, "population_history.json")] = _dump(PH, indent=2)

    A = _load(LEGACY_ACTORS)
    link = {}
    for aid, a in P["actors"].items():
        if isinstance(a.get("historical_actor"), int):
            link[a["historical_actor"]] = (aid, "polity")
    link.update(LEGACY_EXTRA)
    renamed = {f: eid[8:] for eid, e in NM.E.items() if eid.startswith("mission:") for f in e.get("former_ids", [])}
    for la in A["actors"]:
        if la.get("primaryShips"):
            la["primaryShips"] = [renamed.get(x, x) for x in la["primaryShips"]]
        if la.get("id") in link:
            uid, kind = link[la["id"]]
            b = NM.part(uid, kind)
            la["name"] = b["exonyms"]["ru"]
            if b.get("demonym", {}).get("ru"):
                la["selfName"] = b["demonym"]["ru"]
            alt = list(b.get("formal_exonyms", {}).get("ru", "") and [b["formal_exonyms"]["ru"]] or [])
            if kind == "polity" and NM.colony_ru(uid) and NM.colony_ru(uid) != la["name"]:
                alt.append(NM.colony_ru(uid))
            alt += [x["text"] for x in b.get("aliases", []) if x.get("lang") == "ru" and not x.get("kind", "").startswith("historical")]
            la["altNames"] = alt
            nh = b.get("naming_history") or {}
            la["nameOrigin"] = nh.get("reason") if nh.get("reason") and nh.get("reason") != "AUTHOR_UNSPECIFIED" else "UNKNOWN (имя автора; происхождение не задано)"
            la["naming"] = f"canon/naming.json → {kind}:{uid}"
            for k, v in (b.get("legacy_actor_profile") or {}).items():
                la[k] = v
    A["layer"] = ("ИСТОРИЧЕСКИЙ СЛОЙ ранней редакции мира (GalaxyDatabase, сейчас не загружается). CANON здесь только id и доказанная связь "
                  "с актором 3126 (political_order.json historical_actor). Поля name / selfName / altNames / nameOrigin — зеркало canon/naming.json "
                  "(пишет tools/colonization_lab/naming.py --write); остальные описания — легаси, не канон.")
    out[LEGACY_ACTORS] = _dump(A, indent=2)
    out[RUNTIME_NAMES] = _dump(NM.runtime_table(), indent=1, sort=True)
    return out


# ---------------------------------------------------------------- Fortuna accessibility
def hyg_star(hip):
    for s in _load(HYG)["stars"]:
        if s.get("hip") == hip:
            return s
    return None


def atlas_systems():
    rows = {}
    for p in glob.glob(os.path.join(GALAXY_DETAILS, "systems", "*", "system.json")):
        s = _load(p)
        q = s["position_ly"]
        rows[s["id"]] = dict(name=s["name"], pos=(q["x"], q["y"], q["z"]))
    return rows


def fortuna(NM):
    import world_economy as WE
    W = _load(os.path.join(CANON, "world.json"))
    L = _load(os.path.join(HERE, "out", "logistics.json"))
    sysrec = fomalhaut_system()
    star = hyg_star(113368)
    F = tuple(star["position_ly"])
    atlas = atlas_systems()
    beacons = {b["system"]: b for b in W["beacons"] if b["year"] <= YEAR}
    inhabited = {c["system"] for c in W["colonies"]} | {0}
    hop = max(h for y, h in W["max_hop_ly"] if y <= YEAR)
    dist = sorted((math.dist(F, v["pos"]), sid) for sid, v in atlas.items() if sid != FOMALHAUT_ID)
    near = [dict(system=sid, name=atlas[sid]["name"], ly=round(d, 2), beacon=sid in beacons, inhabited=sid in inhabited,
                 operator=(beacons.get(sid) or {}).get("operator")) for d, sid in dist[:8]]
    stage = next(n for n in near if n["beacon"])
    sol = round(math.dist(F, (0.0, 0.0, 0.0)), 2)
    mean = L["calibration"]["mean_jump_ly"]
    JE = WE.JUMP_ERROR

    def sigma_km(d_ly, lock, age_years):
        km = d_ly * 9.4607e12
        return km * JE["base_fraction"] * (1 + age_years * JE["ephemeris_age_factor_per_year"]) / JE["lock"][lock]

    err = [dict(from_=n, ly=d, ephemeris_age_years=a, lock=lk, sigma_km=round(sigma_km(d, lk, a)), box_3sigma_km=round(3 * sigma_km(d, lk, a)))
           for n, d in ((stage["name"], stage["ly"]), ("Sol", sol)) for a in (0, 50, 120) for lk in ("origin_only",)]
    ref = [dict(between="две системы с маяками", ly=round(mean, 2), sigma_km=round(sigma_km(mean, "both_ends", 0)))]
    tanks = {}

    def sp(v):
        return f"{v:,}".replace(",", " ")

    for cls, r in sorted(L["range_mean_jumps"].items()):
        rng = r["half"] * mean
        tanks[cls] = dict(range_half_load_ly=round(rng, 1), round_trip_from_stage_share=round(2 * stage["ly"] / rng, 2),
                          round_trip_from_sol_share=round(2 * sol / rng, 2))
    return dict(
        star=dict(id="star:HIP_113368", hyg=star["id"], hd=star["hd"], hip=star["hip"], gl=star["gl"], bf=star["bf"], name=star["name"],
                  constellation=star["constellation"], v_mag_catalog=star["visual_magnitude"], spectral_catalog=star["spectral_type"],
                  distance_ly_catalog=round(star["distance_ly"], 2), position_ly=[round(x, 3) for x in F]),
        catalog=dict(system_id=FOMALHAUT_ID, dir="src/assets/data/galaxy_details/systems/061_fomalhaut", atlas_visible=sysrec["atlas_visible"],
                     bodies=len(fomalhaut_bodies()), reservoirs=[r["name"] for r in fomalhaut_reservoirs()]),
        observations=sysrec["observational_reference"],
        visible_from_earth=dict(v_mag=sysrec["observational_reference"]["v_mag"]["value"], note="одна из ~20 ярчайших звёзд неба Земли; видна невооружённым глазом с юга и средних широт севера"),
        max_hop_ly_3126=hop,
        nearest=near,
        staging=stage,
        sol_direct_ly=sol,
        within_max_hop=dict(stage=stage["ly"] <= hop, sol=sol <= hop),
        commercial_unbeaconed_limit_ly=WE.UNBEACONED_MAX_LY,
        in_commercial_network=stage["ly"] <= WE.UNBEACONED_MAX_LY,
        destination_beacon=FOMALHAUT_ID in beacons,
        jump_error=err, jump_error_reference=ref,
        tanks=tanks,
        verdict=[
            f"одно плечо {stage['ly']} св. лет от {stage['name']} (маяк {stage['operator']}); дальше маяков нет; прямой прыжок с Сола — {sol} св. лет (в пределах {hop})",
            f"вне коммерческой сети: связь без маяка у торговли не длиннее {WE.UNBEACONED_MAX_LY} св. лет — нет рейсов, страховки и спасательной службы",
            f"на выходе нет маяка: ошибка выхода в {JE['lock']['both_ends'] / JE['lock']['origin_only']:.0f} раз больше, чем между двумя маяками, и растёт с возрастом эфемерид "
            f"(от {stage['name']}: 3σ {sp(err[0]['box_3sigma_km'])} км при свежих, {sp(err[2]['box_3sigma_km'])} км при эфемеридах 120-летней давности); "
            "поиск планеты и сближение — внутрисистемный перелёт",
            "в системе нет топлива, ремонта и SAR: обратный заряд и резерв везти с собой; заряд прыжка у Провидения — товар двойного назначения (лицензия)",
            f"топливо по текущей модели прыжка полёт не запрещает: Cobrett-класс делает круг от Провидения на {tanks['cobrett_i_n1']['round_trip_from_stage_share']:.2f} бака, "
            f"круг прямо от Сола — на {tanks['cobrett_i_n1']['round_trip_from_sol_share']:.2f} бака: без резерва на ошибку выхода и манёвры, поэтому с Сола нужен доп. бак или кэш",
        ])


def fomalhaut_system():
    return _load(os.path.join(GALAXY_DETAILS, "systems", "061_fomalhaut", "system.json"))


def fomalhaut_bodies():
    return [_load(p) for p in sorted(glob.glob(os.path.join(GALAXY_DETAILS, "systems", "061_fomalhaut", "generated", "bodies", "*.json")))]


def fomalhaut_reservoirs():
    return [_load(p) for p in sorted(glob.glob(os.path.join(GALAXY_DETAILS, "systems", "061_fomalhaut", "generated", "reservoirs", "*.json")))]


def lexie_body():
    return next(b for b in fomalhaut_bodies() if b["id"] == LEXIE_BODY)


def compute(NM):
    b = lexie_body()
    o, ph, th, at, ge, spn = b["orbit"], b["physical"], b["thermal"], b["atmosphere"], b["geophysics"], b["spin"]
    physical = dict(
        a_au=o["semi_major_axis_au"], e=o["eccentricity"], period_years=round(o["period_days"] / 365.25, 2),
        mass_earth=ph["mass_earth"], radius_km=round(ph["radius_km"]), surface_gravity_g=round(ph["surface_gravity_ms2"] / 9.80665, 2),
        instellation_earth=round(th["instellation_earth"], 3), equilibrium_temperature_k=round(th["equilibrium_temperature_k"]),
        surface_temperature_k=dict(mean=th["mean_surface_temperature_k"], min=round(th["min_surface_temperature_k"]), max=round(th["max_surface_temperature_k"])),
        atmosphere=dict(surface_pressure_bar=round(at["surface_pressure_pa"] / 1e5, 3), composition=at["composition"], breathable=b["extra"]["breathable"]),
        hydrosphere=ge["hydrosphere"], tectonics=ge["tectonic_regime"], volcanism=ge["volcanism"], magnetic_field_earth=round(ge["magnetic_field_earth"], 2),
        biosphere=ge["biosphere"], rotation_hours=spn["rotation_period_hours"], axial_tilt_deg=spn["obliquity_deg"],
        terrain=b["surface_generation"]["terrain_profile"], environment_preset=b["environment_preset_id"],
        neighbours=[f"{x['designation']} {x['orbit']['semi_major_axis_au']:.2f} а. е." for x in fomalhaut_bodies()
                    if x["kind"] == "planet" and x["id"] != LEXIE_BODY and abs(x["orbit"]["semi_major_axis_au"] - o["semi_major_axis_au"]) < 2.5])
    return dict(schema="elite.naming.v1", generated_by="tools/colonization_lab/naming.py", status=NM.N["status"],
                entities=len(NM.E), units=len(NM.units),
                verdicts={k: sum(1 for e in NM.E.values() for bb in ([e.get("colony"), e.get("polity")] if e.get("type") == "unit" else [e]) if bb and bb.get("verdict") == k)
                          for k in NM.N["verdicts"]},
                fortuna=fortuna(NM),
                lexie=dict(id=LEXIE_ENTITY, runtime_body_id=LEXIE_BODY, names={k: NM.resolve(LEXIE_ENTITY, k) for k in ("en", "ru", "zh-Hans")},
                           translit=NM.E[LEXIE_ENTITY]["names"]["zh_translit"], physical=physical))


# ---------------------------------------------------------------- checks
def _pat(n):
    if n.isascii():   # Latin forms: whole words only, machine ids (lalande_mecca, mecca_farms) are not display text
        return re.compile(r"(?<![\w.-])" + re.escape(n) + r"(?![\w])", re.I)
    return re.compile(r"(?<![\w-])" + re.escape(n))


class Retired:
    """Entity-aware ban of retired authoring names (canon/naming.json retired_authoring_names):
    unique names are banned anywhere in the current display layer; contextual words (the real Mecca, the
    Islamic world, historical caliphates) only in a line that also names a current Lalande 21185 entity,
    and in any name of a current entity."""

    def __init__(self, NM):
        r = NM.N["retired_authoring_names"]
        self.unique = [(n, _pat(n)) for n in r["unique"]]
        self.contextual = [(n, _pat(n)) for n in r["contextual"]]
        self.context = [_pat(n) for n in r["context"]]

    def text_hits(self, text):
        hits = [m.group(0) for _, rx in self.unique for m in [rx.search(text)] if m]
        for line in text.split("\n"):
            if any(c.search(line) for c in self.context):
                hits += [m.group(0) for _, rx in self.contextual for m in [rx.search(line)] if m]
        return hits

    def name_hits(self, name):
        return [m.group(0) for _, rx in self.unique + self.contextual for m in [rx.search(name or "")] if m]


DISPLAY_SCAN = [
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "economy", "*.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "*.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "generation_ships.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "actors.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "subactors.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "nodes.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "routes.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "galaxy", "systems.json")),
    ("runtime", os.path.join(ROOT, "src", "assets", "data", "initial_world_state.json")),
    ("lore", os.path.join(ROOT, "Notes", "Lore", "**", "*.md")),
    ("lore", os.path.join(ROOT, "Notes", "History", "**", "*.md")),
    ("lore", os.path.join(ROOT, "Notes", "Economy", "*.md")),
    ("lore", os.path.join(ROOT, "Notes", "fate.md")),
]


def checks(NM, mirror_texts=None):
    res = []

    def add(name, ok, detail):
        res.append(dict(check=name, ok=bool(ok), detail=detail))

    P = _load(os.path.join(CANON, "political_order.json"))
    W = _load(os.path.join(CANON, "world.json"))
    MS = _load(os.path.join(CANON, "missions.json"))
    S = _load(os.path.join(CANON, "settlements.json"))
    mids = {m["id"] for m in MS["missions"]}
    atlas = atlas_systems()
    # 1
    miss = [a for a in P["actors"] if a not in NM.E or not NM.part(a, "polity")]
    add("t01_actor_has_stable_id_and_names", not miss, f"{len(P['actors'])} акторов; без записи: {miss}")
    # 2 + 11
    bad = []
    for eid, e in NM.E.items():
        if eid.startswith("system:") and int(eid.split(":")[1]) not in atlas:
            bad.append(eid)
        for ref in [e.get("system"), e.get("star")]:
            if ref and ref not in NM.E and ref not in ("system:" + str(i) for i in atlas):
                if not (ref.startswith("system:") and ref[7:].isdigit() and int(ref[7:]) in atlas):
                    bad.append(f"{eid}->{ref}")
        if eid.startswith("mission:") and eid[8:] not in mids:
            bad.append(eid)
        for f in e.get("former_ids", []):
            if f in mids:
                bad.append(f"{eid}: former id {f} still a mission")
    site_ids = {s["id"] for rows in S["polities"].values() for s in rows}
    try:
        rt = _load(os.path.join(GALAXY_DETAILS, "world_sites", "sites_3126.json"))
        rt_ids = {s["id"] for s in rt["sites"]} | {"beacon." + b["id"] for b in _load(os.path.join(HERE, "out", "world_3126", "beacons_3126.json"))["bridge"]}
    except (OSError, KeyError):
        rt_ids = set()
    for eid in NM.E:
        if eid.startswith("site:") and eid[5:] not in site_ids and eid[5:] not in rt_ids:
            bad.append(eid)
    bodies = {b["id"] for b in fomalhaut_bodies()}
    bad += [eid for eid in NM.E if eid.startswith("body:") and eid[5:] not in bodies]
    add("t02_t11_ids_resolve", not bad, f"сущностей {len(NM.E)}; битые ссылки: {bad}")
    # 3
    if mirror_texts is not None:
        stale = []
        for p, txt in mirror_texts.items():
            try:
                cur = open(p, encoding="utf-8").read()
            except OSError:
                cur = None
            if cur != txt:
                stale.append(os.path.relpath(p, ROOT))
        add("t03_one_source_mirrors_in_sync", not stale, f"зеркал {len(mirror_texts)}; расходятся (перезапустите naming.py --write): {stale}")
    # 4
    bad = [f for c in W["colonies"] for f in c["founders"] if f not in mids]
    bad += [u for u in S["polities"] if u not in NM.E and u not in {o["id"] for o in W["outposts"] + W["frontier_pools"]}]
    bad += [a for a, v in P["actors"].items() if v.get("ru") != NM.polity_label_ru(a)]
    add("t04_cross_refs_by_id", not bad, f"основатели, поселения и акторы связаны по id; проблемы: {bad}")
    # 5 + 6
    bad = []
    for u in NM.units:
        for k in ("colony", "polity"):
            b = NM.part(u, k)
            if not b:
                continue
            if not (NM.native_of(b) and b["exonyms"].get("ru") and NM.resolve(f"{k}:{u}", "en")):
                bad.append(f"{k}:{u}")
            if k == "polity" and u != "proxima_outpost" and NM.native_of(b)["lang"] == "ru":
                bad.append(f"{u}: русский эндоним у нерусских основателей")
    add("t05_t06_resolver_native_ru_en_and_endonyms", not bad, f"единиц {len(NM.units)}; проблемы: {bad}")
    # 7 + 23: retired names, entity-aware
    RT = Retired(NM)
    hits = []
    files = 0
    for kind, g in DISPLAY_SCAN:
        for p_ in sorted(glob.glob(g, recursive=True)):
            files += 1
            h = RT.text_hits(open(p_, encoding="utf-8", errors="replace").read())
            if h:
                hits.append(f"{os.path.relpath(p_, ROOT)}: «{h[0]}»")
    table = NM.runtime_table()["names"].values()
    names = [r.get(k) for r in table for k in ("ru", "en", "native", "label_ru")]
    names += [x for r in table for x in r.get("slang", [])]
    names += [s_["name"] for rows in S["polities"].values() for s_ in rows]
    try:
        names += [h.get(k) for h in _load(os.path.join(ROOT, "src", "assets", "data", "economy", "hub_modules_3126.json"))["hubs"] for k in ("name", "owner_name")]
    except OSError:
        pass
    hits += [f"имя текущей сущности: «{n}»" for n in names if n and RT.name_hits(n)]
    add("t07_t23_no_retired_names_in_display_layer", not hits, f"файлов {files}, имён {len(names)}; найдено: {hits[:12]}{' …' if len(hits) > 12 else ''}")
    probe_fail = ["Тауранга, бывшая Новая Мекка", "столица Капеху — Мекка", "VAKA-MOANA-1 (прежде AL-QUDS-1)"]
    probe_pass = ["паломничество в Мекку на Земле в XXI веке", "не отстать от США, Китая и исламского мира", "исторические исламские халифаты Земли"]
    ok = all(RT.text_hits(x) for x in probe_fail) and not any(RT.text_hits(x) for x in probe_pass) and RT.name_hits("Новая Мекка")
    add("t07b_retired_names_entity_aware", ok, "старое имя Тауранги в текущей сущности — ловится; реальная Мекка / исламский мир Земли — допустимы")
    # 8
    bad = []
    for u in NM.units:
        for k in ("colony", "polity"):
            b = NM.part(u, k) or {}
            for a in b.get("aliases", []):
                if a.get("kind", "").startswith("historical") and a["text"] in b.get("exonyms", {}).values():
                    bad.append(f"{u}: «{a['text']}» и историческое, и текущее")
    add("t08_historical_names_not_current", not bad, f"{bad}")
    # 9
    try:
        hubs = _load(os.path.join(ROOT, "src", "assets", "data", "economy", "hub_modules_3126.json"))["hubs"]
        orphan = sorted({h["actor"] for h in hubs if h.get("actor") and f"polity:{h['actor']}" not in NM.runtime_table()["names"]})
        add("t09_runtime_hubs_resolve_by_actor_id", not orphan, f"хабов {len(hubs)}; акторы без имени в резолвере: {orphan}")
    except OSError as e:
        add("t09_runtime_hubs_resolve_by_actor_id", False, str(e))
    # 10
    add("t10_deterministic", _dump(compute(NM)) == _dump(compute(Names())), "два расчёта из канона дают одинаковый результат")
    # 12
    bad = []
    for eid, e in NM.E.items():
        blocks = [e.get("colony"), e.get("polity")] if e.get("type") == "unit" else [e.get("names") or e]
        for b in blocks:
            if not b:
                continue
            n = NM.native_of(b)
            if n and n["lang"] in b.get("exonyms", {}) and b["exonyms"][n["lang"]] != n["text"]:
                bad.append(f"{eid}: {n['lang']} «{n['text']}» ≠ «{b['exonyms'][n['lang']]}»")
    add("t12_one_official_name_per_language", not bad, f"{bad}")
    # 13
    st = NM.E["star:HIP_113368"]
    hy = hyg_star(113368)
    ok = (hy and hy["hd"] == 216956 and hy["name"] == "Fomalhaut" and hy["constellation"] == "PsA" and hy["bf"].endswith("Alp PsA")
          and all(NM.lookup(x) == ["star:HIP_113368"] for x in ("Fomalhaut", "α Piscis Austrini", "HD 216956", "HIP 113368", "Фомальгаут", "北落師門")))
    add("t13_fomalhaut_one_star_identity", ok, f"HYG {hy and hy['id']}: HD {hy and hy['hd']}, {hy and hy['bf']}, {hy and hy['constellation']} (Piscis Austrinus / Южная Рыба)")
    # 14 15
    add("t14_fortuna_is_fomalhaut_system", NM.lookup("Fortuna") == [f"system:{FOMALHAUT_ID}"] == NM.lookup("Фортуна") and NM.E[f"system:{FOMALHAUT_ID}"]["star"] == "star:HIP_113368",
        f"Fortuna / Фортуна → system:{FOMALHAUT_ID} (StarAtlas, atlas_visible) → star:HIP_113368")
    add("t15_mingyun_zhi_lun_is_fortuna", NM.lookup("命運之輪") == [f"system:{FOMALHAUT_ID}"] == NM.lookup("命运之轮") == NM.lookup("Mìngyùn zhī lún"), f"命運之輪 / 命运之轮 / Mìngyùn zhī lún → system:{FOMALHAUT_ID}")
    # 16 17
    add("t16_lexi_one_planet", all(NM.lookup(x) == [LEXIE_ENTITY] for x in ("Lexie", "Лекси", "柏辛", "Bǎixīn")), f"Lexie / Лекси / 柏辛 / Bǎixīn → {LEXIE_ENTITY}")
    nh = NM.E[LEXIE_ENTITY]["naming_history"]
    add("t17_lexi_history_author_unspecified", nh == {"status": "AUTHOR_UNSPECIFIED", "origin": "UNKNOWN"}, f"{nh}")
    # 18
    cu = _load(os.path.join(CANON, "cultures.json"))["cultures"]
    bad = [m["id"] for m in MS["missions"] if any(c in ("gulf_arab", "pakistani") for c in (m.get("cultures") or {}))
           or any(c in ("gulf_arab", "pakistani") for co in m["cohorts"].values() for c in co.get("culture", {}))
           or re.search(r"islamic|caliph|arab league", m.get("sponsor", ""), re.I)]
    bad += [c for c in ("gulf_arab", "pakistani") if c in cu]
    mec = next(m for m in MS["missions"] if m.get("colony") == "lalande_mecca")
    add("t18_former_arabic_branch_absent", not bad and mec["id"] == "VAKA-MOANA-1" and NM.native_of(NM.part("lalande_mecca", "polity"))["lang"] == "mi",
        f"миссия lalande_mecca: {mec['id']} ({', '.join(mec['cultures'])}); остатки: {bad}")
    # 19
    try:
        ca = _load(os.path.join(HERE, "out", "lab_state_3126.json"))["core_autonomy"]["southern_circle"]
        jt = next(j for j in W["jump_technology"] if j["unit"] == "lalande_mecca")
        lo, hi = ca["expected_share"]
        ok = (ca["maker"] == "lalande_mecca" and ca["members"] == ["hd20794_newindia", "hd40307_maland"] and ca["survives"]
              and lo <= ca["maker_share"] <= hi and jt["how"] == "own" and jt.get("capacity_x") == 2.5
              and P["actors"]["lalande_mecca"]["bloc"] == "southern_circle")
        add("t19_southern_circle_function_unchanged", ok,
            f"производитель {ca['maker']}, доля {ca['maker_share']} в [{lo}, {hi}] (принято 0.533), отсечение с мобилизацией {ca['cutoff_mobilised']['members_coverage']} (принято 0.678), выживает {ca['survives']}; ядра свои, capacity_x {jt.get('capacity_x')}")
    except (OSError, KeyError, StopIteration) as e:
        add("t19_southern_circle_function_unchanged", False, str(e))
    # 19b: cultural identity is the social profile; it must not set the industrial capability directly
    try:
        import lab
        import endowment
        En = endowment.load_endowment()
        col = next(c for c in W["colonies"] if c["id"] == "lalande_mecca")
        tr = {d: 0.8 for d in lab.CAPS}
        a = lab.colony_profile(dict(col, culture_drift={}), {}, En, {}, pop=1e8, tech_rel=tr)
        b = lab.colony_profile(dict(col, culture_drift={x: 30 for x in lab.AXES}), {}, En, {}, pop=1e8, tech_rel=tr)
        add("t19b_culture_does_not_set_industry", a["caps"] == b["caps"] and a["culture"] != b["culture"] and bool(col.get("industrial_heritage")),
            "сдвиг всех 24 культурных осей на +30 не меняет отраслевые способности; отраслевой профиль — world.json industrial_heritage")
    except Exception as e:      # noqa: BLE001 - report, do not crash the check run
        add("t19b_culture_does_not_set_industry", False, repr(e))
    # 20 21 22
    nordic = []
    for eid, e in NM.E.items():
        if eid in ("wolf359_citadel", "system:3", "mission:IRON-SHIELD-1", "place:wolf359.asteroid_2") or eid.startswith("site:wolf359") or "BCN-003" in eid or "site.003." in eid:
            for b in ([e.get("colony"), e.get("polity")] if e.get("type") == "unit" else [e]):
                if b:
                    nordic.append((eid, b))
    bad20 = [eid for eid, b in nordic if (NM.native_of(b) or {}).get("lang") not in ("nb", "nn")]
    add("t20_nordic_language_metadata", not bad20 and len(nordic) >= 8, f"норвежских записей {len(nordic)}; без nb / nn: {bad20}")
    bad21 = []
    for eid, e in NM.E.items():
        for b in ([e.get("colony"), e.get("polity")] if e.get("type") == "unit" else [e.get("names") or e]):
            if not b:
                continue
            for k, v in b.items():
                if isinstance(v, dict) and v.get("lang") == "non" and k != "ceremonial":
                    bad21.append(f"{eid}.{k}")
                if isinstance(v, dict) and v.get("lang") in ("nb", "nn") and re.search("[ǫðþ]|ll?r$", v.get("text", "")):
                    bad21.append(f"{eid}.{k}: древняя форма как современная")
    add("t21_old_norse_only_ceremonial", not bad21, f"{bad21}")
    bad22 = [f"{eid}.{k}" for eid, e in NM.E.items() for b in ([e.get("colony"), e.get("polity")] if e.get("type") == "unit" else [e])
             if b for k, v in b.items() if isinstance(v, dict) and v.get("lang") in ("nb", "nn", "non") and _cyr(v.get("text"))]
    add("t22_no_russian_in_nordic_endonyms", not bad22, f"{bad22}")
    # 16b: the author's spelling is Lexie, never Lexi
    lexi = re.compile(r"(?<![\w])Lexi(?![\w])")
    scan = DISPLAY_SCAN + [("data", os.path.join(GALAXY_DETAILS, "systems", "061_fomalhaut", "**", "*.json")),
                           ("data", os.path.join(ROOT, "src", "assets", "localization", "world", "star_systems", "fomalhaut.json")),
                           ("data", os.path.join(ROOT, "src", "assets", "data", "naming", "*.json")),
                           ("data", os.path.join(CANON, "*.json"))]
    bad = [os.path.relpath(p_, ROOT) for kind, g in scan for p_ in sorted(glob.glob(g, recursive=True))
           if lexi.search(open(p_, encoding="utf-8", errors="replace").read())]
    add("t16b_lexie_spelling", not bad and NM.resolve(LEXIE_ENTITY, "en") == "Lexie" and lexie_body()["designation"] == "Lexie",
        f"Lexie (не Lexi) в каноне, каталоге, локализации, runtime и лоре; найдено «Lexi»: {bad}")
    # 24: legacy actor 7 carries the Pacific branch, not the retired one
    A = _load(LEGACY_ACTORS)
    a7 = next(x for x in A["actors"] if x["id"] == 7)
    txt7 = json.dumps({k: a7.get(k) for k in ("ideology", "culture", "description", "tags")}, ensure_ascii=False)
    add("t24_legacy_actor7_pacific", not re.search(r"(?i)араб|ислам|халиф|улем|theocr|теократ(?!ия»)", txt7.replace("не теократия", ""))
        and "navigator_council" in a7.get("tags", []), f"actors.json id 7: {a7.get('ideology')} / {a7.get('culture')}")
    # 25: the current Tauranga state is a navigator council, not a theocracy
    col = next(c for c in W["colonies"] if c["id"] == "lalande_mecca")
    import lab as _lab
    bad = []
    if col["regime_3126"] != "navigator_council" or _lab.REGIME_FAMILY.get("navigator_council") == "theocratic":
        bad.append("regime")
    if _load(os.path.join(CANON, "world_sites.json"))["regime_law"].get("navigator_council") == "religious_law":
        bad.append("regime_law: тип власти не задаёт религиозное право")
    for rel in ("Notes/Lore/COLONIES/lalande_mecca.md", "Notes/Lore/SHIPS/VAKA-MOANA-1.md", "Notes/Lore/POLITICS/POLITICAL_ORDER.md",
                "Notes/History/COLONIZATION_CANON_3126.md", "src/assets/data/galaxy/systems.json", "src/assets/data/galaxy/nodes.json"):
        for ln, line in enumerate(open(os.path.join(ROOT, rel), encoding="utf-8").read().split("\n"), 1):
            low = line.replace("не теократ", "")
            if re.search(r"(?i)теократ", low) and (re.search(r"Тауранг|Капеху", low) or rel in ("Notes/Lore/COLONIES/lalande_mecca.md", "Notes/Lore/SHIPS/VAKA-MOANA-1.md")):
                bad.append(f"{rel}:{ln}")
    pol = json.dumps(P, ensure_ascii=False)
    bad += [m.group(0) for m in re.finditer(r"[^.;]{0,60}учёная теократия[^.;]{0,20}", pol)]
    lp = col.get("law_policy") or {}
    add("t25_tauranga_not_theocracy", not bad and lp.get("law_family") == "religious_law" and lp.get("goods_policy") == "faith",
        f"режим navigator_council; религиозно-консервативная политика общества — world.json law_policy ({lp.get('law_family')}, {lp.get('goods_policy')}); проблемы: {bad[:6]}")
    # 26: industrial heritage numbers are model state, not canon
    bad = [c["id"] for c in W["colonies"] if not c.get("industrial_heritage")
           or c.get("field_status", {}).get("industrial_heritage", {}).get("status") != "PROPOSAL/model"]
    add("t26_industrial_heritage_proposal_model", not bad, f"колоний {len(W['colonies'])}; без статуса PROPOSAL/model: {bad}")
    # 27: Fomalhaut is a flight-catalogue system (StarAtlas), on the maps, with Lexie as an authored body
    sysdoc = fomalhaut_system()
    hy = hyg_star(113368)
    loc = _load(os.path.join(ROOT, "src", "assets", "localization", "world", "star_systems", "fomalhaut.json"))
    q = sysdoc["position_ly"]
    lx = lexie_body()
    key = f"{FOMALHAUT_ID}:{LEXIE_BODY}"
    ok = (sysdoc["id"] == FOMALHAUT_ID and sysdoc["catalog_scope"] == "local" and sysdoc["atlas_visible"] and sysdoc["route_target"]
          and math.dist((q["x"], q["y"], q["z"]), tuple(hy["position_ly"])) < 0.01 and lx["origin"] == "observed_anchor"
          and key in loc["bodies"] and loc["bodies"][key]["ru"] == "Лекси"
          and _load(os.path.join(GALAXY_DETAILS, "systems", "061_fomalhaut", "generated", "system.json"))["stability_passed"]
          and not any(b["kind"] == "planet" and b["physical"]["mass_earth"] > 317.8 for b in fomalhaut_bodies()))
    add("t27_fomalhaut_in_flight_catalog", ok, f"StarAtlas id {FOMALHAUT_ID}: позиция = HYG, на картах, {len(fomalhaut_bodies())} тел, "
                                                f"устойчивость PASS, гигантов > 1 M_Jup нет; Лекси — авторское тело {LEXIE_BODY}")
    # language map consistency
    lm = _load(os.path.join(CANON, "world_sites.json"))["languages"]["culture_to_language"]
    add("territory_language_matches_endonyms", lm.get("nordic_pmc") == "nb" and lm.get("pacific_polynesian") == "mi" and lm.get("christian_traditionalist") == "en",
        "язык территории: Скьольдборг nb, Капеху mi, Ново-Фиваида en")
    return res


def write_all(NM):
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    os.makedirs(os.path.dirname(RUNTIME_NAMES), exist_ok=True)
    for p, txt in mirrors(NM).items():
        with open(p, "w", encoding="utf-8", newline="\n") as f:
            f.write(txt)
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(_dump(compute(NM), sort=True))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--lookup")
    a = ap.parse_args()
    NM = Names()
    if a.lookup:
        print(NM.lookup(a.lookup))
        return 0
    if a.write:
        write_all(NM)
        print("naming: mirrors + out/world_3126/naming_3126.json written")
    res = checks(NM, mirrors(NM))
    try:
        cur = open(OUT, encoding="utf-8").read()
    except OSError:
        cur = None
    res.append(dict(check="out_naming_in_sync", ok=cur == _dump(compute(NM), sort=True), detail="out/world_3126/naming_3126.json"))
    bad = 0
    for r in res:
        print(("OK   " if r["ok"] else "FAIL ") + r["check"] + ": " + r["detail"])
        bad += not r["ok"]
    print(f"naming checks: {len(res) - bad}/{len(res)} OK")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
