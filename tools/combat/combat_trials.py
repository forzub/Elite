#!/usr/bin/env python3
"""Combat trials (author 2026-11-10: "fly the ship, stage a good firefight with rams, see how the hull and everything
behaves, estimate whether it is fun and how survivable the ships are, collect data to tune the balance").

Each trial launches the game (build/EliteGame.exe --dev-direct-flight --diagnostic-scenario nav-stress:simple) with a
DEV script: a Cobra target is spawned ahead of the player, then the player fires (lasers / missiles) and / or the
target fights back (ELITE_DEV_TARGET_FIGHTS=duel|ram) with ship-ship contacts (ELITE_DEV_SHIP_COLLISIONS=1). The server
writes every applied damage portion to a CSV (ELITE_DEV_COMBAT_LOG). The report docs/COMBAT_TRIALS.md gives per trial:
damage dealt per side, modules lost over time and the milestones (first module lost, lasers gone, cockpit gone, main
engines gone, propulsion reactors gone), and the ram events.

usage: python tools/combat/combat_trials.py [trial-name ...] [--reanalyse]   (all trials without arguments; ~15 min)
"""
import csv
import json
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
BUILD = os.path.join(ROOT, "build")
TMP = "D:/tmp/elite_combat"
DOC = os.path.join(ROOT, "docs", "COMBAT_TRIALS.md")
MINGW = "C:/msys64/mingw64/bin"

# script verbs: t:verb (space-state seconds); target:<m> spawns the Cobra target ahead
TRIALS = [
    # lasers (author 2026-11-10 line-up, catalog order: Pulsar, Discharger, Squall, Stiletto, Cutter)
    {"name": "pulse_laser", "title": "Пульсар 2,5 МВт (по умолчанию, импульсный) против неподвижной Cobra, 400 м",
     "script": "6:target:400;9:fire:on", "seconds": 150, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "discharger", "title": "Разрядник 60 МДж (конденсатор: редкий мощный выстрел), 400 м",
     "script": "6:target:400;7:laser:next;9:fire:on", "seconds": 150, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "squall", "title": "Шквал 8 МВт (непрерывный, сильно греет), 400 м",
     "script": "6:target:400;7:laser:next;7.5:laser:next;9:fire:on", "seconds": 150, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "stiletto", "title": "Стилет 1,2 МВт (дальний узкий), 400 м",
     "script": "6:target:400;7:laser:next;7.3:laser:next;7.6:laser:next;9:fire:on", "seconds": 120, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "cutter", "title": "Резак 15 МВт (ближний широкий), 400 м",
     "script": "6:target:400;7:laser:next;7.2:laser:next;7.4:laser:next;7.6:laser:next;9:fire:on", "seconds": 120, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "missiles_needle", "title": "4 ракеты «Игла» (кумулятивные 30 МДж) по неподвижной Cobra, 1500 м",
     "script": "6:target:1500;9:missile;15:missile;21:missile;27:missile", "seconds": 60, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "missiles_mixed", "title": "«Термит» 90 МДж + «Лом» (кинетика 40 кг) по неподвижной Cobra, 1500 м",
     "script": "6:target:1500;7:missile:next;9:missile;14:missile:next;16:missile;22:missile", "seconds": 60, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "duel_pulse", "title": "Дуэль: обе Cobra с Пульсарами; мишень отвечает сама (штатный боевой ИИ: 1,5 км, ведение прицела, ракеты)",
     "script": "6:target:400;9:fire:on", "seconds": 180, "env": {}},
    {"name": "reactor_blast", "title": "Взрыв маршевого реактора мишени (DEV-триггер), корабль игрока в 150 м",
     "script": "6:target:150", "seconds": 40, "env": {"ELITE_DEV_TARGET_PASSIVE": "1", "ELITE_DEV_REACTOR_EXPLODE": "8"}},
    {"name": "discharger_kill", "title": "Разрядник до гибели реакторов мишени (естественный жребий взрыва 35 %)",
     "script": "6:target:400;7:laser:next;9:fire:on", "seconds": 240, "env": {"ELITE_DEV_TARGET_PASSIVE": "1"}},
    {"name": "ram_90", "title": "Таран: мишень идёт на корабль игрока на 90 м/с (повторные заходы)",
     "script": "6:target:900", "seconds": 90,
     "env": {"ELITE_DEV_TARGET_FIGHTS": "ram", "ELITE_DEV_SHIP_COLLISIONS": "1", "ELITE_DEV_RAM_SPEED": "90"}},
    {"name": "ram_30", "title": "Таран: 30 м/с (толчок / абордажная скорость)",
     "script": "6:target:600", "seconds": 90,
     "env": {"ELITE_DEV_TARGET_FIGHTS": "ram", "ELITE_DEV_SHIP_COLLISIONS": "1", "ELITE_DEV_RAM_SPEED": "30"}},
    {"name": "ram_and_fire", "title": "Таран 60 м/с под огнём: игрок стреляет Шквалом в идущую на таран Cobra",
     "script": "6:target:1500;7:laser:next;7.5:laser:next;9:fire:on", "seconds": 90,
     "env": {"ELITE_DEV_TARGET_FIGHTS": "ram", "ELITE_DEV_SHIP_COLLISIONS": "1", "ELITE_DEV_RAM_SPEED": "60"}},
]

MILESTONES = [
    ("первый модуль", lambda k: True),
    ("лазеры", lambda k: "laser" in k),
    ("кабина", lambda k: "cockpit" in k),
    ("маршевые двигатели", lambda k: re.search(r"ship_(fore_)?engine_[LR]$", k) is not None),
    ("реакторы ER-5", lambda k: k.endswith("_reactor") and "service" not in k and "frame" not in k),
]


def run(trial):
    os.makedirs(TMP, exist_ok=True)
    csv_path = f"{TMP}/{trial['name']}.csv"
    log_path = f"{TMP}/{trial['name']}.txt"
    env = dict(os.environ)
    env["PATH"] = MINGW + os.pathsep + env.get("PATH", "")
    env.update({"ELITE_DEV_COMBAT_LOG": csv_path, "ELITE_DEV_SOL_MAP_SCRIPT": trial["script"],
                "ELITE_DEV_PLAYER_TRACK": "1"})   # the pilot keeps the crosshair on the target
    env.update(trial["env"])
    with open(log_path, "w", encoding="utf-8", errors="replace") as log:
        try:
            subprocess.run([os.path.join(BUILD, "EliteGame.exe"), "--dev-direct-flight", "--diagnostic-scenario",
                            "nav-stress:simple"], cwd=BUILD, env=env, stdout=log, stderr=subprocess.STDOUT,
                           timeout=trial["seconds"] + 25)
        except subprocess.TimeoutExpired:
            subprocess.run(["taskkill", "/F", "/IM", "EliteGame.exe"], capture_output=True)
    return csv_path, log_path


def analyse(trial, csv_path, log_path):
    rows = []
    if os.path.exists(csv_path):
        rows = list(csv.DictReader(open(csv_path, encoding="utf-8")))
    text = open(log_path, encoding="utf-8", errors="replace").read() if os.path.exists(log_path) else ""
    player = re.search(r"target Cobra id=(\d+) [\d.]+ m ahead of ship (\d+)", text)
    target_id, player_id = (player.group(1), player.group(2)) if player else (None, None)
    if not player and rows:   # the game log can be cut by the timeout kill: the ids from the damage log (player < target)
        ids = sorted({r["target"] for r in rows}, key=int)
        player_id, target_id = (ids[0], ids[-1]) if len(ids) > 1 else (None, ids[0])
    t0 = None
    sides = {}
    for who, sid in (("мишень", target_id), ("игрок", player_id)):
        rs = [r for r in rows if r["target"] == sid]
        if not rs:
            continue
        t0 = min(float(r["t_s"]) for r in rows) if t0 is None else t0
        dealt = sum(float(r["loss"]) for r in rs)
        destroyed = [(float(r["t_s"]), r["key"].split("::", 1)[-1]) for r in rs if r["destroyed"] == "1"]
        miles = {}
        for name, pred in MILESTONES:
            hits = [t for t, k in destroyed if pred(k)]
            miles[name] = round(min(hits) - t0, 1) if hits else None
        last = rs[-1]
        sides[who] = {"dealt": dealt, "destroyed": destroyed, "miles": miles, "rows": len(rs),
                      "attached": f"{last['modules_attached']}/{last['modules_total']}",
                      "span": round(float(rs[-1]["t_s"]) - t0, 1)}
    blasts = re.findall(r"\[Explosion\] ship (\d+): REACTOR (\S+) EXPLODED \((.*?)\), ([\d.]+) MJ: flash on (\d+) modules, (\d+)/(\d+) fragments hit the own hull, (\d+) hit other ships", text)
    rams = re.findall(r"\[Ram\] ships (\d+) x (\d+): closing ([\d.]+) m/s, masses ([\d.]+) / ([\d.]+) t, dissipated ([\d.]+) MJ", text)
    burns = len(re.findall(r"BURN-THROUGH", text))
    hits = len(re.findall(r"missile \d+ HIT ship", text))
    misses = len(re.findall(r"self-destruct \(no hit\)|passed the target", text))
    return {"sides": sides, "rams": rams, "blasts": blasts, "out": re.findall(r"ship (\d+): OUT OF THE FIGHT - ([^;]+); crew ([^(]+)\(combat clock ([\d.]+)", text), "burns": burns, "missile_hits": hits, "missile_misses": misses,
            "t0": t0}


def report(results):
    out = ["# Боевые прогоны (DEV, 2026-11-10)", "",
           "Автор: «погонять корабль, устроить перестрелку с таранами, посмотреть, как ведёт себя корпус, насколько",
           "корабли живучие, собрать данные для баланса». Прогоны: `tools/combat/combat_trials.py` (игра на этой машине,",
           "сцена nav-stress:simple у орбитального хаба Земли, Cobra против Cobra-мишени). Урон: журнал сервера",
           "ELITE_DEV_COMBAT_LOG (каждая порция: модуль / связь, потеря здоровья, разрушен ли). Здоровье = поглощённые МДж x 2",
           "(weapons.json health_per_mj).", "",
           "Режимы, которых в игре нет и которые сделаны только для прогонов (за флагами, игра по умолчанию не меняется):",
           "- ответный огонь мишени (ELITE_DEV_TARGET_FIGHTS=duel|ram): поворот 40-60 град/с к игроку, дуэль на 1,5 км или таран;",
           "- столкновения корабль-корабль (ELITE_DEV_SHIP_COLLISIONS=1): сферы по габариту, неупругий удар e = 0,2, рассеянная",
           "  энергия идёт в урон модулям обоих кораблей в 4 м от точки контакта.", ""]
    for trial, res in results:
        out += [f"## {trial['title']}", f"`{trial['name']}`, {trial['seconds']} с.", ""]
        if not res["sides"] and not res["rams"]:
            out += ["Урона нет (проверить журнал прогона).", ""]
            continue
        out += ["| Сторона | Урон получен (ед. здоровья) | Порций | Модулей цело в конце | Разрушено модулей | "
                + " | ".join(f"t: {m[0]}, с" for m in MILESTONES) + " |",
                "|---|---|---|---|---|" + "---|" * len(MILESTONES)]
        for who, sd in res["sides"].items():
            ms = " | ".join("-" if sd["miles"][m[0]] is None else str(sd["miles"][m[0]]) for m in MILESTONES)
            out.append(f"| {who} | {sd['dealt']:.0f} | {sd['rows']} | {sd['attached']} | {len(sd['destroyed'])} | {ms} |")
        out.append("")
        for who, sd in res["sides"].items():
            if sd["destroyed"]:
                out.append(f"- {who}: разрушены по порядку - " + ", ".join(f"{k} ({t - res['t0']:.0f} с)" for t, k in sd["destroyed"][:14])
                           + (" ..." if len(sd["destroyed"]) > 14 else ""))
        extra = []
        if res["burns"]:
            extra.append(f"прожигов (burn-through): {res['burns']}")
        if res["missile_hits"] or res["missile_misses"]:
            extra.append(f"ракеты: попаданий {res['missile_hits']}, промахов {res['missile_misses']}")
        if res["rams"]:
            mj = [float(r[5]) for r in res["rams"]]
            sp = [float(r[2]) for r in res["rams"]]
            extra.append(f"таранов: {len(res['rams'])}, скорость сближения {min(sp):.0f}-{max(sp):.0f} м/с, "
                         f"рассеяно {min(mj):.1f}-{max(mj):.1f} МДж за удар (масса {res['rams'][0][3]} / {res['rams'][0][4]} т)")
        for b in res.get("blasts", []):
            extra.append(f"ВЗРЫВ реактора {b[1]} ({b[2]}): {b[3]} МДж, вспышка по {b[4]} модулям, осколков в свой корпус {b[5]}/{b[6]}, в другие корабли {b[7]}")
        for o in res.get("out", []):
            extra.append(f"корабль {o[0]} выведен из строя на {float(o[3]) - (res['t0'] or 0):.0f} с: {o[1]}, экипаж {o[2].strip()}")
        if extra:
            out.append("- " + "; ".join(extra))
        out.append("")
    open(DOC, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print("report ->", DOC)


def main():
    names = [a for a in sys.argv[1:] if not a.startswith("--")]
    reanalyse = "--reanalyse" in sys.argv     # rebuild the report from the stored logs, no game runs
    results = []
    for t in TRIALS:
        if names and t["name"] not in names:
            continue
        print("trial", t["name"], flush=True)
        c, l = (f"{TMP}/{t['name']}.csv", f"{TMP}/{t['name']}.txt") if reanalyse else run(t)
        res = analyse(t, c, l)
        json.dump(res, open(f"{TMP}/{t['name']}.json", "w", encoding="utf-8"), ensure_ascii=False)
    # the report covers every trial with a stored result (a partial rerun keeps the others)
    for t in TRIALS:
        path = f"{TMP}/{t['name']}.json"
        if os.path.exists(path):
            results.append((t, json.load(open(path, encoding="utf-8"))))
    report(results)


if __name__ == "__main__":
    main()
