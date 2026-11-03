"""Economy / logistics / defence / navigation layer of the Notes/Lore reference.

Called by lore_md.py. Reads the machine outputs of world_economy.py
(out/world_3126/*.json) and the population history, and renders GENERATED
blocks: a full report for every polity (COLONIES/<id>.md, Sol in
POLITICS/EARTH_3126.md), a report for every system (SETTLEMENTS/*.md), the
humanity-wide totals and the new ECONOMY files. Text outside the blocks stays
hand-written.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
WE_DIR = os.path.join(HERE, "out", "world_3126")
NAMES = ("economy", "infrastructure", "trade_flows", "fleet_census", "defence", "beacons", "scenarios", "checks")
TAG = "`PROPOSAL` (модель)"
TARGET = "`AUTHOR_TARGET`"
ROLE_RU = {"bulk_cargo": "навал / контейнеры (поезда)", "medium_cargo": "средний груз", "tanker": "танкер", "shuttle": "челнок", "tug": "буксир",
           "maintenance": "сервис", "patrol": "патруль", "escort": "эскорт", "interceptor": "перехватчик", "military": "военный",
           "impact_defence": "противоастероидная оборона"}
SC_RU = {"cobrett_i_n1": "Cobrett-I N1", "medium_freighter": "фрейтер «Конвейер»", "jump_train": "прыжковый поезд", "jump_tanker": "поезд-танкер",
         "ocs4_lastochka": "OCS-4 «Ласточка»", "gas_tanker": "газовый танкер", "ore_tug": "рудный буксир", "service_tender": "сервисный тендер",
         "patrol_cutter": "патрульный катер", "interceptor": "перехватчик", "escort_frigate": "эскортный фрегат", "line_combatant": "линейный ракетный корабль",
         "recon_picket": "разведчик / РЭБ-пикет", "fleet_tender": "тендер флота", "impact_defence_tug": "буксир ПАО"}
CHANNEL_RU = {"official": "официальный", "re_export": "реэкспорт через нейтрала", "sanctioned": "под санкциями", "smuggling": "контрабанда", "covert_state": "тайный госканал"}
ERA_RU = {"sealed_habitat": "замкнутые поселения", "industrial_recovery": "промышленное восстановление", "habitat_boom": "жилищный бум",
          "terraforming": "терраформинг", "high_fertility": "высокая рождаемость", "hyper_migration": "гипер-миграция", "urban_saturation": "насыщение"}


def load():
    if not os.path.isfile(os.path.join(WE_DIR, "economy_3126.json")):
        return None
    out = {}
    for n in NAMES:
        with open(os.path.join(WE_DIR, f"{n}_3126.json"), encoding="utf-8") as f:
            out[n] = json.load(f)
    with open(os.path.join(HERE, "canon", "ship_economics.json"), encoding="utf-8") as f:
        out["ships"] = json.load(f)
    with open(os.path.join(HERE, "out", "logistics.json"), encoding="utf-8") as f:
        out["logistics"] = json.load(f)
    with open(os.path.join(HERE, "canon", "population_history.json"), encoding="utf-8") as f:
        out["ph"] = json.load(f)
    return out


def n0(x):
    if x is None:
        return "—"
    return f"{int(round(x)):,}".replace(",", " ")


def mt(x):
    return f"{x / 1e6:,.2f}".replace(",", " ")


def bn(x):
    return f"{x / 1e9:,.2f}".replace(",", " ")


def mln(x):
    return f"{x / 1e6:,.1f}".replace(",", " ")


def T(head, rows, lm):
    return lm.table(head, rows)


# ---------------------------------------------------------------- polity report
def polity_report(D, lm, X, k, path):
    E = X["economy"]["polities"][k]
    inf = X["infrastructure"]["polities"][k]
    fl = X["fleet_census"]["polities"][k]
    de = X["defence"]["polities"][k]
    com = {c["id"]: c["name"] for c in X["economy"]["commodities"]}
    L = []
    tg = E.get("target")
    L += ["### Население", "",
          f"{n0(E['population'])} человек" + (f"; цель автора {mln(tg[0])}–{mln(tg[1])} млн ({TARGET})" if tg else "") + f"; рост {100 * E['growth_per_year']:.2f} %/год ({TAG})."]
    L += ["", "### Расселение", "", T(["Где живут", "Людей", "Статус"], [(a, n0(b), TAG) for a, b in E["settlement_structure"].items() if b > 0], lm), "",
          T(["Город / объект", "Тип", "Тело", "Население", "Статус"], [(c["name"], c["role"], c["body"], n0(c["pop"]), lm.tag(c["status"])) for c in E["cities"]], lm),
          "", f"Поселений всего ~{n0(E['city_stats']['settlements_total'])}, крупнейший город ~{n0(E['city_stats']['largest_city'])} (Ципф, {TAG})."]
    bal = E["balance"]
    L += ["", "### Экономический выпуск", "",
          f"Выпуск {E['gdp_cr_y'] / 1e12:,.2f} трлн Cr/год ({n0(E['gdp_per_capita_cr'])} Cr на человека; индекс дохода {E['income_index']}).".replace(",", " "), "",
          T(["Категория", "Производство, млн т", "Потребление", "Импорт", "Экспорт", "Дефицит", "Статус"],
            [(com[c], mt(b["production"]), mt(b["consumption"]), mt(b["imports"]), mt(b["exports"]), mt(b["unmet"]), TAG)
             for c, b in bal.items() if b["production"] + b["imports"] > 1e4], lm)]
    flows = X["trade_flows"]["flows"]
    imp = [f for f in flows if f["buyer"] == k][:10]
    exp = [f for f in flows if f["seller"] == k][:10]

    def frow(f, other):
        return (lm.link(path, f"COLONIES/{other}.md", D.pname(other)) if other in D.pol else D.pname(other), com[f["commodity"]], mt(f["tonnes_y"]),
                f"{f['value_cr_y'] / 1e9:,.1f}".replace(",", " "), " → ".join(D.sname(s) for s in f["path"]), SC_RU.get(f["ship_class"], f["ship_class"]),
                f"{f['ships']:.1f}", CHANNEL_RU.get(f["channel"], f["channel"]), TAG)
    head = ["Партнёр", "Товар", "млн т/год", "млрд Cr/год", "Маршрут", "Корабли", "Кораблей", "Канал", "Статус"]
    L += ["", "### Импорт", "", f"Всего {E['imports_cr_y'] / 1e9:,.1f} млрд Cr/год. Крупнейшие потоки:".replace(",", " "), "",
          T(head, [frow(f, f["seller"]) for f in imp], lm) if imp else "Межзвёздного импорта нет."]
    L += ["", "### Экспорт", "", f"Всего {E['exports_cr_y'] / 1e9:,.1f} млрд Cr/год. Крупнейшие потоки:".replace(",", " "), "",
          T(head, [frow(f, f["buyer"]) for f in exp], lm) if exp else "Межзвёздного экспорта нет."]
    rows = []
    for a, r in sorted(inf["assets"].items(), key=lambda kv: (-kv[1]["count"], kv[0])):
        if r["count"] == 0:
            continue
        nm = inf["named"].get(a, {})
        rows.append((r["name"], n0(r["count"]), n0(nm.get("major")), ", ".join(x["name"] for x in nm.get("named", [])) or "—", n0(r["workers"]), n0(r["power_mw"]),
                     n0(r["spare_parts_t_y"]), TAG))
    L += ["", "### Инфраструктура", "", f"Работников инфраструктуры {n0(inf['totals']['workers'])}, мощность {n0(inf['totals']['power_mw'])} МВт, "
          f"оборудования {n0(inf['totals']['equipment_t'])} т, износ → запчасти {n0(inf['totals']['spare_parts_t_y'])} т/год.", "",
          T(["Тип", "Всего", "Крупных", "Именованные", "Работников", "МВт", "Запчасти т/год", "Статус"], rows, lm)]
    if inf["mines"]:
        L += ["", T(["Добыча", "Объектов", "Выпуск, млн т/год", "Качество", "Работников", "Автоматизация", "Оборудование, т", "Запчасти т/год", "Транспорт", "Выработка м³/год", "Статус"],
                    [(com[m["resource"]], n0(m["count"]), mt(m["annual_output_t"]), m["grade"], n0(m["workers"]), m["automation"], n0(m["equipment_stock_t"]),
                      n0(m["spare_parts_t_y"]), m["transport"], n0(m["mined_volume_m3_y"]), TAG) for m in inf["mines"]], lm)]
    fu = E["fuel"]
    L += ["", "### Топливо", "",
          T(["Показатель", "т/год", "Статус"], [
              ("реакторное топливо: потребность (баланс товаров: население, отрасли, флот)", n0(fu["reactor_fuel_t_y"]), TAG), ("— из них электростанции населения и объектов", n0(fu["reactor_fuel_power_t_y"]), TAG),
              ("— флот", n0(fu["reactor_fuel_ships_t_y"]), TAG), ("— своё производство", n0(fu["reactor_fuel_local_t_y"]), TAG),
              ("— импорт", n0(fu["reactor_fuel_imports_t_y"]), TAG), ("— не покрыто", n0(fu["reactor_fuel_unmet_t_y"]), TAG),
              ("заряд прыжка: потребность", n0(fu["jump_charge_t_y"]), TAG), ("— своё производство", n0(fu["jump_charge_local_t_y"]), TAG),
              ("— импорт", n0(fu["jump_charge_imports_t_y"]), TAG), ("— не покрыто", n0(fu["jump_charge_unmet_t_y"]), TAG),
              (f"стратегический резерв ({fu['reserve_days']} сут): реакторное / заряд", f"{n0(fu['strategic_reserve_reactor_t'])} / {n0(fu['strategic_reserve_charge_t'])}", TAG),
              ("топливных депо", n0(fu["depots"]), TAG)], lm)]
    lg_ = [f for f in flows if k in (f["seller"], f["buyer"])]
    tons = sum(f["tonnes_y"] for f in lg_)
    L += ["", "### Логистика", "", f"Межзвёздный грузооборот {mt(tons)} млн т/год в {len(lg_)} потоках; судов на этих потоках {sum(f['ships'] for f in lg_):.0f} "
          f"(владелец — экспортёр). Внутри системы: челноки, рудные буксиры, газовые танкеры (см. флот)."]
    rows = [(SC_RU.get(c, c), ROLE_RU.get(r["role"], r["role"]), n0(r["active"]), n0(r["reserve"]), n0(r["repair"]), r["new_per_year"], r["lost_per_year"], r["retired_per_year"], TAG)
            for c, r in sorted(fl.items(), key=lambda kv: (-kv[1]["active"], kv[0]))]
    L += ["", "### Флот", "", T(["Класс", "Роль", "Активных", "Резерв", "В ремонте", "Новых/год", "Потерь/год", "Списано/год", "Статус"], rows, lm)]
    y = inf["shipyards"]
    L += ["", "### Судостроение", "",
          f"Потребность в новых корпусах: гражданских {n0(y['civil_dry_t_y'])} т сухой массы/год, военных {n0(y['military_dry_t_y'])} т/год, "
          f"прыжковых двигателей {y['jump_drives_y']:.0f}/год." + (f" Своих верфей нет: корабли строит {D.pname(y['supplied_by'])}." if y.get("supplied_by") else ""),
          f"Верфей: крупных гражданских {inf['assets'].get('civil_shipyard', {}).get('count', 0)}, малых {inf['assets'].get('small_yard', {}).get('count', 0)}, военных {inf['assets'].get('military_shipyard', {}).get('count', 0)} ({TAG})."]
    L += ["", "### Бюджет безопасности и обороны", "",
          f"**{100 * de['share_of_output']:.2f} % выпуска** = {de['budget_cr_y'] / 1e9:,.1f} млрд Cr/год; личный состав ~{n0(de['personnel'])}.".replace(",", " "), "",
          f"Почему: {de['why'] or '—'}.", "",
          T(["Составляющая доли", "Доля выпуска"], [(a, f"{100 * b:.2f} %") for a, b in de["components"].items()], lm), "",
          T(["Статья", "млрд Cr/год"], [(a, f"{b / 1e9:,.1f}".replace(",", " ")) for a, b in de["structure"].items()], lm)]
    rows = [(SC_RU.get(c, c), f"{v:.0f}", f"{de['task_ships'].get(c, 0):.0f}" if c in de["task_ships"] else "сдерживание", n0(de["annual_cost_per_ship"].get(c)), TAG)
            for c, v in de["forces"].items() if v >= 0.5]
    L += ["", "### Военные и полицейские роли", "", "Роли доктрины: " + (", ".join(de["roles"]) or "нет") + ".", "",
          T(["Класс", "Кораблей", "Из задач", "Стоимость корабля в год, Cr", "Статус"], rows, lm),
          "", f"Базы: военных {de['bases']['military']}, патрульных {de['bases']['patrol']}. Рискованных рейсов поездов {de['risky_train_trips']}/год; прибытий межзвёздных рейсов {n0(de['arrivals_per_year'])}/год."
          + (" **Задачи не покрываются бюджетом.**" if de["under_resourced"] else "")]
    mo = E["motives"]
    host = ", ".join(f"{D.pname(h['other'])} (обида {h['grievance']}{', та же система' if h['same_system'] else ''})" for h in de["hostile"]) or "нет"
    L += ["", "### Союзники и зависимости", "", f"Враждебные соседи: {host}.", "",
          T(["Мотив", "Вес", "Статус"], [(a, b, TAG) for a, b in mo.items() if a != "status"], lm)]
    L += ["", "### Стратегические уязвимости", "",
          T(["Что", "Тяжесть", "Последствие", "Статус"], [(v["what"], v["severity"], v["consequence"], TAG) for v in E["vulnerabilities"]], lm) if E["vulnerabilities"] else "Нет."]
    return "\n".join(L)


def eras_block(D, lm, X, uid):
    u = D.S["demography"]["units"].get(uid)
    if not u or not u.get("eras"):
        return None
    ph = X["ph"]["units"].get(uid, {})
    L = []
    if ph.get("story"):
        L += [f"{ph['story']} (`PROPOSAL`)", ""]
    if u.get("target"):
        L += [f"Цель автора 3126: {mln(u['target'][0])}–{mln(u['target'][1])} млн ({TARGET}); модель: {mln(u['pop_3126'])} млн; интенсивность эпох {u.get('intensity')} "
              "(1 = значения рассказа; подобрана `calibrate_population.py`, население не умножается).", ""]
    L.append(T(["Эпоха", "Годы", "Население: начало → конец", "Рост %/год", "СКР", "Рожд. ‰", "Смерт. ‰", "Миграция ‰", "Теснота", "Ярус жилья", "Доход", "Статус"],
               [(ERA_RU.get(e["era"], e["era"]), f"{e['start']}–{e['end']}", f"{n0(e['pop_start'])} → {n0(e['pop_end'])}", e["growth_pct"], e["tfr"], e["cbr"], e["cdr"],
                 e["net_migration_per_1000"], e["crowding"], e["housing_tier"], e["income"], TAG) for e in u["eras"]], lm))
    return "\n".join(L)


# ---------------------------------------------------------------- system report
def system_report(D, lm, X, sid, path):
    eco = X["economy"]["polities"]
    owners = [k for k, e in eco.items() if e["system"] == sid]
    sites = []
    for k, e in eco.items():
        for a, v in e["settlement_structure"].items():
            if a.startswith("аванпост:") and D.W and any(o["system"] == sid and o["name"] in a for o in D.W["outposts"] + D.W.get("frontier_pools", [])):
                sites.append((k, a, v))
    Es = D.ST_E.get(sid) if hasattr(D, "ST_E") else None
    L = ["## Экономика системы 3126", ""]
    if Es:
        L.append(f"Тела каталога: пригодных площадок {Es['n_sealed']} (купола), открытых {Es['n_open']}, малых тел {Es['n_minor']}, газовых гигантов {len(Es.get('giants', []))}; "
                 f"пояс {Es['belt_me']:.4f} M⊕, кометы {Es['comet_me']:.3f} M⊕.")
        L.append("")
    rows = [(lm.link(path, f"COLONIES/{k}.md", D.pname(k)) if k in D.pol else D.pname(k), n0(eco[k]["population"]), f"{eco[k]['gdp_cr_y'] / 1e12:,.2f}".replace(",", " "), TAG) for k in owners]
    rows += [(D.pname(k), n0(v), "—", TAG) for k, a, v in sites]
    L += [T(["Полития / аванпост", "Население", "Выпуск, трлн Cr", "Статус"], rows, lm) if rows else "Постоянного населения нет."]
    inf_rows = []
    for k in owners:
        inf = X["infrastructure"]["polities"][k]
        a = inf["assets"]
        g = lambda t: a.get(t, {}).get("count", 0)
        inf_rows.append((D.pname(k), g("mine") + g("ice_mine"), g("metallurgy") + g("chemical_plant") + g("construction_works"), g("factory") + g("electronics_fab") + g("consumer_factory"),
                         g("trade_hub"), g("orbital_port"), g("fuel_depot") if "fuel_depot" in a else X["economy"]["polities"][k]["fuel"]["depots"], g("military_base") + g("patrol_base"),
                         g("civil_shipyard") + g("small_yard") + g("military_shipyard"), TAG))
    if inf_rows:
        L += ["", T(["Владелец", "Рудники и промыслы", "Переработка", "Заводы", "Торговые узлы", "Орбитальные порты", "Топливные депо", "Военные и патрульные базы", "Верфи", "Статус"], inf_rows, lm)]
    bs = [b for b in X["beacons"]["by_system"] if b["system"] == sid]
    if bs:
        b = bs[0]
        L += ["", f"Маяки: мост {b['bridge']}, торговых узлов {b['trade_hub']}, станционных {b['station']}, ретрансляторов трассы {b['relay']}, военных {b['military']}, "
              f"удалённых научных {b['remote_science']}; всего {b['total']}, содержание {b['cost_cr_y'] / 1e9:,.2f} млрд Cr/год ({TAG}).".replace(",", " ")]
    fl_rows = []
    for k in owners:
        for c, r in sorted(X["fleet_census"]["polities"][k].items(), key=lambda kv: -kv[1]["active"])[:6]:
            fl_rows.append((D.pname(k), SC_RU.get(c, c), n0(r["active"]), TAG))
    if fl_rows:
        L += ["", "Местный флот (крупнейшие классы):", "", T(["Владелец", "Класс", "Активных", "Статус"], fl_rows, lm)]
    com = {c["id"]: c["name"] for c in X["economy"]["commodities"]}
    fls = [f for f in X["trade_flows"]["flows"] if f["path"][0] == sid or f["path"][-1] == sid][:12]
    if fls:
        L += ["", "Крупнейшие потоки через систему:", "",
              T(["Откуда", "Куда", "Товар", "млн т/год", "Корабли", "Канал", "Статус"],
                [(D.pname(f["seller"]), D.pname(f["buyer"]), com[f["commodity"]], mt(f["tonnes_y"]), SC_RU.get(f["ship_class"], f["ship_class"]), CHANNEL_RU.get(f["channel"], f["channel"]), TAG) for f in fls], lm)]
    return "\n".join(L)


# ---------------------------------------------------------------- ECONOMY files
def docs(D, lm):
    X = D.X
    if X is None:
        return []
    out = []
    W = X["economy"]["world"]
    com = {c["id"]: c["name"] for c in X["economy"]["commodities"]}
    # world balance
    path = "ECONOMY/WORLD_BALANCE.md"
    d = lm.new_doc(D, path, "Баланс мира 3126", "economy")
    p = W["population"]
    ph = X["ph"]["world_targets"]
    g = ["Источник: `tools/colonization_lab/world_economy.py` → `out/world_3126/*.json`. Одна физическая система (запасы / потоки / решения): политика, экономика и война работают над ней.", "",
         T(["Слой", "Население", "Цель автора", "Статус"], [
             ("Земля", bn(p["earth"]) + " млрд", f"{bn(ph['earth']['range'][0])}–{bn(ph['earth']['range'][1])} млрд", TARGET),
             ("Сол вне Земли", bn(p["sol_outside_earth"]) + " млрд", f"{bn(ph['sol_outside_earth']['range'][0])}–{bn(ph['sol_outside_earth']['range'][1])} млрд", TARGET),
             ("вне Сола", bn(p["off_sol"]) + " млрд", f"{bn(ph['off_sol']['range'][0])}–{bn(ph['off_sol']['range'][1])} млрд", TARGET)], lm), "",
         f"Выпуск человечества {W['gdp_cr_y'] / 1e12:,.0f} трлн Cr/год. Межзвёздная торговля {mt(W['interstellar_tonnes_y'])} млн т/год, {W['interstellar_value_cr_y'] / 1e12:,.2f} трлн Cr/год, {W['interstellar_flows']} потоков ({TAG}).".replace(",", " "), "",
         "### Инфраструктура человечества", "",
         T(["Тип", "Всего", "Крупных", "Именованных", "Статус"], [(v["name"], n0(v["count"]), n0(v["major"]), v["named"], TAG) for a, v in sorted(W["infrastructure"].items(), key=lambda kv: (-kv[1]["count"], kv[0]))], lm), "",
         "Маяки: " + ", ".join(f"{a} {n0(b)}" for a, b in sorted(W["beacons"].items())) + f" ({TAG}).", "",
         "### Флот человечества", "",
         T(["Класс", "Роль", "Активных", "Резерв", "В ремонте", "Новых/год", "Потерь/год", "Списано/год", "Статус"],
           [(SC_RU.get(c, c), ROLE_RU.get(v["role"], v["role"]), n0(v["active"]), n0(v["reserve"]), n0(v["repair"]), n0(v["new_per_year"]), n0(v["lost_per_year"]), n0(v["retired_per_year"]), TAG)
            for c, v in sorted(W["fleet"].items(), key=lambda kv: (-kv[1]["active"], kv[0]))], lm), "",
         "### Товары (15 категорий)", "",
         T(["Категория", "Производство, млн т/год", "Потребление", "Межзвёздно", "Статус"],
           [(com[c], mt(v["production"]), mt(v["consumption"]), mt(v["interstellar"]), TAG) for c, v in W["goods"].items()], lm), "",
         "Корабельные агрегаты — не товар, а отдельный рынок обслуживания с серийными номерами: " + ", ".join(x["name"] for x in X["economy"]["part_catalog"]["parts"]) + ".", "",
         "### Политии", "",
         T(["Полития", "Население", "Выпуск, трлн Cr", "Импорт, млрд Cr", "Экспорт, млрд Cr", "Оборона, % выпуска", "Кораблей (все)", "Статус"],
           [(lm.link(path, f"COLONIES/{k}.md", e["name"]) if k in D.pol else lm.link(path, "POLITICS/EARTH_3126.md", e["name"]), n0(e["population"]), f"{e['gdp_cr_y'] / 1e12:,.2f}".replace(",", " "),
             f"{e['imports_cr_y'] / 1e9:,.1f}".replace(",", " "), f"{e['exports_cr_y'] / 1e9:,.1f}".replace(",", " "), f"{100 * X['defence']['polities'][k]['share_of_output']:.2f} %",
             n0(sum(r["active"] for r in X["fleet_census"]["polities"][k].values())), TAG) for k, e in X["economy"]["polities"].items()], lm), "",
         "### Проверки непротиворечивости", "", "\n".join(f"- {'OK' if c['ok'] else 'FAIL'} `{c['check']}` — {c['detail']}" for c in X["checks"]["checks"])]
    d.gen("world_balance", "\n".join(g))
    d.narr("balance_story", "Как это ощущается в игре", "что видит пилот: где тесно, где дорого, где пусто; опираться на таблицы выше.")
    out.append(d)
    # fleets
    path = "ECONOMY/FLEETS.md"
    d = lm.new_doc(D, path, "Флоты и классы кораблей 3126", "economy")
    S = X["ships"]["ship_classes"]
    L_ = X["logistics"]
    cc = L_["cobrett_cargo"]
    g = ["### Грузоподъёмность Cobrett (единая модель)", "",
         f"{cc['container_slots']} мест под контейнер {cc['container']} (до 20 т полезной массы, 23.3 м³, тара 1.7 т): {cc['cargo_m3']} м³, предел конструкции {cc['cargo_t_max']} т. "
         f"Обычный смешанный груз упирается в объём (~0.4 т/м³): типичная загрузка {cc['typical_mixed_load_t']} т; плотный груз (металл, боеприпасы) — до {cc['cargo_t_max']} т. "
         "Источник для рантайма: `src/game/catalog/ships/ShipCargoPassports.h`; трафик больше не держит свою грузоподъёмность (`PROPOSAL`).", "",
         "### Классы", "",
         T(["Класс", "Роль", "Назначение", "Сухая масса, т", "Груз, т", "м³", "C20", "Ускорение, g", "Экипаж", "Цена, Cr", "Срок, лет", "Прыжков на бак (пуст/пол/полн)", "Статус"],
           [(SC_RU.get(c, c), ROLE_RU.get(s["role"], s["role"]), s["purpose"], n0(s["dry_mass_t"]), n0(s["cargo_t"]), n0(s["cargo_m3"]), s.get("container_slots", 0), s["burn_accel_g"], s["crew"],
             n0(L_["build_cost"][c]["list_price_cr"]), s["service_life_years"],
             "/".join(str(L_["range_mean_jumps"][c][x]) for x in ("empty", "half", "full")) if c in L_["range_mean_jumps"] else "—", TAG) for c, s in S.items()], lm), "",
         "Тяжёлый перевозчик — не монолит, а **прыжковый поезд**: тягач-«Локомотив» с ядром прыжка + до 12 модулей по 4 800 т. Тягач оставляет модули на погрузке в депо и забирает готовые, поэтому дорогое ядро не простаивает; поле прыжка строится по оболочке, и длинный поезд платит в основном за массу (дёшево на тонну).", "",
         "### Флоты по политиям", ""]
    rows = []
    for k, rws in X["fleet_census"]["polities"].items():
        civ = sum(r["active"] for r in rws.values() if r["family"] == "civil")
        sec = sum(r["active"] for r in rws.values() if r["family"] == "security")
        mil = sum(r["active"] for r in rws.values() if r["family"] == "military")
        new = sum(r["new_per_year"] for r in rws.values())
        lost = sum(r["lost_per_year"] for r in rws.values())
        rows.append((X["economy"]["polities"][k]["name"], n0(civ), n0(sec), n0(mil), n0(new), n0(lost), TAG))
    g.append(T(["Полития", "Гражданских", "Безопасность", "Военных", "Новых/год", "Потерь/год", "Статус"], rows, lm))
    g += ["", "Флот выводится из потоков: судов = т/год ÷ (полезная загрузка × гружёных рейсов в год), + резерв + ремонт; новых в год = списание (флот / срок) + потери + рост. "
          f"Прыжковые двигатели для новых кораблей: {X['fleet_census']['yards']['jump_drives_total_y']:.0f} в год; изготовители ядер: " +
          ", ".join(f"{D.pname(a)} ({b})" for a, b in X["fleet_census"]["yards"]["core_makers"].items()) + "."]
    d.gen("fleets", "\n".join(g))
    d.narr("fleet_story", "Как выглядят флоты", "окраска, традиции экипажей, где стоят поезда, чем пахнет челнок; классы и числа — из таблиц выше.")
    out.append(d)
    # defence
    path = "ECONOMY/DEFENCE.md"
    d = lm.new_doc(D, path, "Безопасность, оборона и тактика 3126", "economy")
    dt = X["defence"]["doctrine"]
    g = ["Доля выпуска на безопасность и оборону выводится из доктрины политии (почему ей нужна сила: история, соседи, ресурсы, режим, маршруты, импактная угроза) и данных; "
         "силы — сначала из задач (таможня, SAR, маяки, противоастероидная оборона, конвои, разведка), остаток — сдерживание. Флот — не весь бюджет: базы, личный состав на берегу, "
         "разведка, обучение, запасы, программа ПАО и охрана маяков — отдельные статьи.", "",
         T(["Полития", "Доля выпуска", "Бюджет, млрд Cr", "Почему", "Линейных", "Эскортов", "Катеров", "Перехватчиков", "Буксиров ПАО", "Личный состав", "Статус"],
           [(X["economy"]["polities"][k]["name"], f"{100 * v['share_of_output']:.2f} %", f"{v['budget_cr_y'] / 1e9:,.0f}".replace(",", " "), v["why"] or "—",
             n0(v["forces"].get("line_combatant")), n0(v["forces"].get("escort_frigate")), n0(v["forces"].get("patrol_cutter")), n0(v["forces"].get("interceptor")),
             n0(v["forces"].get("impact_defence_tug")), n0(v["personnel"]), TAG) for k, v in X["defence"]["polities"].items()], lm), "",
         "### Тактика в космосе (доктрина)", ""] + [f"- {x}" for x in dt["principles"]] + ["", "### Минимальный набор боевых ролей", ""] + [f"- **{a}** — {b}" for a, b in dt["roles"].items()]
    d.gen("defence", "\n".join(g))
    d.narr("defence_story", "Флоты в рассказе", "известные эскадры, базы, традиции; коррупция возможна, но служба настоящая (таможня, SAR, ПАО).")
    out.append(d)
    # fuel and jump drive
    path = "ECONOMY/FUEL.md"
    d = lm.new_doc(D, path, "Топливо: реакторное и заряд прыжка", "economy")
    c = L_["calibration"]
    g = [f"Правило прыжка: `заряд_т = q · d_св.лет · (κ · площадь_оболочки + масса)`, q = {c['q_per_ly']}, κ = {c['kappa_t_per_m2']} т/м². Поле строится по оболочке корпуса (член κ·A), "
         "масса внутри — второй член: груз внутри корпуса добавляет массу, но не площадь. Расходник (заряд, т), энергия (от реактора при зарядке) и пиковая мощность (из накопителя ядра при разряде) — разные величины.", "",
         f"Средний прыжок сети 3126 — {c['mean_jump_ly']} св. лет (прыжки, которые реально используют кратчайшие маршруты). Заряд стоит {c['jump_charge_cr_per_kg']:.2f} Cr/кг: средний прыжок Cobrett с половиной груза ≈ {c['mean_jump_cost_cr']:.0f} Cr = 2.5 × внутрисистемного плеча (`PROPOSAL`).", "",
         T(["Класс", "Член поля, т", "Масса (пол-груза), т", "Заряд на средний прыжок, т", "кг/(св.год·1000 т корабля)", "кг/(св.год·1000 т груза)", "Энергия, ТДж", "Зарядка, ч", "Пик, ТВт", "Cr", "Статус"],
           [(SC_RU.get(a, a), v["alpha_field_term_t"], v["mass_half_load_t"], v["charge_per_mean_jump_t"], v["kg_per_ly_per_1000t"], v["kg_per_ly_per_1000t_cargo"] or "—",
             v["energy_tj"], v["charge_hours"], v["peak_discharge_tw"], n0(v["cost_cr"]), TAG) for a, v in L_["jump_profile"].items()], lm), "",
         "Внутрисистемная тяга (типовое плечо 1.2 а.е., половина груза):", "",
         T(["Класс", "Тяга, МН", "g", "Мощность привода, МВт", "Плечо, ч", "Реакторное топливо, кг", "Cr", "Тепло, МВт", "Радиаторы, МВт", "Статус"],
           [(SC_RU.get(a, a), v["thrust_mn"], v["burn_accel_g"], n0(v["drive_power_mw"]), v["leg_hours"], v["reactor_fuel_kg"], n0(v["fuel_cost_cr"]), n0(v["thermal_load_mw"]), n0(v["radiator_mw"]), TAG)
            for a, v in L_["insystem_profile"].items()], lm), "",
         "### Баланс топлива по политиям", "",
         T(["Полития", "Реакторное, т/год", "своё", "импорт", "Заряд, т/год", "своё", "импорт", "Не покрыто (реакт./заряд)", "Резерв, сут", "Депо", "Статус"],
           [(e["name"], n0(e["fuel"]["reactor_fuel_t_y"]), n0(e["fuel"]["reactor_fuel_local_t_y"]), n0(e["fuel"]["reactor_fuel_imports_t_y"]), n0(e["fuel"]["jump_charge_t_y"]),
             n0(e["fuel"]["jump_charge_local_t_y"]), n0(e["fuel"]["jump_charge_imports_t_y"]), f"{n0(e['fuel']['reactor_fuel_unmet_t_y'])} / {n0(e['fuel']['jump_charge_unmet_t_y'])}",
             e["fuel"]["reserve_days"], e["fuel"]["depots"], TAG) for k, e in X["economy"]["polities"].items()], lm)]
    d.gen("fuel", "\n".join(g))
    out.append(d)
    # beacons and navigation
    path = "ECONOMY/BEACONS_AND_NAVIGATION.md"
    d = lm.new_doc(D, path, "Маяки, транспондеры и навигация прыжка", "economy")
    B = X["beacons"]
    je = B["jump_error_model"]
    g = ["Маяк — физический объект: постоянно знает и передаёт своё положение, вектор скорости, точное время, сетевой идентификатор и состояние. Он движется (естественная орбита), его можно отбуксировать: "
         f"{B['towing']['note']}. Корабли в зоне маяка передают: " + ", ".join(B["transponder_fields"]) + " — аналог ADS-B/AIS: маяк собирает и ретранслирует картину трафика.", "",
         f"**{B['light_lag_rule']}.**", "",
         "### Ошибка выхода из прыжка", "", f"`σ_r = d · {je['base_fraction']} · двигатель · компьютер · (1 + возраст_эфемерид · {je['ephemeris_age_factor_per_year']}) · повреждение / привязка`; "
         f"привязка: маяки на обоих концах ×{je['lock']['both_ends']:.0f}, только на старте ×{je['lock']['origin_only']:.0f}, только на финише ×{je['lock']['destination_only']:.0f}, нет — ×1. "
         "Ошибка предсказуема диапазоном (3σ — коробка планирования), а не рулетка.", "",
         T(["Дистанция, св. лет", "Привязка", "Возраст эфемерид, лет", "Двигатель / повреждение", "σ положения, км", "σ скорости, км/с", "Статус"],
           [(x["d_ly"], x["lock"], x["ephemeris_age_y"], f"{x.get('drive', 'standard')} / {x.get('damage', 'none')}", n0(x["sigma_km"]), x["sigma_v_km_s"], TAG) for x in B["jump_error_examples"]], lm), "",
         "Поэтому на интенсивных трассах много маяков: они уменьшают ошибку выхода, дают навигационную опору и картину трафика, ретранслируют данные, облегчают спасение и позволяют держать расписания. "
         "Одиночные удалённые маяки (наука, тайные объекты, рудники, далёкие колонии) — хорошие игровые точки.", "",
         "### Мост-маяки систем", "",
         T(["Маяк", "Система", "Оператор", "С года", "Движение", "Покрытие, св. сек", "Треков", "Персонал", "Содержание, млн Cr/год", "Статус"],
           [(b["id"], D.sname(b["system"]), D.pname(b["operator"]), b["since"], b["motion"], b["coverage_ls"], n0(b["transponder_tracks"]), b["staff"], n0(b["cost_cr_y"] / 1e6), lm.tag(b["status"])) for b in B["bridge"]], lm), "",
         "### Маяки по системам", "",
         T(["Система", "Мост", "Торговые узлы", "Станции", "Ретрансляторы трассы", "Военные", "Удалённые научные", "Всего", "Содержание, млрд Cr/год", "Статус"],
           [(b["name"], b["bridge"], b["trade_hub"], b["station"], b["relay"], b["military"], b["remote_science"], b["total"], f"{b['cost_cr_y'] / 1e9:,.2f}".replace(",", " "), TAG) for b in B["by_system"]], lm), "",
         "### Трассы: коридор против траектории", "",
         "Шахта и завод движутся (орбиты, движение систем, станций), поэтому маршрут — не статичная линия: положение источника при отправлении → предсказанный перехват → прыжковое плечо по маякам → предсказанное положение назначения. "
         "Карта показывает **коридор** (статистический путь между экономическими узлами), а конкретный корабль летит по **траектории**, рассчитанной по текущим эфемеридам.", "",
         T(["Ребро", "св. лет", "млн т/год", "млрд Cr/год", "Класс", "Без маяка", "Охраняют", "Маяки", "Статус"],
           [(" — ".join(r["names"]), r["ly"], mt(r["tonnes_y"]), f"{r['value_cr_y'] / 1e9:,.1f}".replace(",", " "), r["klass"], "да" if r["unbeaconed"] else "",
             ", ".join(D.pname(x) for x in r["guarded_by"]), ", ".join(r["beacons_supporting"]), TAG) for r in sorted(X["trade_flows"]["routes"], key=lambda r: -r["tonnes_y"])], lm)]
    d.gen("beacons", "\n".join(g))
    d.narr("beacon_story", "Маяки в рассказе", "кто живёт на маяках, как их охраняют, что случается, когда маяк замолкает.")
    out.append(d)
    # scenarios
    path = "ECONOMY/SCENARIOS.md"
    d = lm.new_doc(D, path, "Аварии, блокады, помощь и война: прогоны модели", "economy")
    g = ["Каждый сценарий прогоняется через ту же физическую систему: удар → мощности / маршруты → потоки → цены → флот → восстановление. Помощь идёт по цепочке SupportProgram: обещано → выделено → отправлено → прибыло → освоено. Все — `" + TAG + "`.", ""]
    for s in X["scenarios"]["scenarios"]:
        g += [f"### {s['text']}", ""]
        if s.get("metrics"):
            mtr = s["metrics"]
            if all(isinstance(v, dict) and "before" in v for v in mtr.values()):
                g.append(T(["Полития", "Импорт до → после, млн т", "Цена импорта, индекс", "Реэкспорт, млн т", "Судов на потоках до → после", "Дефицит до → после, млн т"],
                           [(D.pname(k), f"{mt(v['before']['import_t'])} → {mt(v['after']['import_t'])}", v.get("import_price_index"), mt(v["after"]["re_export_t"]),
                             f"{v['before']['ships_on_its_flows']:.0f} → {v['after']['ships_on_its_flows']:.0f}", f"{mt(v['before']['shortage_t'])} → {mt(v['after']['shortage_t'])}") for k, v in mtr.items()], lm))
            else:
                g.append(T(["Показатель", "Значение"], [(a, b if not isinstance(b, list) else ", ".join(D.pname(x) for x in b)) for a, b in mtr.items()], lm))
            g.append("")
        ch = [c for c in s.get("changes", []) if c.get("price_index")]
        if ch:
            g += [T(["Полития", "Товар", "Индекс цены", "Дефицит до → после, млн т"], [(D.pname(c["polity"]), com[c["commodity"]], c["price_index"], f"{mt(c['shortage_t_before'])} → {mt(c['shortage_t_after'])}") for c in ch[:10]], lm), ""]
        sp = s.get("support_program") or (s.get("recovery") or {}).get("support")
        if sp:
            g += [f"SupportProgram: {D.pname(sp['donor'])} → {D.pname(sp['recipient'])}, мотив {sp['motive']}, бюджет {sp['budget_cr'] / 1e9:,.2f} млрд Cr.".replace(",", " "), "",
                  T(["Товар", "Обещано, т", "Выделено", "Отправлено", "Прибыло", "Освоено", "Корабли", "Маршрут"],
                    [(com[r["commodity"]], n0(r["pledged_t"]), n0(r["allocated_t"]), n0(r["shipped_t"]), n0(r["arrived_t"]), n0(r["absorbed_t"]), SC_RU.get(r["ship_class"], r["ship_class"]),
                      " → ".join(D.sname(x) for x in (r["route"] or []))) for r in sp["status_counts"]], lm), ""]
        rec = s.get("recovery") or {}
        if rec.get("re_export_flows"):
            g += [T(["Продавец", "Покупатель", "Через", "Товар", "т/год", "Наценка"], [(D.pname(r["seller"]), D.pname(r["buyer"]), D.pname(r["via"]), com[r["commodity"]], n0(r["tonnes"]), r["markup"]) for r in rec["re_export_flows"][:10]], lm), ""]
        if rec.get("mobilisation"):
            mb = rec["mobilisation"]
            g += [f"Мобилизация: обычная доля обороны {100 * mb['normal_defence_share']:.2f} % → чрезвычайная {100 * mb['emergency_share']:.1f} %. Конверсия: " +
                  "; ".join(f"{c['from_']} → {c['to']}" for c in mb["conversion"]) + ". Цена: " + "; ".join(mb["costs"]) + ".", ""]
        g += [f"Восстановление: {rec.get('text', '—')}" + (f" (~{rec['years']} лет)" if rec.get("years") else ""), ""]
    d.gen("scenarios", "\n".join(g))
    out.append(d)
    return out


def blocks_for_existing(D, lm, path):
    """Extra GENERATED blocks for existing ECONOMY files (returned as (block id, content))."""
    X = D.X
    if X is None:
        return []
    com = {c["id"]: c["name"] for c in X["economy"]["commodities"]}
    if path == "ECONOMY/TRADE_ROUTES.md":
        fl = X["trade_flows"]["flows"][:40]
        g = ["## Физические потоки 3126 (world_economy.py)", "",
             "Тонны, корабли и каналы — из баланса мира; таблицы ниже (рынок `lab.py`) — ценностная модель для шоков и отношений.", "",
             T(["Продавец", "Покупатель", "Товар", "млн т/год", "млрд Cr/год", "Маршрут", "Корабли", "Кораблей", "Канал", "Доставка, Cr/т", "Статус"],
               [(lm.link(path, f"COLONIES/{f['seller']}.md", D.pname(f["seller"])) if f["seller"] in D.pol else D.pname(f["seller"]),
                 lm.link(path, f"COLONIES/{f['buyer']}.md", D.pname(f["buyer"])) if f["buyer"] in D.pol else D.pname(f["buyer"]), com[f["commodity"]], mt(f["tonnes_y"]),
                 f"{f['value_cr_y'] / 1e9:,.1f}".replace(",", " "), " → ".join(D.sname(s) for s in f["path"]), SC_RU.get(f["ship_class"], f["ship_class"]), f"{f['ships']:.0f}",
                 CHANNEL_RU.get(f["channel"], f["channel"]), n0(f["transport_cr_per_t"]), TAG) for f in fl], lm), "",
             "Классы трасс: trunk ≥ 20 млн т/год или ≥ 5 млрд Cr/год; secondary ≥ 1 млн т или ≥ 0.5 млрд Cr; остальное — feeder. Список рёбер — ECONOMY/BEACONS_AND_NAVIGATION.md."]
        return [("flows_3126", "\n".join(g))]
    if path == "ECONOMY/INDUSTRIAL_ASSETS.md":
        rows = []
        for k, inf in X["infrastructure"]["polities"].items():
            a = inf["assets"]
            g_ = lambda t: a.get(t, {}).get("count", 0)
            rows.append((X["economy"]["polities"][k]["name"], g_("mine"), g_("ice_mine"), g_("metallurgy"), g_("chemical_plant"), g_("construction_works"), g_("factory"), g_("electronics_fab"),
                         g_("agro_module"), g_("biotech_plant"), g_("power_plant"), g_("gas_platform"), g_("charge_refinery"), g_("habitat_yard"), g_("research_station"), TAG))
        g = ["## Инфраструктура 3126 по политиям (world_economy.py)", "", "Количества выведены из выпуска (объектов = выпуск ÷ (мощность × 0.85)); имена — только у крупных, уникальных и стратегических.", "",
             T(["Полития", "Рудники", "Лёд", "Металлургия", "Химия", "Стройматериалы", "Машиностроение", "Электроника", "Агро", "Биотех", "Электростанции 20 ГВт", "Газ. платформы", "Заряд прыжка", "Верфи хабитатов", "Наука", "Статус"], rows, lm)]
        return [("infrastructure_3126", "\n".join(g))]
    if path == "ECONOMY/MINES.md":
        rows = []
        for k, inf in X["infrastructure"]["polities"].items():
            for m in inf["mines"]:
                rows.append((X["economy"]["polities"][k]["name"], com[m["resource"]], n0(m["count"]), mt(m["annual_output_t"]), m["grade"], n0(m["workers"]), m["automation"], n0(m["power_mw"]),
                             n0(m["equipment_stock_t"]), m["equipment_wear_per_year"], n0(m["spare_parts_t_y"]), m["reactor_fuel_t_y"], n0(m["storage_t"]), m["transport"], m["security"],
                             n0(m["mined_volume_m3_y"]), n0(m["excavation_sites"]), TAG))
        g = ["## Добыча 3126 (world_economy.py)", "",
             "Рудник — не бесконечный генератор руды: запас, качество, мощность, люди, автоматизация, оборудование и его износ (запчасти везут буксиры или делают на месте), топливо, склад, транспорт, охрана. "
             "Выработанный объём и места выработок — данные для будущего рендера астероидов (отверстия, выработки, причалы, жильё).", "",
             T(["Полития", "Ресурс", "Объектов", "Выпуск, млн т/год", "Качество", "Работников", "Автоматизация", "МВт", "Оборудование, т", "Износ/год", "Запчасти т/год", "Реакт. топливо т/год", "Склад, т",
                "Транспорт", "Охрана", "Выработка м³/год", "Мест выработки", "Статус"], rows, lm)]
        return [("mines_3126", "\n".join(g))]
    if path == "ECONOMY/SHIPYARDS.md":
        rows = []
        for k, inf in X["infrastructure"]["polities"].items():
            y = inf["shipyards"]
            a = inf["assets"]
            nm = inf["named"].get("civil_shipyard", {}).get("named", []) + inf["named"].get("military_shipyard", {}).get("named", [])
            rows.append((X["economy"]["polities"][k]["name"], ", ".join(x["name"] for x in nm) or "—", a.get("civil_shipyard", {}).get("count", 0), a.get("small_yard", {}).get("count", 0),
                         a.get("military_shipyard", {}).get("count", 0), n0(y["civil_dry_t_y"]), n0(y["military_dry_t_y"]), f"{y['jump_drives_y']:.0f}",
                         D.pname(y["supplied_by"]) if y.get("supplied_by") else "свои", TAG))
        g = ["## Верфи 3126 (world_economy.py)", "", "Потребность = списание (флот / срок) + потери + рост; верфь крупная — 120 тыс. т сухой массы/год, малая — 8 тыс., военная — 40 тыс. "
             "Без своих верфей корабли строит крупнейший торговый партнёр с верфями.", "",
             T(["Полития", "Именованные верфи", "Крупных", "Малых", "Военных", "Гражд. т/год", "Воен. т/год", "Ядер прыжка/год", "Строит", "Статус"], rows, lm)]
        return [("yards_3126", "\n".join(g))]
    if path == "ECONOMY/STRATEGIC_DEPENDENCIES.md":
        rows = []
        for k, e in X["economy"]["polities"].items():
            for v in e["vulnerabilities"]:
                rows.append((e["name"], v["kind"], v["what"], v["severity"], v["consequence"], TAG))
        return [("vulnerabilities_3126", "## Стратегические уязвимости 3126 (world_economy.py)\n\n" + T(["Полития", "Тип", "Что", "Тяжесть", "Последствие", "Статус"], rows, lm))]
    return []
