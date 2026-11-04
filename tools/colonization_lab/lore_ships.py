"""Lore documents of the fleet models 3126 (ship passports and equipment passports).

Source: out/ships_3126/ship_designs_3126.json (ship_designer.py) and canon/ship_designs.json.
Drawings and functional schematics: out/ships_3126/index.html (one page, every model).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
CAT_RU = {
    "drive": "главный двигатель", "reactor": "двигательный реактор", "service_reactor": "служебный реактор", "radiator": "радиаторы",
    "coolant_loop": "контур охлаждения", "jump_core": "ядро прыжка", "tank": "баки", "rcs": "маневровая система", "computer": "MotherComputer",
    "radar": "радар", "comm": "связь", "transponder": "транспондер", "ecm": "РЭБ", "sensor_mast": "сенсорная мачта",
    "life_support": "жизнеобеспечение", "escape_capsule": "спасательная капсула", "lifeboat": "спасательная шлюпка", "medbay": "медотсек",
    "container_rack": "места C20", "passenger_module": "пассажирские модули", "train_module": "модули поезда", "cryo_tank": "криоцистерны",
    "bulk_hold": "грузовой отсек", "docking_port": "стыковочный узел", "boat_bay": "ангар катера", "drone_bay": "отсек дронов",
    "hangar": "ангар перехватчиков", "tow_rig": "буксирная рама", "workshop": "мастерская", "heat_shield": "теплозащита",
    "landing_gear": "посадочные опоры", "weapon": "вооружение", "armour": "броня",
}
REPAIR_RU = {"crew": "экипаж", "station": "станция", "yard": "верфь", "factory": "завод"}


def load():
    p = os.path.join(HERE, "out", "ships_3126", "ship_designs_3126.json")
    if not os.path.exists(p):
        return None
    with open(p, encoding="utf-8") as f:
        ships = json.load(f)["ships"]
    with open(os.path.join(HERE, "canon", "ship_designs.json"), encoding="utf-8") as f:
        canon = json.load(f)
    with open(os.path.join(HERE, "canon", "industry.json"), encoding="utf-8") as f:
        mfr = {m["id"]: m["name"] for m in json.load(f)["manufacturers"]}
    return dict(ships=ships, products={p["id"]: p for p in canon["products"]}, mfr=mfr)


def docs(D, lm):
    S = load()
    if S is None:
        return []
    ships, prods, mfr = S["ships"], S["products"], S["mfr"]
    n = lambda v: f"{v:,.0f}".replace(",", " ")
    out = []

    d = lm.new_doc(D, "ECONOMY/FLEET_MODELS.md", "Модели кораблей 3126", "economy")
    g = ["Источник: `tools/colonization_lab/canon/ship_designs.json` → `ship_designer.py` → `out/ships_3126/` (чертежи и функциональные схемы — "
         "`out/ships_3126/index.html`); рантайм: `assets/data/ships/ship_catalog_3126.json` → общий построитель `GenericShipDescriptorBuilder`. "
         "Одна каноническая модель на каждый из 17 классов (`FLEETS_AND_SHIPYARDS.md`) и варианты только там, где есть конструктивная или лорная причина. "
         "Физика — законы вселенной игры: двигательные площадки 6 МВт/м², излучение ×10, отходы двигателя = вход × (1 − КПД).", "",
         lm.table(["Модель", "Класс", "Производитель (блок)", "Схема", "Сухая, т", "Нагрузка", "Тяга, МН", "g полная / в снаряжении", "Ядро", "Цена, млн Cr"],
                  [(s["name"] + ("" if s["base"] else " — вариант"), s["cls"], f"{s['manufacturer_name']} ({s['bloc']})", f"{s['flight']['law']} {s['flight']['layout']}",
                    n(s["mass"]["dry_t"]), f"{n(s['payload']['cargo_t'])} т" + (f", {n(s['passengers'])} пасс." if s["passengers"] else ""),
                    s["propulsion"]["thrust_mn"], f"{s['propulsion']['accel_g_loaded']} / {s['propulsion']['accel_g_service']}",
                    next((prods[i["product"]]["model"] for i in s["installed"] if i["cat"] == "jump_core"), "—"), f"{s['price']['list_cr'] / 1e6:.1f}")
                   for s in ships]), "",
         "Похожие корабли у разных акторов — это не копии: школа блока меняет решение (C-120 Бхарат — три двигателя ОФУ и ядро Мизан; "
         "Хэ-120 Гармонии — один двигатель Тяньхэ, тяжелее и дешевле, стандартный корпус Пакта). Оборудование идёт только по каналам блоков: "
         "оружие и ядра — внутри блока или по лицензии; через «ограниченный» канал — только гражданское; «чёрный» канал закрыт."]
    for s in ships:
        g += ["", f"### {s['name']} ({s['designation']})", "",
              f"{s['purpose'][0].upper() + s['purpose'][1:]}." + (f" Вариант: {s['variant_reason']}." if not s["base"] else ""), "",
              lm.table(["Параметр", "Значение"], [
                  ("Паспорт", f"`{s['id']}`"), ("Производитель", f"{s['manufacturer_name']} — {s['polity']}, блок {s['bloc']}, школа {s['school']}"),
                  ("Верфь", s["yard"]), ("Корпус", f"{s['hull']['form']}, {' × '.join(str(x) for x in s['hull']['envelope_m'])} м; {s['hull']['structure']}; {s['hull']['material']}"),
                  ("Схема управления", f"{s['flight']['law']} {s['flight']['layout']} (рантайм {s['flight']['runtime_model']})"),
                  ("Экипаж / пассажиры", f"{s['crew']} / {s['passengers']}" + (f" (анабиоз {s['cold_sleep']})" if s["cold_sleep"] else "")),
                  ("Груз", f"{n(s['payload']['cargo_t'])} т, C20 {s['payload']['c20']}"),
                  ("Масса", f"сухая {n(s['mass']['dry_t'])} т (оборудование {n(s['mass']['equipment_t'])} т, силовой корпус {n(s['mass']['structure_t'])} т), "
                            f"в рабочем снаряжении {n(s['mass']['service_t'])} т, полная {n(s['mass']['gross_t'])} т "
                            f"(груз и пассажиры {n(s['mass']['net_payload_t'])} т, тара C20 {s['mass']['container_tare_t']} т)"),
                  ("Тяга", f"{s['propulsion']['thrust_mn']} МН, {s['propulsion']['accel_g_loaded']} g гружёный; длительно {int(s['propulsion']['sustainable_thrust01'] * 100)} %"),
                  ("Энергия", f"{s['power']['generators']} × {prods[s['power']['reactor']]['model']} = {n(s['power']['propulsion_mwe'])} МВт; бытовая нагрузка {s['power']['hotel_mw']} МВт"),
                  ("Охлаждение", f"{n(s['thermal']['radiator_area_m2'])} м² радиаторов = {n(s['thermal']['radiator_mw'])} МВт; контуры {n(s['thermal']['coolant_capacity_mw'])} МВт"),
                  ("Автономность / ресурс", f"{s['endurance_days']} сут / {s['service_life_years']} лет"),
                  ("Цена", f"{s['price']['list_cr'] / 1e6:.2f} млн Cr"),
                  ("Единичные точки отказа", ", ".join(s["schematic"]["single_points_of_failure"]) or "нет"),
              ]), "",
              lm.table(["Позиция", "Изделие", "Кол.", "Производитель", "Ремонт", "Замена, ч"],
                       [(nd["code"], prods[nd["product"]]["name"], nd["qty"], mfr.get(prods[nd["product"]]["mfr"], prods[nd["product"]]["mfr"]),
                         REPAIR_RU[nd["repair_level"]] + (", сменный блок" if nd["line_replaceable"] else ""), nd["replace_hours"] or "—")
                        for nd in s["schematic"]["nodes"]])]
    d.gen("fleet_models", "\n".join(g))
    out.append(d)

    e = lm.new_doc(D, "ECONOMY/SHIP_EQUIPMENT.md", "Паспорта корабельного оборудования 3126", "economy")
    users = {}
    for s in ships:
        for i in s["installed"]:
            users.setdefault(i["product"], []).append(s["designation"])
    rows = []
    for p in sorted(prods.values(), key=lambda x: (x["cat"], x["mfr"], x["id"])):
        if p["id"] not in users:
            continue
        rating = ", ".join(f"{k} {p[k]}" for k in ("thrust_mn", "input_mw", "eff", "mwe", "area_m2", "capacity_mw", "rating_t", "capacity_t", "range_km",
                                                  "persons", "days", "seats", "c20_slots", "payload_t", "tow_t", "power_mw") if k in p)
        rows.append((CAT_RU.get(p["cat"], p["cat"]), p["model"], p["name"], mfr.get(p["mfr"], p["mfr"]), rating, p["mass_t"], n(p["price_cr"]),
                     f"{n(p.get('overhaul_h', 0))} / {n(p.get('life_h', 0))}", ", ".join(sorted(set(users[p["id"]])))))
    e.gen("equipment", "\n".join([
        "Паспорт изделия — ТИП (производитель, модель, номер, характеристики, ресурс). Позиция установки — МЕСТО внутри модели (код `<модель>-A<узел>A<n>`). "
        "Серийный номер — конкретный экземпляр (рантайм, не паспорт). Ресурс — эквивалентные часы до капремонта / до списания.", "",
        lm.table(["Категория", "Модель", "Изделие", "Производитель", "Характеристики", "Масса, т", "Цена, Cr", "Капремонт / ресурс, ч", "Ставится на"], rows)]))
    out.append(e)
    return out
