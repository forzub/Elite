# GENESIS-1 — GENESIS-1

<!-- GENERATED:legend BEGIN -->
> **Статусы строк:** `CANON` — есть в заметках автора (generation ships/*, fate.md, history.md, каталог); `PROPOSAL` — добавлено или пересчитано Claude / результат модели; `UNKNOWN` — намеренно без ответа, даже для автора. **Видимость:** `PUBLIC` — мир 3126 это знает или может знать; `AUTHOR TRUTH` — скрытая правда автора.
>
> Блоки `GENERATED` пишет только `tools/colonization_lab/lore_md.py` из машинного канона; текст вне них — ручной и сохраняется.
<!-- GENERATED:legend END -->

## Общие данные

<!-- GENERATED:summary BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| ID / имена | GENESIS-1, SpaceArk, GENESIS-2 (merged) | PROPOSAL · PUBLIC |
| Волна | I | PROPOSAL · PUBLIC |
| Спонсор | SpaceArk consortium: Singapore + Shanghai diaspora capital + Pacific tech, engineers who left both Beijing and Washington | PROPOSAL · PUBLIC |
| Старт | 2120, Sol | PROPOSAL · PUBLIC |
| Цель | Tau Ceti | PROPOSAL · PUBLIC |
| Прибытие | 2263 | PROPOSAL · PUBLIC |
| Длительность полёта, лет | 143 | PROPOSAL · PUBLIC |
| Колония | [Новый Шанхай](../COLONIES/tau_ceti_cognium.md) | CANON · PUBLIC |
| Источники | generation_ships.json (GENESIS-1, GENESIS-2); 06. CELESTIAL-DESTINY-1.md (упоминание); actors.json Когниум | — |
<!-- GENERATED:summary END -->

## Причины полёта

<!-- GENERATED:motives BEGIN -->
Машинная разметка досье (у этих полей нет собственного статуса; статус унаследован от миссии: `PROPOSAL · PUBLIC`).

| Мотив | Вес |
|---|---|
| elite_capital_flight | 0.30 |
| rival_claim_fear | 0.20 |
| civilisational_insurance | 0.15 |
| scientific_vanity | 0.15 |
| name_in_history | 0.10 |
| stupid | 0.10 |

Культуры набора: chinese_diaspora 0.60, american_frontier 0.25, malay_indonesian 0.15.

Чёрная комедия миссии: Консорциум купил у Мэйфлауэра «право второго причала» — по документу, который американский Совет Инвесторов считал шуткой.
<!-- GENERATED:motives END -->

### Почему полетели (рассказ)

<!-- NARRATIVE:causes -->
_TODO: кто и зачем снарядил корабль, общественный фон эпохи старта, что обещали людям; опираться на мотивы выше и на досье из «Источники»._

## Корабль

<!-- GENERATED:ship BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Заявленная масса, Мт (полная / сухая) | 22 / 18 | PROPOSAL · PUBLIC (досье) |
| Эра двигателя | I | PROPOSAL · PUBLIC (модель) |
| Крейсерская скорость, c | 0.090 | PROPOSAL · PUBLIC (модель) |
| Расчётная масса, Мт (полная / сухая) | 55.7 / 4.0 | PROPOSAL · PUBLIC (модель) |
| Отношение масс | 13.8 | PROPOSAL · PUBLIC (модель) |
| Δv, c | 0.105 | PROPOSAL · PUBLIC (модель) |
| Топливо He-3, Мт | 8.0 | PROPOSAL · PUBLIC (модель) |
| Ускорение разгона, g | 0.0073 | PROPOSAL · PUBLIC (модель) |
| Мощность струи, ПВт | 7.7 | PROPOSAL · PUBLIC (модель) |
| Мощность на крейсерском участке, ГВт | 0.35 | PROPOSAL · PUBLIC (модель) |
| Радиаторы разгона / крейсера, км² | 52.1 / 0.41 | PROPOSAL · PUBLIC (модель) |
| Вердикт physics.py | fix: сухая масса 4.0 Мт против заявленных 18 Мт | PROPOSAL · PUBLIC (модель) |
| Доля крио | 0.55 | PROPOSAL · PUBLIC |
| Размеры, двигатель, груз | нет полей в машинном каноне — см. досье | — |
<!-- GENERATED:ship END -->

### Как выглядит корабль

<!-- NARRATIVE:ship_description -->
_TODO: облик, размеры, модули, двигатель, груз и запасы по досье; не противоречить расчёту physics.py (если досье расходится — это помечено в вердикте)._

## Население

<!-- GENERATED:population BEGIN -->
| Параметр | Значение | Статус |
|---|---|---|
| Старт: людей | 18 000 | PROPOSAL · PUBLIC |
| Бодрствующие / крио | 8 100 / 9 900 | PROPOSAL · PUBLIC |
| Вахта (минимум функций по модели) | 1 200 (325) | PROPOSAL · PUBLIC |
| Лет вахты / безопасных циклов | 15 / 2 | PROPOSAL · PUBLIC (досье) |
| Политика рождений | replacement_awake | PROPOSAL · PUBLIC (досье) |
| Наследственная вахта / хранители | да / 58 | PROPOSAL · PUBLIC (модель) |
| Сон владельцев, лет | 143 | PROPOSAL · PUBLIC (модель) |
| Неучтённые потери | 1 386 | PROPOSAL · PUBLIC (модель) |

Когорты (доля / доля бодрствующих):

| Когорта | Доля | Бодрствует | Культура |
|---|---|---|---|
| engineers | 0.45 | 0.50 | как у миссии |
| workers | 0.26 | 0.30 | как у миссии |
| patricians | 0.15 | 0.10 | как у миссии |
| scientists | 0.10 | 0.40 | как у миссии |
| command | 0.04 | 1.00 | как у миссии |
<!-- GENERATED:population END -->

## Хроника полёта

<!-- GENERATED:flight BEGIN -->
Значимые события: якоря канона, смена режима, потери от 1 % людей; 0 мелких заполняющих событий модели (seed-зависимых) опущены — полный список в `out/lab_report.md` §2.

| Год | Событие | Население | Awake | Cryo | Власть/режим | Техсостояние | Статус |
|---|---|---|---|---|---|---|---|
| 2120 | launch: старт: 18000 чел., крио 55 % | 18 000 | 8 100 | 9 900 | олигархия | 1.00 | PROPOSAL · PUBLIC |
| 2235 | mission_fatigue: усталость от миссии: «зачем лететь» | 17 340 | 8 100 | 9 240 | партийный режим | 0.93 | PROPOSAL · PUBLIC (модель) |
| 2263 | arrival: Прибытие к Тау Кита; «Старшие семьи» Мэйфлауэра против «Новой крови» | 15 800 | 15 800 | 0 | технократия | 0.92 | PROPOSAL · PUBLIC |
| 2270 | merger: Слияние двух колоний; главный хаб GENESIS-1 даёт имя городу — «Новый Шанхай» | 15 800 | 15 800 | 0 | технократия | 0.92 | PROPOSAL · PUBLIC |
<!-- GENERATED:flight END -->

### Жизнь на борту (рассказ)

<!-- NARRATIVE:flight_story -->
_TODO: быт, конфликты и смена власти между событиями таблицы; числа брать только из таблицы._

## Итог

<!-- GENERATED:outcome BEGIN -->
| Поле | Значение | Статус |
|---|---|---|
| Итог | arrived | PROPOSAL · PUBLIC |
| Корабль сейчас | station_core | PROPOSAL · PUBLIC |
| Примечание | ядро станции «Новый Шанхай» | PROPOSAL · PUBLIC |
| Можно найти в игре | да | PROPOSAL · PUBLIC |
<!-- GENERATED:outcome END -->

## Прибытие и высадка

<!-- NARRATIVE:arrival -->
_TODO: что увидели у цели, первая высадка/стройка, кто взял власть, что стало с кораблём; для не долетевших — «нет»/последний контакт._
