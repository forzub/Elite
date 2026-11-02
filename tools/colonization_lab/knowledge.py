"""Knowledge status of every fact / entity / event in the machine canon.

Two independent layers:
  status      CANON     material of the author's source notes (Notes/History/generation ships/*,
                        Notes/History/generation ships.md, Notes/fate.md, Notes/History/history.md,
                        src/assets/data/galaxy_details)
              PROPOSAL  Claude's additions and recalculations (reconciliation, model inputs, model outputs)
              UNKNOWN   deliberately unanswered, even for the author
  visibility  PUBLIC_RECORD  what the world of 3126 knows / may know
              AUTHOR_TRUTH   hidden truth known to the author (and the lab), not to the world

Rule for events (auditable, not taste): an anchor is CANON when its year occurs in the author
source files of its mission; a recalculated value keeps the authored one in `authored` and is
PROPOSAL; explicit reconciliation inventions are listed in OVERRIDES; `hidden_truth` makes the
visibility AUTHOR_TRUTH; `author_knows: false` makes the fate UNKNOWN.

usage:
  python knowledge.py            report counts (no write)
  python knowledge.py --write    (re)annotate canon/*.json in place (idempotent)
"""
import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
NOTES = os.path.join(ROOT, "Notes")
GS_DIR = os.path.join(NOTES, "History", "generation ships")
SRC_A = os.path.join(NOTES, "History", "generation ships.md")
SRC_B = os.path.join(GS_DIR, "generation ships.md")
FATE = os.path.join(NOTES, "fate.md")
HISTORY = os.path.join(NOTES, "History", "history.md")

C, P, U = "CANON", "PROPOSAL", "UNKNOWN"
PUB, AT = "PUBLIC_RECORD", "AUTHOR_TRUTH"

# reconciliation inventions that the year rule would not catch (COLONIZATION_CANON_RECONCILIATION.md)
OVERRIDES = {
    ("UHURU-2", 2276, "launch_disaster"): (P, PUB, "реконсиляция A4: пожар в доке объясняет номер «-2»"),
    ("NOMAD-1", 2490, "return_timer"): (P, PUB, "реконсиляция E6: таймер возврата (воспоминание автора, не найдено в заметках)"),
    ("NOMAD-1", 2497, "refused"): (P, PUB, "реконсиляция E6"),
    ("NEW-JERUSALEM-1", 2291, "target_change"): (P, PUB, "реконсиляция B9: смена цели на разгоне"),
    ("UBUNTU-1", 2790, "found"): (P, PUB, "реконсиляция B20: обломки найдены гипер-разведкой"),
    ("GAIA-2", 2800, "arrival"): (P, AT, "реконсиляция B14: люди исчезли после посадки, сводки шлёт ИИ"),
    ("FREE-VENTURE-1", 2405, "arrival"): (P, PUB, "реконсиляция A5: цель перенесена на Эпсилон Эридана"),
}
MISSION_PROPOSAL = {"GENESIS-1": "реконсиляция A2 (идея старой ветки, числа Claude)",
                    "TRADERS-GUILD-1": "реконсиляция A6 (идея старой ветки)",
                    "LAST-LIGHT-1": "реконсиляция A8 (идея старой ветки, груз — Claude)",
                    "FREE-VENTURE-1": "реконсиляция A5 (имя есть у автора, паспорт — Claude)"}
AUTHOR_TRUTH_MISSIONS = {"GAIA-2": "живые «тихие» и пустой корабль", "DEEP-SEEK-1": "уничтожен автономным роем CD-2",
                         "LAST-LIGHT-1": "груз: архив Хартий и верительные грамоты Земли", "GREEN-ARK-1": "симбионт из корабельного микробиома"}
UNKNOWNS = [
    dict(id="IRON-SHIELD-2.cause", text="что случилось с IRON-SHIELD-2 (3 000 человек, 2355)", status=U, visibility=AT),
    dict(id="NIRVANA-1.fate", text="судьба и положение NIRVANA-1", status=U, visibility=AT),
    dict(id="UBUNTU-1.cause", text="причина гибели UBUNTU-1 («огни»)", status=U, visibility=AT),
    dict(id="FAREWELL-1.people", text="судьба людей FAREWELL-1", status=U, visibility=AT),
    dict(id="EARTH.jump_cores", text="умеет ли Земля 3126 делать собственные ядра прыжка (только маяк из памятника HYPERION-1)", status=U, visibility=AT),
]


def k(status, vis, basis):
    return {"status": status, "visibility": vis, "basis": basis}


def _read(p):
    try:
        with open(p, encoding="utf-8") as f:
            return f.read()
    except OSError:
        return ""


def corpus():
    files = sorted(glob.glob(os.path.join(GS_DIR, "*.md")))
    texts = {os.path.basename(p): _read(p) for p in files}
    texts["fate.md"] = _read(FATE)
    texts["history.md"] = _read(HISTORY)
    texts["generation ships.md (A)"] = _read(SRC_A)
    return texts


def mission_texts(m, T):
    out = {}
    for s in m.get("sources", []):
        name = s.split(" §")[0].split(" (")[0].strip()
        if name in T and name != "generation ships.md":
            out[name] = T[name]
        elif s.startswith("generation ships.md (A)"):
            out["generation ships.md (A)"] = T["generation ships.md (A)"]
        elif s.startswith("generation ships.md"):
            out["generation ships.md"] = T.get("generation ships.md", "")
    out.setdefault("fate.md", T["fate.md"])
    return out


def _year_in(year, texts):
    y = str(year)
    return sorted(n for n, t in texts.items() if y in t)


def annotate_missions(MS, T):
    for m in MS["missions"]:
        mt = mission_texts(m, T)
        own = {n: t for n, t in mt.items() if n[:2].isdigit()}
        named = any(m["id"] in t or any(a in t for a in m.get("aliases", [])) for t in T.values())
        if m["id"] in MISSION_PROPOSAL:
            m["knowledge"] = k(P, AT if m["id"] in AUTHOR_TRUTH_MISSIONS else PUB, MISSION_PROPOSAL[m["id"]])
        else:
            m["knowledge"] = k(C if named else P, PUB, "миссия есть в заметках автора" if named else "нет в заметках автора")
        fs = {}
        ly = _year_in(m["launch_year"], own or mt)
        fs["launch_year"] = k(C if ly and m["id"] not in MISSION_PROPOSAL else P, PUB, ("год в " + ", ".join(ly[:2])) if ly else "год не найден в источниках миссии")
        fs["population"] = k(C if (m["id"] not in MISSION_PROPOSAL and any(f"{m['population']:,}".replace(",", " ") in t or str(m["population"]) in t for t in (own or mt).values())) else P, PUB,
                             "число из досье (реконсиляция C1: стартовые численности по S1)")
        arr = next((a for a in m["anchors"] if a["type"] == "arrival"), None)
        if arr:
            if "authored" in arr:
                fs["arrival_year"] = k(P, PUB, f"пересчитано по каталогу и потолку скорости; авторское {arr['authored']}")
            else:
                yy = _year_in(arr["year"], mt)
                fs["arrival_year"] = k(C if yy and m["id"] not in MISSION_PROPOSAL else P, PUB, ("год в " + ", ".join(yy[:2])) if yy else "не найдено в источниках")
            fs["arrival_population"] = k(C if any(f"{arr['pop_after']:,}".replace(",", " ") in t for t in mt.values()) else P, PUB,
                                         "fate.md" if any(f"{arr['pop_after']:,}".replace(",", " ") in t for t in mt.values()) else "модель вахты/крио (реконсиляция C2)")
        fate = m["fate"]
        if fate.get("author_knows") is False:
            fs["fate"] = k(U, AT, "неизвестно даже автору")
        elif m["id"] in AUTHOR_TRUTH_MISSIONS:
            fs["fate"] = k(P, AT, "скрытая правда (реконсиляция): " + AUTHOR_TRUTH_MISSIONS[m["id"]])
        else:
            fs["fate"] = k(C if m["id"] not in MISSION_PROPOSAL else P, PUB, "fate.md / досье")
        m["field_status"] = fs
        for a in m["anchors"]:
            key = (m["id"], a["year"], a["type"])
            if key in OVERRIDES:
                st, vis, basis = OVERRIDES[key]
            elif a["type"] == "arrival":
                st, vis, basis = fs["arrival_year"]["status"], PUB, fs["arrival_year"]["basis"]
            elif m["id"] in MISSION_PROPOSAL:
                st, vis, basis = P, PUB, "событие миссии-предложения"
            elif "(provisional)" in a.get("text", ""):
                st, vis, basis = P, PUB, "помечено provisional"
            else:
                yy = _year_in(a["year"], mt)
                st, vis, basis = (C, PUB, "год в " + ", ".join(yy[:2])) if yy else (P, PUB, "год не найден в источниках миссии")
            if a.get("hidden_truth"):   # the event itself is public (official version); its explanation is hidden
                a["hidden_truth_knowledge"] = k(P, AT, "объяснение реконсиляции; публично — официальная версия")
            a["knowledge"] = k(st, vis, basis)


def annotate_world(W, T, MS):
    canon_missions = {m["id"]: m["knowledge"]["status"] for m in MS["missions"]}
    fate = T["fate.md"]
    every = "\n".join(T.values())
    for c in W["colonies"]:
        first = c["founders"][0]
        st = canon_missions.get(first, P)
        if c.get("founded_by_split") or c["id"] in ("eps_eri_strife", "gj832_gaia_zone", "gj581_freeclans", "delta_pav_freekarma"):
            st = C if canon_missions.get(first) == C else P
        c["knowledge"] = k(st, PUB, f"основатель {first}: {canon_missions.get(first, P)}")
        pa = c.get("pop_3126_authored")
        pa_s = f"{pa:,}".replace(",", " ") if pa else ""
        fs = {"pop_3126_authored": k(C if pa_s and pa_s in fate else P, PUB, "fate.md" if pa_s and pa_s in fate else "реконсиляция (разделение/пересчёт)"),
              "regime_3126": k(P, PUB, "машинная метка режима по досье (Claude)"),
              "assets": k(P, PUB, "активы и их важность — реконсиляция/экономика"),
              "culture_drift": k(P, PUB, "гипотеза Colonycore v2"),
              "demography": k(P, PUB, "входы причинной демографии (допущения F1–F12)"),
              "pop_3126_model": k(P, PUB, "результат модели demography.py, не канон")}
        if c.get("name") and c["name"] in every:
            fs["name"] = k(C, PUB, "имя есть в заметках автора")
        else:
            fs["name"] = k(P, PUB, "имя дано реконсиляцией / старой веткой")
        c["field_status"] = fs
    for o in W["outposts"]:
        o["knowledge"] = k(P, PUB, "аванпост гипер-эпохи (реконсиляция)")
    for o in W.get("frontier_pools", []):
        o["knowledge"] = k(P, PUB, "фронтир у маяка (вход демографии, Claude)")
    hyp = T.get("31. HYPERION-1.md", "") + fate
    for b in W["beacons"]:
        b["knowledge"] = k(C if str(b["year"]) in hyp and b["system"] in (8,) else P, PUB, "год и оператор маяка — реконсиляция")
    for x in W.get("extra_links", []):
        x["knowledge"] = k(P, PUB, "реконсиляция")
    for r in W["relations_anchor"]:
        yy = str(r["since"]) in every
        r["knowledge"] = k(C if yy else P, PUB, ("событие в заметках; " if yy else "") + "величина обиды — Claude")
    for e in W["jump_chronology"]:
        yy = str(e["year"]) in every
        prop_words = ("Конвенция", "Кордон", "теорему", "концессия", "Концессия", "маяк и флотскую", "UBUNTU")
        st = C if yy and not any(w in e["event"] for w in prop_words) else P
        e["knowledge"] = k(st, PUB, "год в заметках автора" if st == C else "реконсиляция (часть III)")
    W["earth_3126"]["knowledge"] = k(P, PUB, "Земля 3126 — предложение для автора (§10 справочника)")
    for d in W.get("demography_events", []):
        d["knowledge"] = k(d["status"], PUB, d["basis"])
    for j in W.get("jump_technology", []):
        j["knowledge"] = k(j["status"], AT if j["how"] == "state_secret" else PUB, j["basis"])
    W["unknowns"] = UNKNOWNS


def collect(MS, W, CU=None):
    """Flat list of every status-carrying item: (path, status, visibility, basis, text)."""
    items = []

    def add(path, kn, text):
        items.append(dict(path=path, status=kn["status"], visibility=kn["visibility"], basis=kn.get("basis", ""), text=text))
    for m in MS["missions"]:
        add(f"missions/{m['id']}", m["knowledge"], m["sponsor"])
        for f, kn in sorted(m.get("field_status", {}).items()):
            add(f"missions/{m['id']}/{f}", kn, f)
        for a in m["anchors"]:
            add(f"missions/{m['id']}/anchors/{a['year']}/{a['type']}", a["knowledge"], a["text"])
            if "hidden_truth_knowledge" in a:
                add(f"missions/{m['id']}/anchors/{a['year']}/{a['type']}/hidden_truth", a["hidden_truth_knowledge"], a["hidden_truth"])
    for c in W["colonies"]:
        add(f"colonies/{c['id']}", c["knowledge"], c["name"])
        for f, kn in sorted(c.get("field_status", {}).items()):
            add(f"colonies/{c['id']}/{f}", kn, f)
    for o in W["outposts"]:
        add(f"outposts/{o['id']}", o["knowledge"], o["name"])
    for o in W.get("frontier_pools", []):
        add(f"frontier_pools/{o['id']}", o["knowledge"], o["name"])
    for b in W["beacons"]:
        add(f"beacons/{b['system']}@{b['year']}", b["knowledge"], b["operator"])
    for x in W.get("extra_links", []):
        add(f"extra_links/{x['a']}-{x['b']}", x["knowledge"], x["note"])
    for r in W["relations_anchor"]:
        add(f"relations/{r['a']}|{r['b']}", r["knowledge"], r["text"])
    for e in W["jump_chronology"]:
        add(f"jump_chronology/{e['year']}", e["knowledge"], e["event"])
    add("earth_3126", W["earth_3126"]["knowledge"], W["earth_3126"]["name"])
    for d in W.get("demography_events", []):
        add(f"demography_events/{d['year']}/{d['unit']}/{d['type']}", d["knowledge"], d["basis"])
    for j in W.get("jump_technology", []):
        add(f"jump_technology/{j['unit']}@{j['year']}", j["knowledge"], j["basis"])
    for u in W.get("unknowns", []):
        add(f"unknowns/{u['id']}", dict(status=u["status"], visibility=u["visibility"], basis="намеренно без ответа"), u["text"])
    if CU is not None:
        add("cultures", CU.get("knowledge", k(P, PUB, "гипотезы")), "культурные априори Colonycore v2")
    return items


def counts(items):
    out = {}
    for it in items:
        out[it["status"]] = out.get(it["status"], 0) + 1
        out[it["visibility"]] = out.get(it["visibility"], 0) + 1
    return dict(sorted(out.items()))


def _dump(path, d):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(d, ensure_ascii=False, indent=1) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    a = ap.parse_args()
    T = corpus()
    pm, pw, pc = (os.path.join(HERE, "canon", n) for n in ("missions.json", "world.json", "cultures.json"))
    with open(pm, encoding="utf-8") as f:
        MS = json.load(f)
    with open(pw, encoding="utf-8") as f:
        W = json.load(f)
    with open(pc, encoding="utf-8") as f:
        CU = json.load(f)
    annotate_missions(MS, T)
    annotate_world(W, T, MS)
    CU["knowledge"] = k(P, PUB, "культурные априори — гипотезы Claude (Colonycore v2), не канон автора")
    items = collect(MS, W, CU)
    print(counts(items))
    if a.write:
        # keep missions.json formatting (indent=1, trailing newline) like the existing files
        _dump(pm, MS)
        _dump(pw, W)
        with open(pc, encoding="utf-8") as f:
            raw = f.read()
        if '"knowledge"' not in raw:   # cultures.json is hand-formatted: insert one line, keep the layout
            line = '  "knowledge": ' + json.dumps(CU["knowledge"], ensure_ascii=False) + ",\n"
            i = raw.index('  "axes"')
            with open(pc, "w", encoding="utf-8", newline="\n") as f:
                f.write(raw[:i] + line + raw[i:])
    return 0


if __name__ == "__main__":
    sys.exit(main())
