"""Who first visited the 3126 systems nobody lives in (PROPOSAL, author request 2026-11-11).

Model, deterministic, from the machine canon:
  * generation-ship fly-bys: a colony ship (canon/missions.json, Sol -> target [-> final]) passes within
    FLYBY_LY of a star on its way; the crew observes it for years and gives it names. Pass year =
    launch + path fraction x (colony founding - launch). This is how the young colonies without their own
    jump technology (Third Hellas, New Thebaid) became discoverers.
  * jump-era science expeditions: a participant sends an expedition from a base it holds - its own system
    (from max(founding, jump technology year)) or a beacon it operates (canon/world.json beacons) - once
    the jump reach of the epoch (world.json max_hop_ly) covers the distance. The year is delayed by the
    participant's science capacity (industrial_heritage.research, jump technology level, population):
    a strong science service comes first.
  * frontier canon: systems whose official names already exist in canon/world_sites.json (World's Ass,
    the Chest, the Monastery) keep the polity that named them.
  * balance: every participant that can reach something gets at least one system - a system moves from a
    holder with more than one to the participant with the smallest delay behind the first visit.

Output: out/world_3126/expeditions_3126.json (input of naming_series_3126.py for the names of the bodies).
Usage: python expeditions_3126.py [--write]
"""

import argparse
import glob
import hashlib
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CANON = os.path.join(HERE, "canon")
OUT = os.path.join(HERE, "out", "world_3126", "expeditions_3126.json")
SYSTEMS = os.path.join(ROOT, "src", "assets", "data", "galaxy_details", "systems")
FLYBY_LY = 4.0
NEIGHBOUR_LY = 1.0
MAX_PER = 4         # a holder keeps at most this many systems outside the frontier canon
YEAR = 3126
FORTUNA = 61

# missions that do not name a colony but whose sender is known
MISSION_SENDER = {"GAIA-2": "gj832_gaia_zone", "LAST-LIGHT-1": "wolf359_citadel", "DEEP-SEEK-1": "tau_ceti_cognium"}
# frontier systems whose names already exist in canon (world_sites frontiers official_names)
FRONTIER_CANON = {19: ("tau_ceti_cognium", 3091, "«Лунные врата» (月门星区): научная служба Когниума после находки Керна у «Лотоса»"),
                  20: ("tau_ceti_cognium", 3060, "Обсерватория «Нулевое измерение» (零维观测站): научная служба Управления Маяков"),
                  36: ("tau_ceti_cognium", 3030, "«Китовый сундук» (鲸箱): разведка Когниума через склад «Китовая Щель»")}


def _load(p):
    with open(p, encoding="utf-8") as f:
        return json.load(f)


def atlas():
    rows = {}
    for p in glob.glob(os.path.join(SYSTEMS, "*", "system.json")):
        s = _load(p)
        q = s["position_ly"]
        rows[s["id"]] = dict(name=s["name"], pos=(q["x"], q["y"], q["z"]))
    return rows


def hop_at(W, year):
    best = 0.0
    for y, h in W["max_hop_ly"]:
        if y <= year:
            best = h
    return best


def first_year_for(W, dist, not_before):
    """earliest year >= not_before when the epoch's jump reach covers dist (None if never by 3126)."""
    for y, h in sorted(W["max_hop_ly"]):
        if h >= dist:
            return max(y, not_before) if max(y, not_before) <= YEAR else None
    return None


def jitter(*key):
    return int(hashlib.sha256("|".join(map(str, key)).encode()).hexdigest()[:6], 16) % 10


def seg_dist(p, a, b):
    ab = [b[i] - a[i] for i in range(3)]
    ap = [p[i] - a[i] for i in range(3)]
    L2 = sum(x * x for x in ab)
    t = 0.0 if L2 == 0 else max(0.0, min(1.0, sum(ap[i] * ab[i] for i in range(3)) / L2))
    q = [a[i] + t * ab[i] for i in range(3)]
    return math.dist(p, q), t


MAX_LEGS = 8
EXTRA_LEG_YEARS = 12


def hop_counts(A, h):
    """all-pairs leg counts over the catalogue stars: a gap <= h is one leg; a gap <= 2h is two legs
    through a charge cache dropped in empty space on the way (the Far Berth / Whale Gap practice)."""
    import heapq
    ids = sorted(A)
    nb = {}
    for i in ids:
        nb[i] = []
        for j in ids:
            if j == i:
                continue
            d = math.dist(A[i]["pos"], A[j]["pos"])
            if d <= h:
                nb[i].append((j, 1))
            elif d <= 2 * h:
                nb[i].append((j, 2))
    out = {}
    for s in ids:
        dist = {s: 0}
        pq = [(0, s)]
        while pq:
            c, x = heapq.heappop(pq)
            if c > dist.get(x, 1e9):
                continue
            for y, w in nb[x]:
                if c + w < dist.get(y, 1e9):
                    dist[y] = c + w
                    heapq.heappush(pq, (c + w, y))
        out[s] = dist
    return out


def participants(W):
    jt = {j["unit"]: j for j in W["jump_technology"]}
    out = {}
    for c in W["colonies"]:
        j = jt.get(c["id"])
        research = c["industrial_heritage"].get("research", 10.0)
        cap = (research / 20.0) * (j["level"] if j else 0.3) * min(1.0, (c["pop_3126_authored"] / 1e6) ** 0.25)
        out[c["id"]] = dict(system=c["system"], founded=c["founded"], jump_year=j["year"] if j else None, cap=round(cap, 3))
    out["sol_authority"] = dict(system=0, founded=0, jump_year=3112, cap=0.6)
    return out


def model():
    W = _load(os.path.join(CANON, "world.json"))
    M = _load(os.path.join(CANON, "missions.json"))
    A = atlas()
    P = participants(W)
    inhabited = {c["system"] for c in W["colonies"]} | {o["system"] for o in W["outposts"] + W["frontier_pools"]} | {0, FORTUNA}
    targets = sorted(s for s in A if s not in inhabited)
    founded = {c["id"]: c["founded"] for c in W["colonies"]}
    cand = {t: [] for t in targets}

    # generation-ship fly-bys
    for m in M["missions"]:
        unit = m.get("colony") if m.get("colony") in P else MISSION_SENDER.get(m["id"])
        tgt = m.get("final_system") if isinstance(m.get("final_system"), int) else m.get("target_system")
        if unit not in P or not isinstance(tgt, int) or tgt not in A:
            continue
        path = [m["source_system"]] + ([m["target_system"]] if isinstance(m["target_system"], int) and m["target_system"] != tgt else []) + [tgt]
        pts = [A[s]["pos"] for s in path]
        legs = [math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1)]
        total = sum(legs) or 1.0
        arrive = founded.get(m.get("colony")) or (m["launch_year"] + total / 0.08)
        for t in targets:
            done = 0.0
            for i, L in enumerate(legs):
                d, f = seg_dist(A[t]["pos"], pts[i], pts[i + 1])
                if d <= FLYBY_LY and (0.0 < f < 1.0 or (f >= 1.0 and i == len(legs) - 1)):
                    yr = round(m["launch_year"] + (done + f * L) / total * (arrive - m["launch_year"]))
                    if yr > YEAR:
                        continue
                    cand[t].append(dict(unit=unit, year=yr, how="flyby", mission=m["id"], dist_ly=round(d, 2)))
                done += L

    # jump-era expeditions from bases
    bases = {u: [] for u in P}
    for u, p in P.items():
        if p["jump_year"]:
            bases[u].append((p["system"], max(p["founded"], p["jump_year"])))
    # without own jump technology a colony charters licensed ships from the beacon in its system
    beacon_year = {}
    for b in W["beacons"]:
        beacon_year[b["system"]] = min(beacon_year.get(b["system"], 9999), b["year"])
    for u, p in P.items():
        if not p["jump_year"] and p["system"] in beacon_year:
            bases[u].append((p["system"], max(p["founded"], beacon_year[p["system"]] + 20)))
    for b in W["beacons"]:
        if b["operator"] in bases:
            bases[b["operator"]].append((b["system"], b["year"]))
    # an expedition ship may chain unbeaconed jumps through any catalogue star (each leg within the
    # epoch's reach); every extra leg costs EXTRA_LEG_YEARS of preparation (depots, charge, survey)
    hops = {h: hop_counts(A, h) for _, h in W["max_hop_ly"]}
    for u, bl in bases.items():
        for t in targets:
            best = None
            for sid, since in bl:
                d = math.dist(A[sid]["pos"], A[t]["pos"])
                for y, h in sorted(W["max_hop_ly"]):
                    n = hops[h].get(sid, {}).get(t)
                    if n is None or n > MAX_LEGS:
                        continue
                    yr = max(y, since) + EXTRA_LEG_YEARS * (n - 1) + round(30 * (1 - min(1.0, P[u]["cap"]))) + jitter(u, t)
                    if yr <= YEAR and (best is None or yr < best["year"]):
                        best = dict(unit=u, year=yr, how="expedition", base=sid, legs=n, dist_ly=round(d, 2))
            if best:
                cand[t].append(best)

    # first visit per participant per target
    for t in targets:
        firsts = {}
        for c in sorted(cand[t], key=lambda c: c["year"]):
            firsts.setdefault(c["unit"], c)
        cand[t] = sorted(firsts.values(), key=lambda c: (c["year"], c["unit"]))

    # a colony living within NEIGHBOUR_LY of a star surveys it from home: it is the namer whatever passed by
    home = {}
    for u, p in P.items():
        for t in targets:
            if math.dist(A[p["system"]]["pos"], A[t]["pos"]) <= NEIGHBOUR_LY:
                home[t] = dict(unit=u, year=p["founded"], how="neighbour", dist_ly=round(math.dist(A[p["system"]]["pos"], A[t]["pos"]), 2))
    owner = {}
    for t in targets:
        if t in home:
            owner[t] = home[t]
        elif t in FRONTIER_CANON:
            u, yr, why = FRONTIER_CANON[t]
            owner[t] = dict(unit=u, year=yr, how="frontier_canon", note=why)
        elif cand[t]:
            owner[t] = dict(cand[t][0])

    # balance: every participant with a candidate gets one
    def count(u):
        return sum(1 for o in owner.values() if o["unit"] == u)
    changed = True
    while changed:
        changed = False
        for u in sorted(P):
            if count(u):
                continue
            opts = []
            for t in targets:
                if t in FRONTIER_CANON or t in home or t not in owner or count(owner[t]["unit"]) < 2:
                    continue
                mine = next((c for c in cand[t] if c["unit"] == u), None)
                if mine:
                    opts.append((mine["year"] - owner[t]["year"], t, mine))
            if opts:
                _, t, mine = min(opts)
                prev = owner[t]["unit"]
                owner[t] = dict(mine, balanced_from=prev)
                changed = True

    # fairness: the first-jump monopoly (Cognium 2750-2760) would take most of the map; a holder above
    # MAX_PER gives its systems, nearest second visit first, to the participant with the fewest systems
    def free_count(u):
        return sum(1 for t, o in owner.items() if o["unit"] == u and t not in FRONTIER_CANON)
    while True:
        heavy = [u for u in P if free_count(u) > MAX_PER]
        if not heavy:
            break
        moves = []
        for t, o in owner.items():
            if t in FRONTIER_CANON or t in home or o["unit"] not in heavy:
                continue
            for c in cand[t]:
                if c["unit"] != o["unit"] and free_count(c["unit"]) < MAX_PER:
                    moves.append((count(c["unit"]), c["year"] - o["year"], t, c))
        if not moves:
            break
        _, _, t, c = min(moves, key=lambda m: (m[0], m[1]))
        owner[t] = dict(c, balanced_from=owner[t]["unit"])

    rows = []
    for t in targets:
        o = owner.get(t)
        rows.append(dict(system=t, catalog=A[t]["name"], discoverer=o["unit"] if o else None, year=o["year"] if o else None,
                         how=o["how"] if o else None, detail={k: v for k, v in (o or {}).items() if k not in ("unit", "year", "how")},
                         candidates=cand[t][:6]))
    per = {}
    for r in rows:
        if r["discoverer"]:
            per.setdefault(r["discoverer"], []).append(r["system"])
    return dict(schema="elite.colonization_lab.expeditions.v1", status="PROPOSAL", year=YEAR,
                generated_by="tools/colonization_lab/expeditions_3126.py",
                note="первые посещения необитаемых систем: пролёты кораблей поколений, научные экспедиции эпохи прыжка с баз и маяков, канон фронтиров; баланс — у каждого участника хотя бы одна система",
                systems=rows, by_participant=dict(sorted(per.items())),
                without_systems=sorted(u for u in P if u not in per))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    sys.stdout.reconfigure(encoding="utf-8")
    R = model()
    for r in R["systems"]:
        d = r["detail"]
        via = d.get("mission") or (f"база {d['base']}" if "base" in d else d.get("note", ""))
        bal = f"  (баланс, было: {d['balanced_from']})" if d.get("balanced_from") else ""
        print(f"{r['system']:3d} {r['catalog'][:24]:24s} {str(r['discoverer']):22s} {r['year']} {str(r['how']):14s} {via}{bal}")
    print("по участникам:", {u: len(v) for u, v in R["by_participant"].items()})
    print("без систем:", R["without_systems"])
    if a.write:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        with open(OUT, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(R, ensure_ascii=False, indent=1) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
