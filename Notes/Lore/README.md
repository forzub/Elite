# Notes/Lore — справочник лора 3126

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

## Назначение

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
- `AUTHOR TRUTH` не цитировать вне `AUTHOR/`; `UNKNOWN` не заполнять ответом.

## Счётчики статусов машинного канона

<!-- GENERATED:counts BEGIN -->
`AUTHOR TRUTH` 18, `CANON` 309, `PROPOSAL` 291, `PUBLIC` 591, `UNKNOWN` 9. Схема данных: `elite.colonization_lab.lore_facts.v1`, seed 3126, сценарий `baseline`.
<!-- GENERATED:counts END -->

## Оглавление

<!-- GENERATED:index BEGIN -->
| Папка | Файлов | Файлы |
|---|---|---|
| [AUTHOR/](AUTHOR/HIDDEN_CANON.md) | 4 | [HIDDEN_CANON.md](AUTHOR/HIDDEN_CANON.md), [MISSING_SHIPS.md](AUTHOR/MISSING_SHIPS.md), [PROPOSALS.md](AUTHOR/PROPOSALS.md), [QUEST_TRUTHS.md](AUTHOR/QUEST_TRUTHS.md); AUTHOR/SHIPS/ — 1 скрыт. досье [СПОЙЛЕР] |
| [COLONIES/](COLONIES/INDEX.md) | 35 | см. INDEX.md |
| [DEMOGRAPHY/](DEMOGRAPHY/MODEL.md) | 2 | [MODEL.md](DEMOGRAPHY/MODEL.md), [POPULATION_3126.md](DEMOGRAPHY/POPULATION_3126.md) |
| [ECONOMY/](ECONOMY/BEACONS_AND_NAVIGATION.md) | 15 | см. INDEX.md |
| [FRONTIER/](FRONTIER/FRONTIERS_AND_DEEP_SPACE.md) | 2 | [FRONTIERS_AND_DEEP_SPACE.md](FRONTIER/FRONTIERS_AND_DEEP_SPACE.md), [WORLDS_ASS.md](FRONTIER/WORLDS_ASS.md) |
| [POLITICS/](POLITICS/EARTH_3126.md) | 4 | [EARTH_3126.md](POLITICS/EARTH_3126.md), [LAW_AND_JURISDICTION.md](POLITICS/LAW_AND_JURISDICTION.md), [RELATIONS.md](POLITICS/RELATIONS.md), [WARS.md](POLITICS/WARS.md) |
| [SETTLEMENTS/](SETTLEMENTS/INDEX.md) | 26 | см. INDEX.md |
| [SHIPS/](SHIPS/INDEX.md) | 36 | см. INDEX.md |
| [TIMELINE/](TIMELINE/EARTH_AND_EXODUS.md) | 3 | [EARTH_AND_EXODUS.md](TIMELINE/EARTH_AND_EXODUS.md), [HYPERDRIVE_TIMELINE.md](TIMELINE/HYPERDRIVE_TIMELINE.md), [MASTER_TIMELINE.md](TIMELINE/MASTER_TIMELINE.md) |
<!-- GENERATED:index END -->

## Пробелы данных

<!-- GENERATED:gaps BEGIN -->
- Корабли: в каноне нет полей размеров, двигателя, энергетики и груза — только заявленная масса (досье) и расчёт physics.py; у полей когорт, мотивов, политики рождений нет собственного статуса (наследуется от миссии).
- Нет расчёта physics.py: IRON-SHIELD-2, LEGACY-1, HOMEWARD-1, HYPERION-1.
- Нет прибытия: UBUNTU-1, NIRVANA-1, IRON-SHIELD-2, NOMAD-1, FINAL-ARK-1, LEGACY-1, DEEP-SEEK-1, KEPLER-1, HOMEWARD-1, FAREWELL-1, HYPERION-1.
- Цель вне каталога или не задана: NOMAD-1, KEPLER-1.
- Скрытые миссии (AUTHOR TRUTH, 1) в этот список не входят — см. AUTHOR/MISSING_SHIPS.md.
- Единицы без экономики 3126: `barnard_post`, `eps_indi_field`, `gj357_frontier`, `gj667_ubuntu_node`, `gj674_frontier`, `gj832_gaia_zone`, `gj892_kern`, `hd10360_relay`, `hd104067_frontier`, `hd114783_claims`, `hd85512_frontier`, `leo83_golden_city`, `sigma2uma_expedition`.
- Единицы без контрольных точек модели: нет.
- Единицы без авторского населения: `gj357_frontier`, `gj674_frontier`, `hd104067_frontier`, `hd85512_frontier`.
- Единицы без режима 3126: `barnard_post`, `eps_indi_field`, `gj357_frontier`, `gj667_ubuntu_node`, `gj674_frontier`, `gj892_kern`, `hd10360_relay`, `hd104067_frontier`, `hd114783_claims`, `hd85512_frontier`, `leo83_golden_city`, `sigma2uma_expedition`.
- Поселения без населения (модель не распределила): нет.
- Поселения: нет года основания (кроме столиц — год политии) и состояния 3126 (кроме промышленных площадок с выпуском).
- Верфи: нет классов кораблей и разделения ремонт / военные / гражданские; есть только тип актива и выпуск корпусов.
<!-- GENERATED:gaps END -->
