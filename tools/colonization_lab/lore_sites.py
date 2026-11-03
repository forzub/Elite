"""Spatial civilisation layer of the Notes/Lore reference (world_sites.py outputs).

Called by lore_md.py. Renders GENERATED blocks from out/world_3126/{sites,
society, law, comms, passengers, fleet_age, frontier, world_events}_3126.json:
the World's Ass page, the frontiers and deep-space objects, hubs and their
social state, local administrators, law, beacon communication, passengers,
fleet age, piracy and wreck fields. Text outside the blocks stays hand-written.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
WE_DIR = os.path.join(HERE, "out", "world_3126")
NAMES = ("sites", "society", "law", "comms", "passengers", "fleet_age", "frontier", "world_events")
TAG = "`PROPOSAL` (модель)"


def load():
    if not os.path.isfile(os.path.join(WE_DIR, "sites_3126.json")):
        return None
    out = {}
    for n in NAMES:
        with open(os.path.join(WE_DIR, f"{n}_3126.json"), encoding="utf-8") as f:
            out[n] = json.load(f)
    return out


def n0(x):
    if x is None:
        return "—"
    return f"{int(round(x)):,}".replace(",", " ")


def f2(x):
    return f"{x:,.2f}".replace(",", " ")


ZONE_RU = {"jump_dead": "мёртвая зона прыжка", "jump_distortion": "искажение выхода", "unstable_navigation": "неустойчивая навигация",
           "sensor_disturbance": "помехи сенсоров и радио", "radiation": "радиация", "sargasso": "саргассы (скопление обломков)",
           "perception_anomaly": "аномалия восприятия", "debris": "обломки / лёд", "quarantine": "карантин"}
KNOW_RU = {"official": "официально", "mapped": "нанесена на карты", "partial": "частично", "rumour": "слухи", "classified": "засекречено", "unknown": "неизвестно"}


def docs(D, lm):
    S = D.SX
    if S is None:
        return []
    out = []
    sites = {s["id"]: s for s in S["sites"]["sites"]}
    soc = S["society"]["sites"]
    F = S["frontier"]
    W = F["worlds_ass"]
    acc = F["access"]

    # ---------------- the World's Ass
    path = "FRONTIER/WORLDS_ASS.md"
    d = lm.new_doc(D, path, "«Жопа мира» — 月工门 (HD 102195 и «Лотос»)", "frontier")
    names = [(D.pname(k) if k in D.X["economy"]["polities"] else k, v["name"] + (f" ({v['translit']})" if v.get("translit") else ""), v["lang"], v["meaning"])
             for k, v in sorted(W["official_names"].items())]
    pil = [(v["lang"], v["name"] + (f" ({v['translit']})" if v.get("translit") else ""), v.get("note", "")) for v in W["pilot_names"]]
    zones = [z for z in F["hazard_zones"] if z.get("anchor", "").startswith("system_19.") or z.get("anchor") == "dso.lotus"]
    res = F["rare_resources"][0]
    mk = W["lotosite_market"]
    g = ["Источник: `tools/colonization_lab/world_sites.py` → `out/world_3126/frontier_3126.json`. Максимум риска и награды известного пространства.", "",
         "### Названия", "", "Официальные названия (документы политий):", "", lm.table(["Полития", "Название", "Язык", "Смысл"], names), "",
         "Пилоты, торговцы, пираты и механики между собой:", "", lm.table(["Язык", "Название", "Заметка"], pil), "",
         "### Где это и как туда попасть", "",
         f"Цепочка реальных прыжков: {' → '.join(str(x) for x in acc['frontier.worlds_ass']['chain'])}; плечи {', '.join(str(x) for x in acc['frontier.worlds_ass']['legs_ly'])} св. лет "
         f"(предел прыжка 32). Прямо 83 Leonis → HD 102195 — 39.3 св. лет: невозможно. Из Сола до начала цепочки {acc['frontier.worlds_ass']['from_sol_jumps_to_start']:.0f} прыжка(ов).", "",
         f"- Cobrett туда-обратно от 83 Leonis: {W['cobrett_round_trip_charge_t']} т заряда при баке {W['cobrett_tank_t']} т → нужен бак J20 ({W['j20_tanks_needed']} шт.: −1 место груза, +6 т заряда), заправка на «Лотосе» или танкер.",
         f"- «Лотос» дрейфует (мировая линия): эфемериды старше месяца бесполезны, свежие координаты продаются.",
         f"- Мёртвая зона у «Лотоса»: сигма выхода без маячного захвата {n0(W['dead_zone']['sigma_km_no_lock'])} км → внутрь выходят {100 * W['dead_zone']['p_inside_no_lock']:.1f} % кораблей; "
         f"с захватом {n0(W['dead_zone']['sigma_km_both_ends'])} км → {100 * W['dead_zone']['p_inside_locked']:.1f} %. Маяк «Лотоса» работает {100 * W['dead_zone']['beacon_uptime']:.0f} % времени.", "",
         "### Объекты", "",
         lm.table(["Объект", "Тип", "Где", "Людей", "Оператор", "Право", "Характер", "Статус"],
                  [(s["name"], s["type_ru"], s.get("body") or s["system_name"], n0(s["population"]), s.get("operator") or "—", s.get("legal", "—"),
                    soc[s["id"]]["archetype_ru"], TAG) for s in sites.values() if s["system"] == 19 or s["id"] in ("dso.lotus",)]), "",
         "### Аномальные зоны (стабильная география)", "",
         lm.table(["Зона", "Тип", "Где", "Эффект", "Знание", "Статус"],
                  [(z["name"], ZONE_RU.get(z["kind"], z["kind"]), z.get("anchor"), ", ".join(f"{k}: {v}" for k, v in z["effects"].items()), KNOW_RU.get(z["knowledge"], z["knowledge"]), TAG) for z in zones]), "",
         "Мир не ломается: симуляция знает, что существует; слой восприятия может показывать ложный объект, несуществующий радиообмен, копию своего корабля, неверную дальность. Причина эффектов «Мандалы» неизвестна и не обязана быть объяснена.", "",
         f"### Ресурс: {res['name']}", "",
         f"{res['what']}. В ядре прыжка: ошибка ×{res['use']['jump_error_multiplier']}, предел прыжка +{res['use']['max_hop_bonus_ly']} св. года ({res['strategic_note']}).", "",
         lm.table(["Месторождение", "Где", "Зона", "Сорт, г/т", "Запас, кг", "Добыто, кг", "Сложность", "Кто добывает", "Статус"],
                  [(x["id"], x["anchor"], x["zone"], x["grade_g_per_t"], x["reserve_kg"], x["extracted_kg"], x["difficulty"], x["operator"], TAG) for x in res["deposits"]]), "",
         f"Рынок: равновесие {f2(mk['equilibrium_kg_y'])} кг/год по ~{n0(mk['price_cr_per_kg'])} Cr/кг; чёрный рынок {100 * mk['black_market_share']:.0f} %; запасов на ~{mk['years_to_depletion']:.0f} лет. {mk['monopoly_threat']}. {mk['price_shock_rule']}.", "",
         "### Экономика риска", "",
         lm.table(["Статья экспедиции", "Cr"], [(k, n0(v)) for k, v in W["expedition_cost_cr"].items()] + [("**итого**", n0(W["expedition_cost_total_cr"]))]), "",
         lm.table(["Исход", "Вероятность", "Cr"], [(o["outcome"], o["p"], n0(o["value_cr"])) for o in W["outcomes"]]), "",
         f"Средняя экспедиция: {n0(W['expected_value_cr'])} Cr (убыточна), медианная теряет всё ({n0(W['median_result_cr'])} Cr). Прибыль делают редкие находки. "
         f"~{n0(W['expeditions_per_year'])} экспедиций в год, ~{W['ships_lost_per_year']:.0f} потерянных кораблей в год.", "",
         "Причинная цепочка: " + " → ".join(W["causal_chain"]) + ".", "",
         "Самый ценный товар — информация: " + "; ".join(W["information_market"]) + ". " + W["information_rule"] + ".", "",
         "### Артефакты", "",
         lm.table(["Id", "Что", "Где найден", "Год", "У кого", "Претензии", "Наука", "Цена, Cr", "Известно", "Неизвестно", "Опасность", "Подделка"],
                  [(a["id"], a["name"], a["found_at"], a["found_year"], a["custody"], "; ".join(a["claims"]), a["scientific_value"], n0(a["market_value_cr"]),
                    "; ".join(a["known"]), "; ".join(a["unknown"]) or "—", a["hazard"], {True: "да", False: "нет"}.get(a["fraud"], "спорно")) for a in F["artifacts"]]), "",
         "### Версии истории", ""] + [f"- **{v['source']}:** {v['text']}" for v in W["story"]["versions"]] + ["", W["story"]["rush"]]
    d.gen("worlds_ass", "\n".join(g))
    d.narr("worlds_ass_story", "Как это рассказывать", "слухи, вывески, бары, старатели; официальные новости скучнее слухов.")
    out.append(d)

    # ---------------- frontiers and deep space
    path = "FRONTIER/FRONTIERS_AND_DEEP_SPACE.md"
    d = lm.new_doc(D, path, "Фронтиры, глубокий космос, дорога в карман Керна", "frontier")
    g = ["Источник: `out/world_3126/frontier_3126.json`. Граф маршрутов не знает исключений дальности: только реальные прыжки ≤ 32 св. лет через системы и объекты глубокого космоса.", "",
         "### Цепочки доступа", "",
         lm.table(["Куда", "Цепочка", "Плечи, св. лет", "Макс.", "≤ 32"], [(k, " → ".join(str(x) for x in a["chain"]), ", ".join(str(x) for x in a["legs_ly"]), a["max_leg_ly"], "да" if a["within_max_hop"] else "НЕТ") for k, a in acc.items()]), "",
         "Карман Керна (HD 114783, HD 104067, 83 Leonis) достижим только через депо «Перевал» у Gliese 436 — его поставили в 3034–3036, до маяков кармана.", "",
         "### Объекты глубокого космоса (мировые линии)", "",
         lm.table(["Id", "Имя", "Тип", "Положение, св. лет (ICRS, 3126.0)", "Скорость, км/с", "Сдвиг за год, км", "Источник", "Статус"],
                  [(x["id"], x["name"], x["object_type"], ", ".join(str(v) for v in x["position_ly"]), ", ".join(str(v) for v in x["velocity_km_s"]), n0(x["drift_km_per_year"]),
                    x["source"], x["status"]) for x in F["deep_space_objects"] if not x["hidden"]]), "",
         "### Фронтиры: специализация, риск и награда", "",
         lm.table(["Фронтир", "Специализация", "Макс. выигрыш, Cr", "Риск", "Опасных зон", "Основание"],
                  [(k, next(f["specialisation"] for f in F["frontiers"] if f["id"] == k), n0(v["max_payoff_cr"]), v["risk"], v["hazard_zones"], v["basis"]) for k, v in F["risk_reward"].items()]), "",
         "### Прочие опасные зоны", "",
         lm.table(["Зона", "Тип", "Где", "Знание", "Заметка"], [(z["name"], ZONE_RU.get(z["kind"], z["kind"]), z.get("anchor") or z.get("bound_object_id"), KNOW_RU.get(z["knowledge"]), z["note"])
                                                            for z in F["hazard_zones"] if z not in [y for y in F["hazard_zones"] if y.get("anchor", "").startswith("system_19.") or y.get("anchor") == "dso.lotus"]])]
    d.gen("frontiers", "\n".join(g))
    out.append(d)

    # ---------------- hubs, society, administrators
    path = "ECONOMY/HUBS_AND_SOCIETY.md"
    d = lm.new_doc(D, path, "Хабы, поселения и их общество", "economy")
    by = {}
    for s in sites.values():
        by.setdefault(s["type_ru"], [0, 0])
        by[s["type_ru"]][0] += 1
        by[s["type_ru"]][1] += s["population"]
    arch = {}
    for v in soc.values():
        arch[v["archetype_ru"]] = arch.get(v["archetype_ru"], 0) + 1
    adm = S["society"]["administrators"]
    g = ["Источник: `out/world_3126/sites_3126.json`, `society_3126.json`. Хаб — объект мира, маркер карты — его вид; счётные объекты экономики агрегированы в хабы.", "",
         lm.table(["Тип", "Объектов", "Людей", "Статус"], [(a, b[0], n0(b[1]), TAG) for a, b in sorted(by.items(), key=lambda kv: -kv[1][0])]), "",
         "### Характер мест", "", lm.table(["Архетип", "Объектов"], sorted(arch.items(), key=lambda kv: -kv[1])), "",
         "Каждое место описано 12 величинами (процветание, темп, изоляция, институты, сплочённость, доверие, преступность, коррупция, нервозность, снабжение, медицина, давление угроз) "
         "и реакцией на 12 потрясений (`society_3126.json`).", "",
         "### Руководители удалённых постов", "",
         lm.table(["Пост", "Руководитель", "Происхождение", "Назначен", "Свобода рук", "Дрейф", "Итог"],
                  [(sites[a["site"]]["name"], a["name"], a["background_ru"], a["appointed"], a["effective_freedom"], a["drift"], a["verdict"]) for a in adm]), "",
         "Земля: " + f"{len(S['sites']['earth']) - 1} крупнейших агломераций (имена — решение автора: Земля в игре процедурная) и остальные поселения агрегатом."]
    d.gen("hubs", "\n".join(g))
    out.append(d)

    # ---------------- communication and information
    path = "ECONOMY/COMMUNICATION.md"
    d = lm.new_doc(D, path, "Маяки как сеть связи; распространение новостей", "economy")
    C = S["comms"]
    ev = S["world_events"]
    g = ["Источник: `out/world_3126/comms_3126.json`, `world_events_3126.json`. Маяк — узел навигации, связи, синхронизации и локального покрытия.", "",
         f"Диаметр сети для обычной новости — {C['diameter_days']} сут; до самого дальнего фронтира от Сола — {max(C['frontier_from_sol_days'].values())} сут (AUTHOR_TARGET ≤ {C['author_target_days']}).", "",
         lm.table(["Куда от Сола", "Обычная, сут", "Срочная, сут"], [(e["name"], e["routine_days"], e["flash_days"]) for e in C["examples"]]), "",
         C["frontier_note"], "", "Вне покрытия: " + C["offline_rule"] + ".", "",
         "### Пример: одно событие — разные знания", "",
         f"Правда (`{ev['events'][0]['id']}`): {ev['events'][0]['truth']['ship']} — {ev['events'][0]['truth']['state']}.", "",
         lm.table(["Кто узнал", "Канал", "Источник", "Через, сут", "Уверенность", "Текст", "Язык"],
                  [(k["holder"], k["channel"], k["source"], k["delay_days"], k["confidence"], k["text"], k["language"]) for k in ev["knowledge"]]), "",
         ev["rule"] + "."]
    d.gen("comms", "\n".join(g))
    out.append(d)

    # ---------------- law
    path = "POLITICS/LAW_AND_JURISDICTION.md"
    d = lm.new_doc(D, path, "Право и юрисдикция", "politics")
    L = S["law"]
    fams = sorted(L["offence_matrix"])
    g = ["Источник: `out/world_3126/law_3126.json`. У каждого объекта — суверен, местная юрисдикция, применимое право, полиция, суд, таможня, иммиграция, карантин, салвидж, регистрация, оружие, обыск, экстрадиция, частная охрана.", "",
         "### Космическое право", "", f"- {L['space_law']['beacon_convention']['ru']} ({L['space_law']['beacon_convention']['since']}): " + "; ".join(L["space_law"]["beacon_convention"]["rules"]) + ".",
         f"- Внутри системы: {L['space_law']['territorial']}.", f"- Вне систем: {L['space_law']['deep_space']}.", f"- Ордера: {L['space_law']['warrants']}.", "",
         "### Что бывает за нарушение (семья права × нарушение)", "",
         lm.table(["Нарушение"] + fams, [(o,) + tuple(L["offence_matrix"][f][o] for f in fams) for o in L["space_law"]["offences"]]), "",
         "### Спорные объекты", "",
         lm.table(["Объект", "Юрисдикция", "Претензии"], [(sites[r["site"]]["name"], r["local_jurisdiction"], "; ".join(r["competing_claims"])) for r in L["sites"] if r["competing_claims"]]), "",
         "Правовой статус корабля — поля `ShipInstance`: " + ", ".join(L["ship_legal_status_fields"]) + "."]
    d.gen("law", "\n".join(g))
    out.append(d)

    # ---------------- passengers, fleet age, piracy, wrecks
    path = "ECONOMY/PASSENGERS_AND_FLEET_AGE.md"
    d = lm.new_doc(D, path, "Пассажиры, возраст флота, пираты и кладбища кораблей", "economy")
    P = S["passengers"]
    A = S["fleet_age"]
    g = ["Источник: `passengers_3126.json`, `fleet_age_3126.json`, `frontier_3126.json`.", "",
         f"Межзвёздных поездок {n0(P['interstellar_trips_y'])}/год, переселенцев {n0(P['migrants_y'])} (визовое окно Земли {n0(P['earth_visa_window_y'])}), беженцев {n0(P['refugees_y'])}, "
         f"поверхность ↔ орбита {n0(P['surface_orbit_trips_y'])}. Провозная способность переселенческого флота {n0(P['migration_capacity_y'])}/год.", "",
         lm.table(["Класс", "Кораблей", "Резерв"], [(c, f2(v), f2(P["reserve"][c])) for c, v in P["fleet"].items()]), "",
         lm.table(["Связь", "Пассажиров/год", "Переселенцев/год", "Корабль", "Кораблей", "Интервал, сут", "Эконом, Cr", "Бизнес, Cr", "Каюта, Cr"],
                  [(" — ".join(D.pname(x) for x in r["link"]), n0(r["pax_y"]), n0(r["migrants_y"]), r["ship_class"], f2(r["ships"]), r["headway_days"],
                    n0(r["fares_cr"]["economy"]), n0(r["fares_cr"]["business"]), n0(r["fares_cr"]["cabin"])) for r in P["od_top"][:12]]), "",
         f"Эвакуация 1.2 млн с кольца Нового Шанхая: ~{P['evacuation_new_shanghai']['days']} сут ({P['evacuation_new_shanghai']['note']}).", "",
         "### Возраст флота", "", A["rule"] + ".", "",
         lm.table(["Состояние", "Кораблей"], [(k, n0(v)) for k, v in A["world_states"].items()]), "",
         f"Богатые продают бедным ~{n0(A['used_ships_sold_rich_to_poor_y'])} списанных кораблей в год. Пиратский флот: " + ", ".join(f"{k} {100 * v:.0f} %" for k, v in A["pirate_fleet"]["composition"].items()) + ".", "",
         lm.table(["Корабль", "new", "mature", "old", "heavily worn", "на слом"], [(c,) + tuple(n0(v[x]) for x in ("new", "mature", "old", "heavily_worn", "scrap_candidate")) for c, v in A["secondary_market_cr"].items()]), "",
         "### Пиратство", "", "Индекс = трафик × ценный груз × слабый надзор × укрытия × убежище × коррупция × рынок сбыта (геометрическое среднее).", "",
         lm.table(["Система", "Индекс", "Трафик", "Груз", "Слабый надзор", "Укрытия", "Убежище", "Коррупция", "Сбыт"],
                  [(r["name"], r["index"], r["traffic"], r["valuable_cargo"], r["weak_enforcement"], r["cover"], r["safe_haven"], r["corruption"], r["resale_market"])
                   for r in sorted(F["piracy_by_system"], key=lambda r: -r["index"])[:15]]), "",
         "### Кладбища кораблей", "",
         lm.table(["Поле", "Тип", "Где", "Обломков", "Владение", "Салвидж"], [(w["id"], w["kind"], w["near"], n0(w["wrecks"]), w["ownership"], w["salvage"]) for w in F["wrecks"]["fields"]])]
    d.gen("pax_age", "\n".join(g))
    out.append(d)
    return out
