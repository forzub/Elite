"""Machine fact export for the lore writers (out/lore_facts.json) and its consistency check.

Every record carries `status` (CANON / PROPOSAL / UNKNOWN) and `visibility`
(PUBLIC_RECORD / AUTHOR_TRUTH). Model outputs (populations, economy, corridors) are
PROPOSAL with basis "model": they are measurements of the lab, not author canon.

Settlements: canon/settlements.json (PROPOSAL) names key cities, habitats, industrial and
military sites per polity on real catalog bodies; the lab distributes the model population
over them (Zipf). Regenerate the skeleton with `python lore.py --propose-settlements`
(only adds missing polities, never overwrites authored entries).
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SETTLEMENTS = os.path.join(HERE, "canon", "settlements.json")
MODEL = {"status": "PROPOSAL", "visibility": "PUBLIC_RECORD", "basis": "model (demography.py / trade.py), not author canon"}
REGIME_RU = {"technocratic": "технократия", "security": "силовой режим", "oligarchic": "олигархия", "theocratic": "теократия",
             "assembly": "собрание", "party": "партийный режим", "clan": "кланы"}
ROLE_FOR_ASSET = {"gas_platform": "gas", "ice_export": "belt", "mine": "belt", "refinery": "belt", "food_habitat": "site2",
                  "factory": "orbit", "electronics_fab": "orbit", "shipyard": "orbit", "arsenal_shipyard": "orbit", "fortress": "orbit",
                  "research": "orbit", "logistics_hub": "orbit", "legal_hub": "orbit", "jump_infrastructure": "orbit", "intel": "site2",
                  "black_market": "belt", "station_core": "orbit", "strategic_mine": "belt"}


def load_settlements():
    if not os.path.isfile(SETTLEMENTS):
        return {"schema": "elite.colonization_lab.settlements.v1", "polities": {}}
    with open(SETTLEMENTS, encoding="utf-8") as f:
        return json.load(f)


def propose_settlements(W, E):
    """Skeleton: capital + up to 2 secondary habitats on the best catalog sites + one site per asset.
    Names are descriptive (body + role); the author renames them."""
    doc = load_settlements()
    pol = doc.setdefault("polities", {})
    units = [(c["id"], c["system"], c["name"], c["assets"]) for c in W["colonies"]]
    units += [(o["id"], o["system"], o["name"], []) for o in W["outposts"] + W.get("frontier_pools", [])]
    for uid, sysid, name, assets in units:
        if uid in pol:
            continue
        e = E[sysid]
        sites = [s["body"] for s in e["sites"]]
        giants = list(e["giants"])
        belt = next((b[0] for b in e["bodies"] if "belt" in b[0].lower()), None)
        cap_body = sites[0] if sites else (giants[0] + " (орбита)" if giants else e["name"] + " (орбитальные кольца)")
        rows = [dict(id=f"{uid}.capital", name=name, role="capital", body=cap_body, kind="habitat_city",
                     knowledge=dict(status="PROPOSAL", visibility="PUBLIC_RECORD", basis="столица = главное поселение колонии; тело — лучший участок каталога"))]
        for i, b in enumerate(sites[1:3]):
            rows.append(dict(id=f"{uid}.habitat{i + 1}", name=f"{b} — поселение", role="habitat", body=b, kind="habitat_town",
                             knowledge=dict(status="PROPOSAL", visibility="PUBLIC_RECORD", basis="следующий по качеству участок каталога")))
        for a in assets:
            where = ROLE_FOR_ASSET.get(a["type"], "orbit")
            body = (giants[0] if where == "gas" and giants else belt if where == "belt" and belt else
                    sites[1] if where == "site2" and len(sites) > 1 else cap_body + " (орбита)")
            rows.append(dict(id=f"{uid}.{a['id']}", name=a["name"], role=a["type"], body=body,
                             kind="military_site" if a["type"] in ("fortress", "arsenal_shipyard", "intel") else "industrial_site",
                             knowledge=dict(status="PROPOSAL", visibility="AUTHOR_TRUTH" if a["type"] == "intel" else "PUBLIC_RECORD",
                                            basis="актив колонии (world.json), размещён на теле каталога")))
        pol[uid] = rows
    doc["note"] = ("PROPOSAL: key settlements, habitats, industrial and military sites per polity on bodies of the physical catalog "
                   "(src/assets/data/galaxy_details). Populations are NOT stored here: the lab distributes the model population (Zipf).")
    doc["polities"] = dict(sorted(pol.items()))
    return doc


def _zipf(total, n):
    h = sum(1.0 / k for k in range(1, n + 1))
    return [total / (k * h) for k in range(1, n + 1)]


def _kn(x):
    return x.get("knowledge", MODEL)


def build(R, lab):
    """R: lab.run() result; lab: the lab module (helpers)."""
    import demography as demo
    import knowledge
    import trade
    E = R["E"]
    W = R["world"]
    MS = {"missions": list(R["missions"].values())}
    units = R["units"]
    items = knowledge.collect(MS, W)
    out = dict(schema="elite.colonization_lab.lore_facts.v1", seed=R["seed"], scenario="baseline",
               status_legend=dict(CANON="есть в заметках автора (generation ships/*, fate.md, history.md, каталог)",
                                  PROPOSAL="добавлено или пересчитано Claude / результат модели",
                                  UNKNOWN="намеренно без ответа, даже для автора",
                                  PUBLIC_RECORD="мир 3126 это знает или может знать", AUTHOR_TRUTH="скрытая правда автора"),
               status_counts=knowledge.counts(items))
    # ---- missions
    ms = []
    for mid, m in sorted(R["missions"].items(), key=lambda kv: (kv[1]["launch_year"], kv[0])):
        v = R["voyages"][mid]
        arr = next((a for a in m["anchors"] if a["type"] == "arrival"), None)
        dest = m.get("final_system") if isinstance(m.get("final_system"), int) else m.get("target_system")
        fs = m.get("field_status", {})
        anchors = {(a["year"], a["type"]): a for a in m["anchors"]}
        rows = []
        for e in v.events:
            a = anchors.get((e["year"], e["type"]))
            if e["type"] == "launch":
                kn = fs.get("launch_year", m["knowledge"])
            elif a is not None:
                kn = a["knowledge"]
            else:
                kn = dict(status="PROPOSAL", visibility="PUBLIC_RECORD", basis="simulated gap filler (seed-dependent)")
            reg = e.get("regime_tendency")
            rows.append(dict(year=e["year"], event=e["type"], text=e["text"], population=e["pop"], awake=e.get("awake"), cryo=e.get("cryo"),
                             regime=REGIME_RU.get(reg, reg), tech_state=e.get("knowledge"), deaths=e["deaths"],
                             status=kn["status"], visibility=kn["visibility"], source=e["source"]))
            if a is not None and a.get("hidden_truth"):
                hk = a["hidden_truth_knowledge"]
                rows.append(dict(year=e["year"], event=e["type"] + ".hidden_truth", text=a["hidden_truth"], population=e["pop"],
                                 awake=e.get("awake"), cryo=e.get("cryo"), regime=None, tech_state=None, deaths=0,
                                 status=hk["status"], visibility=hk["visibility"], source="anchor"))
        ms.append(dict(
            id=mid, aliases=m["aliases"], status=m["knowledge"]["status"], visibility=m["knowledge"]["visibility"], basis=m["knowledge"]["basis"],
            wave=m["wave"], sponsor=m["sponsor"], launch=dict(year=m["launch_year"], **_short(fs.get("launch_year"))),
            origin_system=m["source_system"], destination=dict(system=dest, name=E[dest]["name"] if isinstance(dest, int) and dest in E else str(dest)),
            duration_years=(arr["year"] - m["launch_year"]) if arr else None,
            start=dict(population=m["population"], awake=int(round(m["population"] * (1 - m["cryo_share"]))),
                       cryo=int(round(m["population"] * m["cryo_share"])), awake_watch=m["awake_watch"], **_short(fs.get("population"))),
            arrival=None if not arr else dict(year=arr["year"], population=arr["pop_after"], authored_year=arr.get("authored"), **_short(fs.get("arrival_year"))),
            fate=dict(state=m["fate"]["status"], ship=m["fate"]["ship"], note=m["fate"].get("ship_note", ""), **_short(fs.get("fate"))),
            colony=m.get("colony"), sources=m["sources"], chronology=rows))
    out["missions"] = ms
    # ---- polities (colonies, outposts, frontier pools)
    st = load_settlements()["polities"]
    gini = {k: v["gini"] for k, v in demo.REGIME.items()}
    base = R["base"]
    cor = R["corridors"]
    pols = []
    cmap = {c["id"]: c for c in R["cols"]}
    for o in W["outposts"] + W.get("frontier_pools", []):
        cmap.setdefault(o["id"], dict(o, kind="outpost"))
    for uid in sorted(cmap):
        c = cmap[uid]
        u = units.get(uid)
        if u is None:
            continue
        kn = c.get("knowledge", MODEL)
        rec = dict(id=uid, name=c["name"], polity=c.get("polity") or (cmap.get(c.get("owners", [None])[0], {}).get("polity")),
                   kind=u.kind, owner=u.owner, system=dict(id=c["system"], name=E[c["system"]]["name"]),
                   status=kn["status"], visibility=kn["visibility"], basis=kn.get("basis", ""),
                   founded=dict(year=u.hist[0]["year"] if u.hist else c.get("founded"), founders=c.get("founders", [])),
                   population_3126=dict(model=int(round(u.P())), authored=c.get("pop_3126_authored"), **MODEL,
                                        authored_status=c.get("field_status", {}).get("pop_3126_authored", {}).get("status")),
                   regime=dict(value=c.get("regime_3126"), **_short(c.get("field_status", {}).get("regime_3126"))),
                   checkpoints=[dict(r, **MODEL) for r in demo.checkpoints(u, 25)] if u.hist else [],
                   tech=dict(levels_3126={d: round(x, 3) for d, x in u.tech.items()}, status_by_domain=demo.tech_status(u) if u.hist else {},
                             min_automation=demo.min_automation(u) if u.hist else None, **MODEL),
                   migration=dict(in_total=int(round(u.mig_in_tot)), out_total=int(round(u.mig_out_tot)), **MODEL),
                   events=[e for e in u.events if e["type"] != "epidemic"], epidemics=len([e for e in u.events if e["type"] == "epidemic"]))
        if uid in base["made"]:
            made, imp, exp = base["made"][uid], base["imp"][uid], base["exp"][uid]
            val = lambda d: {g: round(d[g] * trade.BASE_PRICE[g], 2) for g in trade.GOODS if d[g] * trade.BASE_PRICE[g] > 0.05}
            deps = sorted({x["good"] for x in cor if x["buyer"] == uid and x["reason"] == "no_local_source"})
            rec["economy_3126"] = dict(
                production=val(made), imports=val(imp), exports=val(exp), labour=int(round(u.workers)),
                automation=round(u.A, 3), living_standard=round(u.y, 3), inequality_gini=gini.get(c.get("regime_3126"), 0.4),
                dependencies=deps, wealth_per_100k=R["wealth"].get(uid), **MODEL)
        if uid in st and u.hist:
            rows = st[uid]
            habit = [r for r in rows if r["kind"].startswith("habitat")]
            shares = _zipf(u.P() * 0.85, max(1, len(habit)))
            rec["settlements"] = [dict(r, population=int(round(shares[i])) if r in habit else None,
                                       population_status="PROPOSAL (model, Zipf)") for i, r in enumerate(rows)
                                  ] if habit else rows
        pols.append(rec)
    out["polities"] = pols
    # ---- Earth
    eh = []
    en, es = units["earth_north"], units["earth_south"]
    sn = {h["year"]: h for h in en.hist}
    for h in es.hist:
        if h["year"] % 25 == 0 or h["year"] == lab.YEAR_END:
            n = sn.get(h["year"])
            if n is None:
                continue
            P = h["pop"] + n["pop"]
            eh.append(dict(year=h["year"], pop=int(round(P)), north=int(round(n["pop"])), south=int(round(h["pop"])),
                           tfr=round((h["tfr"] * h["pop"] + n["tfr"] * n["pop"]) / P, 2), e0=round((h["e0"] * h["pop"] + n["e0"] * n["pop"]) / P, 1),
                           phase_north=n["phase"], phase_south=h["phase"], **MODEL))
    out["earth"] = dict(trajectory=eh, checkpoints_authored=W["earth_demography"]["checkpoints"],
                        return_3126=dict(R["earth_return"], **MODEL), anchor=dict(W["earth_3126"]))
    # ---- trade, assets, relations, unknowns
    top = max([x["value"] for x in cor] or [1.0])
    out["trade_corridors"] = [dict(x, volume_class=trade.volume_class(x["value"], top), **MODEL) for x in sorted(cor, key=lambda x: (-x["value"], x["seller"], x["buyer"], x["good"]))
                              if x["value"] >= 0.002 * top]
    out["industrial_assets"] = [dict(a, **MODEL) for a in R["assets"]]
    out["relations"] = [dict(a=r["a"], b=r["b"], grievance=r["grievance"], since=r["since"], text=r["text"], **_short(r.get("knowledge")),
                             model=R["rel"].get("|".join(sorted((r["a"], r["b"])))))
                        for r in W["relations_anchor"]]
    out["unknowns"] = W.get("unknowns", [])
    out["demography_sensitivity"] = dict(R["sens"], **MODEL)
    return out


def _short(kn):
    if not kn:
        return dict(status="PROPOSAL", visibility="PUBLIC_RECORD")
    return dict(status=kn["status"], visibility=kn["visibility"])


def proposals_md(R):
    import knowledge
    W = R["world"]
    MS = {"missions": list(R["missions"].values())}
    items = knowledge.collect(MS, W)
    c = knowledge.counts(items)
    L = ["# Предложения Claude (PROPOSAL) и скрытые / неизвестные факты", "",
         "Генерируется `lab.py` из статусов `canon/*.json` (knowledge.py). CANON = есть в заметках автора; PROPOSAL = добавлено/пересчитано Claude или является результатом модели; "
         "UNKNOWN = намеренно без ответа. Видимость: PUBLIC_RECORD / AUTHOR_TRUTH.", "",
         "Счётчики: " + ", ".join(f"{k} {v}" for k, v in c.items()), "",
         "Все выходы модели (население 3126, экономика, коридоры, технологии) — PROPOSAL (model) и в этот список не перечисляются поштучно.", ""]
    groups = {}
    for it in items:
        if it["status"] == "CANON" and it["visibility"] == "PUBLIC_RECORD":
            continue
        g = it["path"].split("/")[0]
        groups.setdefault(g, []).append(it)
    for g in sorted(groups):
        L += [f"## {g} ({len(groups[g])})", "", "| путь | статус | видимость | основание | что |", "|---|---|---|---|---|"]
        for it in groups[g]:
            L.append(f"| {it['path']} | {it['status']} | {it['visibility']} | {it['basis']} | {str(it['text'])[:120].replace('|', '/')} |")
        L.append("")
    sp = load_settlements()["polities"]
    L += [f"## settlements ({sum(len(v) for v in sp.values())})", "", "Все поселения и площадки в `canon/settlements.json` — PROPOSAL (на телах каталога).", ""]
    return "\n".join(L) + "\n"


def check(path_json, R=None, lab=None):
    """Lore export must match the canon: missions, statuses, authored values; and (with R) be byte-identical
    to a fresh build."""
    with open(path_json, encoding="utf-8") as f:
        J = json.load(f)
    here = HERE
    with open(os.path.join(here, "canon", "missions.json"), encoding="utf-8") as f:
        MS = json.load(f)
    with open(os.path.join(here, "canon", "world.json"), encoding="utf-8") as f:
        W = json.load(f)
    problems = []
    by = {m["id"]: m for m in J["missions"]}
    for m in MS["missions"]:
        x = by.get(m["id"])
        if x is None:
            problems.append(f"{m['id']}: нет в lore_facts")
            continue
        if x["launch"]["year"] != m["launch_year"]:
            problems.append(f"{m['id']}: launch {x['launch']['year']} != {m['launch_year']}")
        if x["sponsor"] != m["sponsor"]:
            problems.append(f"{m['id']}: sponsor расходится")
        if x["start"]["population"] != m["population"]:
            problems.append(f"{m['id']}: start population расходится")
        if x["status"] != m["knowledge"]["status"] or x["visibility"] != m["knowledge"]["visibility"]:
            problems.append(f"{m['id']}: статус {x['status']}/{x['visibility']} != канон {m['knowledge']['status']}/{m['knowledge']['visibility']}")
        arr = next((a for a in m["anchors"] if a["type"] == "arrival"), None)
        if arr and (x["arrival"] is None or x["arrival"]["year"] != arr["year"] or x["arrival"]["population"] != arr["pop_after"]):
            problems.append(f"{m['id']}: прибытие расходится")
        for a in m["anchors"]:
            row = next((r for r in x["chronology"] if r["year"] == a["year"] and r["event"] == a["type"]), None)
            if row is None:
                problems.append(f"{m['id']}: якорь {a['year']} {a['type']} не экспортирован")
            elif row["status"] != a["knowledge"]["status"]:
                problems.append(f"{m['id']}: якорь {a['year']} {a['type']} статус {row['status']} != {a['knowledge']['status']}")
    if len(by) != len(MS["missions"]):
        problems.append(f"миссий в экспорте {len(by)} != {len(MS['missions'])}")
    pols = {p["id"]: p for p in J["polities"]}
    for c in W["colonies"]:
        p = pols.get(c["id"])
        if p is None:
            problems.append(f"{c['id']}: колонии нет в lore_facts")
            continue
        if p["population_3126"]["authored"] != c["pop_3126_authored"]:
            problems.append(f"{c['id']}: авторское население расходится")
        if p["status"] != c["knowledge"]["status"]:
            problems.append(f"{c['id']}: статус расходится")
    for rec in J["polities"]:
        for s in rec.get("settlements", []):
            if s["knowledge"]["status"] not in ("CANON", "PROPOSAL", "UNKNOWN"):
                problems.append(f"{s['id']}: нет статуса")
    if R is not None and lab is not None:
        fresh = json.dumps(build(R, lab), ensure_ascii=False, indent=1, sort_keys=True)
        with open(path_json, encoding="utf-8") as f:
            if f.read() != fresh:
                problems.append("lore_facts.json устарел: перезапустите python lab.py")
    print("lore check:", "OK" if not problems else f"{len(problems)} проблем")
    for p in problems:
        print("  -", p)
    return 0 if not problems else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--propose-settlements", action="store_true")
    a = ap.parse_args()
    if a.propose_settlements:
        import endowment
        with open(os.path.join(HERE, "canon", "world.json"), encoding="utf-8") as f:
            W = json.load(f)
        doc = propose_settlements(W, endowment.load_endowment())
        with open(SETTLEMENTS, "w", encoding="utf-8", newline="\n") as f:
            f.write(json.dumps(doc, ensure_ascii=False, indent=1) + "\n")
        print("settlements:", sum(len(v) for v in doc["polities"].values()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
