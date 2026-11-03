"""Generator of the developer lore reference Notes/Lore/ (Markdown) from the machine canon.

The reference is a human view of the data; it can never contradict it:

  machine canon (canon/*.json -> out/lore_facts.json, out/lab_state_3126.json)
      -> generated blocks in Notes/Lore/**.md   (this script, never edited by hand)
      -> hand-written narrative around them      (writers; preserved untouched)

Generated blocks are delimited by

    <!-- GENERATED:<block-id> BEGIN -->
    ...
    <!-- GENERATED:<block-id> END -->

Everything outside the markers is preserved. A missing file is created as a skeleton:
title, status legend block, generated blocks and empty narrative sections marked
`<!-- NARRATIVE:<section> -->` with a short TODO for the writer.

usage (from tools/colonization_lab):
  python lore_md.py --write    create / refresh every file (idempotent)
  python lore_md.py --check    exit 1 if a generated block differs from the data, a block is
                               missing / stale / malformed, a file references an unknown entity
                               id, or a hand-written line contradicts a population figure

--check heuristics (deliberately simple):
  * entity ids: (a) tokens shaped like a mission id (`ABC-1`, `IRON-SHIELD-2`) anywhere in the
    file and (b) back-quoted snake_case ids (`tau_ceti_cognium`, `wolf359_citadel.bastion`) must
    occur in the data (as an id, alias, key or string value); (c) relative links to .md files
    inside Notes/Lore must resolve.
  * populations: every line OUTSIDE generated blocks containing a number followed by
    'млрд' / 'млн' / 'тыс.' and the name of an entity (polity / colony name, mission id or
    alias, 'Земля') must match one of that entity's known figures (model 3126, authored,
    model checkpoints, mission start / arrival / chronology, Earth trajectory and anchors)
    within the rounding of the written number (half a unit of its last digit).
    The upper end of a range ("10–33 млн") is not checked.
    A line with `lore-check: ignore` is skipped (use it to quote an outdated figure on purpose).
  * public-safety: files outside AUTHOR/ must not mention missions whose existence is
    AUTHOR_TRUTH.

Only the Python 3 standard library; deterministic output (sorted iteration, no clocks).
"""
import argparse
import json
import os
import posixpath
import re
import sys

import endowment
import lore_economy
import lore_sites

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
LORE = os.path.join(ROOT, "Notes", "Lore")
OUT = os.path.join(HERE, "out")
CANON = os.path.join(HERE, "canon")

sys.path.insert(0, HERE)

# ---------------------------------------------------------------- vocabulary
VIS = {"PUBLIC_RECORD": "PUBLIC", "AUTHOR_TRUTH": "AUTHOR TRUTH"}
GOODS = {"biotech": "биотех", "components": "компоненты", "drive_cores": "ядра прыжка", "electronics": "электроника",
         "food": "пища", "he3": "He-3", "hulls": "корпуса", "isotopes": "изотопы", "metals": "металлы",
         "munitions": "боеприпасы", "precision": "точная механика", "volatiles": "вода/летучие"}
BULK = ("volatiles", "metals", "food")
REASONS = {"specialisation_high_value": "специализация: дорогой лёгкий товар",
           "industrial_specialisation_military": "военная специализация",
           "state_monopoly_or_own_design": "госмонополия / своя конструкция",
           "local_source_dearer": "свой источник дороже доставленного импорта",
           "same_system": "внутрисистемная торговля",
           "residual_shortfall": "остаточный дефицит",
           "no_local_source": "нет своего источника"}
ROLES = {"capital": "столица", "habitat": "поселение", "gas_platform": "газовые платформы (He-3)",
         "ice_export": "ледовый экспорт", "mine": "рудник", "refinery": "переработка", "food_habitat": "агрохабитат",
         "factory": "завод", "electronics_fab": "фаб электроники", "shipyard": "верфь", "arsenal_shipyard": "арсенал и военная верфь",
         "fortress": "крепость", "research": "наука", "logistics_hub": "логистический хаб", "legal_hub": "правовой хаб",
         "jump_infrastructure": "инфраструктура прыжка", "intel": "разведка", "black_market": "чёрный рынок",
         "station_core": "ядро станции", "strategic_mine": "стратегический рудник", "fleet": "флот и мобилизационный резерв"}
KINDS = {"colony": "колония", "outpost": "аванпост", "frontier": "фронтир"}
TECH_ST = {"imported": "импорт", "state_secret": "гостайна", "kept": "сохранена", "lost": "утрачена", "restored": "восстановлена",
           "degraded": "деградировала", "licensed": "лицензия", "stolen": "кража", "own": "своя"}
DOMAINS = ("automation", "bio", "construction", "electronics", "energy", "extraction", "jump", "manufacturing",
           "metallurgy", "military", "research", "shipbuilding")
EXTRACTION = ("mine", "strategic_mine", "refinery", "gas_platform", "ice_export")
YARDS = ("shipyard", "arsenal_shipyard")
SHIP_INPUTS = ("metals", "components", "electronics", "precision", "drive_cores")
CONFLICT_EVENTS = ("class_conflict", "purge", "mutiny", "mutiny_attempt", "war", "skirmish", "factional_conflict",
                   "religious_conflict", "resource_conflict", "security_takeover", "secession", "sabotage_alleged",
                   "civil_war", "water_war", "exodus", "schism", "rivalry", "refusal", "refused")
LOST_EVENTS = ("lost", "contact_lost", "signal", "navigation_failure", "launch_disaster", "anomaly", "flyby", "news", "contact")


# ---------------------------------------------------------------- data
def _load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


class Data:
    def __init__(self):
        self.F = _load(os.path.join(OUT, "lore_facts.json"))
        self.S = _load(os.path.join(OUT, "lab_state_3126.json"))
        self.MS = _load(os.path.join(CANON, "missions.json"))
        self.W = _load(os.path.join(CANON, "world.json"))
        self.CU = _load(os.path.join(CANON, "cultures.json"))
        self.ST = _load(os.path.join(CANON, "settlements.json"))
        self.missions = {m["id"]: m for m in self.F["missions"]}
        self.mission_order = [m["id"] for m in self.F["missions"]]
        self.cm = {m["id"]: m for m in self.MS["missions"]}
        self.pol = {p["id"]: p for p in self.F["polities"]}
        self.pol_order = sorted(self.pol)
        self.wcol = {c["id"]: c for c in self.W["colonies"]}
        self.wout = {o["id"]: o for o in self.W["outposts"] + self.W.get("frontier_pools", [])}
        self.assets = {a["id"]: a for a in self.F["industrial_assets"]}
        self.sysname = {0: "Sol"}
        for p in self.F["polities"]:
            self.sysname[p["system"]["id"]] = p["system"]["name"]
        for m in self.F["missions"]:
            if isinstance(m["destination"]["system"], int):
                self.sysname[m["destination"]["system"]] = m["destination"]["name"]
        for r in self.F["earth"]["return_3126"]["earth_jump_reach"]:
            self.sysname[r["system"]] = r["name"]
        self.unknowns = {u["id"]: u for u in self.F["unknowns"]}
        # economy / logistics / defence layer (world_economy.py), population history
        self.X = lore_economy.load()
        self.SX = lore_sites.load()
        self.ST_E = endowment.load_endowment()
        with open(os.path.join(CANON, "population_history.json"), encoding="utf-8") as f:
            self.PH = json.load(f)

    # -- visibility
    def mission_public(self, mid):
        return self.missions[mid]["visibility"] == "PUBLIC_RECORD"

    def lost(self, mid):
        f = self.missions[mid]["fate"]
        return f.get("visibility") == "AUTHOR_TRUTH" or f.get("status") == "UNKNOWN"

    def sname(self, sid):
        if sid is None:
            return "—"
        return self.sysname.get(sid, str(sid))

    def pname(self, uid):
        if uid == "sol_authority":
            return self.F["earth"]["anchor"]["name"]
        p = self.pol.get(uid)
        return p["name"] if p else str(uid)


# ---------------------------------------------------------------- formatting
def tag(status, vis="PUBLIC_RECORD"):
    return f"{status or 'PROPOSAL'} · {VIS.get(vis or 'PUBLIC_RECORD', vis)}"


def tagk(o):
    if not o:
        return tag("PROPOSAL")
    return tag(o.get("status"), o.get("visibility"))


MODEL_TAG = tag("PROPOSAL") + " (модель)"


def num(x):
    if x is None:
        return "—"
    return f"{int(round(x)):,}".replace(",", " ")


def fl(x, d=2):
    if x is None:
        return "—"
    return f"{x:.{d}f}"


def mln(x):
    if x is None:
        return "—"
    if x >= 1e9:
        return f"{x / 1e9:.2f} млрд"
    if x >= 1e6:
        return f"{x / 1e6:.2f} млн"
    if x >= 1e3:
        return f"{x / 1e3:.1f} тыс."
    return num(x)


def cell(c):
    if c is None or c == "":
        return "—"
    return str(c).replace("|", "/").replace("\n", " ")


def table(head, rows):
    L = ["| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for r in rows:
        L.append("| " + " | ".join(cell(c) for c in r) + " |")
    return "\n".join(L)


def link(cur, target, text):
    rel = posixpath.relpath(target, posixpath.dirname(cur) or ".")
    return f"[{text}]({rel})"


def slug(s):
    s = s.lower()
    out = []
    for ch in s:
        out.append(ch if ("a" <= ch <= "z" or "0" <= ch <= "9") else "_")
    return re.sub("_+", "_", "".join(out)).strip("_")


def anchor(h):
    """GitHub-style heading anchor (enough for our ids)."""
    h = h.strip().lower()
    h = re.sub(r"[^\w\- ]", "", h)
    return h.replace(" ", "-")


# ---------------------------------------------------------------- document model
class Doc:
    def __init__(self, path, title, kind):
        self.path = path            # relative to Notes/Lore, posix
        self.title = title
        self.kind = kind            # file type label (for the placeholder report)
        self.parts = []

    def text(self, s):
        self.parts.append(("text", s))

    def gen(self, bid, content):
        self.parts.append(("gen", bid, content))

    def narr(self, sec, heading, todo, level=2):
        self.parts.append(("narr", sec, heading, todo, level))

    def blocks(self):
        return [(p[1], p[2]) for p in self.parts if p[0] == "gen"]


def block(bid, content):
    return f"<!-- GENERATED:{bid} BEGIN -->\n{content.rstrip()}\n<!-- GENERATED:{bid} END -->"


def render_new(doc):
    L = [f"# {doc.title}"]
    for p in doc.parts:
        if p[0] == "text":
            L.append(p[1].rstrip())
        elif p[0] == "gen":
            L.append(block(p[1], p[2]))
        else:
            _, sec, heading, todo, level = p
            L.append(f"{'#' * level} {heading}\n\n<!-- NARRATIVE:{sec} -->\n_TODO: {todo}_")
    return "\n\n".join(L) + "\n"


BLOCK_RE = re.compile(r"<!-- GENERATED:([A-Za-z0-9_.:\-]+) BEGIN -->\n(.*?)<!-- GENERATED:\1 END -->", re.S)
BEGIN_RE = re.compile(r"<!-- GENERATED:([A-Za-z0-9_.:\-]+) BEGIN -->")


def refresh(old, doc):
    want = dict(doc.blocks())
    seen, stale = [], []

    def sub(m):
        bid = m.group(1)
        if bid in want and bid not in seen:
            seen.append(bid)
            return block(bid, want[bid])
        stale.append(bid)
        return ""
    new = BLOCK_RE.sub(sub, old)
    missing = [b for b, _ in doc.blocks() if b not in seen]
    if missing:
        new = new.rstrip("\n") + "\n\n" + "\n\n".join(block(b, want[b]) for b in missing) + "\n"
    malformed = len(BEGIN_RE.findall(old)) != len(BLOCK_RE.findall(old))
    return new, missing, stale, malformed


# ---------------------------------------------------------------- shared blocks
def legend_block(D):
    sl = D.F["status_legend"]
    return ("> **Статусы строк:** `CANON` — " + sl["CANON"] + "; `PROPOSAL` — " + sl["PROPOSAL"] +
            "; `UNKNOWN` — " + sl["UNKNOWN"] + ". **Видимость:** `PUBLIC` — " + sl["PUBLIC_RECORD"] +
            "; `AUTHOR TRUTH` — " + sl["AUTHOR_TRUTH"] + ".\n>\n> Блоки `GENERATED` пишет только "
            "`tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.")


def new_doc(D, path, title, kind, intro=None):
    d = Doc(path, title, kind)
    d.gen("legend", legend_block(D))
    if intro:
        d.text(intro)
    return d


# ---------------------------------------------------------------- SHIPS
def ship_path(D, mid):
    return f"SHIPS/{mid}.md" if D.mission_public(mid) else f"AUTHOR/SHIPS/{mid}.md"


def significant(rows):
    out, prev = [], None
    for i, r in enumerate(rows):
        keep = (r["source"] != "simulated" or (r.get("deaths") or 0) >= max(50, 0.01 * (r.get("population") or 0))
                or (prev is not None and r.get("regime") and r.get("regime") != prev) or i == len(rows) - 1)
        if r.get("regime"):
            prev = r["regime"]
        if keep:
            out.append(r)
    return out


def ship_doc(D, mid, author):
    m = D.missions[mid]
    c = D.cm[mid]
    path = ship_path(D, mid)
    d = new_doc(D, path, f"{mid} — {m['aliases'][0]}", "ship",
                "> [СПОЙЛЕР] Существование этой миссии — AUTHOR TRUTH. Файл лежит в AUTHOR/ и не цитируется в публичных разделах."
                if not D.mission_public(mid) else None)
    pub = lambda o: author or (o or {}).get("visibility", "PUBLIC_RECORD") == "PUBLIC_RECORD"
    # -- general
    arr = m.get("arrival")
    col = m.get("colony")
    rows = [("ID / имена", ", ".join([mid] + [a for a in m["aliases"] if a != mid]), tag(m["status"], m["visibility"])),
            ("Волна", m["wave"], tag(m["status"], m["visibility"])),
            ("Спонсор", m["sponsor"], tag(m["status"], m["visibility"])),
            ("Старт", f"{m['launch']['year']}, {D.sname(m['origin_system'])}", tagk(m["launch"])),
            ("Цель", f"{m['destination']['name']}", tag(m["status"], m["visibility"]))]
    if arr and pub(arr):
        rows.append(("Прибытие", f"{arr['year']}" + (f" (в досье было {arr['authored_year']})" if arr.get("authored_year") else ""), tagk(arr)))
        rows.append(("Длительность полёта, лет", m.get("duration_years"), tagk(arr)))
    if col and col in D.pol:
        rows.append(("Колония", link(path, f"COLONIES/{col}.md", D.pname(col)), tagk(D.pol[col])))
    rows.append(("Источники", "; ".join(m["sources"]), "—"))
    gen = table(["Поле", "Значение", "Статус"], rows)
    d.text("## Общие данные")
    d.gen("summary", gen)
    # -- causes
    mot = sorted(c.get("motives", {}).items(), key=lambda kv: (-kv[1], kv[0]))
    cul = sorted(c.get("cultures", {}).items(), key=lambda kv: (-kv[1], kv[0]))
    g = ["Машинная разметка досье (у этих полей нет собственного статуса; статус унаследован от миссии: "
         f"`{tag(m['status'], m['visibility'])}`).", "",
         table(["Мотив", "Вес"], [(k, fl(v)) for k, v in mot]), "",
         "Культуры набора: " + ", ".join(f"{k} {fl(v)}" for k, v in cul) + "."]
    if c.get("dark_comedy"):
        g += ["", f"Чёрная комедия миссии: {c['dark_comedy']}"]
    d.text("## Причины полёта")
    d.gen("motives", "\n".join(g))
    d.narr("causes", "Почему полетели (рассказ)", "кто и зачем снарядил корабль, общественный фон эпохи старта, что обещали людям; опираться на мотивы выше и на досье из «Источники».", level=3)
    # -- ship
    sn = D.S["sanity"].get(mid)
    cmass = c.get("claimed_mass_mt", {})
    rows = [("Заявленная масса, Мт (полная / сухая)", f"{cmass.get('full', '—')} / {cmass.get('dry', '—')}", tag(m["status"], m["visibility"]) + " (досье)")]
    if sn:
        ds = sn["design"]
        rows += [("Эра двигателя", ds["era"], MODEL_TAG),
                 ("Крейсерская скорость, c", fl(sn["cruise_c"], 3), MODEL_TAG),
                 ("Расчётная масса, Мт (полная / сухая)", f"{fl(ds['full_mt'], 1)} / {fl(ds['dry_mt'], 1)}", MODEL_TAG),
                 ("Отношение масс", fl(ds["mass_ratio"], 1), MODEL_TAG),
                 ("Δv, c", fl(ds["dv_c"], 3), MODEL_TAG),
                 ("Топливо He-3, Мт", fl(ds["he3_mt"], 1), MODEL_TAG),
                 ("Ускорение разгона, g", fl(ds["accel_g"], 4), MODEL_TAG),
                 ("Мощность струи, ПВт", fl(ds["jet_power_pw"], 1), MODEL_TAG),
                 ("Мощность на крейсерском участке, ГВт", fl(ds["coast_power_gw"], 2), MODEL_TAG),
                 ("Радиаторы разгона / крейсера, км²", f"{fl(ds['boost_radiator_km2'], 1)} / {fl(ds['coast_radiator_km2'], 2)}", MODEL_TAG),
                 ("Вердикт physics.py", sn["verdict"] + ("" if not sn["reasons"] else ": " + "; ".join(sn["reasons"])), MODEL_TAG)]
    else:
        rows.append(("Физическая проверка", "нет расчёта physics.py для этой миссии", "—"))
    rows.append(("Доля крио", fl(c.get("cryo_share"), 2), tagk(m["start"])))
    rows.append(("Размеры, двигатель, груз", "нет полей в машинном каноне — см. досье", "—"))
    d.text("## Корабль")
    d.gen("ship", table(["Параметр", "Значение", "Статус"], rows))
    d.narr("ship_description", "Как выглядит корабль", "облик, размеры, модули, двигатель, груз и запасы по досье; не противоречить расчёту physics.py (если досье расходится — это помечено в вердикте).", level=3)
    # -- population
    st = m["start"]
    lm = D.S["missions"].get(mid, {})
    rows = [("Старт: людей", num(st["population"]), tagk(st)),
            ("Бодрствующие / крио", f"{num(st['awake'])} / {num(st['cryo'])}", tagk(st)),
            ("Вахта (минимум функций по модели)", f"{num(st['awake_watch'])} ({num(lm.get('required_watch'))})", tagk(st)),
            ("Лет вахты / безопасных циклов", f"{c.get('watch_years', '—')} / {c.get('safe_cycles', '—')}", tag(m["status"], m["visibility"]) + " (досье)"),
            ("Политика рождений", c.get("birth_policy"), tag(m["status"], m["visibility"]) + " (досье)"),
            ("Наследственная вахта / хранители", f"{'да' if lm.get('hereditary_watch') else 'нет'} / {fl(lm.get('keeper_index'), 0)}", MODEL_TAG),
            ("Сон владельцев, лет", fl(lm.get("owner_sleep_years"), 0), MODEL_TAG),
            ("Неучтённые потери", num(lm.get("unrecorded_losses")), MODEL_TAG)]
    coh = sorted(c.get("cohorts", {}).items(), key=lambda kv: (-kv[1]["share"], kv[0]))
    g = [table(["Параметр", "Значение", "Статус"], rows), "", "Когорты (доля / доля бодрствующих):", "",
         table(["Когорта", "Доля", "Бодрствует", "Культура"], [(k, fl(v["share"]), fl(v.get("awake")),
                ", ".join(f"{a} {fl(b)}" for a, b in sorted(v.get("culture", {}).items())) or "как у миссии") for k, v in coh])]
    if lm.get("flags"):
        g += ["", "Флаги модели: " + "; ".join(lm["flags"])]
    d.text("## Население")
    d.gen("population", "\n".join(g))
    # -- flight events
    rows = [r for r in significant(m["chronology"]) if pub(r)]
    hidden = [r for r in m["chronology"] if r["visibility"] != "PUBLIC_RECORD"]
    g = [f"Значимые события: якоря канона, смена режима, потери от 1 % людей; {len(m['chronology']) - len(significant(m['chronology']))} мелких "
         "заполняющих событий модели (seed-зависимых) опущены — полный список в `out/lab_report.md` §2.", "",
         table(["Год", "Событие", "Население", "Awake", "Cryo", "Власть/режим", "Техсостояние", "Статус"],
               [(r["year"], f"{r['event']}: {r['text']}" + (f" (погибло {num(r['deaths'])})" if r.get("deaths") else ""),
                 num(r["population"]), num(r["awake"]), num(r["cryo"]), r.get("regime"), fl(r.get("tech_state"), 2),
                 tag(r["status"], r["visibility"]) + (" (модель)" if r["source"] == "simulated" else "")) for r in rows])]
    if hidden and not author:
        g += ["", f"[СПОЙЛЕР] Есть {len(hidden)} скрыт. запис(и) AUTHOR TRUTH: " + link(path, "AUTHOR/HIDDEN_CANON.md", "AUTHOR/HIDDEN_CANON.md") + "."]
    d.text("## Хроника полёта")
    d.gen("flight", "\n".join(g))
    d.narr("flight_story", "Жизнь на борту (рассказ)", "быт, конфликты и смена власти между событиями таблицы; числа брать только из таблицы.", level=3)
    # -- outcome
    f = m["fate"]
    g = []
    if pub(f):
        g.append(table(["Поле", "Значение", "Статус"], [("Итог", f["state"], tagk(f)), ("Корабль сейчас", f["ship"], tagk(f)),
                                                         ("Примечание", f.get("note"), tagk(f)),
                                                         ("Можно найти в игре", "да" if c["fate"].get("findable") else "нет", tagk(f))]))
    else:
        last = [r for r in m["chronology"] if r["visibility"] == "PUBLIC_RECORD" and r["source"] != "simulated"]
        g.append("### PUBLIC RECORD\n")
        g.append("Что известно миру 3126 (последние публичные записи):\n")
        g.append(table(["Год", "Запись", "Статус"], [(r["year"], f"{r['event']}: {r['text']}", tag(r["status"], r["visibility"])) for r in last[-4:]]))
        g.append("\n### AUTHOR TRUTH\n")
        unk = [u for u in D.F["unknowns"] if u["id"].startswith(mid + ".")]
        if f.get("status") == "UNKNOWN" or unk:
            g.append("[СПОЙЛЕР] Судьба — `UNKNOWN`: намеренно без ответа, даже для автора. ")
        g.append("[СПОЙЛЕР] Скрытая правда: " + link(path, "AUTHOR/MISSING_SHIPS.md", "AUTHOR/MISSING_SHIPS.md") + f" (раздел {mid}).")
    d.text("## Итог")
    d.gen("outcome", "\n".join(g))
    if not pub(f):
        d.narr("public_record", "Публичная версия (рассказ)", "как мир 3126 пересказывает исчезновение / гибель: слухи, официальные версии, кто что скрывает. Не раскрывать AUTHOR TRUTH.", level=3)
    d.narr("arrival", "Прибытие и высадка", "что увидели у цели, первая высадка/стройка, кто взял власть, что стало с кораблём; для не долетевших — «нет»/последний контакт.")
    return d


def ships_index(D):
    path = "SHIPS/INDEX.md"
    d = new_doc(D, path, "Корабли поколений — индекс", "index")
    rows = []
    for mid in D.mission_order:
        if not D.mission_public(mid):
            continue
        m = D.missions[mid]
        f = m["fate"]
        fate = f["state"] if f.get("visibility") == "PUBLIC_RECORD" else "[спойлер: см. досье]"
        rows.append((link(path, ship_path(D, mid), mid), m["aliases"][0], m["wave"], m["launch"]["year"], m["destination"]["name"],
                     (m["arrival"] or {}).get("year"), num(m["start"]["population"]), num((m["arrival"] or {}).get("population")),
                     fate, link(path, f"COLONIES/{m['colony']}.md", D.pname(m["colony"])) if m.get("colony") in D.pol else "—",
                     tag(m["status"], m["visibility"])))
    hidden = [mid for mid in D.mission_order if not D.mission_public(mid)]
    g = [table(["ID", "Имя", "Волна", "Старт", "Цель", "Прибытие", "Людей на старте", "На прибытии", "Итог", "Колония", "Статус"], rows)]
    if hidden:
        g += ["", f"[СПОЙЛЕР] Ещё {len(hidden)} миссия(и) существуют только как AUTHOR TRUTH: " + link(path, "AUTHOR/MISSING_SHIPS.md", "AUTHOR/MISSING_SHIPS.md") + "."]
    d.gen("index", "\n".join(g))
    return d


# ---------------------------------------------------------------- COLONIES
def event_status(D, e):
    if e.get("source") in ("canon",):
        return tag("CANON")
    if e.get("source") == "anchor" and e.get("ship") in D.missions:
        return tagk(D.missions[e["ship"]].get("arrival"))
    return tag("PROPOSAL")


def colony_doc(D, uid):
    p = D.pol[uid]
    path = f"COLONIES/{uid}.md"
    d = new_doc(D, path, f"{p['name']} ({p['polity'] or '—'})", "colony")
    w = D.wcol.get(uid) or D.wout.get(uid) or {}
    pop = p["population_3126"]
    rows = [("ID", f"`{uid}`", tagk(p)), ("Тип", KINDS.get(p["kind"], p["kind"]), tagk(p)),
            ("Полития", p["polity"], tagk(p)),
            ("Владелец", link(path, f"COLONIES/{p['owner']}.md", D.pname(p["owner"])) if p.get("owner") in D.pol else "—", tagk(p)),
            ("Система", f"{p['system']['name']} (#{p['system']['id']})", tagk(p)),
            ("Основана", p["founded"]["year"], tagk(p)),
            ("Миссии-основатели", ", ".join(link(path, ship_path(D, x), x) for x in p["founded"]["founders"] if D.mission_public(x)) or "—", tagk(p)),
            ("Режим 3126", p["regime"]["value"], tagk(p["regime"])),
            ("Население 3126 (модель)", num(pop["model"]), MODEL_TAG),
            ("Население 3126 (авторское)", num(pop["authored"]), tag(pop.get("authored_status") or "PROPOSAL")),
            ("Модель / автор", fl(pop["model"] / pop["authored"], 2) if pop.get("authored") else "—", MODEL_TAG),
            ("Миграция 2750–3126: приток / отток", f"{num(p['migration']['in_total'])} / {num(p['migration']['out_total'])}", MODEL_TAG),
            ("Эпидемий (модель)", p["epidemics"], MODEL_TAG)]
    if w.get("hyper_year"):
        rows.append(("Гипер с", w["hyper_year"], tagk((w.get("field_status") or {}).get("hyper_year") or p)))
    if w.get("purpose"):
        rows.append(("Назначение", w["purpose"], tagk(w.get("knowledge"))))
    if w.get("owners"):
        rows.append(("Совладельцы", ", ".join(D.pname(o) for o in w["owners"]) + (" (спорный)" if w.get("contested") else ""), tagk(w.get("knowledge"))))
    for pr in w.get("problems", []):
        rows.append(("Проблема", pr, tagk((w.get("field_status") or {}).get("problems") or w.get("knowledge"))))
    d.text("## Общие данные")
    d.gen("summary", table(["Поле", "Значение", "Статус"], rows))
    d.narr("why_there", "Почему здесь", "почему миссия выбрала эту систему и что она обещала; что в системе реально есть (см. ресурсы и поселения ниже).")
    d.narr("arrival", "Прибытие и первое поселение", "первые годы после высадки: где жили, что сломалось, кто взял власть.")
    # events
    ev = sorted(p["events"], key=lambda e: (e["year"], e["type"]))
    d.text("## События колонии")
    d.gen("events", table(["Год", "Событие", "Люди", "Кто / куда", "Статус"],
                          [(e["year"], e["type"], num(e.get("people") or e.get("loss")),
                            e.get("ship") or e.get("to") or e.get("by"), event_status(D, e)) for e in ev]) if ev else "Нет событий в данных.")
    # checkpoints
    cps = p["checkpoints"]
    if cps:
        keep = [r for i, r in enumerate(cps) if i == 0 or i == len(cps) - 1 or r["year"] % 50 == 0]
        g = table(["Год", "Население", "СКР", "e0 / смертн. ‰", "Чистая миграция ‰", "Фаза", "Урбанизация", "Хабитат", "Тех. индекс", "Автоматизация", "Статус"],
                  [(r["year"], num(r["pop"]), fl(r["tfr"]), f"{fl(r['e0'], 0)} / {fl(r['cdr'], 1)}", fl(r["net_migration_per_1000"], 1), r["phase"],
                    fl(r["urbanisation"]), r["habitat"], fl(r["tech_index"]), fl(r["automation"]), MODEL_TAG) for r in keep])
    else:
        g = "Нет контрольных точек модели (единица без истории)."
    d.text("## Население: контрольные точки")
    d.gen("checkpoints", g)
    d.narr("growth", "История роста", "как росли и почему (жильё, рождаемость, миграция); числа — только из таблицы выше.")
    # tech
    t = p["tech"]
    if t.get("levels_3126"):
        g = [table(["Область", "Уровень 3126", "Состояние", "Статус"],
                   [(k, fl(t["levels_3126"].get(k), 2), TECH_ST.get(t["status_by_domain"].get(k), t["status_by_domain"].get(k)), MODEL_TAG) for k in DOMAINS])]
        if t.get("min_automation"):
            g += ["", f"Минимум автоматизации: {fl(t['min_automation'][1], 2)} в {t['min_automation'][0]} году (`{MODEL_TAG}`)."]
        jt = [j for j in D.W.get("jump_technology", []) if j["unit"] == uid and j["knowledge"]["visibility"] == "PUBLIC_RECORD"]
        if jt:
            g += ["", table(["Год", "Ядра прыжка", "Уровень", "Основание", "Статус"], [(j["year"], j["how"], fl(j["level"]), j["basis"], tagk(j["knowledge"])) for j in jt])]
        g = "\n".join(g)
    else:
        g = "Нет данных о технологиях."
    d.text("## Технологии: сохранено / утрачено / восстановлено")
    d.gen("tech", g)
    # economy
    e = p.get("economy_3126")
    if e:
        goods = sorted(set(e["production"]) | set(e["imports"]) | set(e["exports"]))
        g = [table(["Товар", "Производство", "Импорт", "Экспорт"], [(GOODS.get(x, x), fl(e["production"].get(x)), fl(e["imports"].get(x)), fl(e["exports"].get(x))) for x in goods]),
             "", table(["Показатель", "Значение", "Статус"], [("Рабочая сила", num(e["labour"]), MODEL_TAG), ("Автоматизация", fl(e["automation"]), MODEL_TAG),
                                                              ("Уровень жизни (доход)", fl(e["living_standard"]), MODEL_TAG), ("Неравенство (Джини)", fl(e["inequality_gini"]), MODEL_TAG),
                                                              ("Богатство на 100 тыс.", fl(e.get("wealth_per_100k")), MODEL_TAG),
                                                              ("Зависимости (нет своего источника)", ", ".join(GOODS.get(x, x) for x in e["dependencies"]) or "нет", MODEL_TAG)])]
        g = "Условные единицы стоимости в год (`" + MODEL_TAG + "`).\n\n" + "\n".join(g)
    else:
        g = "Нет экономики 3126 в данных (единица слишком мала или без истории)."
    d.text("## Экономика 3126")
    d.gen("economy", g)
    # settlements
    sts = [s for s in p.get("settlements", []) if s["knowledge"]["visibility"] == "PUBLIC_RECORD"]
    sp = settlement_file(D, p["system"]["id"])
    d.text("## Главные поселения")
    d.gen("settlements", (table(["Поселение", "Роль", "Тело / орбита", "Население", "Статус"],
                                [(s["name"], ROLES.get(s["role"], s["role"]), s["body"], num(s.get("population")), tagk(s["knowledge"])) for s in sts])
                          + "\n\nРеестр системы: " + link(path, sp, sp)) if sts else "Нет поселений в данных.")
    # relations
    rel = [r for r in D.F["relations"] if uid in (r["a"], r["b"])]
    other = lambda r: r["b"] if r["a"] == uid else r["a"]
    g = []
    if rel:
        g.append(table(["С кем", "С года", "Суть", "Обида", "Стойка (модель)", "Статус"],
                       [(link(path, f"COLONIES/{other(r)}.md", D.pname(other(r))), r["since"], r["text"], r["grievance"],
                         (r.get("model") or {}).get("stance"), tag(r["status"], r["visibility"])) for r in rel]))
    mr = []
    for k, v in sorted(D.S["relations"].items()):
        a, b = k.split("|")
        if uid in (a, b) and v["stance"] != "нейтрал":
            o = b if a == uid else a
            mr.append((link(path, f"COLONIES/{o}.md", D.pname(o)), v["stance"], fl(v["trade"], 1), fl(v["culture"], 1), v["grievance"], MODEL_TAG))
    if mr:
        g += ["", "Ненейтральные отношения по модели (торговля % оборота / культурная близость / обида):", "",
              table(["С кем", "Стойка", "Торговля %", "Культура", "Обида", "Статус"], mr)]
    d.text("## Отношения")
    d.gen("relations", "\n".join(g) if g else "Нет отношений в данных.")
    # trade partners
    cors = [x for x in D.F["trade_corridors"] if uid in (x["seller"], x["buyer"])]
    cors = sorted(cors, key=lambda x: (-x["value"], x["seller"], x["buyer"], x["good"]))[:12]
    d.text("## Торговые партнёры")
    d.gen("trade", table(["Направление", "Партнёр", "Товар", "Объём", "Стоимость", "Причина", "Статус"],
                         [("экспорт" if x["seller"] == uid else "импорт",
                           link(path, f"COLONIES/{x['buyer'] if x['seller'] == uid else x['seller']}.md", D.pname(x["buyer"] if x["seller"] == uid else x["seller"])),
                           GOODS.get(x["good"], x["good"]), x["volume_class"], fl(x["value"], 1), REASONS.get(x["reason"], x["reason"]), MODEL_TAG) for x in cors])
          if cors else "Нет межзвёздных коридоров.")
    lm = sys.modules[__name__]
    eras = lore_economy.eras_block(D, lm, D.X, uid) if D.X else None
    if eras:
        d.gen("population_eras", "## История населения: эпохи\n\n" + eras)
    if D.X and uid in D.X["economy"]["polities"]:
        d.gen("economy_3126", "## Экономика и логистика 3126\n\n" + lore_economy.polity_report(D, lm, D.X, uid, path))
    elif D.X and p.get("owner") in D.X["economy"]["polities"]:
        d.gen("economy_3126", "## Экономика и логистика 3126\n\nЭкономика, флот и оборона этой единицы считаются в составе владельца: "
              + link(path, f"COLONIES/{p['owner']}.md", D.pname(p["owner"])) + ".")
    d.narr("crises", "Кризисы и войны", "эпидемии, войны, расколы, исходы — по событиям выше и POLITICS/WARS.md.")
    d.narr("power", "Смена власти", "как режим пришёл к состоянию 3126 (см. режим в общих данных).")
    d.narr("fears", "Чего боятся", "внешние и внутренние страхи, в том числе «Возвращение Земли» (POLITICS/EARTH_3126.md).")
    d.narr("collapse", "Что может их обрушить", "уязвимости: зависимости по импорту, узкие места, демография; опираться на экономику и ECONOMY/STRATEGIC_DEPENDENCIES.md.")
    return d


def colonies_index(D):
    path = "COLONIES/INDEX.md"
    d = new_doc(D, path, "Колонии, аванпосты и фронтиры — индекс", "index")
    order = sorted(D.pol_order, key=lambda u: (D.pol[u]["polity"] or "", {"colony": 0, "outpost": 1, "frontier": 2}.get(D.pol[u]["kind"], 3), u))
    rows = []
    for u in order:
        p = D.pol[u]
        rows.append((p["polity"], link(path, f"COLONIES/{u}.md", p["name"]), KINDS.get(p["kind"], p["kind"]), p["system"]["name"], p["founded"]["year"],
                     num(p["population_3126"]["model"]), num(p["population_3126"]["authored"]), p["regime"]["value"], tagk(p)))
    d.gen("index", "Население 3126: модель — `" + MODEL_TAG + "`; авторское — статус в досье колонии.\n\n" +
          table(["Полития", "Единица", "Тип", "Система", "Основана", "Население 3126 (модель)", "Авторское", "Режим", "Статус"], rows))
    return d


# ---------------------------------------------------------------- SETTLEMENTS
def settlement_file(D, sid):
    return f"SETTLEMENTS/{sid:03d}_{slug(D.sname(sid))}.md"


def settlement_rows(D, sid, cur):
    rows = []
    for u in D.pol_order:
        p = D.pol[u]
        if p["system"]["id"] != sid:
            continue
        for s in p.get("settlements", []):
            if s["knowledge"]["visibility"] != "PUBLIC_RECORD":
                continue
            aid = s["id"].split(".", 1)[1]
            a = D.assets.get(aid) if aid in D.assets and D.assets[aid]["owner"] == u else None
            state = f"выпуск {fl(a['output'], 2)} [{fl(a['range'][0], 2)}–{fl(a['range'][1], 2)}]" if a and a.get("good") else "—"
            founded = f"{p['founded']['year']} (полития)" if s["role"] == "capital" else "—"
            rows.append((s["name"], D.sname(sid), s["body"], founded, num(s.get("population")), ROLES.get(s["role"], s["role"]),
                         link(cur, f"COLONIES/{u}.md", p["name"]), GOODS.get(a["good"], a["good"]) if a and a.get("good") else "—",
                         fl(a["importance"]) if a else "—", state, tagk(s["knowledge"])))
    return rows


SET_HEAD = ["Название", "Система", "Тело / орбита / пояс", "Основано", "Население", "Назначение", "Владелец", "Отрасль", "Стратег. важность", "Состояние 3126", "Статус"]


def settlement_docs(D):
    systems = sorted({p["system"]["id"] for p in D.F["polities"]})
    docs = []
    idx_rows = []
    for sid in systems:
        path = settlement_file(D, sid)
        d = new_doc(D, path, f"Поселения — {D.sname(sid)}", "settlements")
        rows = settlement_rows(D, sid, path)
        d.gen("registry", "Население поселений — доля модельного населения политии по Ципфу (`" + MODEL_TAG + "`). Тела — из физического каталога.\n\n" + table(SET_HEAD, rows))
        if D.X:
            d.gen("system_economy_3126", lore_economy.system_report(D, sys.modules[__name__], D.X, sid, path))
        d.narr("places", "Места (рассказ)", "облик главных поселений, кто там живёт, чем пахнет и что продают; одно-два предложения на поселение.")
        docs.append(d)
        units = [u for u in D.pol_order if D.pol[u]["system"]["id"] == sid]
        idx_rows.append((link("SETTLEMENTS/INDEX.md", path, D.sname(sid)), sid, len(rows), ", ".join(D.pol[u]["name"] for u in units)))
    d = new_doc(D, "SETTLEMENTS/INDEX.md", "Реестр поселений — индекс по системам", "index")
    d.gen("index", f"Всего публичных записей: {sum(r[2] for r in idx_rows)} (все — `PROPOSAL`, `canon/settlements.json`).\n\n" +
          table(["Система", "#", "Записей", "Единицы"], idx_rows))
    return [d] + docs


# ---------------------------------------------------------------- ECONOMY
def asset_table(D, cur, assets, extra=None):
    rows = []
    for a in assets:
        r = [link(cur, f"COLONIES/{a['owner']}.md", D.pname(a["owner"])), a["name"], ROLES.get(a["type"], a["type"]),
             GOODS.get(a["good"], a["good"]) if a.get("good") else "—", fl(a["output"], 3), f"{fl(a['range'][0], 2)}–{fl(a['range'][1], 2)}", fl(a["importance"])]
        if extra:
            r += extra(a)
        r.append(tagk(a))
        rows.append(r)
    head = ["Владелец", "Актив", "Тип", "Товар", "Выпуск", "Диапазон", "Важность"] + (["Флот (корпуса)", "Резерв мирн./войн."] if extra else []) + ["Статус"]
    return table(head, rows)


def economy_docs(D):
    A = sorted(D.F["industrial_assets"], key=lambda a: (a["owner"], a["type"], a["id"]))
    docs = []
    note = "Выпуск — условные единицы в год, диапазон — неопределённость модели (`" + MODEL_TAG + "`)."
    d = new_doc(D, "ECONOMY/INDUSTRIAL_ASSETS.md", "Промышленные активы 3126", "economy")
    d.gen("assets", note + "\n\n" + asset_table(D, d.path, [a for a in A if a["type"] != "fleet"]))
    d.narr("assets_story", "Заметки об активах", "кто построил крупнейшие активы, что о них знают пилоты и торговцы.")
    docs.append(d)
    d = new_doc(D, "ECONOMY/MINES.md", "Добыча: рудники, газовые платформы, лёд", "economy")
    d.gen("mines", note + "\n\n" + asset_table(D, d.path, sorted([a for a in A if a["type"] in EXTRACTION], key=lambda a: (-a["output"], a["id"]))))
    d.narr("mines_story", "Жизнь на добыче", "условия труда, опасности, кто работает (автоматы / люди), конфликты за месторождения.")
    docs.append(d)
    # shipyards
    path = "ECONOMY/SHIPYARDS.md"
    d = new_doc(D, path, "Верфи и флоты", "economy")
    yards = [a for a in A if a["type"] in YARDS]
    g = [note, "", asset_table(D, path, yards), "", "Флоты и мобилизационные резервы:", "",
         asset_table(D, path, sorted([a for a in A if a["type"] == "fleet"], key=lambda a: (-a["output"], a["id"])),
                     extra=lambda a: [f"{fl(a['fleet_hulls'][0], 1)}–{fl(a['fleet_hulls'][1], 1)}", f"{num(a.get('manpower_standing'))} / {num(a.get('manpower_total_war'))}"])]
    g += ["", "Происхождение входов верфей (импорт владельцев верфей по коридорам):", ""]
    rows = []
    for a in yards:
        for good in SHIP_INPUTS:
            sellers = sorted([x for x in D.F["trade_corridors"] if x["buyer"] == a["owner"] and x["good"] == good], key=lambda x: (-x["value"], x["seller"]))
            if sellers:
                rows.append((D.pname(a["owner"]), a["name"], GOODS[good], ", ".join(f"{D.pname(x['seller'])} ({x['volume_class']})" for x in sellers[:4]), MODEL_TAG))
            else:
                made = (D.pol[a["owner"]].get("economy_3126") or {}).get("production", {}).get(good)
                rows.append((D.pname(a["owner"]), a["name"], GOODS[good], "своё производство" if made else "нет данных", MODEL_TAG))
    g.append(table(["Владелец", "Верфь", "Вход", "Откуда", "Статус"], rows))
    er = D.F["earth"]["return_3126"]
    g += ["", f"Выпуск корпусов всех колоний: {fl(er['colony_hulls_per_year'], 1)} ед./год; потенциал Земли {fl(er['earth_hulls_per_year_range'][0], 1)}–{fl(er['earth_hulls_per_year_range'][1], 1)} ед./год (`{MODEL_TAG}`). "
          "Единица — корпус торгового класса ~10 тыс. т (допущение F12).", "",
          "Классы кораблей, темп по классам и разделение ремонт / военные / гражданские в данных отсутствуют: тип актива различает только `shipyard` (гражданская) и `arsenal_shipyard` (военная)."]
    d.gen("shipyards", "\n".join(g))
    d.narr("yards_story", "Что строят и где", "классы кораблей по верфям, ремонтные доки, военные заказы; не противоречить темпу выпуска выше.")
    docs.append(d)
    # trade routes
    path = "ECONOMY/TRADE_ROUTES.md"
    d = new_doc(D, path, "Торговые коридоры 3126", "economy")
    C = D.F["trade_corridors"]
    big = [x for x in C if x["volume_class"] in ("major", "medium")]

    def route(x):
        return " → ".join(D.sname(s) for s in x["route"])
    g = ["Правило фрахта: межзвёздная перевозка оплачивается за массу; навалочный груз (вода/летучие, металл, пища) везут только при конкретной причине. "
         f"Всего коридоров в данных: {len(C)} (major {sum(1 for x in C if x['volume_class'] == 'major')}, medium {sum(1 for x in C if x['volume_class'] == 'medium')}, "
         f"minor {sum(1 for x in C if x['volume_class'] == 'minor')}). Ниже — major и medium; все — `{MODEL_TAG}`.", "",
         table(["Продавец", "Покупатель", "Товар", "Класс", "Стоимость", "Маршрут", "Причина", "Альтернативы", "Узкие места", "Контролёр", "Break-even фрахт"],
               [(link(path, f"COLONIES/{x['seller']}.md", D.pname(x["seller"])), link(path, f"COLONIES/{x['buyer']}.md", D.pname(x["buyer"])),
                 GOODS.get(x["good"], x["good"]), x["volume_class"], fl(x["value"], 1), route(x), REASONS.get(x["reason"], x["reason"]),
                 ", ".join(D.pname(a) for a in x["alternatives"]), ", ".join(D.sname(s) for s in x["choke_points"]),
                 ", ".join(D.pname(c) if c != "?" else "?" for c in x["controllers"]), fl(x["break_even_freight"], 1)) for x in big])]
    sh = D.S["trade"]["flow_diff"]["share_by_good"]
    g += ["", "Доля стоимости межзвёздного оборота по товарам, % (старый рынок → рынок с фрахтом):", "",
          table(["Товар", "Было", "Стало"], [(GOODS.get(k, k), fl(v["old"], 1), fl(v["new"], 1)) for k, v in sorted(sh.items())])]
    d.gen("corridors", "\n".join(g))
    # dropped flows
    have = {(x["buyer"], x["good"]): x for x in sorted(C, key=lambda x: -x["value"])}
    rows = []
    for s, b, good in D.S["trade"]["flow_diff"]["vanished"]:
        alt = have.get((b, good))
        made = (D.S["economy"].get(b) or {}).get("made", {}).get(good, 0)
        if alt and alt["seller"] != s:
            why = f"заменён поставщиком {D.pname(alt['seller'])} ({REASONS.get(alt['reason'], alt['reason'])})"
        elif made > 0:
            why = "свой источник покупателя дешевле доставленного импорта"
        else:
            why = "поток ниже порога значимости после пересчёта фрахта"
        rows.append((GOODS.get(good, good), D.pname(s), D.pname(b), "да" if good in BULK else "нет", why, MODEL_TAG))
    d.text("## Отброшенные межзвёздные потоки")
    d.gen("dropped", f"Потоки старого рынка, которых нет на рынке с фрахтом: {len(rows)}. Причина выведена из данных: есть ли у покупателя другой поставщик / своё производство.\n\n" +
          table(["Товар", "Продавец", "Покупатель", "Навалочный", "Почему отброшен", "Статус"], rows))
    d.narr("routes_story", "Как выглядит торговля", "кто возит, на чём, пиратство, пошлины у узких мест; не придумывать навалочные потоки без причины.")
    docs.append(d)
    # strategic dependencies
    path = "ECONOMY/STRATEGIC_DEPENDENCIES.md"
    d = new_doc(D, path, "Стратегические зависимости", "economy")
    rows = []
    for x in sorted([x for x in C if x["reason"] == "no_local_source"], key=lambda x: (x["buyer"], x["good"], -x["value"])):
        rows.append((link(path, f"COLONIES/{x['buyer']}.md", D.pname(x["buyer"])), GOODS.get(x["good"], x["good"]), D.pname(x["seller"]), x["volume_class"],
                     ", ".join(D.sname(s) for s in x["choke_points"]) or "—", MODEL_TAG))
    g = ["Нет своего источника (импорт по необходимости):", "", table(["Покупатель", "Товар", "Поставщик", "Класс", "Узкие места", "Статус"], rows)]
    chk = {}
    for x in C:
        for i, s in enumerate(x["choke_points"]):
            e = chk.setdefault(s, dict(n=0, v=0.0, ctl=set(), goods=set()))
            e["n"] += 1
            e["v"] += x["value"]
            e["goods"].add(x["good"])
            if i < len(x["controllers"]):
                e["ctl"].add(x["controllers"][i])
    g += ["", "Узкие места сети (системы-перевалки на маршрутах):", "",
          table(["Система", "Коридоров", "Стоимость через узел", "Контролёр", "Товары", "Статус"],
                [(D.sname(s), e["n"], fl(e["v"], 1), ", ".join(D.pname(c) if c != "?" else "?" for c in sorted(e["ctl"])), ", ".join(GOODS.get(x, x) for x in sorted(e["goods"])), MODEL_TAG)
                 for s, e in sorted(chk.items(), key=lambda kv: (-kv[1]["v"], kv[0]))])]
    er = D.F["earth"]["return_3126"]
    g += ["", "Контроль сети маяков (сколько кратчайших маршрутов между акторами идёт через узлы оператора):", "",
          table(["Оператор", "Маяки (системы)", "Маршрутов", "Статус"], [(D.pname(o), ", ".join(D.sname(s) for s in ss), n, MODEL_TAG) for o, ss, n in er["network_control"]])]
    single = {}
    for x in C:
        single.setdefault((x["buyer"], x["good"]), []).append(x)
    rows = [(D.pname(b), GOODS.get(good, good), D.pname(xs[0]["seller"]), xs[0]["volume_class"], MODEL_TAG)
            for (b, good), xs in sorted(single.items()) if len(xs) == 1 and xs[0]["volume_class"] != "minor" and xs[0]["reason"] != "same_system"]
    g += ["", "Единственный межзвёздный поставщик (класс major/medium):", "", table(["Покупатель", "Товар", "Поставщик", "Класс", "Статус"], rows)]
    d.gen("dependencies", "\n".join(g))
    d.narr("leverage", "Рычаги давления", "кто кого может задушить и почему этого ещё не сделал.")
    docs.append(d)
    # technology
    path = "ECONOMY/TECHNOLOGY.md"
    d = new_doc(D, path, "Технологии по политиям", "economy")
    rows, rows2 = [], []
    for u in D.pol_order:
        t = D.pol[u]["tech"]
        if not t.get("levels_3126"):
            continue
        rows.append([link(path, f"COLONIES/{u}.md", D.pname(u))] + [fl(t["levels_3126"].get(k), 2) for k in DOMAINS])
        rows2.append([D.pname(u)] + [TECH_ST.get(t["status_by_domain"].get(k), t["status_by_domain"].get(k)) for k in DOMAINS])
    g = ["Уровни 3126 по 12 областям (1.0 — эталон эпохи старта; ниже критической массы специалистов область падает к ремесленному уровню). Все — `" + MODEL_TAG + "`.", "",
         table(["Единица"] + list(DOMAINS), rows), "", "Состояние областей:", "", table(["Единица"] + list(DOMAINS), rows2), "",
         "Ядра прыжка (публичные записи):", "",
         table(["Год", "Кто", "Как", "Уровень", "Основание", "Статус"], [(j["year"], D.pname(j["unit"]), j["how"], fl(j["level"]), j["basis"], tagk(j["knowledge"]))
                                                                        for j in D.W.get("jump_technology", []) if j["knowledge"]["visibility"] == "PUBLIC_RECORD"])]
    if any(j["knowledge"]["visibility"] != "PUBLIC_RECORD" for j in D.W.get("jump_technology", [])):
        g += ["", "[СПОЙЛЕР] Часть записей о ядрах — AUTHOR TRUTH: " + link(path, "AUTHOR/HIDDEN_CANON.md", "AUTHOR/HIDDEN_CANON.md") + "."]
    d.gen("technology", "\n".join(g))
    d.narr("tech_story", "Технологический быт", "что работает, что чинят вручную, какие машины — реликвии; культ и ремесло вокруг утраченного.")
    docs.append(d)
    lm = sys.modules[__name__]
    for doc in docs:
        for bid, content in lore_economy.blocks_for_existing(D, lm, doc.path):
            doc.gen(bid, content)
    docs += lore_economy.docs(D, lm)
    return docs


# ---------------------------------------------------------------- DEMOGRAPHY
MODEL_SKELETON = """## Причинная модель (кратко)

Источник: `tools/colonization_lab/demography.py`. Год за годом для каждой колонии, аванпоста, фронтира и двух регионов Земли (возрастная структура по 5-летним группам):

1. **Экономика детей:** жильё = построенный объём (купола → луны/астероиды → цилиндры, ярусы и рента по телам каталога); ценность детского труда = (1 − автоматизация) × дефицит рук; пенсии; длина образования; детская смертность.
2. **Семейная норма** следует за экономикой с запаздыванием в полтора поколения (в обе стороны).
3. **Религиозная субкультура:** своя рождаемость, отток и приток.
4. **Политика режима** и лицензии на рождение при заполненных хабитатах.
5. **Смертность** из таблицы жизни по медицине и уровню жизни; сидированные эпидемии; войны только из якорей.
6. **Технологии по 12 областям** с критической массой специалистов: деградация, каннибализация машин, восстановление, импорт, кража, гостайна (ядра прыжка).
7. **Миграция** только после гипера, по графу маяков, с ограниченными местами в кораблях и барьерами режимов; Земля участвует через окна (Кордон).

Население больше не задаётся: авторские числа хранятся как `pop_3126_authored` и сравниваются с моделью (см. POPULATION_3126.md).

## Допущения F1–F12 (футурология, не данные)

- **F1** таблица смертности из медицины, дохода и тесноты; войны — только якоря канона.
- **F2** жильё — построенный объём, три яруса (купола / выработки / вращающиеся хабитаты), рента внутри яруса; предел — темп строительства, не вещество.
- **F3** индекс доступности жилья, откалиброван по богатому поясу Земли 2026.
- **F4** технология нелинейна: критическая масса специалистов по 12 областям; ядра прыжка — гостайна.
- **F5** религиозная субкультура: премия рождаемости меньшинства, отток растёт с достатком.
- **F6** политика: малые эффекты; антинатальные квоты и лицензии на рождение при перенаселении.
- **F7** Земля: два региона; катастрофы канона получают величины-предложения.
- **F8** миграция по графу маяков с потолками эмиграции / иммиграции и окнами Земли.
- **F9** наследуемая склонность к семье.
- **F10** автоматизированный уход снимает часть цены образования.
- **F11** фрахт оплачивается за массу.
- **F12** единица флота — торговый корпус ~10 тыс. т.

Полный текст допущений, уравнение рождаемости и реальные исторические данные (часть A): [RESEARCH.md](RESEARCH.md) (копия исследования, перенесённая в репозиторий).

Главная неопределённость мира — футурологическая: без F9/F10 (сценарий `transition_persists`) население вне Сола и Земли падает на порядок (таблица ниже)."""


def demography_docs(D):
    docs = []
    d = new_doc(D, "DEMOGRAPHY/MODEL.md", "Демография: модель и допущения", "demography", MODEL_SKELETON)
    sens = D.F["demography_sensitivity"]
    g = [table(["Сценарий", "Земля 3126", "Вне Сола", "Крупнейшие три", "Статус"],
               [(k, num(sens[k]["earth"]), num(sens[k]["off_sol"]), ", ".join(f"{D.pname(u)} {num(v)}" for u, v in sens[k]["top"][:3]), MODEL_TAG)
                for k in ("baseline", "transition_persists")])]
    mg = D.W["migration"]
    g += ["", "Окна миграции Земли:", "", table(["С", "По", "Людей в год", "Суть", "Статус"],
                                                 [(w["from"], w["to"], num(w["per_year"]), w["text"], tag(w["status"])) for w in mg["earth_windows"]])]
    sm = D.S["demography"]["migration"]
    g += ["", f"Миграция по полувекам (всего {num(sum(sm['by_half_century'].values()))} человек, `{MODEL_TAG}`):", "",
          table(["Полвека", "Людей"], [(k, num(v)) for k, v in sorted(sm["by_half_century"].items())]), "",
          table(["Откуда", "Куда", "Людей"], [(D.pname(r["src"]) if r["src"] in D.pol else r["src"], D.pname(r["dst"]), num(r["people"])) for r in sm["top_routes"]])]
    d.gen("sensitivity", "\n".join(g))
    docs.append(d)
    # population 3126
    path = "DEMOGRAPHY/POPULATION_3126.md"
    d = new_doc(D, path, "Население 3126", "demography")
    E = D.F["earth"]
    last = E["trajectory"][-1]
    g = [table(["Показатель", "Значение", "Статус"],
               [("Земля 3126 (модель)", num(last["pop"]), MODEL_TAG), ("— богатый пояс", num(last["north"]), MODEL_TAG), ("— развивающийся пояс", num(last["south"]), MODEL_TAG),
                ("Земля + Сол 3126, цель автора (AUTHOR_TARGET)", f"{num(D.PH['world_targets']['earth']['centre'])} + {num(D.PH['world_targets']['sol_outside_earth']['centre'])}", "`AUTHOR_TARGET`"),
                ("Вне Сола (модель)", num(sens["baseline"]["off_sol"]), MODEL_TAG)]),
         "", "Земля, авторские точки:", "", table(["Год", "Население", "Текст", "Источник", "Статус"], [(c["year"], num(c["pop"]), c["text"], c["source"], tag(c["status"])) for c in E["checkpoints_authored"]])]
    d.text("## Земля и итог")
    d.gen("earth", "\n".join(g))
    rows = []
    for u in sorted(D.pol_order, key=lambda u: (-D.pol[u]["population_3126"]["model"], u)):
        p = D.pol[u]
        cp = p["checkpoints"][-1] if p["checkpoints"] else {}
        rows.append((link(path, f"COLONIES/{u}.md", p["name"]), p["polity"], p["system"]["name"], num(p["population_3126"]["model"]), num(p["population_3126"]["authored"]),
                     fl(cp.get("tfr")), cp.get("phase"), MODEL_TAG))
    d.text("## Все единицы вне Сола")
    d.gen("units", table(["Единица", "Полития", "Система", "Модель 3126", "Авторское", "СКР", "Фаза", "Статус"], rows))
    g = []
    for t in D.S["demography"]["trajectories"]:
        p = D.pol[t["unit"]]
        cps = [r for r in p["checkpoints"] if r["year"] % 100 == 0 or r is p["checkpoints"][0] or r is p["checkpoints"][-1]]
        g += [f"### {t['kind']} — {link(path, 'COLONIES/' + t['unit'] + '.md', p['name'])}", "", f"Правило выбора: {t['why']}.", "",
              table(["Год", "Население", "СКР", "Автоматизация", "Фаза"], [(r["year"], num(r["pop"]), fl(r["tfr"]), fl(r["automation"]), r["phase"]) for r in cps]), ""]
    d.text("## Шесть контрастных траекторий")
    d.gen("trajectories", "Все — `" + MODEL_TAG + "`.\n\n" + "\n".join(g))
    rows = []
    for u in D.pol_order:
        pp = D.pol[u]["population_3126"]
        tg = D.PH["units"].get(u, {}).get("target")
        if tg:
            if not (tg[0] <= pp["model"] <= tg[1]):
                rows.append((link(path, f"COLONIES/{u}.md", D.pname(u)), num(pp["model"]), f"{num(tg[0])}–{num(tg[1])}", "вне цели", "`AUTHOR_TARGET`"))
        elif pp.get("authored") and (pp["model"] / pp["authored"] > 2 or pp["model"] / pp["authored"] < 0.5):
            rows.append((link(path, f"COLONIES/{u}.md", D.pname(u)), num(pp["model"]), num(pp["authored"]), fl(pp["model"] / pp["authored"], 2), tag(pp.get("authored_status") or "PROPOSAL")))
    d.text("## Конфликты: авторское население против модели")
    d.gen("conflicts", "Крупные политии сверяются с авторским масштабом 3126 (`AUTHOR_TARGET`, canon/population_history.json; старые числа fate.md им заменены), "
          "малые — с авторским числом (расхождение больше чем в 2 раза). Модель не переписывает канон: решение за автором.\n\n" +
          (table(["Единица", "Модель 3126", "Цель / авторское", "Расхождение", "Статус"], rows) if rows else "Расхождений нет."))
    sc = D.S["demography"].get("scale_3126")
    if sc:
        g3 = [table(["Слой", "Модель", "Цель автора", "В диапазоне", "Статус"],
                    [(nm, num(sc[k]["model"]), f"{num(sc[k]['target'][0])}–{num(sc[k]['target'][1])}", "да" if sc[k]["inside"] else "НЕТ", "`AUTHOR_TARGET`")
                     for k, nm in (("earth", "Земля"), ("sol_outside_earth", "Сол вне Земли"), ("off_sol", "вне Сола"))]), "",
              f"Эмигрантов с Земли 2752–3126: {num(sc['earth_emigrants_2752_3126'])} (`{MODEL_TAG}`). Эпохи, которые объясняют масштаб, — в досье каждой политии (блок «История населения: эпохи»). "
              "Окна вербовки на Земле:", "",
              table(["С", "По", "Людей в год", "Суть", "Статус"], [(w["from"], w["to"], num(w["per_year"]), w["text"], tag(w["status"])) for w in D.PH["earth_programme_windows"]])]
        d.text("## Авторский масштаб 3126")
        d.gen("author_scale", "\n".join(g3))
    d.narr("conflict_decisions", "Решения автора по конфликтам", "для каждой строки выше: принять модель, оставить авторское число (с объяснением) или поменять входы модели.")
    docs.append(d)
    return docs


# ---------------------------------------------------------------- POLITICS
def politics_docs(D):
    docs = []
    path = "POLITICS/RELATIONS.md"
    d = new_doc(D, path, "Отношения между политиями", "politics")
    g = [table(["Сторона A", "Сторона B", "С года", "Суть", "Обида", "Стойка (модель)", "Торговля %", "Статус"],
               [(link(path, f"COLONIES/{r['a']}.md", D.pname(r["a"])), link(path, f"COLONIES/{r['b']}.md", D.pname(r["b"])), r["since"], r["text"], r["grievance"],
                 (r.get("model") or {}).get("stance"), fl((r.get("model") or {}).get("trade"), 1), tag(r["status"], r["visibility"])) for r in D.F["relations"]])]
    rows = []
    for k, v in sorted(D.S["relations"].items()):
        if v["stance"] == "нейтрал":
            continue
        a, b = k.split("|")
        rows.append((D.pname(a), D.pname(b), v["stance"], fl(v["trade"], 1), fl(v["culture"], 1), v["grievance"], v["competition"], MODEL_TAG))
    g += ["", "Ненейтральные пары по модели:", "", table(["A", "B", "Стойка", "Торговля %", "Культура", "Обида", "Конкуренция", "Статус"], rows)]
    cnt = {}
    for v in D.S["relations"].values():
        cnt[v["stance"]] = cnt.get(v["stance"], 0) + 1
    g += ["", "Все пары модели: " + ", ".join(f"{k} {v}" for k, v in sorted(cnt.items())) + "."]
    d.gen("relations", "\n".join(g))
    d.narr("diplomacy", "Дипломатия (рассказ)", "союзы, договоры, посольства и старые обиды; не вводить войн без записи в данных.")
    docs.append(d)
    # wars
    path = "POLITICS/WARS.md"
    d = new_doc(D, path, "Войны, стычки и расколы", "politics")
    rows = []
    for r in D.F["relations"]:
        rows.append((r["since"], f"{D.pname(r['a'])} — {D.pname(r['b'])}", r["text"], "—", "отношения", tag(r["status"], r["visibility"])))
    for u in D.pol_order:
        for e in D.pol[u]["events"]:
            if e["type"] in CONFLICT_EVENTS or e["type"].startswith("exodus"):
                rows.append((e["year"], D.pname(u), e["type"] + (f" → {D.pname(e['to'])}" if e.get("to") in D.pol else ""), num(e.get("loss") or e.get("people")), "колония", event_status(D, e)))
    for de in D.W.get("demography_events", []):
        if de["type"] == "deaths":
            rows.append((de["year"], D.pname(de["unit"]), de["basis"], num(de["value"]), "потери (вход демографии)", tagk(de["knowledge"])))
    for mid in D.mission_order:
        if not D.mission_public(mid):
            continue
        for r in D.missions[mid]["chronology"]:
            if r["source"] != "simulated" and r["event"] in CONFLICT_EVENTS and r["visibility"] == "PUBLIC_RECORD":
                rows.append((r["year"], link(path, ship_path(D, mid), mid), f"{r['event']}: {r['text']}", num(r.get("deaths")), "борт корабля", tag(r["status"], r["visibility"])))
    rows.sort(key=lambda r: (r[0], str(r[1]), str(r[2])))
    d.gen("wars", table(["Год", "Кто", "Что", "Погибло / людей", "Где", "Статус"], rows))
    thr = [(de["year"], de.get("until"), D.pname(de["unit"]) if de["unit"] in D.pol else de["unit"], fl(de["value"]), de["basis"], tagk(de["knowledge"]))
           for de in D.W.get("demography_events", []) if de["type"] == "threat"]
    d.text("## Длящиеся угрозы (холодные войны)")
    d.gen("threats", table(["С", "По", "Кто", "Сила угрозы", "Основание", "Статус"], thr))
    d.narr("war_stories", "Как это было", "по одной короткой истории на крупный конфликт: причина, ход, цена, память; потери — только из таблицы.")
    docs.append(d)
    # earth
    path = "POLITICS/EARTH_3126.md"
    d = new_doc(D, path, "Земля 3126 и «Возвращение»", "politics")
    E = D.F["earth"]
    er = E["return_3126"]
    a = E["anchor"]
    g = [table(["Поле", "Значение", "Статус"], [("Имя", a["name"], tagk(a["knowledge"])), ("Население (предложение)", num(a["pop"]), tagk(a["knowledge"])),
                                                 ("Runtime", a["runtime_anchor"], tagk(a["knowledge"]))]), "",
         "Баланс сил (`" + MODEL_TAG + "`):", "",
         table(["Показатель", "Земля", "Колонии-акторы", "Отношение"],
               [(k, num(er["earth"][f]) if f not in ("industrial_index", "shipbuilding_index") else fl(er["earth"][f], 1),
                 num(er["colonies"][f]) if f not in ("industrial_index", "shipbuilding_index") else fl(er["colonies"][f], 1),
                 fl(er["earth"][f] / er["colonies"][f], 1)) for k, f in (("Население", "pop"), ("Промышленный индекс", "industrial_index"),
                                                                          ("Верфевой индекс", "shipbuilding_index"), ("Мужчины 20–39", "men_20_39"),
                                                                          ("Мобилизуемые (8 %)", "mobilisable_8pc"))]), "",
         f"Корпуса в год: колонии {fl(er['colony_hulls_per_year'], 1)}, Земля {fl(er['earth_hulls_per_year_range'][0], 1)}–{fl(er['earth_hulls_per_year_range'][1], 1)}; "
         f"восполнение флота колоний — {fl(er['fleet_replacement_years']['colonies'], 1)} лет. Прыжок ≤ {fl(er['hop_ly'], 1)} св. лет, нужен маяк у цели.", "",
         "Досягаемость с маяка Сола:", "", table(["Система", "Св. лет"], [(r["name"], fl(r["ly"], 1)) for r in sorted(er["earth_jump_reach"], key=lambda r: (r["ly"], r["system"]))]), "",
         "Главные колонии по промышленному индексу:", "", table(["Колония", "Население", "Индекс"], [(link(path, f"COLONIES/{c['id']}.md", D.pname(c["id"])), num(c["pop"]), fl(c["industrial_index"], 1)) for c in er["main_colonies"]]), "",
         "Стратегические активы:", "", table(["Актив", "Владелец", "Товар", "Выпуск"], [(s["name"], D.pname(s["owner"]), GOODS.get(s["good"], s["good"]), fl(s["output"], 1)) for s in er["strategic_assets"]]), "",
         "Вероятные реакции колоний на «Возвращение» (топ-3, вероятности модели):", "",
         table(["Колония", "Реакции"], [(D.pname(u), ", ".join(f"{k} {fl(v, 1)} %" for k, v in r)) for u, r in sorted(D.S["earth_3126"].items())])]
    if "EARTH.jump_cores" in D.unknowns:
        g += ["", "[СПОЙЛЕР] Открытый вопрос о ядрах прыжка Земли — " + link(path, "AUTHOR/QUEST_TRUTHS.md", "AUTHOR/QUEST_TRUTHS.md") + "."]
    d.gen("earth_return", "\n".join(g))
    if D.X:
        lm = sys.modules[__name__]
        g2 = ["## Экономика и логистика Сола 3126", ""]
        for e in ("earth_north", "earth_south"):
            eb = lore_economy.eras_block(D, lm, D.X, e)
            if eb:
                g2 += [f"### История населения: {D.S['demography']['units'][e]['name']}", "", eb, ""]
        g2.append(lore_economy.polity_report(D, lm, D.X, "sol_authority", path))
        d.gen("sol_economy_3126", "\n".join(g2))
    d.narr("earth_politics", "Политика Земли", "кто правит Солом, чего хочет «Возвращение», как Земля видит колонии и наоборот.")
    docs.append(d)
    return docs


# ---------------------------------------------------------------- TIMELINE
def timeline_docs(D):
    docs = []
    path = "TIMELINE/MASTER_TIMELINE.md"
    d = new_doc(D, path, "Главная хронология", "timeline")
    ev = []
    for mid in D.mission_order:
        if not D.mission_public(mid):
            continue
        m = D.missions[mid]
        L = link(path, ship_path(D, mid), mid)
        ev.append((m["launch"]["year"], 0, L, f"старт к {m['destination']['name']}, {num(m['start']['population'])} чел.", tagk(m["launch"])))
        if m.get("arrival") and m["arrival"].get("visibility") == "PUBLIC_RECORD":
            ev.append((m["arrival"]["year"], 1, L, f"прибытие, {num(m['arrival']['population'])} чел.", tagk(m["arrival"])))
        for r in m["chronology"]:
            if r["source"] != "simulated" and r["event"] not in ("launch", "arrival") and r["visibility"] == "PUBLIC_RECORD":
                ev.append((r["year"], 2, L, f"{r['event']}: {r['text']}", tag(r["status"], r["visibility"])))
    for u in D.pol_order:
        p = D.pol[u]
        if p["kind"] != "colony":
            ev.append((p["founded"]["year"], 3, link(path, f"COLONIES/{u}.md", p["name"]), f"основан(а): {KINDS.get(p['kind'], p['kind'])}", tagk(p)))
        for e in p["events"]:
            if e["type"] not in ("arrival", "founded"):
                ev.append((e["year"], 3, link(path, f"COLONIES/{u}.md", p["name"]), e["type"], event_status(D, e)))
    for j in D.W["jump_chronology"]:
        if j["knowledge"]["visibility"] == "PUBLIC_RECORD":
            ev.append((j["year"], 4, "гипер", j["event"], tagk(j["knowledge"])))
    for b in D.W["beacons"]:
        ev.append((b["year"], 5, "маяк", f"{D.sname(b['system'])} — оператор {D.pname(b['operator'])}", tagk(b["knowledge"])))
    for r in D.F["relations"]:
        ev.append((r["since"], 6, "отношения", f"{D.pname(r['a'])} — {D.pname(r['b'])}: {r['text']}", tag(r["status"], r["visibility"])))
    ev.sort(key=lambda e: (e[0], e[1], e[2], e[3]))
    d.gen("timeline", "Без заполняющих событий модели и без AUTHOR TRUTH. Подробные хроники — в досье кораблей и колоний.\n\n" +
          table(["Год", "Кто / что", "Событие", "Статус"], [(e[0], e[2], e[3], e[4]) for e in ev]))
    d.narr("eras", "Эпохи (рассказ)", "короткие названия эпох и их границы (исход, тёмные века, гипер, Кордон, Возвращение); годы — только из таблицы.")
    docs.append(d)
    # earth and exodus
    path = "TIMELINE/EARTH_AND_EXODUS.md"
    d = new_doc(D, path, "Земля и исход", "timeline")
    E = D.F["earth"]
    tr = [r for r in E["trajectory"] if r["year"] % 50 == 0 or r is E["trajectory"][-1]]
    g = ["Авторские точки:", "", table(["Год", "Население", "Текст", "Статус"], [(c["year"], num(c["pop"]), c["text"], tag(c["status"])) for c in E["checkpoints_authored"]]), "",
         "Траектория модели (`" + MODEL_TAG + "`):", "",
         table(["Год", "Население", "Богатый пояс", "Развивающийся пояс", "СКР", "e0", "Фаза север / юг"],
               [(r["year"], num(r["pop"]), num(r["north"]), num(r["south"]), fl(r["tfr"]), fl(r["e0"], 1), f"{r['phase_north']} / {r['phase_south']}") for r in tr]), "",
         "Катастрофы и шоки Земли (величины — допущения модели):", "",
         table(["С", "По", "Регион", "Тип", "Величина", "Основание", "Статус"],
               [(e["year"], e.get("until"), e["unit"], e["type"], e["value"], e["basis"], tagk(e["knowledge"])) for e in D.W["demography_events"] if e["unit"].startswith("earth")]), "",
         "Старты с Земли:", "",
         table(["Год", "Миссия", "Спонсор", "Людей", "Статус"],
               [(D.missions[m]["launch"]["year"], link(path, ship_path(D, m), m), D.missions[m]["sponsor"], num(D.missions[m]["start"]["population"]), tagk(D.missions[m]["launch"]))
                for m in D.mission_order if D.mission_public(m) and D.missions[m]["origin_system"] == 0])]
    d.gen("earth", "\n".join(g))
    d.narr("earth_story", "Земля между стартами", "что происходило на Земле между 2026 и 3126: климат, распад, Кордон, маяк; числа — только из таблиц.")
    docs.append(d)
    # hyperdrive
    path = "TIMELINE/HYPERDRIVE_TIMELINE.md"
    d = new_doc(D, path, "Хронология гипера и маяков", "timeline")
    g = [table(["Год", "Событие", "Статус"], [(j["year"], j["event"], tagk(j["knowledge"])) for j in D.W["jump_chronology"] if j["knowledge"]["visibility"] == "PUBLIC_RECORD"]), "",
         "Маяки:", "", table(["Год", "Система", "Оператор", "Статус"], [(b["year"], D.sname(b["system"]), D.pname(b["operator"]), tagk(b["knowledge"])) for b in sorted(D.W["beacons"], key=lambda b: (b["year"], b["system"]))]), "",
         "Дальность прыжка:", "", table(["С года", "Св. лет"], [(y, fl(v, 1)) for y, v in D.W["max_hop_ly"]]), "",
         "Особые линии:", "", table(["Год", "Между", "Множитель цены", "Суть", "Статус"],
                                    [(x["year"], f"{D.sname(x['a'])} — {D.sname(x['b'])}", fl(x["cost_factor"], 1), x["note"], tagk(x["knowledge"])) for x in D.W.get("extra_links", [])]), "",
         "Сеть по годам (`" + MODEL_TAG + "`):", "",
         table(["Год", "Узлов", "Компонент связности", "Прыжок, св. лет"], [(y, len(v["nodes"]), len(v["components"]), fl(v["hop"], 1)) for y, v in sorted(D.S["network"].items())])]
    d.gen("hyper", "\n".join(g))
    d.narr("hyper_story", "Как гипер изменил мир", "открытие, гостайна ядер, первые прыжки, утечки; не раскрывать AUTHOR TRUTH.")
    docs.append(d)
    return docs


# ---------------------------------------------------------------- AUTHOR
def author_docs(D):
    import knowledge
    docs = []
    items = knowledge.collect(D.MS, D.W, D.CU)
    # hidden canon
    path = "AUTHOR/HIDDEN_CANON.md"
    d = new_doc(D, path, "Скрытый канон (AUTHOR TRUTH)", "author", "> [СПОЙЛЕР] Весь файл — скрытая правда. Не цитировать в публичных разделах.")
    rows = [(it["path"], cell(it["text"])[:200], it["basis"], tag(it["status"], it["visibility"])) for it in items if it["visibility"] == "AUTHOR_TRUTH"]
    for u in D.pol_order:
        for s in D.pol[u].get("settlements", []):
            if s["knowledge"]["visibility"] == "AUTHOR_TRUTH":
                rows.append((f"settlements/{s['id']}", f"{s['name']} — {s['body']}", s["knowledge"]["basis"], tagk(s["knowledge"])))
    for mid in D.mission_order:
        for r in D.missions[mid]["chronology"]:
            if r["visibility"] == "AUTHOR_TRUTH":
                rows.append((f"chronology/{mid}/{r['year']}/{r['event']}", r["text"], "хроника миссии", tag(r["status"], r["visibility"])))
    d.gen("hidden", table(["Путь в данных", "Что", "Основание", "Статус"], rows))
    d.narr("hidden_notes", "Заметки автора", "как и когда скрытая правда может всплыть в игре; кто из персонажей её знает.")
    docs.append(d)
    # missing ships
    path = "AUTHOR/MISSING_SHIPS.md"
    d = new_doc(D, path, "Пропавшие, погибшие и скрытые корабли", "author", "> [СПОЙЛЕР] Правда о судьбе кораблей. Публичная версия — в SHIPS/.")
    for mid in D.mission_order:
        if not (D.lost(mid) or not D.mission_public(mid)):
            continue
        m = D.missions[mid]
        f = m["fate"]
        unk = [u for u in D.F["unknowns"] if u["id"].startswith(mid + ".")]
        g = [table(["Поле", "Значение", "Статус"], [("Досье", link(path, ship_path(D, mid), ship_path(D, mid)), tag(m["status"], m["visibility"])),
                                                    ("Итог", f["state"], tagk(f)), ("Корабль", f["ship"], tagk(f)), ("Правда", f.get("note"), tagk(f)),
                                                    ("Можно найти в игре", "да" if D.cm[mid]["fate"].get("findable") else "нет", tagk(f))]
                     + [("Скрытая запись " + str(r["year"]), r["text"], tag(r["status"], r["visibility"])) for r in m["chronology"] if r["visibility"] == "AUTHOR_TRUTH"]
                     + [("Открытый вопрос", u["text"], tag(u["status"], u["visibility"])) for u in unk])]
        d.text(f"## {mid}")
        d.gen(f"ship.{mid}", "\n".join(g))
        if f.get("status") == "UNKNOWN" or unk:
            d.narr(f"{mid}.truth", f"{mid}: правда", "НЕ ЗАПОЛНЯТЬ ответом: судьба намеренно UNKNOWN даже для автора. Можно описать только версии и улики.", level=3)
        else:
            d.narr(f"{mid}.truth", f"{mid}: правда", "развернуть скрытую правду (PROPOSAL — нужна санкция автора): что случилось, какие улики остались, кто знает.", level=3)
    docs.append(d)
    # quest truths
    path = "AUTHOR/QUEST_TRUTHS.md"
    d = new_doc(D, path, "Правды для квестов", "author", "> [СПОЙЛЕР] Что игрок может найти и что намеренно останется без ответа.")
    rows = []
    for mid in D.mission_order:
        m = D.missions[mid]
        hid = [r for r in m["chronology"] if r["visibility"] == "AUTHOR_TRUTH"]
        if D.lost(mid) or hid or not D.mission_public(mid):
            rows.append((link(path, ship_path(D, mid), mid), m["fate"]["state"], m["fate"]["ship"], "да" if D.cm[mid]["fate"].get("findable") else "нет",
                         "; ".join(r["text"] for r in hid) or m["fate"].get("note"), tagk(m["fate"])))
    g = [table(["Миссия", "Итог", "Корабль", "Находим", "Правда", "Статус"], rows), "", "Намеренно без ответа (UNKNOWN):", "",
         table(["ID", "Вопрос", "Статус"], [(u["id"], u["text"], tag(u["status"], u["visibility"])) for u in D.F["unknowns"]])]
    d.gen("quests", "\n".join(g))
    d.narr("quest_hooks", "Крючки квестов", "для каждой находимой правды: где улика, кто её охраняет, что меняется в мире после раскрытия. UNKNOWN не раскрывать.")
    docs.append(d)
    # proposals
    path = "AUTHOR/PROPOSALS.md"
    d = new_doc(D, path, "Предложения Claude (PROPOSAL) — на утверждение автору", "author")
    groups = {}
    for it in items:
        if it["status"] == "PROPOSAL":
            groups.setdefault(it["path"].split("/")[0], []).append(it)
    g = ["Всё, что добавлено или пересчитано Claude. Результаты модели (население, экономика, коридоры, технологии) — тоже PROPOSAL, но поштучно не перечислены.", ""]
    for k in sorted(groups):
        g += [f"### {k} ({len(groups[k])})", "", table(["Путь", "Что", "Основание", "Статус"], [(it["path"], cell(it["text"])[:160], it["basis"], tag(it["status"], it["visibility"])) for it in groups[k]]), ""]
    sp = D.ST["polities"]
    g += [f"### settlements ({sum(len(v) for v in sp.values())})", "", table(["Полития", "Записей", "Поселения"], [(D.pname(u), len(v), ", ".join(r["name"] for r in v)) for u, v in sorted(sp.items())])]
    d.gen("proposals", "\n".join(g))
    docs.append(d)
    return docs


# ---------------------------------------------------------------- README
README_SKELETON = """## Назначение

Постоянный справочник лора для разработчиков и сценаристов: корабли поколений, колонии, поселения, экономика, демография и политика к 3126 году. Это **справочник, а не учебник**: каждая таблица выводится из машинного канона, рассказ пишется вокруг неё.

## Иерархия источников

1. **Машинный канон** — `tools/colonization_lab/canon/*.json` и выходы лаборатории `tools/colonization_lab/out/lore_facts.json`, `out/lab_state_3126.json`. Это **факты**. При противоречии прав JSON.
2. **Notes/Lore** (эта папка) — человеческий справочник. Блоки `GENERATED` выводятся из (1) скриптом и не правятся руками; рассказ (`NARRATIVE`) пишут сценаристы.
3. **Notes/Lore/AUTHOR/** — скрытая правда и спойлеры (`AUTHOR TRUTH`), предложения на утверждение. Публичные разделы ссылаются туда только с меткой [СПОЙЛЕР].
4. **Старые заметки** (`Notes/History/generation ships/*`, `Notes/fate.md`, `Notes/History/history.md`, `Notes/History/COLONIZATION_CANON_3126.md`, `WORLD_ECONOMY_AND_TRAFFIC_DESIGN.md`) — исходный материал. Их числа населения устарели.

## Как обновлять и проверять

```bash
cd tools/colonization_lab
python lab.py                 # пересчитать лабораторию (out/)
python lore_md.py --write     # обновить блоки GENERATED (текст вне блоков не трогается)
python lore_md.py --check     # блоки совпадают с данными, ссылки на сущности существуют, рассказ не спорит с населением
```

Правила для сценариста:
- писать только в секциях `NARRATIVE` и вообще вне маркеров `GENERATED`; строку `_TODO: ..._` удалить, когда секция написана;
- числа населения в рассказе — только из таблиц (проверка сверяет строки с «млн / млрд / тыс.» и именем сущности); чтобы сознательно процитировать устаревшее число, добавить в строку `lore-check: ignore`;
- ссылки на сущности — ссылками на файлы досье или id в обратных кавычках; неизвестный id ломает проверку;
- `AUTHOR TRUTH` не цитировать вне `AUTHOR/`; `UNKNOWN` не заполнять ответом."""


def readme_doc(D, docs):
    d = new_doc(D, "README.md", "Notes/Lore — справочник лора 3126", "readme", README_SKELETON)
    sc = D.F["status_counts"]
    d.text("## Счётчики статусов машинного канона")
    d.gen("counts", ", ".join(f"`{VIS.get(k, k)}` {v}" for k, v in sorted(sc.items())) + f". Схема данных: `{D.F['schema']}`, seed {D.F['seed']}, сценарий `{D.F['scenario']}`.")
    folders, nested = {}, {}
    for x in docs:
        parts = x.path.split("/")
        if len(parts) > 2:   # AUTHOR/SHIPS/<hidden mission>: count only, never name it in a public file
            nested[parts[0] + "/" + parts[1] + "/"] = nested.get(parts[0] + "/" + parts[1] + "/", 0) + 1
            continue
        folders.setdefault(parts[0] if len(parts) > 1 else ".", []).append(x)
    rows = []
    for f, xs in sorted(folders.items()):
        if f == ".":
            continue
        idx = [x for x in xs if x.path.endswith("INDEX.md")]
        first = idx[0] if idx else sorted(xs, key=lambda x: x.path)[0]
        files = ", ".join(link("README.md", x.path, posixpath.basename(x.path)) for x in sorted(xs, key=lambda x: x.path) if len(xs) <= 8) or "см. INDEX.md"
        for k, n in sorted(nested.items()):
            if k.startswith(f + "/"):
                files += f"; {k} — {n} скрыт. досье [СПОЙЛЕР]"
        rows.append((link("README.md", first.path, f + "/"), len(xs), files))
    d.text("## Оглавление")
    d.gen("index", table(["Папка", "Файлов", "Файлы"], rows))
    d.text("## Пробелы данных")
    d.gen("gaps", data_gaps(D))
    return d


def data_gaps(D):
    L = []
    pub = [m for m in D.mission_order if D.mission_public(m)]
    nophys = [m for m in pub if m not in D.S["sanity"]]
    noarr = [m for m in pub if not D.missions[m].get("arrival")]
    nodest = [m for m in pub if not isinstance(D.missions[m]["destination"]["system"], int)]
    L.append("- Корабли: в каноне нет полей размеров, двигателя, энергетики и груза — только заявленная масса (досье) и расчёт physics.py; у полей когорт, мотивов, политики рождений нет собственного статуса (наследуется от миссии).")
    L.append(f"- Нет расчёта physics.py: {', '.join(nophys) or 'нет'}.")
    L.append(f"- Нет прибытия: {', '.join(noarr) or 'нет'}.")
    L.append(f"- Цель вне каталога или не задана: {', '.join(nodest) or 'нет'}.")
    nh = len(D.mission_order) - len(pub)
    if nh:
        L.append(f"- Скрытые миссии (AUTHOR TRUTH, {nh}) в этот список не входят — см. AUTHOR/MISSING_SHIPS.md.")
    noeco = [u for u in D.pol_order if not D.pol[u].get("economy_3126")]
    nocp = [u for u in D.pol_order if not D.pol[u]["checkpoints"]]
    noauth = [u for u in D.pol_order if not D.pol[u]["population_3126"].get("authored")]
    noreg = [u for u in D.pol_order if not D.pol[u]["regime"].get("value")]
    nopop = [u for u in D.pol_order if not any(s.get("population") for s in D.pol[u].get("settlements", []))]
    L.append(f"- Единицы без экономики 3126: {', '.join(f'`{u}`' for u in noeco) or 'нет'}.")
    L.append(f"- Единицы без контрольных точек модели: {', '.join(f'`{u}`' for u in nocp) or 'нет'}.")
    L.append(f"- Единицы без авторского населения: {', '.join(f'`{u}`' for u in noauth) or 'нет'}.")
    L.append(f"- Единицы без режима 3126: {', '.join(f'`{u}`' for u in noreg) or 'нет'}.")
    L.append(f"- Поселения без населения (модель не распределила): {', '.join(f'`{u}`' for u in nopop) or 'нет'}.")
    L.append("- Поселения: нет года основания (кроме столиц — год политии) и состояния 3126 (кроме промышленных площадок с выпуском).")
    L.append("- Верфи: нет классов кораблей и разделения ремонт / военные / гражданские; есть только тип актива и выпуск корпусов.")
    return "\n".join(L)


# ---------------------------------------------------------------- assemble
def build_docs(D):
    docs = []
    for mid in D.mission_order:
        docs.append(ship_doc(D, mid, author=not D.mission_public(mid)))
    docs.append(ships_index(D))
    docs.append(colonies_index(D))
    for u in D.pol_order:
        docs.append(colony_doc(D, u))
    docs += settlement_docs(D)
    docs += economy_docs(D)
    docs += lore_sites.docs(D, sys.modules[__name__])
    docs += demography_docs(D)
    docs += politics_docs(D)
    docs += timeline_docs(D)
    docs += author_docs(D)
    docs.append(readme_doc(D, list(docs)))
    return sorted(docs, key=lambda x: x.path)


# ---------------------------------------------------------------- check
MISSION_TOKEN = re.compile(r"(?<![A-Za-z0-9\-])[A-Z][A-Z0-9]*(?:-[A-Z0-9]+)*-[0-9]+(?![A-Za-z0-9])")
SNAKE_TOKEN = re.compile(r"`([a-z][a-z0-9_]*_[a-z0-9_]*(?:\.[a-z0-9_]+)?)`")
LINK_RE = re.compile(r"\]\(([^)#\s]+\.md)(?:#[^)]*)?\)")
NUM_RE = re.compile(r"(\d+(?:[   ]\d{3})*(?:[.,]\d+)?)\s*(млрд|млн|тыс\.)")
UNIT = {"млрд": 1e9, "млн": 1e6, "тыс.": 1e3}


def _walk_strings(x, acc):
    if isinstance(x, dict):
        for k, v in x.items():
            acc.add(str(k))
            _walk_strings(v, acc)
    elif isinstance(x, list):
        for v in x:
            _walk_strings(v, acc)
    elif isinstance(x, str):
        acc.add(x)
        for t in MISSION_TOKEN.findall(x):
            acc.add(t)


def known_tokens(D):
    acc = set()
    for obj in (D.F, D.MS, D.W, D.CU, D.ST, D.X or {}, D.SX or {}):
        _walk_strings(obj, acc)
    for u in D.pol_order:
        for s in D.pol[u].get("settlements", []):
            acc.add(s["id"])
    acc |= {"UTF-8", "SHA-256"}
    return acc


def entity_prefixes(D):
    """First snake segment of every entity id (polities, assets, settlements, unknown ids)."""
    ids = set(D.pol_order) | set(D.assets) | {"sol_authority", "earth_north", "earth_south"}
    for u in D.pol_order:
        ids |= {s["id"] for s in D.pol[u].get("settlements", [])}
    return {i.split("_")[0].split(".")[0] for i in ids}


def entity_figures(D):
    """[(name, figures)] for the population heuristic."""
    ents = []

    def names_of(n):
        out = {n}
        for part in re.split(r"\s+/\s+|\(|\)", n):
            part = part.strip(" «»\"")
            if len(part) >= 4:
                out.add(part)
        return out
    groups = {}
    for u in D.pol_order:
        p = D.pol[u]
        vals = {p["population_3126"]["model"]} | ({p["population_3126"]["authored"]} if p["population_3126"].get("authored") else set())
        vals |= {r["pop"] for r in p["checkpoints"]}
        vals |= {s["population"] for s in p.get("settlements", []) if s.get("population") and s["role"] == "capital"}
        tg = D.PH["units"].get(u, {}).get("target")
        if tg:
            vals |= {tg[0], tg[1], (tg[0] + tg[1]) / 2.0}
        for n in names_of(p["name"]):
            ents.append((n, vals))
        if p.get("polity"):
            groups.setdefault(p["polity"], []).append(p)
    for g, ps in groups.items():
        vals = set()
        for p in ps:
            vals |= {p["population_3126"]["model"]} | ({p["population_3126"]["authored"]} if p["population_3126"].get("authored") else set())
            vals |= {r["pop"] for r in p["checkpoints"]}
        vals.add(sum(p["population_3126"]["model"] for p in ps))
        vals.add(sum(p["population_3126"].get("authored") or 0 for p in ps))
        for n in names_of(g):
            ents.append((n, vals))
    for mid in D.mission_order:
        m = D.missions[mid]
        vals = {m["start"]["population"], m["start"]["awake"], m["start"]["cryo"]}
        if m.get("arrival"):
            vals.add(m["arrival"]["population"])
        for r in m["chronology"]:
            vals |= {r["population"], r.get("awake") or 0, r.get("cryo") or 0}
        for n in set([mid] + m["aliases"]):
            ents.append((n, vals))
    E = D.F["earth"]
    ev = {E["anchor"]["pop"]} | {c["pop"] for c in E["checkpoints_authored"]}
    for r in E["trajectory"]:
        ev |= {r["pop"], r["north"], r["south"]}
    ev |= {r["pop_2026"] for r in D.W["earth_demography"]["regions"]}
    ev.add(E["return_3126"]["earth"]["pop"])
    for k in ("baseline", "transition_persists"):
        ev.add(D.F["demography_sensitivity"][k]["earth"])
    ev |= {w["per_year"] for w in D.W["migration"]["earth_windows"]}
    ev |= {w["per_year"] for w in D.PH["earth_programme_windows"]}
    for key in ("earth", "sol_outside_earth"):
        t = D.PH["world_targets"][key]
        ev |= {t["centre"], t["range"][0], t["range"][1]}
    if D.X:
        wp = D.X["economy"]["world"]["population"]
        ev |= {wp["earth"], wp["sol_outside_earth"], wp["earth"] + wp["sol_outside_earth"]}
    for n in ("Земля", "Sol Authority", "Солнечная Администрация"):
        ents.append((n, ev))
    off = {D.F["demography_sensitivity"][k]["off_sol"] for k in ("baseline", "transition_persists")}
    t = D.PH["world_targets"]["off_sol"]
    off |= {t["centre"], t["range"][0], t["range"][1]}
    for n in ("Вне Сола", "вне Сола"):
        ents.append((n, off))
    return ents


def number_matches(txt, unit, vals):
    t = txt.replace(" ", "").replace(" ", "").replace(" ", "").replace(",", ".")
    x = float(t)
    dec = len(t.split(".")[1]) if "." in t else 0
    tol = 0.5 * 10 ** (-dec) * UNIT[unit] * 1.0001
    best = min(vals, key=lambda v: abs(v - x * UNIT[unit])) if vals else None
    return (best is not None and abs(best - x * UNIT[unit]) <= tol), best


def check_text(D, rel, text, tokens, prefixes, ents, hidden_missions, problems):
    outside = BLOCK_RE.sub(lambda m: "\n" * m.group(0).count("\n"), text)
    for i, line in enumerate(text.split("\n"), 1):
        for t in MISSION_TOKEN.findall(line):
            if t not in tokens:
                problems.append(f"{rel}:{i}: неизвестный id миссии/сущности {t}")
            if t in hidden_missions and not rel.startswith("AUTHOR/"):
                problems.append(f"{rel}:{i}: публичный файл упоминает скрытую (AUTHOR TRUTH) миссию {t}")
        for t in SNAKE_TOKEN.findall(line):
            if t.endswith((".py", ".md", ".json")) or t in tokens or t.split("_")[0] not in prefixes:
                continue
            problems.append(f"{rel}:{i}: неизвестный id `{t}`")
        for target in LINK_RE.findall(line):
            if "://" in target:
                continue
            p = posixpath.normpath(posixpath.join(posixpath.dirname(rel), target))
            if not os.path.isfile(os.path.join(LORE, *p.split("/"))) and not os.path.isfile(os.path.normpath(os.path.join(LORE, os.path.dirname(rel), target))):
                problems.append(f"{rel}:{i}: битая ссылка {target}")
    for i, line in enumerate(outside.split("\n"), 1):
        if "lore-check: ignore" in line:
            continue
        nums = NUM_RE.findall(line)
        if not nums:
            continue
        hit = [(n, v) for n, v in ents if n in line]
        if not hit:
            continue
        vals = set()
        for _, v in hit:
            vals |= v
        for m in NUM_RE.finditer(line):
            txt, unit = m.group(1), m.group(2)
            if re.search(r"[–\-…]\s*$", line[:m.start()]):
                continue   # upper end of a range ("10–33 млн"): a rounded bracket, not a figure
            ok, best = number_matches(txt, unit, vals)
            if not ok:
                problems.append(f"{rel}:{i}: «{txt} {unit}» при «{', '.join(sorted({n for n, _ in hit}))}» не совпадает с данными (ближайшее {num(best)})")


def run(write):
    D = Data()
    docs = build_docs(D)
    problems, written, unchanged = [], 0, 0
    spec = {x.path for x in docs}
    for doc in docs:
        fp = os.path.join(LORE, *doc.path.split("/"))
        if os.path.isfile(fp):
            with open(fp, encoding="utf-8") as f:
                old = f.read()
            new, missing, stale, malformed = refresh(old, doc)
            if malformed:
                problems.append(f"{doc.path}: незакрытый маркер GENERATED")
            if new != old:
                if write:
                    with open(fp, "w", encoding="utf-8", newline="\n") as f:
                        f.write(new)
                    written += 1
                else:
                    diff = [b for b, c in doc.blocks() if b not in missing and block(b, c) not in old]
                    problems.append(f"{doc.path}: блоки устарели {diff}" + (f", нет блоков {missing}" if missing else "") + (f", лишние блоки {stale}" if stale else ""))
            else:
                unchanged += 1
        else:
            if write:
                os.makedirs(os.path.dirname(fp), exist_ok=True)
                with open(fp, "w", encoding="utf-8", newline="\n") as f:
                    f.write(render_new(doc))
                written += 1
            else:
                problems.append(f"{doc.path}: файла нет (запустите --write)")
    # orphans + content checks over every md file of the reference
    tokens = known_tokens(D)
    ents = entity_figures(D)
    prefixes = entity_prefixes(D)
    hidden = {m for m in D.mission_order if not D.mission_public(m)}
    todo = {}
    for dirpath, dirnames, files in os.walk(LORE):
        dirnames.sort()
        for fn in sorted(files):
            if not fn.endswith(".md"):
                continue
            fp = os.path.join(dirpath, fn)
            rel = os.path.relpath(fp, LORE).replace(os.sep, "/")
            with open(fp, encoding="utf-8") as f:
                text = f.read()
            if rel not in spec and BEGIN_RE.search(text):
                problems.append(f"{rel}: сгенерированный файл без спецификации (сущность исчезла из данных?)")
            check_text(D, rel, text, tokens, prefixes, ents, hidden, problems)
            n = len(re.findall(r"<!-- NARRATIVE:[^>]+-->\n_TODO:", text))
            if n:
                todo[rel] = n
    return D, docs, problems, written, unchanged, todo


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--write", action="store_true")
    g.add_argument("--check", action="store_true")
    a = ap.parse_args()
    try:
        sys.stdout.reconfigure(encoding="utf-8")
    except (AttributeError, ValueError):
        pass
    D, docs, problems, written, unchanged, todo = run(a.write)
    kinds = {}
    for x in docs:
        kinds[x.kind] = kinds.get(x.kind, 0) + 1
    print(f"lore_md: {len(docs)} files ({', '.join(f'{k} {v}' for k, v in sorted(kinds.items()))}); "
          f"{'written ' + str(written) + ', ' if a.write else ''}unchanged {unchanged}; open TODO narrative sections {sum(todo.values())} in {len(todo)} files")
    if a.check or problems:
        print("lore_md check:", "OK" if not problems else f"{len(problems)} проблем")
        for p in problems:
            print("  -", p)
    return 1 if (a.check and problems) else 0


if __name__ == "__main__":
    sys.exit(main())
