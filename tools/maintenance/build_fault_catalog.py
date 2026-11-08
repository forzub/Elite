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
                                "docking_port", "container_rack", "jump_core", "passenger_module", "hangar", "boat_bay", "drone_bay",
                                "workshop", "medbay", "ecm", "sensor_mast", "mining_rig", "tow_rig", "cryo_tank", "train_module",
                                "landing_gear", "lifeboat"]},
    "coolant": {"ru": "контур охлаждения", "source": ["coolant_loop"], "sinks": ["reactor", "drive", "service_reactor", "weapon", "computer",
                                                                                     "ecm", "mining_rig", "cryo_tank", "passenger_module"]},
    "heat_rejection": {"ru": "отвод тепла радиаторами", "source": ["radiator"], "sinks": ["coolant_loop"]},
    "data": {"ru": "шина данных", "source": ["computer"], "sinks": ["drive", "rcs", "reactor", "radar", "weapon", "docking_port", "jump_core",
                                                                     "sensor_mast", "ecm", "tow_rig", "mining_rig", "landing_gear", "hangar"]},
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
C("drive.plasma", "плазменный главный двигатель", "drive",
  ["kb.drive.n1-axial", "kb.drive.cr1-vector", "ofu.drive.u33", "ofu.drive.u65m", "ofu.drive.u70", "ofu.drive.u73l",
   "th.drive.tz11", "th.drive.tz16v", "th.drive.tz28", "kern.drive.kt1"],
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
C("reactor.fusion", "двигательный термоядерный реактор", "reactor", ["kb.reactor.er5", "kb.reactor.er1", "srw.reactor.sr48", "srw.reactor.sr26",
  "th.reactor.tr10", "th.reactor.tr26v"],
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
C("apu.fusion", "служебный реактор (ВСУ)", "service_reactor", ["kb.apu.s150", "srw.apu.s60", "srw.apu.s200", "th.apu.s150", "th.apu.s600"],
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
C("radiator.panel", "панели радиатора", "radiator", ["kb.rad.cs-panel", "sol.rad.panel50", "th.rad.panel50"],
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
C("coolant.loop", "контур охлаждения с насосом", "coolant_loop", ["kb.cool.loop200", "sol.cool.loop300", "th.cool.loop600"],
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
C("jump_core", "ядро прыжка (неразборный узел)", "jump_core", ["um.core.z05", "um.core.z1", "um.core.z5", "harmony.core.h5", "harmony.core.h12",
  "harmony.core.h80", "kapehu.core.k5", "kapehu.core.k12", "bastion.core.b12"],
  {"field": "эмиттер поля", "damper": "демпферы лотосита", "seal": "пломбы и контроль вскрытия"},
  [
   M("JCR-01", "field", "дрейф калибровки поля", ["radiation", "wear"], ["field_cal"],
     {"C": {"power": 5}, "W": {"power": 20, "instab": 10}, "F": {"func": -100}},
     [T(1, "BITE: калибровочный тест поля (без прыжка)", "bite", 15, fail="=JCR-01", ok="->bus:data")],
     [R("yard", "перекалибровка на верфи изготовителя / лицензиата", 12, note="неразборный узел: вскрытие на борту - нарушение лицензии")],
     upstream=[("bus:data/CMP-02", 1.5)], symptoms={"W": "заряд прыжка расходуется больше, точность выхода хуже"}),
   M("JCR-02", "damper", "износ демпферов лотосита", ["wear", "radiation"], ["field_cal", "vibration"],
     {"W": {"instab": 20}, "F": {"func": -100}},
     [T(1, "BITE: спектр вибрации ядра при зарядке", "load_test", 20, fail="=JCR-02", ok="=JCR-01")],
     [R("yard", "переотжиг демпферов", 48, note="только изготовитель ядра (см. примечания к модели)")], symptoms={"F": "ядро отказывается заряжаться"}),
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
C("rcs.thruster", "маневровое сопло / кластер RCS", "rcs", ["kb.rcs.fc-nozzle", "std.rcs.cluster", "std.rcs.cluster_h"],
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
C("computer", "бортовой компьютер", "computer", ["xz.mc.c1", "xz.mc.c3", "bastion.mc.w1", "hyp.mc.r1", "kapehu.mc.k1"],
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
C("radar", "радар / антенная решётка", "radar", ["xz.radar.n2", "xz.radar.n10", "bastion.radar.s50", "hyp.radar.pa100", "kern.radar.deb5"],
  {"array": "передающие модули решётки", "rx": "приёмник"},
  [M("RDR-01", "array", "отказ модулей решётки", ["impact", "laser", "thermal"], ["range"],
     {"C": {"func": -5}, "W": {"func": -30}, "F": {"func": -100}}, [T(1, "BITE: самотест решётки", "bite", 3, fail="=RDR-01", ok="->bus:data")],
     [R("drone", "замена модулей решётки", 3, spare="SRU модули"), R("nanobay", "восстановление модулей", 12, nanite_kg=2)],
     symptoms={"W": "дальность обнаружения упала, ложные цели"})])
C("comm", "связной комплекс", "comm", ["xz.comm.c1", "xz.comm.c3", "bastion.comm.w1", "hyp.relay.r1"],
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
C("life_support", "система жизнеобеспечения", "life_support", ["eurohab.ls.l4", "eurohab.ls.l20", "eurohab.ls.l200", "ofu.ls.o4", "terra.ls.t10",
  "terra.ls.t60", "terra.ls.t200", "uhuru.ls.u500"],
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
C("escape_capsule", "спасательная капсула (катапультируемая кабина)", "escape_capsule", ["kb.surv.capsule2", "std.surv.pod4"],
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
C("weapon.pd", "лазерная турель ПРО", "weapon", ["kb.wpn.pd1", "fa.wpn.pd2", "harmony.wpn.pd3", "bastion.wpn.pd4"], {"emitter": "излучатель", "mount": "привод наведения"},
  [M("PDW-01", "emitter", "деградация излучателя", ["thermal", "wear"], ["output_mw", "temp"], {"W": {"func": -30, "heat": 15}, "F": {"func": -100}},
     [T(1, "BITE: тестовый импульс", "bite", 1, fail=2, ok="=PDW-02"), T(2, "Охлаждение турели в норме?", "cross_check", 1, fail="->bus:coolant", ok="=PDW-01")],
     [R("nanobay", "восстановление излучателя", 20, nanite_kg=4)], upstream=[("bus:coolant/CLP-01", 2.0)],
     symptoms={"W": "мощность импульса ниже"}),
   M("PDW-02", "mount", "люфт привода наведения", ["wear", "impact"], ["position"], {"W": {"sec": -40}},
     [T(1, "BITE: прогон наведения", "bite", 1, fail="=PDW-02", ok="->bus:data")], [R("drone", "замена привода", 2, spare="SRU привод")],
     symptoms={"W": "турель мажет по быстрым целям"})])



# ---------------------------------------------------------------- equipment beyond the Cobra (catalog slice 2)
def S1(code, sru, name, causes, signals, effect, test, kind, minutes, repairs, needs=None, **kw):
    """a mode isolated by one confirming check (then repair)"""
    return M(code, sru, name, causes, signals, effect, [T(1, test, kind, minutes, needs=needs, fail="=" + code)], repairs, **kw)

C("weapon.coilgun", "магнитная (coil) пушка", "weapon", ["fa.wpn.coil30", "bastion.wpn.coil10"],
  {"coil": "ускоряющие катушки ствола", "caps": "накопитель (конденсаторная батарея)", "feed": "механизм подачи снарядов", "mount": "привод наведения"},
  [S1("WCG-01", "coil", "эрозия ускоряющих катушек", ["wear", "thermal"], ["output_mw", "temp"], {"C": {"func": -5}, "W": {"func": -25, "heat": 15}, "F": {"func": -100}},
      "Тестовый выстрел: скорость снаряда ниже нормы?", "load_test", 5, [R("drone", "замена секции ствола", 4, spare="SRU секция катушек"), R("nanobay", "перемотка катушек", 30, nanite_kg=10)],
      upstream=[("bus:coolant/CLP-01", 2.0)], downstream=[("WCG-02", 40, "стрельба очередями")], symptoms={"W": "снаряд медленнее, разброс больше"}),
   M("WCG-02", "caps", "деградация накопителя", ["thermal", "wear", "internal"], ["current", "output_mw"], {"C": {"power": 5}, "W": {"func": -30}, "F": {"func": -100}},
     [T(1, "BITE: время заряда накопителя выше нормы?", "bite", 2, fail=2, ok="=WCG-01"), T(2, "Служебная шина стабильна?", "cross_check", 2, fail="->bus:service_power", ok="=WCG-02",
       note="нестабильная шина «убивает» новые конденсаторы так же, как старые")],
     [R("nanobay", "восстановление конденсаторов", 20, nanite_kg=6), R("yard", "замена накопителя", 8)],
     upstream=[("bus:service_power/APU-02", 3.0)], symptoms={"W": "темп стрельбы падает"}),
   S1("WCG-03", "feed", "заклинивание подачи", ["wear", "impact"], ["position"], {"F": {"func": -100}}, "BITE: прогон подачи вхолостую", "bite", 2,
      [R("operator", "перезарядка с пульта", 0.05), R("drone", "чистка / замена механизма подачи", 2, spare="SRU подача")], intermittent="после ударов и перегрузок", symptoms={"F": "пушка молчит"}),
  ])
C("weapon.vls", "вертикальная пусковая установка ракет", "weapon", ["harmony.wpn.vls8", "bastion.wpn.vls16"],
  {"hatch": "крышки ячеек", "ignition": "цепь пуска", "cell": "термостатирование ячеек"},
  [S1("WVL-01", "hatch", "заклинивание крышки ячейки", ["impact", "thermal", "wear"], ["position"], {"W": {"func": -15}}, "BITE: прогон крышек ячеек", "bite", 3,
      [R("drone", "освобождение / замена привода крышки", 1.5, spare="SRU привод крышки")], symptoms={"W": "часть ячеек не может пускать"}),
   S1("WVL-02", "ignition", "обрыв цепи пуска", ["radiation", "impact", "internal"], ["data_err"], {"W": {"func": -20}, "F": {"func": -100}}, "BITE: прозвонка цепей пуска", "bite", 3,
      [R("nanobay", "замена модуля цепи", 6, nanite_kg=1)], upstream=[("bus:data/CMP-02", 2.0)], symptoms={"F": "пуск невозможен"}),
   S1("WVL-03", "cell", "перегрев ячеек", ["thermal"], ["temp"], {"W": {"func": -30}}, "Температура ячеек выше допуска?", "bite", 1,
      [R("operator", "снизить темп пуска", 0.05)], upstream=[("bus:coolant/CLP-01", 3.0)], symptoms={"W": "ракеты на запрете по температуре"}),
  ])
C("weapon.mass_driver", "масс-драйвер ударников", "weapon", ["kern.md.impactor"],
  {"coil": "разгонные катушки", "feed": "подача ударников"},
  [S1("WMD-01", "coil", "износ разгонных катушек", ["wear", "thermal"], ["output_mw"], {"W": {"func": -30}, "F": {"func": -100}}, "Тестовый пуск: скорость ударника ниже нормы?", "load_test", 10,
      [R("nanobay", "перемотка катушек", 40, nanite_kg=15)], upstream=[("bus:coolant/CLP-01", 2.0)]),
   S1("WMD-02", "feed", "заклинивание подачи ударников", ["wear", "impact"], ["position"], {"F": {"func": -100}}, "BITE: прогон подачи", "bite", 3,
      [R("drone", "чистка подачи", 2)])])
C("armour", "броня корпуса", "armour", ["std.armour.ceramic", "bastion.armour.heavy"], {"plate": "броневая плита", "mount": "крепление плиты"},
  [S1("ARM-01", "plate", "пробоина / растрескивание плиты", ["impact", "laser"], ["seal"], {"W": {"func": -40}, "F": {"func": -100}}, "Дрон: осмотр плит после попадания", "drone_inspect", 20,
      [R("drone", "замена плиты из запаса", 2, spare="плита 1 т"), R("nanobay", "наплавка плиты", 15, nanite_kg=20), R("yard", "перебронирование", 24)],
      downstream=[("ARM-02", 100, "")], symptoms={"W": "защита в этом секторе ослаблена"}),
   S1("ARM-02", "mount", "ослабление крепления плиты", ["impact", "wear"], ["vibration"], {"C": {"func": -10}}, "Дрон: проверка креплений", "drone_inspect", 15,
      [R("drone", "подтяжка / замена крепежа", 1)], symptoms={"C": "плита дребезжит на манёврах"})])
C("passenger.cabin", "пассажирская / жилая секция", "passenger_module", ["agora.hab.pax100", "bharat.hab.bunk500", "ofu.cargo.p20", "std.cabin.hold6"],
  {"seal": "гермоуплотнения люков", "climate": "климат-установка секции", "power": "распределение питания секции"},
  [S1("PAX-01", "seal", "утечка воздуха из секции", ["wear", "impact"], ["cabin_press"], {"W": {"func": -30}, "F": {"func": -100}}, "Давление в секции падает при закрытых люках?", "bite", 15,
      [R("operator", "закрыть секцию, перевести людей", 0.2), R("drone", "замена уплотнения / заплатка", 2, spare="уплотнения")], downstream=[("LSS-02", 30, "")],
      symptoms={"W": "секцию приходится закрывать"}),
   S1("PAX-02", "climate", "отказ климат-установки секции", ["wear", "internal"], ["temp", "co2"], {"W": {"func": -20}}, "BITE: климат секции", "bite", 3,
      [R("nanobay", "ремонт климат-установки", 8, nanite_kg=2)], upstream=[("bus:coolant/CLP-01", 1.5)], symptoms={"W": "в секции жарко и душно - жалобы пассажиров"}),
   S1("PAX-03", "power", "отказ распределения питания секции", ["internal", "thermal"], ["bus_voltage"], {"F": {"func": -100}}, "Напряжение на вводе секции", "bite", 2,
      [R("drone", "замена распределителя", 2, spare="SRU распределитель")], upstream=[("bus:service_power/APU-02", 2.0)])])
C("passenger.anabiosis", "блок анабиоза", "passenger_module", ["uhuru.cryo.pod100"], {"cooler": "криокулер блока", "monitor": "мониторинг капсул"},
  [M("ANB-01", "cooler", "деградация криокулера блока", ["wear", "thermal"], ["temp"], {"C": {"power": 5}, "W": {"power": 20}, "F": {"func": -100}},
     [T(1, "Температура в капсулах растёт?", "bite", 5, fail=2), T(2, "Теплоноситель контура в норме?", "cross_check", 2, fail="->bus:coolant", ok="=ANB-01")],
     [R("nanobay", "восстановление криокулера", 20, nanite_kg=6), R("yard", "замена криокулера", 12)],
     upstream=[("bus:coolant/CLP-01", 3.0), ("bus:service_power/APU-01", 2.0)],
     symptoms={"W": "капсулы теплеют - обратный отсчёт до пробуждения", "F": "аварийное пробуждение людей (корабль к этому не готов)"}),
   S1("ANB-02", "monitor", "сбой мониторинга капсул (ложная тревога)", ["radiation", "internal"], ["data_err"], {"W": {}}, "Сверить датчики капсулы с ручным осмотром", "cross_check", 20,
      [R("operator", "перезапуск мониторинга", 0.1)], false_alarm=True, masks=["ANB-01"])])
C("lifeboat", "спасательная шлюпка / убежище", "lifeboat", ["std.surv.boat20", "std.surv.refuge100"],
  {"battery": "батарея", "release": "замки отстрела", "beacon": "маяк SOS"},
  [S1("LBT-01", "battery", "разряд батареи шлюпки", ["wear", "thermal"], ["battery"], {"W": {"func": -40}, "F": {"func": -100}}, "BITE: ёмкость батареи", "load_test", 10,
      [R("operator", "подзарядка от шины", 1)], upstream=[("bus:service_power/APU-02", 1.5)], pmcs="тест батарей шлюпок перед рейсом"),
   S1("LBT-02", "release", "заклинивание замков отстрела", ["wear", "impact"], ["latch"], {"F": {"func": -100}}, "PMCS: тест замков (без отстрела)", "bite", 2,
      [R("drone", "замена замка", 1.5, spare="SRU замок")], pmcs="тест замков перед рейсом"),
   S1("LBT-03", "beacon", "отказ маяка SOS шлюпки", ["radiation", "internal"], ["beacon"], {"F": {"func": -100}}, "Самотест маяка", "bite", 1,
      [R("nanobay", "замена маяка", 4, nanite_kg=0.5)])])
C("bay", "ангар / отсек катера / отсек дронов", "hangar", ["bastion.bay.hangar4", "harmony.bay.hangar2", "std.bay.boat", "std.bay.drone4"],
  {"doors": "створки", "cradle": "люльки и направляющие", "charger": "заправка / зарядка аппаратов"},
  [S1("BAY-01", "doors", "отказ привода створок", ["wear", "impact", "thermal"], ["position"], {"F": {"func": -100}}, "BITE: прогон створок", "bite", 3,
      [R("drone", "замена привода створок", 3, spare="SRU привод")], upstream=[("bus:data/CMP-02", 1.5)], symptoms={"F": "аппараты не выйти и не принять"}),
   S1("BAY-02", "cradle", "деформация люльки", ["impact"], ["position"], {"W": {"func": -50}}, "Дрон: осмотр люлек", "drone_inspect", 20,
      [R("drone", "правка / замена люльки", 3)]),
   S1("BAY-03", "charger", "отказ зарядки аппаратов", ["internal", "thermal"], ["current"], {"W": {"func": -40}}, "BITE: ток зарядки", "bite", 2,
      [R("nanobay", "замена модуля зарядки", 6, nanite_kg=1)], upstream=[("bus:service_power/APU-02", 2.0)])])
C("train_module", "модуль прыжкового поезда", "train_module", ["harmony.mod.cargo4800", "harmony.mod.tank4200"],
  {"coupler": "сцепка", "locks": "замки груза", "valve": "клапаны налива (цистерна)"},
  [S1("TRN-01", "coupler", "износ сцепки", ["wear", "impact"], ["latch", "vibration"], {"W": {"func": -30}, "F": {"func": -100}}, "Дрон: осмотр сцепки", "drone_inspect", 30,
      [R("drone", "замена пальцев сцепки", 4, spare="SRU сцепка")], pmcs="осмотр сцепок на каждой станции", symptoms={"F": "модуль нельзя брать в прыжок"}),
   S1("TRN-02", "locks", "отказ замков груза модуля", ["wear"], ["latch"], {"W": {"func": -20}}, "BITE: замки модуля", "bite", 2, [R("drone", "замена замков", 2)]),
   S1("TRN-03", "valve", "негерметичность клапана налива", ["wear", "thermal"], ["tank_press", "tank_level"], {"W": {}}, "Убыль груза цистерны?", "bite", 30,
      [R("drone", "замена клапана", 2, spare="SRU клапан")])])
C("bulk_hold", "грузовой трюм", "bulk_hold", ["std.cargo.hold100"], {"door": "грузовые ворота", "seal": "уплотнение"},
  [S1("BLK-01", "door", "заклинивание грузовых ворот", ["wear", "impact"], ["position"], {"F": {"func": -100}}, "BITE: прогон ворот", "bite", 3, [R("drone", "освобождение / замена привода", 2)]),
   S1("BLK-02", "seal", "износ уплотнения трюма", ["wear", "thermal"], ["seal"], {"W": {}}, "Тест давлением", "bite", 15, [R("drone", "замена уплотнения", 2)])])
C("cryo_tank", "криоцистерна", "cryo_tank", ["sol.cryo.t1000"], {"insulation": "многослойная изоляция", "cooler": "криокулер"},
  [S1("CRT-01", "insulation", "повреждение изоляции", ["impact", "laser", "wear"], ["temp", "tank_press"], {"W": {"power": 15}}, "Дрон: термограмма оболочки", "drone_inspect", 30,
      [R("drone", "заплатка изоляции", 3)], downstream=[("CRT-02", 50, "кулер работает без остановки")], symptoms={"W": "растёт выкипание груза"}),
   S1("CRT-02", "cooler", "износ криокулера", ["wear", "thermal"], ["temp", "current"], {"W": {"power": 20}, "F": {"func": -100}}, "BITE: холодопроизводительность", "load_test", 10,
      [R("nanobay", "восстановление криокулера", 25, nanite_kg=8)], upstream=[("bus:coolant/CLP-01", 2.0)], symptoms={"F": "груз выкипает, давление растёт"})])
C("workshop", "бортовая мастерская", "workshop", ["sol.workshop.w1"], {"fabricator": "станки и печать", "crane": "кран-балка"},
  [S1("WSH-01", "fabricator", "отказ станков", ["internal", "wear"], ["data_err"], {"W": {"func": -50}}, "Самотест станков", "bite", 5, [R("nanobay", "ремонт станков", 10, nanite_kg=3)],
      upstream=[("bus:service_power/APU-02", 2.0)]),
   S1("WSH-02", "crane", "отказ кран-балки", ["wear"], ["position"], {"W": {"func": -30}}, "Прогон крана", "bite", 2, [R("operator", "ремонт силами бригады", 2)])])
C("medbay", "медотсек", "medbay", ["std.med.bay"], {"autodoc": "автодок", "steril": "стерилизация"},
  [S1("MED-01", "autodoc", "сбой автодока", ["internal", "radiation"], ["data_err"], {"W": {"func": -50}}, "Самотест автодока", "bite", 5, [R("nanobay", "замена платы автодока", 6, nanite_kg=1)],
      upstream=[("bus:service_power/APU-02", 2.0)]),
   S1("MED-02", "steril", "отказ стерилизации", ["wear"], ["temp"], {"W": {"func": -20}}, "Контроль цикла стерилизации", "bite", 30, [R("operator", "ручная обработка", 1)])])
C("tow_rig", "буксирное устройство", "tow_rig", ["kern.tow.heavy", "kern.tow.raft20k"], {"clamp": "захваты", "winch": "лебёдка / привод", "frame": "силовая рама"},
  [S1("TOW-01", "clamp", "износ захватов", ["wear", "impact"], ["latch"], {"W": {"func": -30}, "F": {"func": -100}}, "Дрон: осмотр захватов", "drone_inspect", 20,
      [R("drone", "замена накладок захватов", 2, spare="SRU накладки")], symptoms={"W": "плот проскальзывает в захватах"}),
   S1("TOW-02", "winch", "отказ привода лебёдки", ["wear", "thermal"], ["position", "current"], {"F": {"func": -100}}, "BITE: прогон привода", "bite", 3,
      [R("drone", "замена привода", 3, spare="SRU привод")], upstream=[("bus:service_power/APU-02", 1.5)]),
   S1("TOW-03", "frame", "трещина силовой рамы", ["impact", "wear"], ["vibration"], {"W": {"func": -40}}, "Дрон: дефектоскопия рамы", "drone_inspect", 60,
      [R("nanobay", "наплавка рамы", 40, nanite_kg=30), R("yard", "замена рамы", 24)], upstream=[("TOW-01", 1.5)], symptoms={"W": "предел тяги буксира снижен"})])
C("mining_rig", "горное оборудование", "mining_rig", ["kern.mine.cutter"], {"head": "фрезерная головка", "crusher": "дробилка", "boom": "привод штанги"},
  [S1("MIN-01", "head", "износ фрезерной головки", ["wear", "impact"], ["vibration", "current"], {"C": {"func": -10}, "W": {"func": -40}}, "Ток привода фрезы растёт при той же подаче?", "load_test", 10,
      [R("drone", "замена резцов", 3, spare="резцы"), R("nanobay", "наплавка головки", 20, nanite_kg=15)], downstream=[("MIN-02", 30, "")]),
   S1("MIN-02", "crusher", "заклинивание дробилки", ["wear", "impact"], ["current"], {"F": {"func": -100}}, "BITE: прогон дробилки", "bite", 3, [R("drone", "чистка дробилки", 2)]),
   S1("MIN-03", "boom", "отказ привода штанги", ["wear", "thermal"], ["position"], {"F": {"func": -100}}, "BITE: прогон штанги", "bite", 3,
      [R("drone", "замена привода", 4)], upstream=[("bus:data/CMP-02", 1.5)])])
C("landing_gear", "посадочные опоры", "landing_gear", ["ofu.lift.gear"], {"actuator": "привод выпуска", "damper": "амортизатор"},
  [S1("LGR-01", "actuator", "отказ выпуска опор", ["wear", "thermal", "impact"], ["position"], {"F": {"func": -100}}, "BITE: выпуск / уборка опор", "bite", 3,
      [R("drone", "замена привода", 3, spare="SRU привод")], upstream=[("bus:data/CMP-02", 1.5)], pmcs="выпуск опор перед каждым спуском", symptoms={"F": "посадка невозможна"}),
   S1("LGR-02", "damper", "износ амортизатора", ["wear", "impact"], ["vibration"], {"W": {"func": -30}}, "Дрон: осмотр амортизаторов", "drone_inspect", 20,
      [R("drone", "замена амортизатора", 2)], symptoms={"W": "жёсткая посадка - удар по корпусу"})])
C("heat_shield", "теплозащитный экран", "heat_shield", ["ofu.lift.shield"], {"ablator": "абляционный слой", "tile": "плитки"},
  [S1("HTS-01", "ablator", "выгорание абляционного слоя", ["thermal", "wear"], ["temp"], {"C": {"func": -10}, "W": {"func": -40}, "F": {"func": -100}}, "Дрон: толщинометрия экрана", "drone_inspect", 40,
      [R("nanobay", "наплавка абляционного слоя", 30, nanite_kg=25), R("yard", "замена экрана", 12)], pmcs="толщинометрия после каждого входа в атмосферу",
      symptoms={"W": "вход в атмосферу опаснее"}),
   S1("HTS-02", "tile", "скол / трещина плитки", ["impact"], ["temp"], {"W": {"func": -30}}, "Дрон: осмотр плиток", "drone_inspect", 30, [R("drone", "замена плитки", 2, spare="плитки")])])
C("ecm", "комплекс РЭБ", "ecm", ["hyp.ecm.e1"], {"amp": "усилитель передатчика", "array": "излучающая решётка"},
  [S1("ECM-01", "amp", "перегрев усилителя", ["thermal", "internal"], ["temp", "tx_power"], {"W": {"func": -40, "heat": 20}, "F": {"func": -100}}, "BITE: мощность на нагрузке", "load_test", 5,
      [R("nanobay", "замена усилителя", 10, nanite_kg=2)], upstream=[("bus:coolant/CLP-01", 2.5)], downstream=[("bus:service_power/APU-02", 20, "работа на полной мощности")]),
   S1("ECM-02", "array", "повреждение излучающей решётки", ["impact", "laser"], ["tx_power"], {"W": {"func": -30}}, "Самотест решётки", "bite", 3, [R("drone", "замена модулей решётки", 3)])])
C("sensor_mast", "сенсорная мачта", "sensor_mast", ["hyp.mast.s1"], {"actuator": "привод выдвижения", "optics": "оптика и апертуры"},
  [S1("MST-01", "actuator", "заклинивание выдвижения мачты", ["wear", "thermal", "impact"], ["position"], {"F": {"func": -100}}, "BITE: прогон выдвижения", "bite", 3,
      [R("drone", "освобождение / замена привода", 2)], upstream=[("bus:data/CMP-02", 1.5)]),
   S1("MST-02", "optics", "загрязнение оптики", ["wear", "radiation"], ["range"], {"C": {"func": -10}, "W": {"func": -30}}, "Дрон: осмотр апертур", "drone_inspect", 15,
      [R("drone", "очистка апертур", 0.5)], symptoms={"W": "дальность пассивного обнаружения падает"})])

# notes per model (maker / variant specifics in a technology class)
PART_NOTES = {
    "kb.drive.cr1-vector": "CR-1 Vector: подвес ±75° - DRV-05 критичнее, чем у осевых двигателей",
    "kb.drive.n1-axial": "N1 Axial: подвес ±4° - отказ подвеса почти не мешает полёту",
    "ofu.drive.u73l": "Урал-73Л (подъёмный): работает в атмосфере - пятак и сопло изнашиваются быстрее",
    "th.drive.tz16v": "ТЦ-1.6В - военная поставка: бронированный пятак, ресурс между осмотрами меньше",
    "th.drive.tz28": "ТЦ-28В (тягач поезда): большой пятак, замена секций только на верфи Пакта",
    "kern.drive.kt1": "KT-1 (буксир): долгие режимы малой тяги - главный износ у форсунок",
    "um.core.z05": "запечатан «Нулевой точкой» (Когниум): обслуживание только у изготовителя / лицензиата",
    "um.core.z1": "запечатан «Нулевой точкой» (Когниум): обслуживание только у изготовителя / лицензиата",
    "um.core.z5": "запечатан «Нулевой точкой» (Когниум): обслуживание только у изготовителя / лицензиата",
    "harmony.core.h5": "собран Гармонией из запечатанных блоков Когниума: блоки - только «Нулевая точка», сборка - верфь Гармонии",
    "harmony.core.h12": "собран Гармонией из запечатанных блоков Когниума: блоки - только «Нулевая точка», сборка - верфь Гармонии",
    "harmony.core.h80": "ядро тягача поезда; собран Гармонией из запечатанных блоков Когниума",
    "kapehu.core.k5": "своя математика Капеху (вне стандарта Когниума): обслуживание только Kāpehu Systems",
    "kapehu.core.k12": "своя математика Капеху (вне стандарта Когниума): обслуживание только Kāpehu Systems",
    "bastion.core.b12": "военное ядро Бастиона: обслуживание только на верфи Mjølner, не экспортируется",
    "srw.reactor.sr48": "Sol Reactor Works: серия на Луне, опытные образцы - Канаверал",
    "th.reactor.tr26v": "ТР-2.6В - военная поставка",
    "uhuru.ls.u500": "биорегенерация Uhuru: поглотитель - живая культура, восстанавливается сама, но гибнет от перегрева",
}
for _c in CLASSES:
    _c["part_notes"] = {pid: PART_NOTES[pid] for pid in _c["applies_to"] if pid in PART_NOTES}


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
    "drive.plasma": {"injector": "IJ", "coil": "L", "chamber": "MP", "pad": "PT", "hx": "HX", "gimbal": "MP", "busbar": "W", "sensor": "B"},
    "reactor.fusion": {"pad": "PT", "plasma_ctl": "A", "damper": "MP", "feed": "MP", "inverter": "PS", "sensor": "B"},
    "apu.fusion": {"cells": "BT", "starter": "MP", "regulator": "PS", "sensor": "B"},
    "radiator.panel": {"panel": "RD", "hinge": "MP", "manifold": "WP"},
    "coolant.loop": {"pump": "PU", "pipes": "WP", "valves": "VL", "fluid": "TK"},
    "jump_core": {"field": "L", "damper": "MP", "seal": "MP"},
    "tank.pellets": {"auger": "MP", "level": "B", "shell": "TK"},
    "tank.jump_charge": {"valve": "VL", "shell": "TK"},
    "tank.argon": {"regulator": "VL", "shell": "TK"},
    "rcs.thruster": {"valve": "VL", "coil": "L", "nozzle": "MP"},
    "computer": {"board": "A", "memory": "A", "psu": "PS"},
    "radar": {"array": "A", "rx": "A"},
    "comm": {"feeder": "W", "transceiver": "A"},
    "transponder.um": {"coder": "A"},
    "life_support": {"scrubber": "FL", "o2": "G", "water": "FL", "fan": "MP"},
    "escape_capsule": {"pyro": "E", "battery": "BT", "beacon": "CM", "seal": "MP"},
    "cargo.c20lock": {"latch": "MP", "rail": "MP"},
    "dock.collar": {"seal": "MP", "latch": "MP"},
    "weapon.pd": {"emitter": "WE", "mount": "MP"},
    "weapon.coilgun": {"coil": "L", "caps": "BT", "feed": "MP", "mount": "MP"},
    "weapon.vls": {"hatch": "MP", "ignition": "E", "cell": "HX"},
    "weapon.mass_driver": {"coil": "L", "feed": "MP"},
    "armour": {"plate": "MP", "mount": "MP"},
    "passenger.cabin": {"seal": "MP", "climate": "HX", "power": "PS"},
    "passenger.anabiosis": {"cooler": "PU", "monitor": "A"},
    "lifeboat": {"battery": "BT", "release": "MP", "beacon": "CM"},
    "bay": {"doors": "MP", "cradle": "MP", "charger": "PS"},
    "train_module": {"coupler": "MP", "locks": "MP", "valve": "VL"},
    "bulk_hold": {"door": "MP", "seal": "MP"},
    "cryo_tank": {"insulation": "MP", "cooler": "PU"},
    "workshop": {"fabricator": "A", "crane": "MP"},
    "medbay": {"autodoc": "A", "steril": "FL"},
    "tow_rig": {"clamp": "MP", "winch": "MP", "frame": "MP"},
    "mining_rig": {"head": "MP", "crusher": "MP", "boom": "MP"},
    "landing_gear": {"actuator": "MP", "damper": "MP"},
    "heat_shield": {"ablator": "MP", "tile": "MP"},
    "ecm": {"amp": "A", "array": "A"},
    "sensor_mast": {"actuator": "MP", "optics": "B"},
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
    # every ship of the reference (canon passports + the reference proposals): data.js
    with open(os.path.join(ROOT, "Notes", "html", "ship_reference", "data.js"), encoding="utf-8") as f:
        txt = f.read()
        ships = json.loads(txt[txt.index("=") + 1:].rstrip().rstrip(";"))["ships"]
    cls_of = class_of_part()
    graphs = [ship_graph(s, cls_of) for s in ships]
    covered = [g for g in graphs if g["installed"]]
    cobra = [g for g in graphs if g["id"].startswith("kb.ship.cobrett-i")]
    for g in graphs:
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
