"""Construction state and expansion plans of every hub (author 2026-11-19: «пересмотреть все хабы - какие из них
полноценно функционируют, а какие только строятся ... новые шахты или маяки, новые планетарные и наземные хабы» +
«разработать план расширения для хабов и постепенное во времени - реализацию, так же ситуации в которых строительство
прекращается или замораживается ... и орбитальных хабов, и поселений и тех, что болтаются в пространстве»).

A pure function of a runtime site row (+ the generated economy / piracy / colony canon), deterministic (md5 of ids, no
random state). world_sites.runtime_export() calls annotate() on its rows, so a regeneration keeps the fields; the
standalone run re-annotates the existing runtime file without recomputing the whole world:

    python hub_construction.py            # (re)annotate src/assets/data/galaxy_details/world_sites/sites_3126.json
    python hub_construction.py --stats    # counts only

Fields added to a site row (and to a relay = beacon row):
  construction  only for a hub that is itself still being built:
                {as_of, status, stage, progress, started, eta, evidence, project}   (status / stage / progress as
                of REF_YEAR 3127.0; the game reads the project's timeline for its own date)
  expansion     [project, ...] for an operating hub that grows (new tower / wing / hall / dock arm / yard / mast)
A project (the timeline the game evaluates by date, once per game day):
  {id, kind, title, cause, budget_mcr, start_day, work_days, halts: [[from_day, to_day | null, cause], ...]}
  days count from the epoch 3126-01-01 (universe second 853005600, UniverseClock.h: 1 game day = 1 real day);
  progress(day) = (working days since start_day, minus the halted days) / work_days, clamped to 0..1;
  a halt with to_day null that has begun = abandoned for good (the site stays a skeleton).
Rules and real-world anchors: docs/HUB_CONSTRUCTION.md.
"""
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
RUNTIME_SITES = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "world_sites", "sites_3126.json")
ECONOMY = os.path.join(HERE, "out", "world_3126", "economy_3126.json")
FRONTIER = os.path.join(HERE, "out", "world_3126", "frontier_3126.json")
WORLD = os.path.join(HERE, "canon", "world.json")
EPOCH_YEAR = 3126.0
REF_YEAR = 3127.0      # "now" of the classification: the game runs on the real date + 1100 y (late 3126 / 3127)
YEAR_D = 365.25
PLAN_FROM, PLAN_TO = 3100.0, 3150.0       # the expansion plans cover these years (the game starts late 3126)

# build durations in years (lo, hi) by site type - real anchors: an open-pit / underground mine 3-8 y from the decision
# to production, a 300 m tower 4-6 y, a container port terminal 3-5 y, a new town 10-20 y (Brasilia core 41 months,
# the city decades), a radio mast months to 1-2 y; space and remoteness x1.5
BUILD_YEARS = {"mining_hub": (4, 7), "industrial_hub": (5, 9), "orbital_hub": (7, 12), "moon_hub": (7, 12),
               "trade_hub": (5, 9), "science_hub": (3, 5), "military_hub": (4, 8), "quarantine_hub": (2, 4),
               "belt_hub": (4, 8), "free_port": (3, 6), "deep_space_refuel_point": (2, 4),
               "planetary_surface_port": (8, 15), "beacon": (1, 2)}
# outposts.py kinds are small: prefab modules flown in
OUTPOST_YEARS = {"lab": (1, 3), "military_outpost": (1, 3), "refuel_station": (2, 3), "smuggler_hub": (1, 2),
                 "pirate_settlement": (1, 2)}
# the share of a type that is new at any time (steady state: build time / service life, + growth x build time)
BASE_NEW = {"mining_hub": 0.12, "industrial_hub": 0.05, "science_hub": 0.06, "trade_hub": 0.03, "military_hub": 0.05,
            "quarantine_hub": 0.04, "belt_hub": 0.05, "free_port": 0.05, "deep_space_refuel_point": 0.06,
            "orbital_hub": 0.03, "moon_hub": 0.03, "planetary_surface_port": 0.02, "beacon": 0.06}
NEVER_NEW = {"ship_graveyard", "abandoned_hub", "pirate_enclave"}
# expansion project kinds: (title, years lo, hi, budget Mcr, weight by hub family)
KINDS = {
    "tower": ("новая жилая башня", 4, 6, 5400),
    "hab": ("жилой блок", 2, 3, 320),
    "slab": ("новое жилое крыло", 3, 5, 2100),
    "hall": ("новый цех / склад", 2, 4, 1500),
    "dock": ("новый причальный пирс", 3, 5, 2600),
    "yard": ("новая грузовая площадка", 1, 2, 400),
    "mast": ("новая мачта маяка / связи", 1, 2, 60),
    "minehead": ("новый ствол шахты", 3, 6, 900),
}
KIND_WEIGHTS = {
    "residential": {"tower": 3, "slab": 3, "hab": 3, "dock": 1, "yard": 1, "mast": 0.5},
    "industrial": {"hall": 4, "yard": 2, "dock": 2, "hab": 1, "mast": 0.5},
    "mining": {"minehead": 4, "hall": 2, "yard": 2, "hab": 1.5, "mast": 0.5},
    "port": {"dock": 4, "yard": 3, "hall": 2, "slab": 1, "mast": 0.5},
    "small": {"hab": 3, "hall": 1, "mast": 2, "yard": 1},
}
FAMILY = {"mining_hub": "mining", "industrial_hub": "industrial", "trade_hub": "port", "free_port": "port",
          "planetary_surface_port": "residential", "orbital_hub": "residential", "moon_hub": "residential",
          "belt_hub": "industrial", "military_hub": "port", "science_hub": "small", "quarantine_hub": "small",
          "deep_space_refuel_point": "port", "beacon": "small"}
# halts: cause -> (share weight source, duration months lo..hi, P(abandoned for good))
HALT = {
    "funding": ("кризис финансирования: банк / казна полиса заморозили кредитную линию", 6, 36, 0.25),
    "supply": ("дефицит стали и стройматериалов: импортные поставки сорваны", 2, 9, 0.05),
    "blockade": ("блокада / пиратство на трассах: подрядчики ушли", 12, 48, 0.15),
    "accident": ("авария на площадке: расследование и перепроект", 1, 6, 0.05),
    "political": ("смена власти / суд: стройка остановлена до решения", 12, 36, 0.3),
}
UNSTABLE_REGIMES = {"clan_contracts": 0.05, "exile_republic": 0.04, "litigating_city_states": 0.06, "council_of_chiefs": 0.02,
                    "security_oligarchy": 0.02, "synod_theocracy": 0.02, "none_quiet": 0.1}
STAGES = ((0.15, "foundation"), (0.45, "frame"), (0.75, "cladding"), (1.0, "fitting"))


def h01(*parts):
    d = hashlib.md5("|".join(str(p) for p in parts).encode("utf-8")).hexdigest()
    return int(d[:12], 16) / float(16 ** 12)


def lerp(a, b, t):
    return a + (b - a) * t


def day_of(year):
    return int(round((year - EPOCH_YEAR) * YEAR_D))


def year_of(day):
    return round(EPOCH_YEAR + day / YEAR_D, 2)


class Context:
    """the inputs besides the row: polity economy (growth, income, import dependency), system piracy, colony founding"""

    def __init__(self):
        self.pol = {}
        if os.path.exists(ECONOMY):
            for pid, p in json.load(open(ECONOMY, encoding="utf-8"))["polities"].items():
                bal = p.get("balance", {})

                def imp(k):
                    b = bal.get(k) or {}
                    return (b.get("imports") or 0) / max(1.0, b.get("consumption") or 1.0)
                self.pol[pid] = dict(g=p.get("growth_per_year") or 0.0, inc=p.get("income_index") or 2.5,
                                     met=imp("metals"), con=imp("construction"), regime=p.get("regime") or "")
        self.piracy = {}
        if os.path.exists(FRONTIER):
            for r in json.load(open(FRONTIER, encoding="utf-8")).get("piracy_by_system", []):
                self.piracy[r["system"]] = r.get("index") or 0.0
        self.colony_founded = {}
        if os.path.exists(WORLD):
            for c in json.load(open(WORLD, encoding="utf-8")).get("colonies", []):
                self.colony_founded[c["id"]] = c.get("founded")

    def polity(self, pid):
        return self.pol.get(pid or "", dict(g=0.004, inc=2.5, met=0.2, con=0.0, regime=""))


def halt_hazards(ctx, row):
    """per working year, by cause - a megaproject anchor: ~1 in 3 large projects is stopped at least once (Flyvbjerg's
    samples: overruns are the norm; stalled projects ~10 % of the value in India's CMIE census)"""
    p = ctx.polity(row.get("polity"))
    return {
        "funding": 0.015 + 0.03 * max(0.0, 3.0 - p["inc"]) / 3.0 + (0.02 if p["g"] < 0 else 0.0),
        "supply": 0.005 + 0.04 * p["met"] + 0.04 * p["con"],
        "blockade": 0.06 * max(0.0, ctx.piracy.get(row.get("system_id"), 0.3) - 0.3),
        "accident": 0.012,
        "political": UNSTABLE_REGIMES.get(p["regime"], 0.004),
    }


def make_project(ctx, row, pid, kind, start_year, years, title, cause, budget):
    """a project timeline: start, working days, the halts drawn along it (deterministic per project id)"""
    start = day_of(start_year)
    work = max(30, int(round(years * YEAR_D)))
    hz = halt_hazards(ctx, row)
    total = sum(hz.values())
    halts = []
    t, done, k = start, 0, 0
    while done < work and k < 4:
        # the working time to the next halt: exponential with the summed hazard (per working year)
        u = h01(pid, "halt", k)
        gap = int(-math.log(max(1e-9, 1.0 - u)) / max(1e-6, total) * YEAR_D)
        if done + gap >= work:
            break
        done += gap
        t += gap
        # the cause in proportion to its hazard
        c = h01(pid, "cause", k) * total
        cause_id = "accident"
        for cid, v in hz.items():
            if c < v:
                cause_id = cid
                break
            c -= v
        _, mlo, mhi, p_abandon = HALT[cause_id]
        if h01(pid, "abandon", k) < p_abandon:
            halts.append([t, None, cause_id])
            break
        dur = int(lerp(mlo, mhi, h01(pid, "dur", k)) * 30.4)
        halts.append([t, t + dur, cause_id])
        t += dur
        k += 1
    return {"id": pid, "kind": kind, "title": title, "cause": cause, "budget_mcr": int(budget),
            "start_day": start, "work_days": work, "halts": halts}


def progress_at(proj, day):
    """the same function as src/model_asset/HubExpansion.cpp: (progress 0..1, status)"""
    if day < proj["start_day"]:
        return 0.0, "planned"
    worked = day - proj["start_day"]
    status = "building"
    for a, b, _ in proj["halts"]:
        if day <= a:
            continue
        if b is None:
            worked -= day - a
            status = "abandoned"
        else:
            worked -= min(day, b) - a
            if day < b:
                status = "halted"
    p = max(0.0, min(1.0, worked / float(proj["work_days"])))
    if p >= 1.0:
        return 1.0, "done"
    return p, status


def finish_day(proj):
    """the day the project completes (None when abandoned)"""
    end = proj["start_day"] + proj["work_days"]
    for a, b, _ in proj["halts"]:
        if b is None:
            return None
        end += b - a
    return end


def stage_of(p):
    for lim, name in STAGES:
        if p < lim:
            return name
    return "done"


def site_kind(row):
    if row.get("_beacon"):
        return "beacon"
    return row.get("type") or ""


def is_new(ctx, row):
    """-> (start_year, years, evidence) when the hub itself is still being built, else None"""
    t = site_kind(row)
    sid = row["id"]
    if t in NEVER_NEW or row.get("runtime_binding") or (row.get("population") or 0) <= 0 and not row.get("_beacon"):
        return None
    if row.get("known_to_player") == "hidden" or row.get("legal") == "illegal":
        return None
    lo, hi = OUTPOST_YEARS.get(row.get("outpost_kind") or "", BUILD_YEARS.get(t, (3, 6)))
    years = lerp(lo, hi, h01(sid, "years"))
    pol = row.get("polity") or ""
    # 1) a colony founded in the last few years: everything it has is being built (the capital first)
    founded_col = ctx.colony_founded.get(pol)
    if founded_col and founded_col >= 3100 and not row.get("outpost_kind") and not row.get("_beacon"):
        lag = 0.0 if sid.endswith("city01") else lerp(0.2, 2.0, h01(sid, "lag"))
        years = years * (1.5 if t == "planetary_surface_port" or "city" in sid else 1.0)
        return founded_col + lag, years, f"колония основана в {founded_col}: город и хабы ещё строятся"
    # 2) a site of its own recorded founding year within the build time before the epoch
    f = row.get("founded")
    if isinstance(f, (int, float)) and f > REF_YEAR - hi:
        if f + years > REF_YEAR:
            return f, years, f"основан в {int(f)}: срок строительства {years:.1f} г. ещё не вышел"
        return None
    if isinstance(f, (int, float)):
        return None                       # an old recorded founding: operating
    # 3) a growth-driven new hub: the share of the type that is new now (steady state + polity growth, frontier bonus)
    if (row.get("population") or 0) > 30000 or "city0" in sid or ".port0" in sid:
        return None                       # big cities / their ports are old (the catalog's populations say so)
    p = ctx.polity(pol)
    share = BASE_NEW.get(t, 0.03) * (1.0 + 25.0 * max(0.0, p["g"]))
    if row.get("outpost_kind"):
        share = max(share, 0.08)          # outposts are cheap and short-lived: more of them are new
    if row.get("_beacon") and row.get("system_id") in (37, 41, 45, 38, 24):
        share = 0.35                      # the newest frontier systems get their relay chains now
    u = h01(sid, "new")
    if u >= share:
        return None
    # uniform over the build: the progress at the epoch is u / share
    q = u / share
    start = REF_YEAR - q * years
    why = ("новый маяк / ретранслятор" if row.get("_beacon") else
           "новая шахта" if t == "mining_hub" else "новый аванпост" if row.get("outpost_kind") else "новый хаб")
    return start, years, f"{why}: доля новых для типа {share:.0%} (рост полиса {p['g'] * 100:.1f} %/г)"


def expansion_plan(ctx, row):
    """the operating hub's projects over PLAN_FROM..PLAN_TO: a Poisson stream whose rate grows with the polity's growth,
    the hub's level / size and its dock utilisation (a busy port builds piers)"""
    t = site_kind(row)
    sid = row["id"]
    if t in NEVER_NEW or (row.get("population") or 0) <= 0 or row.get("known_to_player") == "hidden":
        return []
    p = ctx.polity(row.get("polity"))
    pop = row.get("population") or 0
    rate = 0.02 + 2.5 * max(0.0, p["g"]) + 0.01 * max(0.0, p["inc"] - 2.5)
    rate *= 1.4 if row.get("level") == "A" else 1.0
    rate *= 1.0 + 0.25 * math.log10(max(1.0, pop / 2000.0))
    if (row.get("utilisation") or 0) > 0.55:
        rate *= 1.3
    if row.get("outpost_kind") or row.get("_beacon"):
        rate *= 0.4
    weights = KIND_WEIGHTS[FAMILY.get(t, "small") if not row.get("outpost_kind") else "small"]
    if row.get("_beacon"):
        weights = {"mast": 1}
    out = []
    y, k = PLAN_FROM, 0
    while k < 12:
        y += -math.log(max(1e-9, 1.0 - h01(sid, "gap", k))) / rate
        if y > PLAN_TO:
            break
        c = h01(sid, "kind", k) * sum(weights.values())
        kind = next(iter(weights))
        for kk, w in weights.items():
            if c < w:
                kind = kk
                break
            c -= w
        title, ylo, yhi, budget = KINDS[kind]
        years = lerp(ylo, yhi, h01(sid, "dur", k))
        cause = ("рост населения" if kind in ("tower", "slab", "hab") else
                 "загрузка причалов" if kind in ("dock", "yard") else "рост добычи" if kind == "minehead" else
                 "связь / навигация" if kind == "mast" else "рост производства")
        out.append(make_project(ctx, row, f"{sid}.x{k:02d}", kind, y, years, title, cause,
                                budget * lerp(0.7, 1.4, h01(sid, "budget", k))))
        k += 1
    return out


def annotate_row(ctx, row):
    row.pop("construction", None)
    row.pop("expansion", None)
    new = is_new(ctx, row)
    if new:
        start, years, why = new
        t = site_kind(row)
        budget = {"mining_hub": 9000, "planetary_surface_port": 60000, "beacon": 80}.get(t, 15000)
        proj = make_project(ctx, row, f"{row['id']}.build", "build", start, years, "строительство хаба",
                            why, budget * lerp(0.6, 1.5, h01(row["id"], "budget")))
        p, status = progress_at(proj, day_of(REF_YEAR))
        if status != "done":
            end = finish_day(proj)
            row["construction"] = {"as_of": REF_YEAR, "status": status,
                                   "stage": stage_of(p) if status != "planned" else "survey",
                                   "progress": round(p, 3), "started": year_of(proj["start_day"]),
                                   "eta": year_of(end) if end is not None else None, "evidence": why, "project": proj}
            return row
    exp = expansion_plan(ctx, row)
    if exp:
        row["expansion"] = exp
    return row


def annotate(rows, relays=None, ctx=None):
    """adds construction / expansion to the runtime site rows (and the relays, as beacons) in place"""
    ctx = ctx or Context()
    for r in rows:
        annotate_row(ctx, r)
    for r in relays or []:
        r["_beacon"] = True
        annotate_row(ctx, r)
        r.pop("_beacon", None)
    return rows


def stats(doc):
    import collections
    rows = doc["sites"] + doc.get("hidden_sites", [])
    uc = [r for r in rows if r.get("construction")]
    rel = [r for r in doc.get("relays", []) if r.get("construction")]
    by = collections.Counter(r["type"] for r in uc)
    st = collections.Counter(r["construction"]["status"] for r in uc + rel)
    exp = [r for r in rows if r.get("expansion")]
    active = collections.Counter()
    for r in exp + [x for x in doc.get("relays", []) if x.get("expansion")]:
        for pj in r["expansion"]:
            active[progress_at(pj, day_of(REF_YEAR))[1]] += 1
    return dict(sites=len(rows), under_construction=len(uc), by_type=dict(by), relays=len(doc.get("relays", [])),
                relays_under_construction=len(rel), status=dict(st), hubs_with_plans=len(exp),
                projects_at_epoch=dict(active))


def main():
    sys.stdout.reconfigure(encoding="utf-8")
    doc = json.load(open(RUNTIME_SITES, encoding="utf-8"))
    annotate(doc["sites"] + doc.get("hidden_sites", []), doc.get("relays", []))
    if "--stats" not in sys.argv:
        with open(RUNTIME_SITES, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(doc, ensure_ascii=False, indent=1, sort_keys=True) + "\n")
    print(json.dumps(stats(doc), ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
