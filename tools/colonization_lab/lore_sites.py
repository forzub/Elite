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
NAMES = ("sites", "society", "law", "comms", "passengers", "fleet_age", "frontier", "world_events", "industry", "rescue")
TAG = "`PROPOSAL` (модель)"


def load():
    if not os.path.isfile(os.path.join(WE_DIR, "sites_3126.json")):
        return None
    out = {}
    for n in NAMES:
        p = os.path.join(WE_DIR, f"{n}_3126.json")
        if os.path.isfile(p):
            with open(p, encoding="utf-8") as f:
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

    # ---------------- the World's Ass (public layer: what the world can know)
    path = "FRONTIER/WORLDS_ASS.md"
    d = lm.new_doc(D, path, "«Жопа мира» — 月工门 (поле «Лотоса», HD 104067, HD 102195)", "frontier")
    LM = W["lotus"]
    T_ = F["developer_truth"]
    names = [(D.pname(k) if k in D.X["economy"]["polities"] else k, v["name"] + (f" ({v['translit']})" if v.get("translit") else ""), v["lang"], v["meaning"])
             for k, v in sorted(W["official_names"].items())]
    pil = [(v["lang"], v["name"] + (f" ({v['translit']})" if v.get("translit") else ""), v.get("note", "")) for v in W["pilot_names"]]
    zones = [z for z in F["hazard_zones"] if z.get("anchor", "").startswith("system_19.") or z.get("anchor") == "dso.lotus"]
    ap = LM["approach"]
    zn = W["zones"]
    g = ["Источник: `tools/colonization_lab/world_sites.py` + `lotus_model.py` → `out/world_3126/frontier_3126.json`. Публичный слой: что мир может знать. "
         "Правда разработчика — `AUTHOR/WORLDS_ASS_TRUTH.md`.", "",
         "### Названия", "", "Официальные названия (документы политий):", "", lm.table(["Полития", "Название", "Язык", "Смысл"], names), "",
         "Пилоты, торговцы, пираты и механики между собой:", "", lm.table(["Язык", "Название", "Заметка"], pil), "",
         "### Три зоны", "",
         lm.table(["Зона", "Объекты", "Аудит", "Изоляция", "Что там"], [(k, ", ".join(v["sites"]), v["audit"], v["isolation"], v["what"]) for k, v in zn.items()]), "",
         "Государство здесь есть — флот, полиция, таможня, санитария, наука. Проблема в другом: прибыль так велика, что разлагает институты; чем ближе к полю, тем дороже честность.", "",
         "### Дорога", "",
         f"Цепочка реальных прыжков: {' → '.join(str(x) for x in acc['frontier.worlds_ass']['chain'])}; плечи {', '.join(str(x) for x in acc['frontier.worlds_ass']['legs_ly'])} св. лет (предел 32).", "",
         f"- Прыжок HD 104067 → «Дальний Причал» ({ap['jump_to_berth']['ly']} св. лет): {ap['jump_to_berth']['note']}.",
         f"- Коридор через поле: {ap['corridor']['length_au']} а.е. на тяге, {ap['corridor']['one_way_days']} сут в одну сторону, в поле {ap['corridor']['exposure_days']} сут. {ap['corridor']['note']}; дрейф {ap['corridor']['drift_au_per_year']} а.е./год.",
         f"- {ap['shortcut_jump_into_field']['note']}.",
         f"- Короткий визит: риск стадии 1 синдрома (сон, повторяющиеся сны) ~{100 * ap['syndrome_exposure']['stage1_risk']:.0f} % на человека; вне поля обратимо.",
         f"- Cobrett туда-обратно от HD 104067: {W['cobrett_round_trip_charge_t']} т заряда при баке {W['cobrett_tank_t']} т → бак J20 ({W['j20_tanks_needed']} шт.).", "",
         lm.table(["Состояние карты коридора", "Возраст, сут", "Риск потери корабля"], [(k, "–".join(str(x) for x in v.get("age_days", ["—"])), v["p_loss"]) for k, v in ap["corridor"]["states"].items()]), "",
         "### Объекты", "",
         lm.table(["Объект", "Тип", "Где", "Людей", "Оператор", "Право", "Характер", "Статус"],
                  [(s_["name"], s_["type_ru"], s_.get("body") or s_["system_name"], n0(s_["population"]), s_.get("operator") or "—", s_.get("legal", "—"),
                    soc[s_["id"]]["archetype_ru"], TAG) for s_ in sites.values() if (s_["system"] in (19, 41) and s_["id"].startswith("site.0") and (s_["id"].startswith("site.019") or s_["id"].startswith("site.041.v") or s_["id"].startswith("site.041.h")))
                    or s_["id"] in ("dso.lotus", "dso.far_berth", "dso.quarantine_picket")]), "",
         "### Аномальные зоны (стабильная география)", "",
         lm.table(["Зона", "Тип", "Где", "Эффект", "Знание", "Статус"],
                  [(z["name"], ZONE_RU.get(z["kind"], z["kind"]), z.get("anchor"), ", ".join(f"{k}: {v}" for k, v in z["effects"].items()), KNOW_RU.get(z["knowledge"], z["knowledge"]), TAG) for z in zones]), "",
         "### Лотосит (что знает мир)", "",
         "Официально (кордон): стратегический материал и биологическая опасность; «минерал у планеты c» — легенда Когниума. Freeband: «его выносят из мертвецов «Лотоса»». "
         "Лотосит — не топливо и не заряд прыжка, обычную дальность он не увеличивает. Его техническое применение — фазовый стабилизатор: "
         "вставка в эмиттер ядра удерживает поле прыжка в мёртвых зонах, зонах искажения и в поле «Лотоса», где обычное ядро срывается. "
         f"Вставка малого корабля — {LM['stabilizer']['g_per_unit']['small']} г, ~{n0(LM['stabilizer']['small_insert_price_cr'])} Cr; в нормальном пространстве пользы нет. "
         f"{'; '.join(LM['stabilizer']['politics'][1:3])}. "
         f"Грамм стоит ~{n0(LM['price_cr_per_g'])} Cr — примерно {LM['price_vs_cobrett']:.1f} нового Cobrett-I; тело с извлекаемым лотоситом — ~{n0(LM['raids']['body_value_retail_cr'])} Cr в рознице. "
         f"В медицине это «эликсир молодости»: курс {LM['medical']['course_g']} г ≈ {n0(LM['medical']['course_price_cr'])} Cr; на рынке {100 * LM['medical']['useless']:.0f} % препаратов бесполезны, {100 * LM['medical']['dangerous']:.0f} % опасны, {100 * LM['medical']['fraud']:.0f} % — подделки.", "",
         "Лотосовая лихорадка («красная», «скарлатина старателей»): жар, яркая красная сыпь, слабость, сосудистые нарушения, дезориентация; в тяжёлых случаях — неврология. Причину медицина мира спорит. Отсюда санитарный контроль, герметичные контейнеры, карантинный статус тел и обломков, досмотр, протокол L страховщиков; контрабанда заражённого материала — особо тяжкое преступление.", "",
         lm.table(["Вспышка", "Где", "Год", "Людей", "Заболело", "Погибло", "Причина", "Последствия"],
                  [(o["id"], o["site"], o["year"], n0(o["population"]), n0(o["cases"]), n0(o["deaths"]), o["cause_chain"], o["consequence"]) for o in LM["outbreaks"]]), "",
         "### Экономика 3126 (модель, не канон)", "",
         f"Первичный запас конечен: {n0(LM['stock']['bodies'])} тел, извлекаемо всего {f2(LM['stock']['recoverable_g_total'])} г, осталось {f2(LM['stock']['recoverable_g_left'])} г (верхняя оценка). "
         f"Цена ~{n0(LM['price_cr_per_g'])} Cr/г (спрос: медицина {f2(LM['demand_at_price']['medical_g_y'])} г/год от ~{n0(LM['demand_at_price']['medical_buyers'])} покупателей, "
         f"наука {f2(LM['demand_at_price']['research_g_y'])}, стабилизаторы {f2(LM['demand_at_price']['stabiliser_g_y'])}); в год — {n0(LM['supply']['raids_y'])} рейдов, {f2(LM['supply']['bodies_stolen_y'])} вынесенных тел + {LM['supply']['legal_bodies_y']} законно, "
         f"{f2(LM['supply']['refined_g_y'])} г лотосита, {f2(LM['raids']['ships_lost_y'])} потерянных кораблей; первичный запас иссякнет через ~{LM['supply']['years_until_primary_exhausted']:.0f} лет.", "",
         lm.table(["Сегмент", "Рейдов/год", "Карта", "Риск потери", "Риск ареста", "Тел за рейс", "Цена тела, Cr", "Итог рейса, Cr"],
                  [(r["ru"], f2(r["runs_y"]), r.get("corridor") or "—", r["p_ship_lost"], r["p_caught"], r["bodies"], n0(r["price_per_body"]), n0(r["ev_cr"])) for r in LM["raids"]["segments"]]), "",
         f"Средний рейд: {n0(LM['raids']['average_ev_cr'])} Cr — убыточен; прибыль у немногих «со связями». Купить у внешнего периметра — ~{n0(LM['buying_vs_self']['outer_perimeter_buy_cr_per_g'])} Cr/г; "
         f"рейс «со связями» обходится в ~{n0(LM['buying_vs_self']['self_sourcing_connected_cr_per_g'])} Cr/г, с купленной картой — ~{n0(LM['buying_vs_self']['self_sourcing_informed_cr_per_g'])} Cr/г. "
         "Перекупщик у поля платит ~0.4 % конечной цены, лаборатория в сером фронтире — ~3 %, периметр — ~25 %: вынос держат сети, одиночки гибнут или продают за бесценок.", "",
         "Рынки: легальный — " + "; ".join(T_["markets"]["legal"]) + ". Серый — " + "; ".join(T_["markets"]["grey"]) + ". Чёрный — " + "; ".join(T_["markets"]["black"]) + ".", "",
         f"Блокада нелинейна: контрабанды меньше всего при строгости {LM['blockade']['least_smuggling_at']}, нынешний кордон — {LM['blockade']['actual']} ({LM['blockade']['note']}).", "",
         lm.table(["Строгость", "Премия чёрного рынка", "Попыток/год", "Утечка (подкуп)", "Прошло/год"], [(r["strictness"], r["black_premium"], r["attempts_y"], r["leakage"], r["smuggled_y"]) for r in LM["blockade"]["rows"]]), "",
         "Давление коррупции (" + LM["corruption"]["formula"] + "), ожидаемая доля продажных по ролям:", "",
         lm.table(["Роль"] + list(LM["corruption"]["by_zone"]), [(r["role"],) + tuple(LM["corruption"]["by_zone"][z][i]["expected_corrupt_share"] for z in LM["corruption"]["by_zone"])
                                                               for i, r in enumerate(next(iter(LM["corruption"]["by_zone"].values())))]), "",
         "### Кто носит форму", "",
         lm.table(["Подразделение", "Чьё", "База", "Мандат", "Командир", "Дисциплина", "Как себя ведёт (видно миру)"],
                  [(u["name"], D.pname(u["owner"]) if u["owner"] in D.X["economy"]["polities"] else u["owner"], u["base"], u["mandate"], u["commander"], u["discipline"], u["behaviour"]) for u in LM["security_units"]]), "",
         "Принадлежность к государственному флоту не доказывает законности: две эскадры одного государства ведут себя по-разному.", "",
         "Самый ценный товар — информация: " + "; ".join(W["information_market"]) + ". " + W["information_rule"] + ".", "",
         "### Артефакты", "",
         lm.table(["Id", "Что", "Где найден", "Год", "У кого", "Претензии", "Наука", "Цена, Cr", "Известно", "Неизвестно", "Опасность", "Подделка"],
                  [(a["id"], a["name"], a["found_at"], a["found_year"], a["custody"], "; ".join(a["claims"]), a["scientific_value"], n0(a["market_value_cr"]),
                    "; ".join(a["known"]), "; ".join(a["unknown"]) or "—", a["hazard"], {True: "да", False: "нет"}.get(a["fraud"], "спорно")) for a in F["artifacts"]]), "",
         "### Версии истории (в мире)", ""] + [f"- **{v['source']}:** {v['text']}" for v in W["story"]["versions"]] + ["", W["story"]["rush"]]
    d.gen("worlds_ass", "\n".join(g))
    d.narr("worlds_ass_story", "Как это рассказывать", "слухи, вывески, бары, старатели; официальные новости скучнее слухов.")
    out.append(d)

    # ---------------- Developer World Truth
    path = "AUTHOR/WORLDS_ASS_TRUTH.md"
    d = lm.new_doc(D, path, "Правда разработчика: «Лотос» = NIRVANA-1, лотосит, синдром поля", "author")
    t = T_["truth"]
    b_ = T_["biology"]
    st = LM["stock"]
    g = ["`AUTHOR_TRUTH` — Developer World Truth. Мир знает это частично или не знает вовсе; публичный слой — `FRONTIER/WORLDS_ASS.md`. Источник: `canon/world_sites.json → lotus`.", "",
         f"**Идентичность:** «Лотос» — **{t['identity']['ship']}** ({t['identity']['status']}, {t['identity']['basis']}).", "",
         "### Сверка с каноном", ""] + [f"- {x}" for x in t["consistency"]] + ["",
         "### Маршрут, авария, траектория", "", f"- Маршрут: {t['route']}.", f"- Авария: {t['accident']['year']} — {t['accident']['where']}. Причина: {t['accident']['cause']} (категория: {t['accident']['category']}).", "",
         lm.table(["С", "По", "Что"], [(x["from"], x["to"], x["what"]) for x in t["trajectory"]]), "",
         "### Экипаж", "", f"На борту при аварии {t['crew']['aboard_at_accident']}, родилось после {t['crew']['born_after']}. {t['crew']['syndrome_onset']}. Последний человек умер в {t['crew']['last_human']} ({t['crew']['survival_years']} лет в поле). "
         f"Умершие до аварии: {t['crew']['deaths_before_accident_remains']}.", "",
         lm.table(["Умерли", "Людей", "Причина", "Лет живыми в поле"], [("–".join(str(x) for x in c["died"]), c["people"], c["cause"], c["alive_in_field_years"]) for c in t["crew"]["cohorts"]]), "",
         f"Корабль: {t['ship_state']}.", "",
         lm.table(["Система", "Работала до", "Заметка"], [(x["system"], x["until"], x["note"]) for x in t["systems"]]), "",
         "### Сообщения", "", lm.table(["Отправлено", "Текст", "Откуда, св. лет", "Кто принял", "Дошло"], [(str(m["sent"]), m["text"], m.get("from_ly", "—"), m["received"], m["arrived"]) for m in t["messages"]]), "",
         "### Животные", "", f"На борту в 2680: " + ", ".join(f"{k} {v}" for k, v in t["animals"]["aboard_2680"].items()) + f". {t['animals']['fate']}. Находка: {t['animals']['found']}.", "",
         "### Находка и утечка", "",
         f"- Почему возобновили: {t['discovery']['why_resumed']}.", f"- Экспедиция: {t['discovery']['expedition']}; первой вошла {t['discovery']['first_aboard']['who']} ({t['discovery']['first_aboard']['when']}).",
         f"- Нашли: {'; '.join(t['discovery']['found'])}.", f"- Лотосит впервые: {t['discovery']['lotusite_first_seen']['when']}, {t['discovery']['lotusite_first_seen']['who']} — {t['discovery']['lotusite_first_seen']['what']}.",
         f"- Ценность понял: {t['discovery']['value_understood']['when']}, {t['discovery']['value_understood']['who']} — {t['discovery']['value_understood']['what']}.",
         f"- Сокрытие: {t['discovery']['cover_up']}.", f"- Утечка: {t['discovery']['leak']}.", "",
         "### Биология (вымышленная аномальная биохимия)", "", b_["rule"] + ".", "",
         lm.table(["Вид", "Уриказа", "Синтез витамина C", "Neu5Gc", "Поражается"], [(x["species"], x["uricase"], x["vitamin_c_synthesis"], x["neu5gc"], x["affected"]) for x in b_["species"]]), "",
         "Распределение лотосита по тканям: " + ", ".join(f"{k} {100 * v:.0f} %" for k, v in b_["tissue_distribution"].items()) + f". Лабораторно: {b_['lab_identification']}.", "",
         lm.table(["Стадия", "Месяцы", "Признаки"], [(x["stage"], "–".join(str(m) for m in x["months"]), "; ".join(x["signs"])) for x in b_["syndrome_stages"]]), "",
         b_["outside_field"] + ".", "",
         "### Явления поля: что реально", "", "Разработчик всегда знает категорию: физика / ошибка датчика / субъективное. Отладочная телеметрия обязана показывать истину (датчик A, сеть B, экипаж C, правда D).", "",
         lm.table(["Явление", "Категория", "Где", "Датчики", "Экипаж", "Правда"], [(x["id"], x["category"], x["where"], x["sensors"], x["crew"], x["truth"]) for x in T_["phenomena"]]), "",
         "### Материал и запас", "", T_["material"]["origin"] + ".", "",
         lm.table(["Когорта", "Людей", "Экспозиция, лет", "Преобразовано", "г на тело", "Потенциал, г"], [(c["cohort"], c["people"], c["exposure_years"], c["transformed_share"], c["g_per_body"], c["potential_g"]) for c in st["cohorts"]]), "",
         f"Потенциал {f2(st['potential_g'])} г; извлекаемо {f2(st['recoverable_g_total'])} г; вывезено {st['removed_bodies']} тел ({T_['material']['removed_note']}); осталось {f2(st['recoverable_g_left'])} г. {st['note']}.", "",
         "### Фазовый стабилизатор", "", f"{T_['phase_stabilizer']['decision']}.", "",
         f"{T_['phase_stabilizer']['name']}: {T_['phase_stabilizer']['function']}. Граммов на вставку: " + ", ".join(f"{k} {v}" for k, v in T_["phase_stabilizer"]["g_per_unit"].items()) + ". "
         f"{T_['phase_stabilizer']['consumption']}. Деградация — только при тяжёлой аномальной экспозиции ({T_['phase_stabilizer']['degradation_per_heavy_exposure_day']} в сутки), перекалибровка раз в {T_['phase_stabilizer']['recalibration_every_exposure_days']} суток экспозиции; "
         f"{T_['phase_stabilizer']['restoration']}. Совместимость: {T_['phase_stabilizer']['compatible']}. Не абсолютна: {T_['phase_stabilizer']['not_absolute']}.", "",
         lm.table(["Где", "Без стабилизатора", "Со стабилизатором"],
                  [("мёртвая зона", T_["phase_stabilizer"]["effects"]["jump_dead"]["without"], T_["phase_stabilizer"]["effects"]["jump_dead"]["with"]),
                   ("зона искажения", f"ошибка выхода ×{T_['phase_stabilizer']['effects']['jump_distortion']['exit_error_multiplier_without']}", f"×{T_['phase_stabilizer']['effects']['jump_distortion']['exit_error_multiplier_with']}"),
                   ("неустойчивая навигация", f"срыв/мисджамп {T_['phase_stabilizer']['effects']['unstable_navigation']['abort_or_misjump_per_jump_without']} за прыжок",
                    f"{T_['phase_stabilizer']['effects']['unstable_navigation']['abort_or_misjump_per_jump_with']}"),
                   ("аварийный выход", "—", T_["phase_stabilizer"]["effects"]["emergency_exit"]["note"]),
                   ("нормальное пространство", "—", T_["phase_stabilizer"]["effects"]["normal_space"]["note"])]), "",
         "Кто реально ставит стабилизаторы (спрос выведен из флота, а не из +3 св. года):", "",
         lm.table(["Пользователь", "Кораблей", "г на корабль", "Суток в аномалии/год", "Сбережённые потери, Cr/год", "Цена владения, Cr/год", "Ставит"],
                  [(r["who"], f2(r["ships"]), r["g_per_ship"], r["exposure_days_y"], n0(r["avoided_loss_cr_y"]), n0(r["cost_cr_y"]), "да" if r["fitted"] else "нет") for r in LM["stabilizer"]["fleet"]]), "",
         "Почему не массово: " + "; ".join(T_["phase_stabilizer"]["why_not_mass"]) + ". " + LM["stabilizer"]["politics"][0] + ".", "",
         "### Медицина", "", T_["medical"]["effect"] + ". " + T_["medical"]["unknowns"] + ".", "",
         "### Лотосовая лихорадка — истинный механизм (игровая модель)", "", T_["fever"]["truth_mechanism"] + ".", "",
         "### Торговля людьми", "", f"Формы: {', '.join(T_['trafficking']['forms'])}. Сейчас ~{LM['trafficking']['victims_y']:.0f} жертв в год (маржа ×{LM['trafficking']['margin']:.2f}, ёмкость «Сада» {LM['trafficking']['capacity_y']:.0f}/год): пока есть мёртвые NIRVANA-1, живое сырьё не окупается — «Сад» держится на долгах и культах. "
         f"Когда тела кончатся, цена поднимется до ~{n0(LM['after_primary_exhausted']['price_cr_per_g'])} Cr/г и торговля людьми станет выгодной (~{LM['after_primary_exhausted']['victims_y']:.0f} жертв в год). "
         f"После исчерпания тел: {LM['after_primary_exhausted']['note']}. {T_['reproduction']['note']}.", "",
         "### Подразделения: что на самом деле", "",
         lm.table(["Подразделение", "Категория", "Коррупция", "Аудит", "Лояльность", "Союзы", "Неофициальный доход, Cr/год"],
                  [(u["name"], u["category"], u["corruption"], u["audit_risk"], u["loyalty"], ", ".join(u["alliances"]) or "—", n0(u["unofficial_income_cr_y"])) for u in LM["security_units"]])]
    d.gen("worlds_ass_truth", "\n".join(g))
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
    out += industry_docs(D, lm, S, sites)
    return out


LVL_RU = {"concern": "концерн", "assembly": "сборка", "parts": "детали", "import": "покупка"}
CH_RU = {"open": "открыто (свой блок)", "licensed": "по лицензии", "restricted": "ограничено", "black": "чёрный рынок / реэкспорт", "own": "своё", "none": "нет"}


def industry_docs(D, lm, S, sites):
    out = []
    if "industry" not in S:
        return out
    IN = S["industry"]
    pn = lambda k: D.pname(k) if k and k in D.X["economy"]["polities"] else (k or "—")
    path = "ECONOMY/INDUSTRY.md"
    d = lm.new_doc(D, path, "Промышленность: кто строит корабли, оборудование и модули", "economy")
    g = ["Источник: `tools/colonization_lab/industry.py` (`canon/industry.json` + экономика + сайты) → `out/world_3126/industry_3126.json`.", "",
         "### Блоки-союзы", "",
         lm.table(["Блок", "Аналог", "Члены", "Почему вместе", "Школа кораблестроения", "Ядра от", "Производителей корпусов", "Доля главного"],
                  [(v["ru"], v["analogue"], ", ".join(pn(m) for m in v["members"]), v["why"], IN["designs"][v["design"]]["ru"], pn(v["core_source"]),
                    IN["fleet_uniformity"][b]["hull_makers"], IN["fleet_uniformity"][b]["main_maker_share"]) for b, v in IN["blocs"].items()]), "",
         "Союзники летают на однотипных кораблях немногих производителей (Небесный пакт); Лига Лаланд покупает у многих — её флот пёстрый.", "",
         "### Школы", "", lm.table(["Школа", "Что это", "Цена ×", "Ресурс ×"], [(v["ru"], v["what"], v["price_x"], v["life_x"]) for v in IN["designs"].values()]), "",
         "### Каналы между блоками", "", lm.table(["Покупатель → продавец", "Канал"], [(k.replace("|", " → "), CH_RU[v]) for k, v in sorted(IN["channels"].items()) if k.split("|")[0] != k.split("|")[1]]), "",
         "### Кто что умеет (корабли)", "",
         lm.table(["Полития", "Роль", "Корпус", "Ядро", "Реактор", "Двигатель", "Авионика", "Жизнеобесп.", "Оружие"],
                  [(pn(k), c["ship_role"]) + tuple(LVL_RU[c["blocks"][b]["level"]] for b in ("hull_structure", "jump_core", "reactor", "main_drive", "avionics", "life_support", "weapons")) for k, c in IN["capability"].items()]), "",
         "### Горное, газовое оборудование и станционные модули", "",
         lm.table(["Полития", "Бур. головки", "Обогатители", "Масс-драйверы", "Газ. платформы", "Сепараторы He-3", "Секции", "Кольца", "Доки", "Агро/ЖО", "Каюты"],
                  [(pn(k),) + tuple(LVL_RU[c["blocks"][b]["level"]] for b in ("drill_heads", "ore_processors", "mass_drivers", "scoop_platforms", "he3_separators", "pressure_segments", "rotating_rings", "dock_modules", "agro_life_modules", "habitat_cabins")) for k, c in IN["capability"].items()]), "",
         "### Концерны", "",
         lm.table(["Производитель", "Полития", "Тип", "Узлы", "Заметка"], [(m["name"], pn(m["polity"]), m["kind"], ", ".join(m["groups"]), m.get("note", "")) for m in IN["manufacturers"]]), "",
         "### Откуда берут (корпус и ядро)", "",
         lm.table(["Полития", "Корпус: источник", "Канал", "Ядро: источник", "Канал"],
                  [(pn(k), " / ".join(f"{pn(x['source'])} {x['share']}" for x in r["hull_structure"]["sources"]) if r["hull_structure"].get("sources") else pn(r["hull_structure"]["source"]),
                    CH_RU[r["hull_structure"]["channel"]], pn(r["jump_core"]["source"]), CH_RU[r["jump_core"]["channel"]]) for k, r in IN["supply"].items()]), "",
         "### Где производят, собирают и продают", "",
         lm.table(["Место", "Производят", "Собирают", "Продают (импорт откуда)"],
                  [(sites[sid]["name"] if sid in sites else sid, "; ".join(f"{x['maker']}: {', '.join(x['blocks'])}" for x in r["produced"]) or "—",
                    "; ".join(", ".join(x["blocks"]) for x in r["assembled"]) or "—",
                    "; ".join(", ".join(f"{pn(y['source'])} ({CH_RU[y['channel']]})" for y in x["imports_from"]) for x in r["sold"]) or "—") for sid, r in sorted(IN["by_site"].items())]), "",
         "### Из чего состоит (крупные узлы и кто их делает)", ""]
    for prod, rows in IN["boms"].items():
        g += [f"**{prod}:** " + "; ".join(f"{r['block']} {100 * r['share']:.0f} % ({', '.join(m for ms in r['makers'].values() for m in ms[:2]) or '—'})" for r in rows), ""]
    g += ["### Проверки", ""] + [f"- {'OK' if c['ok'] else 'FAIL'} `{c['check']}` — {c['detail']}" for c in IN["checks"]]
    d.gen("industry", "\n".join(g))
    out.append(d)
    if "rescue" not in S:
        return out
    R = S["rescue"]
    path = "ECONOMY/RESCUE_SERVICES.md"
    d = lm.new_doc(D, path, "Спасательные службы: поисковики, пожарные, медэвакуация, буксиры", "economy")
    resp = [r for r in R["sites"] if r["response_h"] is not None]
    worst = sorted(resp, key=lambda r: -r["response_h"])[:12]
    g = ["Источник: `out/world_3126/rescue_3126.json`. " + R["note"] + ".", "",
         "Службы: " + "; ".join(f"{k} — {v}" for k, v in R["services"].items()) + ".", "",
         lm.table(["Полития", "Организация", "Пожарные и аварийные (чел.)", "SAR-катера", "Спасательные буксиры", "Поисковые корабли", "Госпитальные суда", "Химзащита/лотос", "Доля частного спасателя"],
                  [(pn(k), v["organisation"], n0(v["fire_damage_personnel"]), f2(v["sar_cutters"]), f2(v["rescue_tugs"]), f2(v["search_ships"]), f2(v["hospital_ships"]), v["hazmat_teams"], v["private_salvage_share"]) for k, v in R["polities"].items()]), "",
         "### Дольше всего ждать помощи", "",
         lm.table(["Место", "Кто приедет", "Через, ч"], [(sites[r["site"]]["name"] if r["site"] in sites else r["site"], r["responder"], r["response_h"]) for r in worst]), "",
         "Пиратские анклавы спасают себя сами; в поле «Лотоса» — только Kern SAR «Ласка» с «Дальнего Причала»."]
    d.gen("rescue", "\n".join(g))
    out.append(d)
    return out

