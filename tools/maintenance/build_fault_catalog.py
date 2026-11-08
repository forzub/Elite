#!/usr/bin/env python3
"""Fault catalog 3126 (docs/MAINTENANCE_LAYER_DESIGN.md): failure modes per EQUIPMENT class, linked into a
cause graph through the ship's buses; a ship's fault graph is assembled from what is installed on it.

Author 2026-11-09: "каталог по оборудованию, поскольку оно установлено во многих местах, а потом по
совокупности оборудования - для корабля". The main game keeps percentages (`effect` per stage); the Tin Can
mini-game uses the chains (`upstream` / `downstream` / `masks`): BITE shows the first out-of-tolerance signal,
a deeper cause burns the replaced unit again.

Writes src/assets/data/maintenance/fault_catalog.json (+ a per-ship graph for the reference:
Notes/html/ship_reference/fault_data.js). First slice: every equipment class installed on Cobrett-I N1 / A3.

usage: python tools/maintenance/build_fault_catalog.py [--check]
"""
import argparse
import json
import os
import sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
OUT = os.path.join(ROOT, "src", "assets", "data", "maintenance", "fault_catalog.json")
REF = os.path.join(ROOT, "Notes", "html", "ship_reference", "fault_data.js")
PASSPORTS = os.path.join(ROOT, "tools", "colonization_lab", "canon", "ship_passports_3126.json")

# ---------------------------------------------------------------- vocabulary
SIGNALS = {
    "temp": "температура узла", "temp_pad": "температура охлаждающего пятака", "coolant_flow": "расход теплоносителя",
    "coolant_press": "давление теплоносителя", "coolant_temp": "температура теплоносителя на выходе", "current": "ток потребления",
    "bus_voltage": "напряжение шины", "thrust": "тяга", "thrust_axis": "отклонение вектора тяги", "vibration": "вибрация",
    "chamber_press": "давление в камере", "plasma_stab": "стабильность плазмы", "output_mw": "выходная мощность",
    "fuel_feed": "подача топлива", "tank_level": "уровень в баке", "tank_press": "давление в баке", "valve_pos": "положение клапана",
    "data_err": "ошибки данных", "watchdog": "сторожевой таймер", "rx_level": "уровень приёма", "tx_power": "мощность передачи",
    "range": "дальность обнаружения", "co2": "CO2 в атмосфере", "o2": "O2 в атмосфере", "cabin_press": "давление в кабине",
    "battery": "заряд батареи", "seal": "герметичность", "latch": "состояние замков", "position": "положение привода",
    "field_cal": "калибровка поля ядра", "beacon": "маяк SOS", "ident": "ответ транспондера",
}
STAGES = {"C": "первые признаки (к сведению)", "W": "работает хуже (предупреждение)", "F": "отказ"}
LEVELS = {"operator": "пилот (с пульта)", "drone": "ремонтный дрон", "nanobay": "нанобокс", "yard": "верфь"}
CAUSES = ("wear", "thermal", "impact", "laser", "radiation", "internal")

# ship buses: source categories -> sink categories (a unit references "bus:<name>" upstream)
BUSES = {
    "propulsion_power": {"ru": "силовая шина двигателей", "source": ["reactor"], "sinks": ["drive", "rcs"]},
    "service_power": {"ru": "служебная шина питания", "source": ["service_reactor"],
                      "sinks": ["computer", "radar", "comm", "transponder", "life_support", "weapon", "coolant_loop", "escape_capsule",
                                "docking_port", "container_rack", "jump_core"]},
    "coolant": {"ru": "контур охлаждения", "source": ["coolant_loop"], "sinks": ["reactor", "drive", "service_reactor", "weapon", "computer"]},
    "heat_rejection": {"ru": "отвод тепла радиаторами", "source": ["radiator"], "sinks": ["coolant_loop"]},
    "data": {"ru": "шина данных", "source": ["computer"], "sinks": ["drive", "rcs", "reactor", "radar", "weapon", "docking_port", "jump_core"]},
    "fuel": {"ru": "подача топливных таблеток", "source": ["tank:fusion_pellets"], "sinks": ["reactor", "service_reactor"]},
    "rcs_propellant": {"ru": "подача аргона", "source": ["tank:argon"], "sinks": ["rcs"]},
    "jump_charge": {"ru": "подача заряда прыжка", "source": ["tank:jump_charge"], "sinks": ["jump_core"]},
}


def M(code, sru, name, causes, signals, effect, isolation, repair, upstream=(), downstream=(), masks=(), intermittent=None,
      false_alarm=False, pmcs=None, symptoms=None):
    """One failure mode. effect: stage -> {func, sec, power, heat, instab} (percent deltas for the main game).
    upstream: [(ref, multiplier)] - what causes / accelerates it; ref = CODE (same unit) or bus:<name>/CODE.
    downstream: [(ref, hours, condition)] - what it damages next if left; masks: codes whose signals it hides."""
    return dict(code=code, sru=sru, name_ru=name, causes=list(causes), signals=list(signals), effect=effect,
                symptoms=symptoms or {}, isolation=isolation, repair=repair,
                upstream=[dict(ref=r, x=x) for r, x in upstream], downstream=[dict(ref=r, after_h=h, when=w) for r, h, w in downstream],
                masks=list(masks), intermittent=intermittent, false_alarm=false_alarm, pmcs=pmcs)


def T(step, test, kind, minutes, needs=None, fail=None, ok=None, note=None):
    """Isolation step: kind bite | load_test | drone_inspect | cross_check | swap | bench. fail / ok -> next step
    number or a conclusion "=CODE" / "->bus:<name>" (look upstream)."""
    return {k: v for k, v in dict(step=step, test=test, kind=kind, minutes=minutes, needs=needs, if_fail=fail, if_ok=ok, note=note).items() if v is not None}


def R(level, action, hours, nanite_kg=0, spare=None, power_mw=0, note=None):
    return {k: v for k, v in dict(level=level, action=action, hours=hours, nanite_kg=nanite_kg, spare=spare, power_mw=power_mw, note=note).items()
            if v not in (None, 0) or k in ("level", "action", "hours")}


# ---------------------------------------------------------------- equipment classes
CLASSES = []

def C(cid, ru, category, applies_to, srus, modes, tank_fluid=None):
    CLASSES.append(dict(id=cid, ru=ru, category=category, applies_to=applies_to, srus=srus, modes=modes, tank_fluid=tank_fluid))


# --- main drive: field-coupled plasma drive (Cobretty N1 axial / CR-1 vector)
C("drive.plasma.cobretty", "плазменный главный двигатель Cobretty (N1 / CR-1)", "drive",
  ["kb.drive.n1-axial", "kb.drive.cr1-vector"],
  {"injector": "коллектор форсунок рабочего тела", "coil": "сегменты катушек магнитного сопла", "chamber": "плазменная камера",
   "pad": "охлаждающий пятак двигателя", "hx": "резервный теплообменник (включается при повреждённом пятаке)",
   "gimbal": "привод подвеса / цапфы (CR-1: ±75°)", "busbar": "силовой ввод с шины реактора", "sensor": "датчики температуры и тяги"},
  [
   M("DRV-01", "injector", "засор форсунок рабочего тела", ["wear", "internal"], ["thrust", "chamber_press"],
     {"C": {"func": -5}, "W": {"func": -20, "instab": 10}, "F": {"func": -60, "instab": 30}},
     [T(1, "BITE: тест тяги на 10 %", "bite", 1, fail=2, ok="=DRV-07"),
      T(2, "Сравнить давление в камере с подачей: подача в норме?", "cross_check", 2, fail="->bus:fuel", ok=3),
      T(3, "Промывка коллектора обратным током (с пульта)", "bite", 10, fail=4, ok="=DRV-01", note="помогает на часы, если в топливе мусор"),
      T(4, "Дрон: вскрыть коллектор, осмотр форсунок (двигатель холодный)", "drone_inspect", 40, needs="двигатель заглушен, без тяги")],
     [R("operator", "промывка обратным током", 0.2, note="временно"), R("drone", "замена коллектора форсунок из запаса", 3, spare="SRU коллектор"),
      R("nanobay", "восстановление форсунок", 18, nanite_kg=6)],
     upstream=[("bus:fuel/PLT-02", 3.0)], downstream=[("DRV-03", 40, "тяга > 50 %")],
     symptoms={"C": "тяга чуть ниже заданной", "W": "тяга -20 %, пульсации давления", "F": "двигатель не держит тягу"},
     pmcs="промывка коллектора каждые 300 ч наработки"),
   M("DRV-02", "coil", "деградация сегмента катушки магнитного сопла", ["thermal", "radiation", "wear"], ["thrust", "temp", "thrust_axis"],
     {"C": {"heat": 5}, "W": {"func": -15, "heat": 20, "instab": 10}, "F": {"func": -70, "heat": 60}},
     [T(1, "BITE: ток по сегментам катушек", "bite", 2, fail=2, ok="=DRV-08"),
      T(2, "Температура пятака двигателя в норме?", "cross_check", 1, fail="=DRV-09", ok=3,
        note="перегретая катушка - чаще следствие плохого пятака, а не причина"),
      T(3, "Дрон: термограмма сопла (двигатель только что работал)", "drone_inspect", 30, needs="без тяги")],
     [R("drone", "замена сегмента катушки", 6, spare="SRU сегмент катушки"), R("nanobay", "перемотка сегмента", 40, nanite_kg=12, power_mw=2)],
     upstream=[("DRV-09", 4.0), ("DRV-04", 2.0)],
     downstream=[("DRV-03", 20, "работа на сегменте с отказом"), ("DRV-06", 60, "")],
     symptoms={"C": "сопло греется на 5-8 % сильнее", "W": "вектор тяги уводит, тяга -15 %", "F": "магнитное сопло не держит плазму"}),
   M("DRV-03", "chamber", "эрозия стенки плазменной камеры", ["wear", "thermal", "impact"], ["chamber_press", "vibration", "temp"],
     {"C": {"instab": 5}, "W": {"func": -25, "instab": 20, "heat": 15}, "F": {"func": -100}},
     [T(1, "BITE: спектр вибрации камеры", "bite", 3, fail=2, ok="=DRV-01"),
      T(2, "Дрон: эндоскоп через смотровой порт", "drone_inspect", 60, needs="двигатель холодный 2 ч")],
     [R("nanobay", "наплавка стенки камеры", 60, nanite_kg=30, power_mw=3), R("yard", "замена камеры", 30)],
     upstream=[("DRV-01", 2.5), ("DRV-02", 3.0)], downstream=[],
     symptoms={"C": "лёгкая вибрация на полной тяге", "W": "вибрация, пульсации тяги", "F": "прогар камеры - двигатель заглушён"}),
   M("DRV-09", "pad", "повреждение охлаждающего пятака двигателя", ["impact", "laser", "thermal", "wear"], ["temp_pad", "temp", "coolant_temp"],
     {"C": {"heat": 10}, "W": {"heat": 30, "func": -10}, "F": {"heat": 70, "func": -40}},
     [T(1, "BITE: температура пятака двигателя выше нормы при той же тяге?", "bite", 2, fail=2, ok="=DRV-08"),
      T(2, "Дрон: осмотр и термограмма пятака", "drone_inspect", 30, needs="без тяги")],
     [R("drone", "замена секции пятака", 5, spare="SRU секция пятака двигателя"), R("nanobay", "восстановление покрытия пятака", 30, nanite_kg=12)],
     upstream=[],
     downstream=[("DRV-02", 25, "тяга > 50 %"), ("bus:coolant/CLP-01", 60, "тепло двигателя ушло в контур"), ("DRV-04", 80, "теплообменник работает постоянно")],
     symptoms={"C": "пятак двигателя горячее обычного", "W": "тепло двигателя пошло в общий контур охлаждения: греются реактор и соседи",
               "F": "двигатель ограничен по тяге перегревом"},
     pmcs="осмотр пятаков двигателей дроном после боя или прохода через обломки"),
   M("DRV-04", "hx", "засорение резервного теплообменника двигателя", ["wear", "internal"], ["coolant_temp", "temp"],
     {"C": {"heat": 5}, "W": {"heat": 25}, "F": {"heat": 60, "func": -30}},
     [T(1, "Пятак двигателя цел, а двигатель всё равно греется?", "cross_check", 2, fail="=DRV-09", ok=2,
        note="теплообменник нагружен, только когда пятак повреждён: начинать с пятака"),
      T(2, "Перепад температуры теплоносителя вход/выход двигателя мал?", "cross_check", 2, fail=3, ok="->bus:coolant"),
      T(3, "Промывка теплообменника (с пульта, контур на байпас)", "bite", 20, fail="=DRV-04", ok="=DRV-04")],
     [R("operator", "промывка теплообменника", 0.4), R("drone", "замена теплообменника", 5, spare="SRU теплообменник")],
     upstream=[("bus:coolant/CLP-03", 3.0), ("DRV-09", 2.0)], downstream=[("DRV-02", 30, "тяга > 70 % при повреждённом пятаке")],
     symptoms={"C": "двигатель греется", "W": "автомат ограничивает тягу по температуре", "F": "перегрев - двигатель глушится"}),
   M("DRV-05", "gimbal", "износ / заклинивание привода подвеса", ["wear", "impact"], ["position", "thrust_axis"],
     {"C": {"sec": -10}, "W": {"sec": -40}, "F": {"sec": -100}},
     [T(1, "BITE: прогон подвеса по всему ходу", "bite", 2, fail=2, ok="->bus:data"),
      T(2, "Дрон: осмотр цапф и привода", "drone_inspect", 30, needs="без тяги")],
     [R("drone", "замена привода подвеса", 4, spare="SRU привод"), R("nanobay", "восстановление подшипника цапфы", 25, nanite_kg=8)],
     upstream=[("bus:data/CMP-02", 1.5)], downstream=[],
     symptoms={"C": "подвес доходит медленнее", "W": "ход подвеса урезан", "F": "двигатель застыл в одном положении (вектор не управляется)"},
     pmcs="прогон подвеса перед вылетом (CR-1)"),
   M("DRV-06", "busbar", "подгар силового ввода", ["thermal", "wear"], ["current", "temp"],
     {"C": {"power": 3}, "W": {"power": 10, "heat": 10}, "F": {"func": -100}},
     [T(1, "BITE: падение напряжения на вводе", "bite", 1, fail=2, ok="->bus:propulsion_power"),
      T(2, "Дрон: осмотр ввода (шина обесточена)", "drone_inspect", 25, needs="двигатель и реактор заглушены")],
     [R("drone", "зачистка и замена контактов ввода", 3, spare="SRU контакты")],
     upstream=[("bus:propulsion_power/RCT-04", 3.0), ("DRV-02", 1.5)], downstream=[],
     symptoms={"C": "лишний расход мощности", "W": "ввод греется", "F": "обрыв питания двигателя"}),
   M("DRV-07", "sensor", "дрейф датчика тяги (ложная тревога)", ["wear", "radiation"], ["thrust"],
     {"C": {}, "W": {"instab": 5}, "F": {"instab": 10}},
     [T(1, "Сравнить тягу по датчику с ускорением корабля", "cross_check", 2, fail="=DRV-07", ok="=DRV-01")],
     [R("operator", "перекалибровка по ускорению", 0.2), R("drone", "замена датчика", 1, spare="SRU датчик")],
     false_alarm=True, masks=["DRV-01"],
     symptoms={"C": "показания тяги расходятся с ускорением", "W": "автопилот дёргает тягу", "F": "BITE кричит о тяге, а корабль разгоняется нормально"}),
   M("DRV-08", "sensor", "отказ датчика температуры сопла (ложная тревога)", ["radiation", "thermal"], ["temp"],
     {"C": {}, "W": {"func": -10}, "F": {"func": -30}},
     [T(1, "Сравнить с соседним датчиком и термограммой", "cross_check", 3, fail="=DRV-08", ok="=DRV-02")],
     [R("drone", "замена датчика", 1, spare="SRU датчик")],
     false_alarm=True, masks=["DRV-02"],
     symptoms={"W": "автомат режет тягу по ложному перегреву"}),
  ])

# --- propulsion reactor ER-5
C("reactor.fusion.cobretty", "двигательный термоядерный реактор Cobretty ER", "reactor", ["kb.reactor.er5", "kb.reactor.er1"],
  {"pad": "охлаждающий пятак", "plasma_ctl": "система управления плазмой", "damper": "демпферы и магниты удержания",
   "feed": "узел подачи таблеток", "inverter": "силовой преобразователь на шину", "sensor": "датчики плазмы"},
  [
   M("RCT-01", "pad", "деградация охлаждающего пятака", ["thermal", "impact", "laser"], ["temp_pad", "output_mw"],
     {"C": {"heat": 5}, "W": {"heat": 20, "func": -10}, "F": {"func": -60, "heat": 50}},
     [T(1, "Температура пятака выше нормы при той же мощности?", "bite", 2, fail=2, ok="=RCT-05"),
      T(2, "Расход теплоносителя через пятак в норме?", "cross_check", 2, fail="->bus:coolant", ok=3,
        note="типичная ловушка: новый пятак перегреется снова, если не идёт расход"),
      T(3, "Дрон: термограмма и осмотр пятака", "drone_inspect", 40, needs="реактор на холостом ходу")],
     [R("drone", "замена секции пятака", 8, spare="SRU секция пятака"), R("nanobay", "восстановление покрытия пятака", 50, nanite_kg=20)],
     upstream=[("bus:coolant/CLP-01", 5.0), ("bus:coolant/CLP-02", 3.0), ("bus:heat_rejection/RAD-01", 2.0)],
     downstream=[("RCT-03", 30, "мощность > 60 %")],
     symptoms={"C": "пятак горячее обычного", "W": "реактор ограничен по мощности", "F": "аварийное снижение мощности"}),
   M("RCT-02", "plasma_ctl", "дрейф управления плазмой", ["radiation", "internal"], ["plasma_stab", "output_mw"],
     {"C": {"instab": 5}, "W": {"instab": 20, "func": -10}, "F": {"func": -100}},
     [T(1, "BITE: стабильность плазмы на ступенчатой нагрузке", "load_test", 10, fail=2, ok="=RCT-06"),
      T(2, "Ошибки по шине данных от компьютера?", "cross_check", 2, fail="->bus:data", ok="=RCT-02")],
     [R("operator", "перекалибровка контура управления", 0.5), R("nanobay", "замена платы управления", 12, nanite_kg=3)],
     upstream=[("bus:data/CMP-02", 2.0), ("RCT-06", 1.5)], downstream=[("RCT-03", 50, "")],
     symptoms={"C": "колебания мощности", "W": "мощность «плавает»", "F": "срыв плазмы - реактор заглушён"}),
   M("RCT-03", "damper", "износ демпферов и магнитов удержания", ["wear", "thermal", "impact"], ["plasma_stab", "vibration"],
     {"C": {"instab": 5}, "W": {"func": -20, "instab": 15}, "F": {"func": -100}},
     [T(1, "BITE: вибрация корпуса реактора на мощности", "load_test", 10, fail=2, ok="=RCT-02"),
      T(2, "Дрон: осмотр креплений демпферов через люк", "drone_inspect", 60, needs="реактор заглушён")],
     [R("nanobay", "восстановление демпферов", 80, nanite_kg=40, power_mw=2), R("yard", "капремонт реактора", 72)],
     upstream=[("RCT-01", 2.0), ("RCT-02", 2.0)], downstream=[],
     symptoms={"C": "гул на высокой мощности", "W": "реактор держит не больше 70 %", "F": "реактор не запускается"}),
   M("RCT-04", "inverter", "отказ силового преобразователя", ["thermal", "internal", "impact"], ["bus_voltage", "current"],
     {"C": {"power": 5}, "W": {"func": -30}, "F": {"func": -100}},
     [T(1, "BITE: напряжение на силовой шине", "bite", 1, fail=2, ok="=RCT-02"),
      T(2, "Переключить потребителей на второй канал преобразователя", "swap", 5, fail="=RCT-04", ok="=RCT-04")],
     [R("operator", "работа на одном канале (половина мощности)", 0.1), R("nanobay", "замена силового модуля", 14, nanite_kg=5)],
     upstream=[("bus:coolant/CLP-01", 2.0)], downstream=[("bus:propulsion_power/DRV-06", 10, "")],
     symptoms={"C": "просадки шины", "W": "двигателям не хватает мощности", "F": "силовая шина обесточена"}),
   M("RCT-05", "feed", "сбой подачи таблеток", ["wear", "internal"], ["fuel_feed", "output_mw"],
     {"C": {"instab": 5}, "W": {"func": -20}, "F": {"func": -100}},
     [T(1, "Уровень в бункере в норме?", "bite", 1, fail="->bus:fuel", ok=2),
      T(2, "BITE: прогон узла подачи", "bite", 3, fail="=RCT-05", ok="=RCT-02")],
     [R("drone", "замена узла подачи", 4, spare="SRU узел подачи"), R("nanobay", "восстановление механизма подачи", 16, nanite_kg=4)],
     upstream=[("bus:fuel/PLT-02", 3.0)], downstream=[],
     symptoms={"W": "мощность проседает рывками", "F": "реактор гаснет без топлива"}),
   M("RCT-06", "sensor", "отказ датчика плазмы (ложная тревога)", ["radiation"], ["plasma_stab"],
     {"W": {"func": -10}, "F": {"func": -25}},
     [T(1, "Сравнить с резервным датчиком", "cross_check", 2, fail="=RCT-06", ok="=RCT-02")],
     [R("nanobay", "замена датчика", 4, nanite_kg=1)], false_alarm=True, masks=["RCT-02"],
     symptoms={"W": "реактор сам ограничивает мощность по ложной нестабильности"}),
  ])

# --- service reactor / APU
C("apu.cobretty", "служебный реактор (ВСУ) Cobretty SR", "service_reactor", ["kb.apu.s150"],
  {"cells": "энергоячейки", "starter": "стартёр", "regulator": "регулятор шины", "sensor": "датчики"},
  [
   M("APU-01", "cells", "деградация энергоячеек", ["wear", "radiation", "thermal"], ["output_mw", "temp"],
     {"C": {"func": -5}, "W": {"func": -25, "heat": 10}, "F": {"func": -60}},
     [T(1, "BITE: выход ВСУ на номинале", "load_test", 5, fail=2, ok="=APU-02"),
      T(2, "Температура ВСУ в норме?", "cross_check", 1, fail="->bus:coolant", ok="=APU-01")],
     [R("nanobay", "восстановление ячеек по одной", 30, nanite_kg=10, power_mw=1)],
     upstream=[("bus:coolant/CLP-01", 2.0)], downstream=[],
     symptoms={"W": "служебной мощности не хватает: сенсоры и ЖО переходят в экономный режим"}),
   M("APU-02", "regulator", "нестабильность регулятора шины", ["internal", "thermal"], ["bus_voltage"],
     {"C": {"instab": 5}, "W": {"instab": 20}, "F": {"func": -100}},
     [T(1, "BITE: осциллограмма служебной шины", "bite", 2, fail="=APU-02", ok="=APU-01")],
     [R("operator", "перевод на резервный регулятор", 0.1), R("nanobay", "замена регулятора", 8, nanite_kg=2)],
     upstream=[], downstream=[("bus:service_power/CMP-02", 20, "скачки шины"), ("bus:service_power/LSS-01", 40, "")],
     symptoms={"C": "мерцание приборов", "W": "сбои компьютера и сенсоров", "F": "служебная шина обесточена"},
     intermittent="проявляется при резкой смене нагрузки (включение РЭБ, оружия, стыковка)"),
   M("APU-03", "starter", "отказ стартёра", ["wear"], ["output_mw"],
     {"F": {"func": -100}},
     [T(1, "Холодный пуск ВСУ с пульта", "bite", 3, fail="=APU-03", ok="=APU-01")],
     [R("drone", "замена стартёра", 2, spare="SRU стартёр")], symptoms={"F": "ВСУ не запускается после глушения"},
     pmcs="тест холодного пуска раз в 100 ч"),
  ])

# --- radiators
C("radiator.csic", "панели радиатора C-SiC", "radiator", ["kb.rad.cs-panel"],
  {"panel": "излучающая панель", "hinge": "привод раскрытия", "manifold": "коллектор теплоносителя"},
  [
   M("RAD-01", "panel", "пробой панели (микрометеорит / осколок)", ["impact", "laser"], ["coolant_temp", "coolant_press"],
     {"C": {"heat": 3}, "W": {"heat": 10}, "F": {"heat": 25}},
     [T(1, "Термограмма панелей с пульта: холодное пятно?", "bite", 2, fail=2, ok="=RAD-03"),
      T(2, "Дрон: осмотр панели", "drone_inspect", 20, needs="без манёвров")],
     [R("drone", "заплатка / замена панели", 2, spare="SRU панель 10 м²"), R("nanobay", "восстановление панели", 10, nanite_kg=3)],
     downstream=[("bus:heat_rejection/CLP-02", 15, "утечка теплоносителя")],
     symptoms={"W": "теплоноситель горячее, давление падает"}),
   M("RAD-02", "hinge", "заклинивание привода раскрытия", ["wear", "impact", "thermal"], ["position"],
     {"W": {"heat": 15}, "F": {"heat": 35}},
     [T(1, "BITE: прогон приводов раскрытия", "bite", 2, fail=2, ok="=RAD-03"), T(2, "Дрон: осмотр шарнира", "drone_inspect", 20)],
     [R("drone", "освобождение / замена шарнира", 1.5, spare="SRU шарнир")],
     symptoms={"W": "часть панелей не раскрылась - тепло отводится хуже"}),
   M("RAD-03", "manifold", "загрязнение коллектора", ["wear"], ["coolant_flow"],
     {"C": {"heat": 3}, "W": {"heat": 10}},
     [T(1, "Расход через панели неравномерный?", "cross_check", 3, fail="=RAD-03", ok="->bus:heat_rejection")],
     [R("operator", "промывка коллектора", 0.5)], symptoms={"C": "часть панелей холоднее остальных"}),
  ])

# --- coolant loop
C("coolant.loop.cobretty", "контур охлаждения Cobretty (насос 180 МВт)", "coolant_loop", ["kb.cool.loop200"],
  {"pump": "насос", "pipes": "магистрали", "valves": "клапаны распределения", "fluid": "теплоноситель"},
  [
   M("CLP-01", "pump", "износ насоса контура", ["wear", "thermal"], ["coolant_flow", "current", "vibration"],
     {"C": {"heat": 5}, "W": {"heat": 20}, "F": {"heat": 60}},
     [T(1, "Расход теплоносителя ниже нормы?", "bite", 2, fail=2, ok="=CLP-03"),
      T(2, "Ток насоса выше нормы при низком расходе?", "cross_check", 2, fail="=CLP-01", ok="=CLP-02",
        note="насос гонит, а расхода нет - утечка или засор, а не насос")],
     [R("drone", "замена насоса", 4, spare="SRU насос"), R("nanobay", "восстановление крыльчатки и подшипников", 20, nanite_kg=6)],
     upstream=[("bus:service_power/APU-02", 1.5)],
     downstream=[("bus:coolant/RCT-01", 20, "реактор > 50 %"), ("bus:coolant/DRV-02", 25, "тяга > 50 % и повреждён пятак двигателя"), ("bus:coolant/APU-01", 60, "")],
     masks=["CLP-02"],
     symptoms={"C": "теплоноситель на 5 % горячее", "W": "все потребители контура греются - BITE покажет их, а не насос", "F": "охлаждение потеряно"}),
   M("CLP-02", "pipes", "утечка в магистрали", ["impact", "wear", "laser"], ["coolant_press", "coolant_flow"],
     {"C": {"heat": 3}, "W": {"heat": 15}, "F": {"heat": 60}},
     [T(1, "Давление в контуре падает со временем?", "bite", 10, fail=2, ok="=CLP-01"),
      T(2, "Дрон: поиск утечки по магистрали", "drone_inspect", 50, needs="без манёвров")],
     [R("drone", "заварка / замена участка", 3, spare="SRU участок магистрали"), R("operator", "долить теплоноситель из запаса", 0.2, note="временно")],
     upstream=[("bus:heat_rejection/RAD-01", 2.0)],
     downstream=[("bus:coolant/RCT-01", 30, ""), ("bus:coolant/DRV-02", 40, "")],
     symptoms={"W": "давление падает, насос работает на износ"}),
   M("CLP-03", "fluid", "загрязнение теплоносителя", ["wear", "internal"], ["coolant_flow"],
     {"C": {"heat": 3}, "W": {"heat": 10}},
     [T(1, "Анализ пробы теплоносителя", "bench", 15, fail="=CLP-03", ok="->bus:heat_rejection")],
     [R("nanobay", "очистка теплоносителя", 8, nanite_kg=2)],
     downstream=[("bus:coolant/DRV-04", 80, ""), ("RAD-03", 80, "")], symptoms={"C": "медленный рост температуры всех потребителей"}),
  ])

# --- jump core (sealed: yard only)
C("jump_core.um", "ядро прыжка «Нулевая точка» (запечатано)", "jump_core", ["um.core.z05"],
  {"field": "эмиттер поля", "damper": "демпферы лотосита", "seal": "пломбы и контроль вскрытия"},
  [
   M("JCR-01", "field", "дрейф калибровки поля", ["radiation", "wear"], ["field_cal"],
     {"C": {"power": 5}, "W": {"power": 20, "instab": 10}, "F": {"func": -100}},
     [T(1, "BITE: калибровочный тест поля (без прыжка)", "bite", 15, fail="=JCR-01", ok="->bus:data")],
     [R("yard", "перекалибровка на верфи с лицензией", 12, note="запечатанный модуль: вскрытие на борту - нарушение лицензии")],
     upstream=[("bus:data/CMP-02", 1.5)], symptoms={"W": "заряд прыжка расходуется больше, точность выхода хуже"}),
   M("JCR-02", "damper", "износ демпферов лотосита", ["wear", "radiation"], ["field_cal", "vibration"],
     {"W": {"instab": 20}, "F": {"func": -100}},
     [T(1, "BITE: спектр вибрации ядра при зарядке", "load_test", 20, fail="=JCR-02", ok="=JCR-01")],
     [R("yard", "переотжиг демпферов", 48, note="только «Нулевая точка» / лицензиат")], symptoms={"F": "ядро отказывается заряжаться"}),
  ])

# --- tanks
C("tank.pellets", "бункер топливных таблеток", "tank", ["std.tank.pellets"],
  {"auger": "шнек подачи", "level": "датчик уровня", "shell": "оболочка"},
  [
   M("PLT-01", "level", "дрейф датчика уровня (ложные показания)", ["wear", "radiation"], ["tank_level"],
     {"W": {}, "F": {}}, [T(1, "Сверить уровень с расходом по журналу", "cross_check", 3, fail="=PLT-01", ok="=PLT-02")],
     [R("drone", "замена датчика", 1, spare="SRU датчик")], false_alarm=True, symptoms={"W": "топлива «меньше», чем должно быть"}),
   M("PLT-02", "auger", "загрязнение / износ шнека (крошка таблеток в подаче)", ["wear", "impact"], ["fuel_feed"],
     {"C": {"instab": 3}, "W": {"instab": 10}, "F": {"func": -100}},
     [T(1, "BITE: прогон шнека", "bite", 2, fail="=PLT-02", ok="=PLT-01")],
     [R("drone", "чистка / замена шнека", 2, spare="SRU шнек")],
     downstream=[("bus:fuel/RCT-05", 30, ""), ("bus:fuel/DRV-01", 80, "крошка в рабочем теле")],
     symptoms={"C": "неровная подача", "W": "реактор и двигатель «чихают»"}, pmcs="продувка шнека каждые 200 ч"),
  ], tank_fluid="fusion_pellets")
C("tank.jump_charge", "бак заряда прыжка", "tank", ["std.tank.jumpcharge"],
  {"valve": "клапан", "shell": "оболочка"},
  [
   M("JCT-01", "valve", "негерметичность клапана", ["wear", "impact"], ["tank_level", "tank_press"],
     {"W": {}, "F": {}}, [T(1, "Убыль заряда при стоянке?", "bite", 30, fail="=JCT-01")],
     [R("drone", "замена клапана", 1, spare="SRU клапан")], downstream=[("bus:jump_charge/JCR-01", 200, "")],
     symptoms={"W": "заряд прыжка медленно уходит"}),
  ], tank_fluid="jump_charge")
C("tank.argon", "бак аргона RCS", "tank", ["kb.rcs.argon-tank"],
  {"regulator": "редуктор давления", "shell": "оболочка"},
  [
   M("ARG-01", "regulator", "дрейф редуктора давления", ["wear", "thermal"], ["tank_press"],
     {"C": {"instab": 3}, "W": {"instab": 15}}, [T(1, "Давление на выходе редуктора в допуске?", "bite", 2, fail="=ARG-01", ok="->bus:rcs_propellant")],
     [R("drone", "замена редуктора", 1.5, spare="SRU редуктор")], downstream=[("bus:rcs_propellant/RCS-01", 50, "")],
     symptoms={"W": "импульсы маневровых неравные"}),
   M("ARG-02", "shell", "утечка аргона", ["impact", "laser"], ["tank_level", "tank_press"],
     {"W": {}, "F": {"func": -100}}, [T(1, "Убыль аргона без манёвров?", "bite", 15, fail=2), T(2, "Дрон: поиск утечки", "drone_inspect", 30)],
     [R("drone", "заплатка", 1.5)], symptoms={"F": "аргон кончился - RCS не работает"}),
  ], tank_fluid="argon")

# --- RCS nozzle
C("rcs.fc.cobretty", "маневровое сопло Cobretty (field-coupled)", "rcs", ["kb.rcs.fc-nozzle"],
  {"valve": "клапан", "coil": "катушка ускорителя", "nozzle": "сопло"},
  [
   M("RCS-01", "valve", "залипание клапана (сопло «тянет»)", ["wear", "thermal"], ["valve_pos", "thrust_axis"],
     {"C": {"instab": 5}, "W": {"instab": 20}, "F": {"sec": -100, "instab": 30}},
     [T(1, "BITE: импульсы по осям, есть ли дрейф без команды?", "bite", 3, fail=2, ok="=RCS-02"),
      T(2, "Какое сопло: поочерёдное отключение пар", "swap", 5, fail="=RCS-01")],
     [R("operator", "отключить пару (ось слабее)", 0.05), R("drone", "замена клапана", 1, spare="SRU клапан")],
     upstream=[("bus:rcs_propellant/ARG-01", 2.0)], symptoms={"W": "корабль медленно закручивает без команды"}),
   M("RCS-02", "coil", "отказ катушки ускорителя", ["thermal", "radiation"], ["current", "thrust"],
     {"W": {"func": -50}, "F": {"func": -100}},
     [T(1, "BITE: ток катушек сопел", "bite", 2, fail="=RCS-02", ok="->bus:propulsion_power")],
     [R("drone", "замена катушки", 1.5, spare="SRU катушка"), R("nanobay", "перемотка", 8, nanite_kg=1)],
     upstream=[("bus:propulsion_power/RCT-04", 1.5)], symptoms={"W": "одна ось манёвра слабее"}),
  ])

# --- computer
C("computer.xz", "бортовой компьютер Синьчжи MC", "computer", ["xz.mc.c1", "xz.mc.c3"],
  {"board": "вычислительная плата", "memory": "память", "psu": "блок питания"},
  [
   M("CMP-01", "memory", "деградация памяти (радиация)", ["radiation"], ["data_err"],
     {"C": {"instab": 3}, "W": {"instab": 10}}, [T(1, "BITE: тест памяти", "bite", 5, fail="=CMP-01", ok="=CMP-02")],
     [R("operator", "перенос на резервный банк", 0.1), R("nanobay", "замена модуля памяти", 4, nanite_kg=0.5)],
     symptoms={"C": "редкие ошибки данных", "W": "сбои автопилота"}),
   M("CMP-02", "psu", "нестабильное питание компьютера", ["thermal", "internal"], ["data_err", "watchdog"],
     {"C": {"instab": 5}, "W": {"instab": 20}, "F": {"func": -100}},
     [T(1, "BITE: перезапуски по сторожевому таймеру?", "bite", 2, fail=2, ok="=CMP-01"),
      T(2, "Служебная шина стабильна?", "cross_check", 2, fail="->bus:service_power", ok="=CMP-02",
        note="замена блока питания не поможет, если скачет шина ВСУ - второй блок сгорит")],
     [R("nanobay", "замена блока питания", 6, nanite_kg=1)],
     upstream=[("bus:service_power/APU-02", 4.0)],
     downstream=[("bus:data/DRV-05", 60, "команды с ошибками"), ("bus:data/RCT-02", 60, ""), ("bus:data/JCR-01", 120, "")],
     symptoms={"C": "редкие перезагрузки", "W": "сбои автопилота и навигации", "F": "компьютер не загружается"},
     intermittent="при скачках служебной шины"),
  ])

# --- radar, comm, transponder
C("radar.xz", "радар Синьчжи N", "radar", ["xz.radar.n2", "xz.radar.n10"],
  {"array": "передающие модули решётки", "rx": "приёмник"},
  [M("RDR-01", "array", "отказ модулей решётки", ["impact", "laser", "thermal"], ["range"],
     {"C": {"func": -5}, "W": {"func": -30}, "F": {"func": -100}}, [T(1, "BITE: самотест решётки", "bite", 3, fail="=RDR-01", ok="->bus:data")],
     [R("drone", "замена модулей решётки", 3, spare="SRU модули"), R("nanobay", "восстановление модулей", 12, nanite_kg=2)],
     symptoms={"W": "дальность обнаружения упала, ложные цели"})])
C("comm.xz", "связной комплекс Синьчжи C", "comm", ["xz.comm.c1", "xz.comm.c3"],
  {"feeder": "фидер антенны", "transceiver": "трансивер"},
  [M("COM-01", "feeder", "обрыв фидера на обшивке", ["impact", "laser"], ["tx_power", "rx_level"],
     {"W": {"func": -50}, "F": {"func": -100}},
     [T(1, "Эхо-тест с маяком", "bite", 5, fail=2, ok="=COM-02"), T(2, "Перезапуск трансивера помог?", "bite", 2, fail=3, ok="=COM-02"),
      T(3, "Дрон: осмотр фидера по обшивке", "drone_inspect", 30)],
     [R("drone", "замена участка фидера", 1.5, spare="SRU фидер")],
     symptoms={"W": "связь слабее - SOS слышен хуже", "F": "нет связи: SOS не уходит"}),
   M("COM-02", "transceiver", "сбой трансивера", ["radiation", "internal"], ["tx_power"],
     {"W": {"func": -30}, "F": {"func": -100}}, [T(1, "Перезапуск трансивера", "bite", 2, fail="=COM-02", ok="=COM-02")],
     [R("operator", "перезапуск", 0.05), R("nanobay", "замена платы", 6, nanite_kg=1)], upstream=[("bus:service_power/APU-02", 2.0)],
     intermittent="после радиационной вспышки", symptoms={"W": "связь пропадает"})])
C("transponder.um", "транспондер Управления Маяков Т-1", "transponder", ["um.xpdr.t1"], {"coder": "кодер"},
  [M("XPD-01", "coder", "сбой кодировки ответа", ["radiation", "internal"], ["ident"], {"W": {"func": -100}},
     [T(1, "Запрос ответа от станции / самотест", "bite", 2, fail="=XPD-01")], [R("operator", "перепрошивка кода", 0.2), R("nanobay", "замена кодера", 3, nanite_kg=0.2)],
     symptoms={"W": "диспетчер не видит корабль - отказ в стыковке"})])

# --- life support (the capsule's own and the cabin's)
C("life_support.eurohab", "жизнеобеспечение Евро-Хабитат L", "life_support", ["eurohab.ls.l4", "eurohab.ls.l20"],
  {"scrubber": "поглотитель CO2", "o2": "генератор O2", "water": "регенерация воды", "fan": "вентиляция"},
  [
   M("LSS-01", "scrubber", "истощение поглотителя CO2", ["wear", "internal"], ["co2"],
     {"C": {"func": -5}, "W": {"func": -30}, "F": {"func": -100}},
     [T(1, "CO2 растёт при номинальном питании ЖО?", "bite", 5, fail="=LSS-01", ok="->bus:service_power"),
      T(2, "Поглотителю хватает питания?", "cross_check", 2, fail="->bus:service_power", ok="=LSS-01",
        note="картридж купит время, но если урезано питание - CO2 снова поползёт")],
     [R("operator", "замена картриджа из запаса", 0.3, spare="картридж поглотителя"), R("nanobay", "регенерация сорбента", 10, nanite_kg=1)],
     upstream=[("bus:service_power/APU-01", 2.0), ("bus:service_power/APU-02", 2.0), ("CAP-02", 3.0)],
     symptoms={"C": "CO2 медленно растёт", "W": "головная боль, CO2 выше нормы", "F": "CO2 опасен - часы до потери сознания"},
     pmcs="запас картриджей перед вылетом"),
   M("LSS-02", "o2", "отказ генератора кислорода", ["internal", "thermal"], ["o2"],
     {"W": {"func": -40}, "F": {"func": -100}}, [T(1, "BITE: выход O2", "bite", 3, fail="=LSS-02")],
     [R("operator", "переход на баллоны O2", 0.1), R("nanobay", "восстановление электролизёра", 20, nanite_kg=4)],
     symptoms={"W": "O2 падает", "F": "только запас в баллонах"}),
   M("LSS-03", "fan", "отказ вентиляции", ["wear"], ["co2", "temp"],
     {"W": {"func": -20}}, [T(1, "Шум / ток вентилятора", "bite", 1, fail="=LSS-03", ok="=LSS-01")],
     [R("operator", "замена вентилятора", 0.5, spare="вентилятор")], masks=["LSS-01"],
     symptoms={"W": "в кабине душно, CO2 в углах выше - BITE путает с поглотителем"}),
  ])

# --- escape capsule
C("capsule.cobretty", "катапультируемая капсула кабины Cobretty", "escape_capsule", ["kb.surv.capsule2"],
  {"pyro": "пиропатроны отстрела", "battery": "батарея капсулы", "beacon": "маяк SOS", "seal": "уплотнение люка"},
  [
   M("CAP-01", "pyro", "отказ цепи пиропатронов", ["wear", "radiation"], ["seal"],
     {"F": {"func": -100}}, [T(1, "PMCS: тест цепи отстрела (без подрыва)", "bite", 2, fail="=CAP-01")],
     [R("drone", "замена пиропатронов", 2, spare="пиропатроны")], symptoms={"F": "капсула не отстрелится"},
     pmcs="тест цепи перед каждым вылетом"),
   M("CAP-02", "battery", "разряд / деградация батареи капсулы", ["wear", "thermal"], ["battery"],
     {"C": {"func": -5}, "W": {"func": -40}, "F": {"func": -100}},
     [T(1, "BITE: ёмкость батареи под нагрузкой", "load_test", 10, fail="=CAP-02", ok="->bus:service_power")],
     [R("operator", "подзарядка от шины корабля", 1), R("nanobay", "восстановление ячеек", 12, nanite_kg=2)],
     upstream=[("bus:service_power/APU-02", 1.5)], downstream=[("LSS-01", 5, "капсула отделена"), ("CAP-03", 10, "капсула отделена")],
     symptoms={"W": "ресурс капсулы меньше расчётного", "F": "капсула без питания"}),
   M("CAP-03", "beacon", "отказ маяка SOS", ["impact", "radiation", "internal"], ["beacon"],
     {"F": {"func": -100}}, [T(1, "Самотест маяка", "bite", 1, fail=2), T(2, "Питание маяка в норме?", "cross_check", 1, fail="=CAP-02", ok="=CAP-03")],
     [R("operator", "переключить SOS на основной связной комплекс", 0.1, note="пока капсула на корабле"), R("nanobay", "замена маяка", 4, nanite_kg=0.5)],
     symptoms={"F": "SOS молчит - спасатели не узнают"}),
   M("CAP-04", "seal", "износ уплотнения люка", ["wear", "thermal", "impact"], ["cabin_press"],
     {"C": {}, "W": {"func": -20}, "F": {"func": -100}}, [T(1, "Давление в кабине падает при закрытом люке?", "bite", 15, fail="=CAP-04")],
     [R("operator", "герметик", 0.3, note="временно"), R("drone", "замена уплотнения", 2, spare="уплотнение")],
     downstream=[("LSS-02", 30, "")], symptoms={"W": "утечка воздуха из капсулы"}),
  ])

# --- container lock, docking collar, point-defence turret
C("cargo.c20lock", "стойка C20", "container_rack", ["std.cargo.c20lock"], {"latch": "замки", "rail": "рельсы"},
  [M("C20-01", "latch", "отказ замков контейнера", ["wear", "impact"], ["latch"], {"W": {"func": -100}},
     [T(1, "BITE: состояние замков", "bite", 1, fail=2), T(2, "Дрон: осмотр замка", "drone_inspect", 15)],
     [R("drone", "замена замка", 1, spare="SRU замок")], symptoms={"W": "контейнер не закреплён - ограничение манёвров"})])
C("dock.collar", "стыковочный узел", "docking_port", ["std.dock.collar"], {"seal": "уплотнение", "latch": "захваты"},
  [M("DOK-01", "latch", "отказ захватов", ["wear", "impact"], ["latch"], {"W": {"func": -100}},
     [T(1, "BITE: прогон захватов", "bite", 2, fail="=DOK-01", ok="->bus:data")], [R("drone", "замена привода захвата", 2, spare="SRU привод")],
     symptoms={"W": "стыковка невозможна"}),
   M("DOK-02", "seal", "износ уплотнения стыковочного узла", ["wear", "thermal"], ["seal"], {"W": {"func": -30}},
     [T(1, "Тест давлением после стыковки", "bite", 10, fail="=DOK-02")], [R("drone", "замена уплотнения", 1.5, spare="уплотнение")],
     symptoms={"W": "утечка при стыковке"})])
C("weapon.pd.cobretty", "оборонительная турель Cobretty PD", "weapon", ["kb.wpn.pd1"], {"emitter": "излучатель", "mount": "привод наведения"},
  [M("PDW-01", "emitter", "деградация излучателя", ["thermal", "wear"], ["output_mw", "temp"], {"W": {"func": -30, "heat": 15}, "F": {"func": -100}},
     [T(1, "BITE: тестовый импульс", "bite", 1, fail=2, ok="=PDW-02"), T(2, "Охлаждение турели в норме?", "cross_check", 1, fail="->bus:coolant", ok="=PDW-01")],
     [R("nanobay", "восстановление излучателя", 20, nanite_kg=4)], upstream=[("bus:coolant/CLP-01", 2.0)],
     symptoms={"W": "мощность импульса ниже"}),
   M("PDW-02", "mount", "люфт привода наведения", ["wear", "impact"], ["position"], {"W": {"sec": -40}},
     [T(1, "BITE: прогон наведения", "bite", 1, fail="=PDW-02", ok="->bus:data")], [R("drone", "замена привода", 2, spare="SRU привод")],
     symptoms={"W": "турель мажет по быстрым целям"})])


# ---------------------------------------------------------------- reference designations
# class letters (docs/REFERENCE_DESIGNATIONS.md, accepted by the author 2026-11-09); ASME Y14.44 / IEEE 315 style
CLASS_LETTERS = {
    "A": "сборочная единица / узел (разборный)", "U": "неразборный узел", "ME": "главный двигатель", "GR": "двигательный реактор",
    "GS": "служебный реактор (ВСУ)", "PT": "охлаждающий пятак", "RD": "панель радиатора", "HX": "теплообменник", "PU": "насос",
    "VL": "клапан", "TK": "бак / бункер", "RC": "маневровое сопло", "JC": "ядро прыжка", "CP": "бортовой компьютер",
    "RA": "радар / антенная решётка", "CM": "связной комплекс", "TX": "транспондер", "LS": "жизнеобеспечение", "EC": "спасательная капсула",
    "BT": "батарея", "W": "кабель / жгут", "WP": "трубопровод / магистраль", "J": "разъём (гнездо)", "P": "разъём (вилка)", "S": "выключатель",
    "B": "датчик", "MP": "механическая деталь", "WE": "вооружение", "DK": "стыковочный узел", "CR": "контейнерная стойка",
    "L": "катушка (IEEE 315)", "IJ": "форсуночный коллектор", "PS": "источник / преобразователь питания (IEEE 315)",
    "FL": "фильтр / поглотитель (IEEE 315)", "G": "генератор (IEEE 315)", "E": "пиро- и прочие электрические элементы (IEEE 315)",
}
SRU_CLASS = {
    "drive.plasma.cobretty": {"injector": "IJ", "coil": "L", "chamber": "MP", "pad": "PT", "hx": "HX", "gimbal": "MP", "busbar": "W", "sensor": "B"},
    "reactor.fusion.cobretty": {"pad": "PT", "plasma_ctl": "A", "damper": "MP", "feed": "MP", "inverter": "PS", "sensor": "B"},
    "apu.cobretty": {"cells": "BT", "starter": "MP", "regulator": "PS", "sensor": "B"},
    "radiator.csic": {"panel": "RD", "hinge": "MP", "manifold": "WP"},
    "coolant.loop.cobretty": {"pump": "PU", "pipes": "WP", "valves": "VL", "fluid": "TK"},
    "jump_core.um": {"field": "L", "damper": "MP", "seal": "MP"},
    "tank.pellets": {"auger": "MP", "level": "B", "shell": "TK"},
    "tank.jump_charge": {"valve": "VL", "shell": "TK"},
    "tank.argon": {"regulator": "VL", "shell": "TK"},
    "rcs.fc.cobretty": {"valve": "VL", "coil": "L", "nozzle": "MP"},
    "computer.xz": {"board": "A", "memory": "A", "psu": "PS"},
    "radar.xz": {"array": "A", "rx": "A"},
    "comm.xz": {"feeder": "W", "transceiver": "A"},
    "transponder.um": {"coder": "A"},
    "life_support.eurohab": {"scrubber": "FL", "o2": "G", "water": "FL", "fan": "MP"},
    "capsule.cobretty": {"pyro": "E", "battery": "BT", "beacon": "CM", "seal": "MP"},
    "cargo.c20lock": {"latch": "MP", "rail": "MP"},
    "dock.collar": {"seal": "MP", "latch": "MP"},
    "weapon.pd.cobretty": {"emitter": "WE", "mount": "MP"},
}
for _c in CLASSES:
    _c["sru_class"] = SRU_CLASS.get(_c["id"], {})


# ---------------------------------------------------------------- build + checks
def class_of_part():
    out = {}
    for c in CLASSES:
        for pid in c["applies_to"]:
            out[pid] = c
    return out


def bus_role(cls):
    """the bus names this class sources and sinks"""
    key = cls["category"] if cls["category"] != "tank" else "tank:" + (cls.get("tank_fluid") or "")
    src = [b for b, d in BUSES.items() if key in d["source"]]
    snk = [b for b, d in BUSES.items() if cls["category"] in d["sinks"]]
    return src, snk


def check(catalog):
    problems = []
    codes = {}
    for c in catalog["classes"]:
        for m in c["modes"]:
            if m["code"] in codes:
                problems.append(f"duplicate code {m['code']}")
            codes[m["code"]] = c
    for c in catalog["classes"]:
        for sru in c["srus"]:
            if c["sru_class"].get(sru) not in CLASS_LETTERS:
                problems.append(f"{c['id']}: SRU {sru} has no class letter")
        src, snk = bus_role(c)
        for m in c["modes"]:
            if m["sru"] not in c["srus"]:
                problems.append(f"{m['code']}: unknown SRU {m['sru']}")
            for cause in m["causes"]:
                if cause not in CAUSES:
                    problems.append(f"{m['code']}: unknown cause {cause}")
            for s in m["signals"]:
                if s not in SIGNALS:
                    problems.append(f"{m['code']}: unknown signal {s}")
            for ref in [u["ref"] for u in m["upstream"]] + [d["ref"] for d in m["downstream"]] + m["masks"]:
                code = ref.split("/")[-1]
                if code not in codes:
                    problems.append(f"{m['code']}: reference to unknown {ref}")
                if ref.startswith("bus:"):
                    bus = ref[4:].split("/")[0]
                    if bus not in BUSES:
                        problems.append(f"{m['code']}: unknown bus {bus}")
            steps = {s["step"] for s in m["isolation"]}
            reach = False
            for s in m["isolation"]:
                for nxt in (s.get("if_fail"), s.get("if_ok")):
                    if isinstance(nxt, int) and nxt not in steps:
                        problems.append(f"{m['code']}: step {s['step']} -> missing step {nxt}")
                    if isinstance(nxt, str) and nxt.startswith("="):
                        if nxt[1:] not in codes:
                            problems.append(f"{m['code']}: concludes unknown {nxt}")
                        if nxt[1:] == m["code"]:
                            reach = True
                    if isinstance(nxt, str) and nxt.startswith("->bus:") and nxt[6:] not in BUSES:
                        problems.append(f"{m['code']}: unknown bus {nxt}")
                if s["kind"] in ("drone_inspect",) and s.get("if_fail") is None and s.get("if_ok") is None:
                    reach = True   # the inspection itself confirms
            if not reach:
                problems.append(f"{m['code']}: no isolation path concludes this mode")
            if not m["repair"]:
                problems.append(f"{m['code']}: no repair")
            for r in m["repair"]:
                if r["level"] not in LEVELS:
                    problems.append(f"{m['code']}: unknown repair level {r['level']}")
    return problems


def ship_graph(ship, cls_of):
    """the fault graph of one ship: its installed classes, their modes and the resolved cross-unit edges"""
    installed = {}
    for b in ship["bom"]:
        c = cls_of.get(b["part_id"])
        if c:
            installed.setdefault(c["id"], {"class": c["id"], "parts": [], "qty": 0})
            installed[c["id"]]["parts"].append(b["part_id"])
            installed[c["id"]]["qty"] += b["qty"]
    present = {cid for cid in installed}
    by_code = {m["code"]: c for c in CLASSES for m in c["modes"]}
    edges = []
    missing = set(b["part_id"] for b in ship["bom"] if b["part_id"] not in cls_of)
    for cid in sorted(present):
        c = next(x for x in CLASSES if x["id"] == cid)
        for m in c["modes"]:
            for u in m["upstream"]:
                code = u["ref"].split("/")[-1]
                if by_code[code]["id"] in present:
                    edges.append(dict(source=code, target=m["code"], kind="causes", x=u["x"], via=u["ref"].split("/")[0] if "/" in u["ref"] else None))
            for d in m["downstream"]:
                code = d["ref"].split("/")[-1]
                if by_code[code]["id"] in present:
                    edges.append(dict(source=m["code"], target=code, kind="damages", after_h=d["after_h"], when=d["when"]))
            for k in m["masks"]:
                if by_code[k]["id"] in present:
                    edges.append(dict(source=m["code"], target=k, kind="masks"))
    return dict(id=ship["id"], name=ship["name"], installed=sorted(installed.values(), key=lambda x: x["class"]), edges=edges,
                uncovered_parts=sorted(missing))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    catalog = dict(schema="elite.fault_catalog.v1", status="PROPOSAL",
                   note=("Режимы отказа по классам оборудования (docs/MAINTENANCE_LAYER_DESIGN.md). Основная игра берёт effect "
                         "(проценты по стадиям); мини-игра ремонта - граф upstream / downstream / masks. Пишет "
                         "tools/maintenance/build_fault_catalog.py; руками не править."),
                   signals=SIGNALS, stages=STAGES, repair_levels=LEVELS, buses=BUSES, class_letters=CLASS_LETTERS, classes=CLASSES)
    problems = check(catalog)
    with open(PASSPORTS, encoding="utf-8") as f:
        ships = json.load(f)["passports"]
    cls_of = class_of_part()
    graphs = [ship_graph(s, cls_of) for s in ships]
    covered = [g for g in graphs if g["installed"]]
    cobra = [g for g in graphs if g["id"].startswith("kb.ship.cobrett-i")]
    for g in cobra:
        if g["uncovered_parts"]:
            problems.append(f"{g['id']}: parts without a fault class {g['uncovered_parts']}")
    text = json.dumps(catalog, ensure_ascii=False, indent=1)
    ref = "window.ELITE_FAULT_DATA=" + json.dumps(dict(catalog=catalog, ships=graphs), ensure_ascii=False, separators=(",", ":")) + ";\n"
    if a.check:
        for path, new in ((OUT, text + "\n"), (REF, ref)):
            if not os.path.exists(path) or open(path, encoding="utf-8").read() != new:
                problems.append(f"out of date: {os.path.relpath(path, ROOT)}")
    else:
        os.makedirs(os.path.dirname(OUT), exist_ok=True)
        with open(OUT, "w", encoding="utf-8", newline="\n") as f:
            f.write(text + "\n")
        with open(REF, "w", encoding="utf-8", newline="\n") as f:
            f.write(ref)
    n_modes = sum(len(c["modes"]) for c in CLASSES)
    for p in problems:
        print("PROBLEM", p)
    print(f"fault catalog: {len(CLASSES)} equipment classes, {n_modes} failure modes, {len(BUSES)} buses; "
          f"ships with a graph {len(covered)}/{len(graphs)}; Cobrett-I: " +
          ", ".join(f"{g['name']} {len(g['edges'])} edges" for g in cobra) + f"; {len(problems)} problems")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
