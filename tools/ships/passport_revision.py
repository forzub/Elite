#!/usr/bin/env python3
"""Passport revision report (author 2026-11-10: revise the passports from the ship designs; passports stay NOT canon until
the ship models are approved). Compares the content-driven sketch size (tools/ships/auto_sketch.py ->
assets/models/layout_sketches/sketch_sizes.json; the Cobrett-I from its hand-made sketch) with the passport frame
envelope and writes docs/SHIP_PASSPORT_REVISION.md with a proposed frame per ship:
  * equipment does not fit (sketch longer / taller than the passport, at the passport width) -> grow the frame;
  * the passport is much bigger than its equipment (volume ratio < 0.35) -> shrink, or the free volume is a declared
    reserve (payload growth, crew space) - author's call;
  * otherwise keep.
usage: python tools/ships/passport_revision.py
"""
import json
import os

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SIZES = os.path.join(ROOT, "assets", "models", "layout_sketches", "sketch_sizes.json")
DOC = os.path.join(ROOT, "docs", "SHIP_PASSPORT_REVISION.md")


def main():
    r = json.load(open(SIZES, encoding="utf-8"))
    r["kb.ship.cobrett-i.frnu-inta.a3"] = {"principle": "monolithic (hand-made sketch v3)", "sketch_size_LWH_m": [25.6, 25.2, 3.5],
                                         "passport_frame_LWH_m": [22.2, 26, 5]}
    rows = []
    for sid, v in sorted(r.items()):
        s, p = v["sketch_size_LWH_m"], v["passport_frame_LWH_m"] or [0, 0, 0]
        vol_s, vol_p = s[0] * s[1] * s[2], max(1e-6, p[0] * p[1] * p[2])
        ratio = vol_s / vol_p
        if s[0] > p[0] * 1.15 or s[2] > p[2] * 1.1:
            verdict = "увеличить: оборудование не помещается"
            prop = [max(s[0], p[0]), max(s[1], p[1]), max(s[2], p[2])]
        elif ratio < 0.35:
            verdict = "уменьшить или объявить резерв объёма (решение автора)"
            prop = [round(max(s[0], p[0] * 0.6), 1), p[1], round(max(s[2], p[2] * 0.6), 1)]
        else:
            verdict = "оставить"
            prop = p
        rows.append((sid, v["principle"], p, s, round(ratio, 2), verdict, prop))
    grow = sum(1 for x in rows if x[5].startswith("увеличить"))
    shrink = sum(1 for x in rows if x[5].startswith("уменьшить"))
    out = ["# Пересмотр паспортов кораблей по эскизам компоновки (PROPOSAL, 2026-11-10)", "",
           "Паспорта не канон до утверждения моделей кораблей (автор). Размер «по эскизу» - габарит, который получился, когда",
           "реальные модели оборудования разложены по отсекам (функциональные группы) по принципу производителя",
           "(`tools/ships/auto_sketch.py`; Cobrett-I - ручной эскиз v3). Эскизы грубые: упаковка плотная, без коридоров и",
           "резерва; итоговый габарит корпуса задаёт дизайн корабля, эта таблица - ориентир.", "",
           f"Итого: {len(rows)} кораблей, увеличить - {grow}, уменьшить или объявить резерв - {shrink}, оставить - {len(rows) - grow - shrink}.", "",
           "| Корабль | Принцип | Паспорт Д×Ш×В, м | Эскиз Д×Ш×В, м | Объём эскиз / паспорт | Вывод | Предложение рамы, м |",
           "|---|---|---|---|---|---|---|"]
    fmt = lambda a: " × ".join(f"{x:g}" for x in a)
    for sid, pr, p, s, ratio, verdict, prop in rows:
        out.append(f"| {sid} | {pr} | {fmt(p)} | {fmt(s)} | {ratio} | {verdict} | {fmt(prop)} |")
    out += ["", "Заметки:",
            "- Малые корабли (перехватчики B-4 / тип 415, патрульные PC-30, катера TS-6) не вмещают своё оборудование в",
            "  паспортные 4-10 м высоты: либо корпус растёт, либо им нужны компактные агрегаты (как отсек Cobra Ø3,5 м).",
            "- Большие грузовые и пассажирские корабли по эскизу в 2-5 раз меньше по объёму: паспортный объём - это грузовые",
            "  и жилые объёмы, которых в эскизе нет (только агрегаты и контейнеры); резерв надо объявить явно.",
            "- Cobrett-I: эскиз v3 25,6 × 25,2 × 3,5 м против паспорта 22,2 × 26 × 5 - длина +15 %, высота -30 % (отсек Ø3,5 м);",
            "  с гондолами излучателей ZP-FE1 на законцовках габаритная ширина ~26,9 м.", "",
            "## Что войдёт в составы (BOM) при утверждении паспортов",
            "Принятые автором изделия (2026-11-10) есть в каноне каталога, но паспорта кораблей ещё не канон, поэтому в BOM их пока нет:",
            "- все корабли с ядром прыжка: пара / 2 пары / 4 пары излучателей поля завода ядра (ZP-FE1, He-FE8, MJ-FE2, K-FE1)",
            "  по длине корпуса (до ~40 м / до ~150 м / больше);",
            "- Cobrett-I A3: 2 × FL-2, 2 × WR-2 (4 ракеты Aspid-10), 4 × LS-1, кабина EC-2 вместо капсулы, RM-2 и RD-1;",
            "  военная версия: 4 × FL-3H;",
            "- B-90 и другие крупные военные корабли Бастиона: кассеты MJ-BK24 с MJ-K3;",
            "- ремонтный дрон RM-2 - всем кораблям с отсеком дронов, разведдрон RD-1 - курьерам, пикетам и эскортам",
            "  (docs/FLEET_DOCTRINE.md); LM-MP8 - малым кораблям Сола вместо / вместе с курсовыми лазерами."]
    open(DOC, "w", encoding="utf-8", newline="\n").write("\n".join(out) + "\n")
    print(f"{len(rows)} ships: grow {grow}, shrink/reserve {shrink} -> {DOC}")


if __name__ == "__main__":
    main()
